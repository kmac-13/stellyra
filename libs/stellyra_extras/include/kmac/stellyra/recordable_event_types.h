#pragma once
#ifndef KMAC_STELLYRA_RECORDABLE_EVENT_TYPES_H
#define KMAC_STELLYRA_RECORDABLE_EVENT_TYPES_H

/**
 * @file recordable_event_types.h
 * @brief Convenient aliases over GenericRecordableEvent that support
 * function-style syntax with some specifying mutex types.
 *
 * BasicRecordableEvent is a thin alias over GenericRecordableEvent (see
 * generic_recordable_event.h) that supports the same template arguments and
 * signature as GenericRecordableEvent but adds a function-style signature
 * form (see event_signature.h) before resolving to GenericRecordableEvent.
 * The other aliases - RecursiveRecordableEvent, SharedRecordableEvent, and
 * SingleThreadedRecordableEvent - all resolve to GenericRecordableEvent with
 * specific mutex types (see platform.h) while also supporting the
 * function-style syntax option.
 */

#include "generic_recordable_event.h"

#include <kmac/stellyra/event_signature.h>

namespace kmac {
namespace stellyra {

/**
 * @brief BasicRecordableEvent<MutexType, Args...> - a resolving alias over
 * GenericRecordableEvent.
 *
 * Resolves the function-style signature form (see event_signature.h)
 * before naming GenericRecordableEvent, exactly the way BasicEvent resolves
 * it before naming GenericEvent - so BasicRecordableEvent<M, void(Args...)>
 * and BasicRecordableEvent<M, Args...> are always the exact same type.  For
 * a plain Args... pack the two names are interchangeable; use
 * BasicRecordableEvent when the function-style form is wanted,
 * GenericRecordableEvent otherwise.
 */
template< typename MutexType, typename... Args >
using BasicRecordableEvent = typename detail::ResolveEventArgs< GenericRecordableEvent, MutexType, Args... >::type;

/**
 * @brief Recordable event using RecursiveMutex, matching
 * RecursiveEvent<Args...>.
 *
 * Accepts either the plain argument pack
 * (RecursiveRecordableEvent<int, std::string>) or the equivalent
 * function-style signature (RecursiveRecordableEvent<void(int, std::string)>)
 * (see event_signature.h).
 */
template< typename... Args >
using RecursiveRecordableEvent = typename detail::ResolveEventArgs< GenericRecordableEvent, platform::RecursiveMutex, Args... >::type;

/**
 * @brief Recordable event using SharedMutex, matching
 * SharedEvent<Args...>.
 *
 * Accepts the function-style signature form too - see
 * RecursiveRecordableEvent above.
 */
template< typename... Args >
using SharedRecordableEvent = typename detail::ResolveEventArgs< GenericRecordableEvent, platform::SharedMutex, Args... >::type;

/**
 * @brief Recordable event using NullMutex, matching
 * SingleThreadedEvent<Args...>.
 *
 * Accepts the function-style signature form too - see
 * RecursiveRecordableEvent above.
 */
template< typename... Args >
using SingleThreadedRecordableEvent = typename detail::ResolveEventArgs< GenericRecordableEvent, platform::NullMutex, Args... >::type;

} // namespace stellyra
} // namespace kmac

#endif // KMAC_STELLYRA_RECORDABLE_EVENT_TYPES_H
