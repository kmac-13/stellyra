#pragma once
#ifndef KMAC_STELLYRA_BASIC_CONCURRENT_EVENT_H
#define KMAC_STELLYRA_BASIC_CONCURRENT_EVENT_H

/**
 * @file basic_concurrent_event.h
 * @brief GenericConcurrentEvent<MutexType, Args...> - the triggerable, primary
 * event type, backed by BasicConcurrentEventImpl through the generalised
 * EventStorage, and BasicConcurrentEvent<MutexType, Args...> - a resolving
 * alias over it that additionally accepts the function-style signature
 * form (see event_signature.h), mirroring GenericEvent/BasicEvent exactly.
 *
 * Mirrors GenericEvent exactly in shape: EventStorage supplies the full
 * connect/connectFree/connectLambda/disconnect/ConnParams/forwardTo
 * surface generically, this file adds only operator()/trigger()/emit().
 *
 * Direct, Deferred, and Auto-resolved connections are all supported, as is
 * sender-deferred triggering (see operator()'s docs below) - the connect-
 * side API is the full one GenericEvent offers today, and dispatch behavior
 * matches it.
 */

#include "basic_concurrent_event_impl.h"

#include <kmac/stellyra/event_loop.h>
#include <kmac/stellyra/event_signature.h>
#include <kmac/stellyra/event_storage.h>
#include <kmac/stellyra/platform.h>
#include <kmac/stellyra/trackable.h>

#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>

namespace kmac {
namespace stellyra {

/**
 * @brief Triggerable concurrent event backed by BasicConcurrentEventImpl.
 *
 * Inherits connection management from EventStorage and adds
 * operator() / trigger() / emit() for dispatching to connected handlers.
 *
 * This is the underlying class; BasicConcurrentEvent (below) is the alias
 * calling code should name instead when the function-style signature form
 * is wanted - for a plain Args... pack the two names are interchangeable.
 *
 * @tparam MutexType synchronisation strategy
 * @tparam Args argument types forwarded to connected handlers
 */
template< typename MutexType, typename... Args >
class GenericConcurrentEvent : public EventStorage< BasicConcurrentEventImpl, MutexType, Args... >
{
	using Base = EventStorage< BasicConcurrentEventImpl, MutexType, Args... >;

public:
	using Base::Base;  // inherit constructors

	~GenericConcurrentEvent() = default;

	GenericConcurrentEvent( const GenericConcurrentEvent& ) = delete;
	GenericConcurrentEvent& operator=( const GenericConcurrentEvent& ) = delete;
	GenericConcurrentEvent( GenericConcurrentEvent&& ) = default;
	GenericConcurrentEvent& operator=( GenericConcurrentEvent&& ) = default;

	/**
	 * @brief Trigger the event - dispatch to all connected, unblocked,
	 * predicate-passing handlers.
	 *
	 * Sender-deferred: if this event's owner has an EventLoop and the
	 * calling thread isn't that loop's designated context (its registered
	 * drain thread, or the thread currently draining it - see
	 * EventLoop::shouldDispatchDirectlyOnThread()), the trigger itself is
	 * posted to that loop instead of dispatching here and now - the
	 * *whole* dispatch() call happens later, on the owner's home thread,
	 * against whatever the handler list looks like at that later time.
	 * Mirrors GenericEvent::operator() (basic_event.h); dispatch() re-enters
	 * the epoch fresh whenever it actually runs, so nothing about a deferred
	 * trigger needs to survive the interval except the event's own weak
	 * reference and the captured arguments.
	 *
	 * Once the sender-deferral gate above lets a trigger through (or
	 * doesn't apply), dispatch is still always synchronous and lock-free
	 * on whichever thread actually runs it.
	 *
	 * Silently dropped while the event is blocked, checked before either
	 * path above.
	 */
	void operator()( Args... args );

	/**
	 * @brief Trigger the event.
	 *
	 * Identical to operator().
	 */
	void trigger( Args... args );

	/**
	 * @brief Trigger the event.
	 *
	 * Identical to operator().
	 */
	void emit( Args... args );
};

template< typename MutexType, typename... Args >
inline void GenericConcurrentEvent< MutexType, Args... >::operator()( Args... args )
{
	if ( this->_impl->isBlocked() )
	{
		return;
	}

	Trackable* owner = this->_impl->owner;
	EventLoop* senderLoop = owner ? owner->eventLoop() : nullptr;

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
		// - GenericEvent::operator()
		// - BasicConcurrentEventImpl::dispatch
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
inline void GenericConcurrentEvent< MutexType, Args... >::trigger( Args... args )
{
	operator()( std::forward< Args >( args )... );
}

template< typename MutexType, typename... Args >
inline void GenericConcurrentEvent< MutexType, Args... >::emit( Args... args )
{
	operator()( std::forward< Args >( args )... );
}

/**
 * @brief BasicConcurrentEvent<MutexType, Args...> - a resolving alias over
 * GenericConcurrentEvent.
 *
 * Resolves the function-style signature form (see event_signature.h)
 * before naming GenericConcurrentEvent, exactly the way BasicEvent resolves
 * it before naming GenericEvent - so BasicConcurrentEvent<M, void(Args...)>
 * and BasicConcurrentEvent<M, Args...> are always the exact same type.  For
 * a plain Args... pack, the two names are interchangeable; use
 * BasicConcurrentEvent when the function-style form is required.
 */
template< typename MutexType, typename... Args >
using BasicConcurrentEvent = typename detail::ResolveEventArgs< GenericConcurrentEvent, MutexType, Args... >::type;

} // namespace stellyra
} // namespace kmac

#endif // KMAC_STELLYRA_BASIC_CONCURRENT_EVENT_H
