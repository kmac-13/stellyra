#pragma once
#ifndef KMAC_STELLYRA_BASIC_EVENT_H
#define KMAC_STELLYRA_BASIC_EVENT_H

/**
 * @file event.h
 * @brief BasicEvent<MutexType, Args...> - the triggerable event type.
 *
 * BasicEvent inherits all connection management from EventStorage and adds
 * operator() / trigger() / emit() for dispatching to connected handlers.
 *
 * Public API is focused here.  Implementation details live in:
 *   event_storage.h : EventStorage<MutexType, Args...> - connect/disconnect/block
 *   event_flags.h   : EventFlags                       - bit-field flag wrapper
 *   event_impl.h    : EventImpl<MutexType, Args...>    - dispatch, mutex, handlers
 *   handler_entry.h : HandlerEntry<Args...>            - per-connection slot
 */

#include "basic_event_impl.h"
#include "event_storage.h"

#include <memory>
#include <tuple>
#include <utility>

namespace kmac {
namespace stellyra {

// ===========================================================================
// BasicEvent
// ===========================================================================

/**
 * @brief Triggerable event.  Inherits connection management from EventStorage
 * and adds operator() / trigger() / emit().
 *
 * @tparam MutexType synchronisation strategy
 * @tparam Args argument types forwarded to connected handlers
 */
template< typename MutexType, typename... Args >
class BasicEvent : public EventStorage< kmac::stellyra::BasicEventImpl, MutexType, Args... >
{
	using Base = EventStorage< kmac::stellyra::BasicEventImpl, MutexType, Args... >;

public:
	using Base::Base;  // inherit constructors

	~BasicEvent() = default;

	BasicEvent( const BasicEvent& ) = delete;
	BasicEvent& operator=( const BasicEvent& ) = delete;
	BasicEvent( BasicEvent&& ) = default;
	BasicEvent& operator=( BasicEvent&& ) = default;

	/**
	 * @brief Trigger the event - dispatch to all connected handlers.
	 *
	 * If the sender has an EventLoop, dispatch happens synchronously on the
	 * calling thread only when that thread should dispatch directly for
	 * this loop (see EventLoop::shouldDispatchDirectlyOnThread) - either
	 * because it has been registered as the loop's designated thread, or
	 * because it is, right now, the thread executing this loop's drain().
	 * Otherwise, the whole trigger (including Direct connections) is
	 * posted to the sender's EventLoop and runs whenever that loop is next
	 * drained.
	 *
	 * If the sender has no EventLoop, dispatch always happens synchronously.
	 *
	 * Silently dropped while the event is blocked.
	 */
	void operator()( Args... args );

	/// Identical to operator().
	void trigger( Args... args );

	/// Identical to operator().
	void emit( Args... args );

	/**
	 * @brief Connects `sourceEvent` so that triggering it also triggers
	 * *this* event - the destination-side counterpart to
	 * EventStorage::forwardTo().
	 *
	 * forwardTo() is written on the source ("xChanged.forwardTo(moved)")
	 * and needs `&TargetEvent::operator()` to be nameable (and therefore
	 * accessible) at the call site, which fails to compile when the target
	 * is a BasicPrivateEvent.  forwardFrom() is written on the destination
	 * ("moved.forwardFrom(xChanged)") instead, and - rather than duplicate
	 * forwardTo()'s machinery - just delegates to it with the roles
	 * swapped: `sourceEvent.forwardTo( *this, ... )`.  The key is *where*
	 * that delegation happens: since forwardFrom() is itself a member of
	 * BasicEvent, `*this` here has the plain `BasicEvent<MutexType, Args...>`
	 * static type, never whatever derived type (e.g. BasicPrivateEvent)
	 * the actual object might be - so forwardTo() deduces its TargetEvent
	 * template parameter as that plain type, and the `&TargetEvent::operator()`
	 * it names is always `&BasicEvent<MutexType,Args...>::operator()`, never
	 * a derived type's name.  This is what lets forwardFrom() work when
	 * `this` is (via the derived BasicPrivateEvent's using-declaration)
	 * actually a private event: the derived type's name, and therefore its
	 * access restriction, never enters into it.
	 *
	 * `sourceEvent`'s argument list must have this event's Args... as its
	 * first-N prefix - the same createPartial-enforced rule forwardTo()
	 * documents, just applied in the other direction.  The forwarding
	 * connection's tracker is auto-sourced from *this* event's own
	 * owner() (not sourceEvent's), mirroring forwardTo()'s "track from the
	 * trigger side" convention, reversed; if this event has no owner, the
	 * forwarding connection is untracked, the same trust model as
	 * connectFree() with no tracker.
	 *
	 * On a BasicPrivateEvent, forwardFrom() is reachable only by the
	 * event's listed friend type(s) - it sits with operator()/trigger()/
	 * emit() behind that restriction, not with the public connect-style
	 * API, because wiring a source event to forward into this one hands
	 * whoever can fire that source the power to fire this event too.
	 *
	 * @code
	 * moved.forwardFrom( xChanged );  // xChanged(int) -> moved()
	 * @endcode
	 */
	template< typename SourceEvent >
	Connection forwardFrom(
		SourceEvent& sourceEvent,
		ConnectionType type = ConnectionType::Auto,
		typename SourceEvent::ConnParams params = {} );

	/**
	 * @brief Same as the type-first forwardFrom() above, with the last two
	 * arguments swapped: moved.forwardFrom( xChanged, connParams, connType );
	 */
	template< typename SourceEvent >
	Connection forwardFrom(
		SourceEvent& sourceEvent,
		typename SourceEvent::ConnParams params,
		ConnectionType type = ConnectionType::Auto );
};


// ===========================================================================
// operator() / trigger / emit
// ===========================================================================

template< typename MutexType, typename... Args >
inline void BasicEvent< MutexType, Args... >::operator()( Args... args )
{
	if ( this->_impl->isBlocked() )
	{
		return;
	}

	EventLoop* senderLoop = this->_impl->owner ? this->_impl->owner->eventLoop() : nullptr;

	if ( senderLoop
		&& ! senderLoop->shouldDispatchDirectlyOnThread( platform::currentThreadId() ) )
	{
		auto capturedArgs =
			std::make_shared< std::tuple< std::decay_t< Args >... > >(
				std::forward< Args >( args )... );

		// pass the shared pointer to the task by value to ensure the impl
		// remains valid for at least until the posted task is drained
		// regardless of whether the event is still valid or not;
		// NOTE that the same strong reference requirement is needed for:
		// - BasicEventImpl::dispatch
		// - BasicConcurrentEventImpl::dispatch
		// - BasicConcurrentEvent::operator()
		// - BasicRecordableEvent::operator()
		auto self = this->_impl;
		senderLoop->post(
			EventLoop::Task::create(
				[ self, capturedArgs ]() {
					std::apply(
						[ &self ]( auto&&... a ) {
							self->dispatch( std::forward< decltype( a ) >( a )... );
						},
						*capturedArgs );
				} ),
			this->_impl->id );
		return;
	}

	this->_impl->dispatch( std::forward< Args >( args )... );
}

template< typename MutexType, typename... Args >
inline void BasicEvent< MutexType, Args... >::trigger( Args... args )
{
	operator()( std::forward< Args >( args )... );
}

template< typename MutexType, typename... Args >
inline void BasicEvent< MutexType, Args... >::emit( Args... args )
{
	operator()( std::forward< Args >( args )... );
}


// ===========================================================================
// forwardFrom
// ===========================================================================

template< typename MutexType, typename... Args >
template< typename SourceEvent >
inline Connection BasicEvent< MutexType, Args... >::forwardFrom(
	SourceEvent& sourceEvent,
	ConnectionType type,
	typename SourceEvent::ConnParams params )
{
	return sourceEvent.forwardTo( *this, type, std::move( params ) );
}

template< typename MutexType, typename... Args >
template< typename SourceEvent >
inline Connection BasicEvent< MutexType, Args... >::forwardFrom(
	SourceEvent& sourceEvent,
	typename SourceEvent::ConnParams params,
	ConnectionType type )
{
	return forwardFrom( sourceEvent, type, std::move( params ) );
}

} // namespace stellyra
} // namespace kmac

#endif // KMAC_STELLYRA_BASIC_EVENT_H
