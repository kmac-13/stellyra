#pragma once
#ifndef KMAC_STELLYRA_RECORDABLE_EVENT_H
#define KMAC_STELLYRA_RECORDABLE_EVENT_H

/**
 * @file recordable_event.h
 *
 * @brief Convenience wrapper for basic_recordable_event.h inclusion along
 * with common aliases for GenericRecordableEvent using specific mutex types.
 */

#include "basic_recordable_event.h"

#include <kmac/stellyra/event_signature.h>

namespace kmac {
namespace stellyra {

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
 * @brief Alias for users that prefer signal/emit terminology.
 */
template< typename... Args >
using RecursiveRecordableSignal = RecursiveRecordableEvent< Args... >;

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
 * @brief Alias for users that prefer signal/emit terminology.
 */
template< typename... Args >
using SharedRecordableSignal = SharedRecordableEvent< Args... >;

/**
 * @brief Recordable event using NullMutex, matching
 * SingleThreadedEvent<Args...>.
 *
 * Accepts the function-style signature form too - see
 * RecursiveRecordableEvent above.
 */
template< typename... Args >
using SingleThreadedRecordableEvent = typename detail::ResolveEventArgs< GenericRecordableEvent, platform::NullMutex, Args... >::type;

/**
 * @brief Alias for users that prefer signal/emit terminology.
 */
template< typename... Args >
using SingleThreadedRecordableSignal = SingleThreadedRecordableEvent< Args... >;

} // namespace stellyra
} // namespace kmac

#endif // KMAC_STELLYRA_RECORDABLE_EVENT_H
