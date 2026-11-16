#pragma once
#ifndef KMAC_STELLYRA_GENERIC_EVENT_H
#define KMAC_STELLYRA_GENERIC_EVENT_H

/**
 * @file generic_event.h
 * @brief GenericEvent<MutexType, Args...> - the triggerable event type, and
 * BasicEvent<MutexType, Args...> - a resolving alias over it.
 *
 * GenericEvent inherits all connection management from EventStorage and adds
 * operator() / trigger() / emit() for dispatching to connected handlers.
 * BasicEvent is a thin alias over GenericEvent that additionally accepts the
 * function-style signature form (see event_signature.h) - both options,
 * BasicEvent<M, Args...> and BasicEvent<M, void(Args...)>, resolve to the
 * exact same GenericEvent<M, Args...> type, the same guarantee
 * RecursiveEvent/SharedEvent/SingleThreadedEvent already give.  For a plain
 * Args... pack the two names are simply interchangeable
 * (BasicEvent<M, Args...> and GenericEvent<M, Args...> are, again, the same
 * type) - only BasicEvent supports the function-style, otherwise either
 * BasicEvent or GenericEvent, whichever reads better at the call site.
 * Friend grants, forward declarations, and inheritance elsewhere in this
 * library name GenericEvent directly because an alias template has no
 * incomplete form, so it can't be forward-declared or used as a
 * friend-template/template-template-parameter target - not because
 * GenericEvent itself is discouraged for ordinary use.
 *
 * Public API is focused here.  Implementation details live in:
 *   event_storage.h    : EventStorage<MutexType, Args...>   - connect/disconnect/block
 *   event_flags.h      : EventFlags                         - bit-field flag wrapper
 *   basic_event_impl.h : BasicEventImpl<MutexType, Args...> - dispatch, mutex, handlers
 *   handler_entry.h    : HandlerEntry<Args...>              - per-connection slot
 */

#include "basic_event_impl.h"
#include "event_storage.h"

#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>

namespace kmac {
namespace stellyra {

// ===========================================================================
// GenericEvent
// ===========================================================================

/**
 * @brief Triggerable event.  Inherits connection management from EventStorage
 * and adds operator() / trigger() / emit().
 *
 * This is the underlying class; BasicEvent (below) is the alias calling code
 * should name instead - it additionally accepts the function-style
 * signature form.
 *
 * @tparam MutexType synchronisation strategy
 * @tparam Args argument types forwarded to connected handlers
 */
template< typename MutexType, typename... Args >
class GenericEvent : public EventStorage< kmac::stellyra::BasicEventImpl, MutexType, Args... >
{
	using Base = EventStorage< kmac::stellyra::BasicEventImpl, MutexType, Args... >;

public:
	using Base::Base;  // inherit constructors

	~GenericEvent() = default;

	GenericEvent( const GenericEvent& ) = delete;
	GenericEvent& operator=( const GenericEvent& ) = delete;
	GenericEvent( GenericEvent&& ) = default;
	GenericEvent& operator=( GenericEvent&& ) = default;

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
	 * GenericEvent, `*this` here has the plain
	 * `GenericEvent<MutexType, Args...>` static type, never whatever derived
	 * type (e.g. BasicPrivateEvent) the actual object might be - so
	 * forwardTo() deduces its TargetEvent template parameter as that plain
	 * type, and the `&TargetEvent::operator()` it names is always
	 * `&GenericEvent<MutexType,Args...>::operator()`, never a derived type's
	 * name.  This is what lets forwardFrom() work when `this` is (via the
	 * derived  BasicPrivateEvent's using-declaration) actually a private
	 * event: the  derived type's name, and therefore its access restriction,
	 * never enters into it.
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
inline void GenericEvent< MutexType, Args... >::operator()( Args... args )
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
		// - GenericConcurrentEvent::operator()
		// - GenericRecordableEvent::operator()
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
inline void GenericEvent< MutexType, Args... >::trigger( Args... args )
{
	operator()( std::forward< Args >( args )... );
}

template< typename MutexType, typename... Args >
inline void GenericEvent< MutexType, Args... >::emit( Args... args )
{
	operator()( std::forward< Args >( args )... );
}


// ===========================================================================
// forwardFrom
// ===========================================================================

template< typename MutexType, typename... Args >
template< typename SourceEvent >
inline Connection GenericEvent< MutexType, Args... >::forwardFrom(
	SourceEvent& sourceEvent,
	ConnectionType type,
	typename SourceEvent::ConnParams params )
{
	return sourceEvent.forwardTo( *this, type, std::move( params ) );
}

template< typename MutexType, typename... Args >
template< typename SourceEvent >
inline Connection GenericEvent< MutexType, Args... >::forwardFrom(
	SourceEvent& sourceEvent,
	typename SourceEvent::ConnParams params,
	ConnectionType type )
{
	return forwardFrom( sourceEvent, type, std::move( params ) );
}

} // namespace stellyra
} // namespace kmac

#endif // KMAC_STELLYRA_GENERIC_EVENT_H
