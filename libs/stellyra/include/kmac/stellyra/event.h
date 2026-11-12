#pragma once
#ifndef KMAC_STELLYRA_EVENT_H
#define KMAC_STELLYRA_EVENT_H

/**
 * @file event.h
 * @brief RecursiveEvent / SharedEvent / SingleThreadedEvent - the triggerable
 * event aliases, fixed to a particular MutexType.
 *
 * Each is a thin alias over GenericEvent (see basic_event.h) that resolves
 * the function-style signature form (event_signature.h) before naming it,
 * the same guarantee BasicEvent gives for a custom MutexType.
 */

#include "basic_event.h"
#include "event_signature.h"
#include "platform.h"

namespace kmac {
namespace stellyra {

/**
 * @brief Thread-safe event using a recursive mutex - the default choice.
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

#endif // KMAC_STELLYRA_EVENT_H
