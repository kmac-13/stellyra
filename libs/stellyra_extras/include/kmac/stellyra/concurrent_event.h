#pragma once
#ifndef KMAC_STELLYRA_CONCURRENT_EVENT_H
#define KMAC_STELLYRA_CONCURRENT_EVENT_H

/**
 * @file concurrent_event.h
 *
 * @brief Convenience wrapper for basic_concurrent_event.h inclusion along
 * with common aliases for GenericConcurrentEvent using specific mutex types.
 */

#include "basic_concurrent_event.h"

#include <kmac/stellyra/event_signature.h>

namespace kmac {
namespace stellyra {

/**
 * @brief Thread-safe GenericConcurrentEvent using a standard mutex for
 * connect/disconnect - the default choice, and the primary event type for
 * code interested in high-performance concurrent event dispatch.
 *
 * Dispatch itself is always lock-free (epoch-based) regardless of MutexType
 * - see basic_concurrent_event_impl.h; MutexType only ever governs
 * connect()/disconnect() contention.
 *
 * Accepts either the plain argument pack (ConcurrentEvent<int, std::string>)
 * or the equivalent function-style signature
 * (ConcurrentEvent<void(int, std::string)>) - see event_signature.h.
 */
template< typename... Args >
using ConcurrentEvent = typename detail::ResolveEventArgs< GenericConcurrentEvent, platform::Mutex, Args... >::type;

/**
 * @brief Single-threaded GenericConcurrentEvent using a no-op mutex: no
 * connect/disconnect locking overhead at all.  Only safe when the every one
 * of the event's connections is confined to a single thread.
 *
 * Accepts the function-style signature form too - see ConcurrentEvent above.
 */
template< typename... Args >
using SingleThreadedConcurrentEvent = typename detail::ResolveEventArgs< GenericConcurrentEvent, platform::NullMutex, Args... >::type;

} // namespace stellyra
} // namespace kmac

#endif // KMAC_STELLYRA_CONCURRENT_EVENT_H
