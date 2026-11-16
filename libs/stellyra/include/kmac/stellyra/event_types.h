#pragma once
#ifndef KMAC_STELLYRA_EVENT_TYPES_H
#define KMAC_STELLYRA_EVENT_TYPES_H

/**
 * @file event_types.h
 * @brief Convenient aliases over GenericEvent that support function-style
 * syntax with some specifying mutex types.
 *
 * BasicEvent is a thin alias over GenericEvent (see generic_event.h) that
 * supports the same template arguments and signature as GenericEvent but adds
 * a function-style signature form (see event_signature.h) before resolving
 * to GenericEvent.  The other aliases - RecursiveEvent, SharedEvent, and
 * SingleThreadedEvent - all resolve to GenericEvent with specific mutex types
 * (see platform.h) while also supporting the function-style syntax option.
 */

#include "event_signature.h"
#include "generic_event.h"
#include "platform.h"

namespace kmac {
namespace stellyra {

/**
 * @brief BasicEvent<MutexType, Args...> - a resolving alias over
 * GenericEvent.
 *
 * Resolves the function-style signature form (see event_signature.h) before
 * naming GenericEvent - so BasicEvent<M, void(Args...)> and
 * BasicEvent<M, Args...> are always the exact same type:
 *
 * @code
 * static_assert( std::is_same_v<
 *     BasicEvent< CustomMutex, void( int, std::string ) >,
 *     BasicEvent< CustomMutex, int, std::string > > );
 * @endcode
 *
 * This is what makes a custom MutexType - any mutex the library's own
 * Event/SharedEvent/SingleThreadedEvent aliases don't already cover - get
 * the function-style spelling with the same identity guarantee those
 * aliases give, rather than a derived-but-distinct type.
 */
template< typename MutexType, typename... Args >
using BasicEvent = typename detail::ResolveEventArgs< GenericEvent, MutexType, Args... >::type;

/**
 * @brief Thread-safe event using a recursive mutex.
 * connect()/disconnect()/trigger() may be called from any thread, and a
 * handler may safely reentrantly connect, disconnect, or trigger on this
 * same event while it fires.  See SharedEvent for concurrent-reader
 * dispatch, or SingleThreadedEvent when the event is confined to one
 * thread and the locking overhead isn't needed at all.
 *
 * Accepts either the plain argument pack (RecursiveEvent<int, std::string>)
 * or the equivalent function-style signature
 * (RecursiveEvent<void(int, std::string)>) - see event_signature.h for
 * details, including named parameters.
 */
template< typename... Args >
using RecursiveEvent = typename detail::ResolveEventArgs< GenericEvent, platform::RecursiveMutex, Args... >::type;

/**
 * @brief Thread-safe event using a shared (reader-writer) mutex: multiple
 * threads may trigger() concurrently and dispatch in parallel, at the cost
 * of connect()/disconnect() taking the exclusive lock (blocking until any
 * in-progress triggers finish) and single-shot connections being unsupported
 * (ConnParams::once() static_asserts) - a handler disconnecting itself while
 * a SharedLock is held would deadlock.
 *
 * Accepts the function-style signature form too - see RecursiveEvent above.
 */
template< typename... Args >
using SharedEvent = typename detail::ResolveEventArgs< GenericEvent, platform::SharedMutex, Args... >::type;

/**
 * @brief Single-threaded event using a no-op mutex: no locking overhead at
 * all.  Only safe when the event and every one of its connections and
 * triggers are confined to a single thread - using it from more than one
 * thread is undefined behaviour, not a runtime-checked error.
 *
 * Accepts the function-style signature form too - see RecursiveEvent above.
 */
template< typename... Args >
using SingleThreadedEvent = typename detail::ResolveEventArgs< GenericEvent, platform::NullMutex, Args... >::type;

} // namespace stellyra
} // namespace kmac

#endif // KMAC_STELLYRA_EVENT_TYPES_H
