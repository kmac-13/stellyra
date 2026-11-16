#pragma once
#ifndef KMAC_STELLYRA_CONCURRENT_EVENT_TYPES_H
#define KMAC_STELLYRA_CONCURRENT_EVENT_TYPES_H

/**
 * @file concurrent_event_types.h
 *
 * @brief Convenient aliases over GenericConcurrentEvent that support
 * function-style syntax with some specifying mutex types.
 */

#include "generic_concurrent_event.h"

#include <kmac/stellyra/event_signature.h>

namespace kmac {
namespace stellyra {

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

/**
 * @brief Thread-safe GenericConcurrentEvent using a standard mutex for
 * connect/disconnect.
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
using StandardConcurrentEvent = typename detail::ResolveEventArgs< GenericConcurrentEvent, platform::Mutex, Args... >::type;

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

#endif // KMAC_STELLYRA_CONCURRENT_EVENT_TYPES_H
