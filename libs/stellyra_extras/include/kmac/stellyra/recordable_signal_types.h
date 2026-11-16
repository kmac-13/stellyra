#pragma once
#ifndef KMAC_STELLYRA_RECORDABLE_SIGNAL_TYPES_H
#define KMAC_STELLYRA_RECORDABLE_SIGNAL_TYPES_H

/**
 * @file recordable_signal_types.h
 * @brief Convenient aliases for GenericRecordableEvent for users that prefer
 * the signal/emit terminology over Event.
 */

#include "recordable_event_types.h"

namespace kmac {
namespace stellyra {

/**
 * @brief BasicRecordableSignal<MutexType, Args...> - a resolving alias
 * matching BasicRecordableEvent.
 *
 * @see BasicRecordableEvent
 */
template< typename MutexType, typename... Args >
using BasicRecordableSignal = BasicRecordableEvent< MutexType, Args... >;

/**
 * @brief RecursiveRecordableSignal<Args...> - a resolving alias matching
 * RecursiveRecordableEvent.
 *
 * @see RecursiveRecordableEvent
 */
template< typename... Args >
using RecursiveRecordableSignal = RecursiveRecordableEvent< Args... >;

/**
 * @brief SharedRecordableSignal<Args...> - a resolving alias matching
 * SharedRecordableEvent.
 *
 * @see SharedRecordableEvent
 */
template< typename... Args >
using SharedRecordableSignal = SharedRecordableEvent< Args... >;

/**
 * @brief SingleThreadedRecordableSignal<Args...> - a resolving alias matching
 * SingleThreadedRecordableEvent.
 *
 * @see SingleThreadedRecordableEvent
 */
template< typename... Args >
using SingleThreadedRecordableSignal = SingleThreadedRecordableEvent< Args... >;

} // namespace stellyra
} // namespace kmac

#endif // KMAC_STELLYRA_RECORDABLE_SIGNAL_TYPES_H
