#pragma once
#ifndef KMAC_STELLYRA_SIGNAL_TYPES_H
#define KMAC_STELLYRA_SIGNAL_TYPES_H

/**
 * @file signal_types.h
 * @brief Convenient aliases for GenericEvent for users that prefer the
 * signal/emit terminology over event.
 */

#include "event_types.h"

namespace kmac {
namespace stellyra {

/**
 * @brief BasicSignal<MutexType, Args...> - a resolving alias matching
 * BasicEvent.
 *
 * @see BasicEvent
 */
template< typename MutexType, typename... Args >
using BasicSignal = BasicEvent< MutexType, Args... >;

/**
 * @brief RecursiveSignal<Args...> - a resolving alias matching
 * RecursiveEvent.
 *
 * @see RecursiveEvent
 */
template< typename... Args >
using RecursiveSignal = RecursiveEvent< Args... >;

/**
 * @brief SharedSignal<Args...> - a resolving alias matching SharedEvent.
 *
 * @see SharedEvent
 */
template< typename... Args >
using SharedSignal = SharedEvent< Args... >;

/**
 * @brief SingleThreadedSignal<Args...> - a resolving alias matching
 * SingleThreadedEvent.
 *
 * @see SingleThreadedEvent
 */
template< typename... Args >
using SingleThreadedSignal = SingleThreadedEvent< Args... >;

} // namespace stellyra
} // namespace kmac

#endif // KMAC_STELLYRA_SIGNAL_TYPES_H
