#pragma once
#ifndef KMAC_STELLYRA_CONCURRENT_SIGNAL_TYPES_H
#define KMAC_STELLYRA_CONCURRENT_SIGNAL_TYPES_H

/**
 * @file concurrent_signal_types.h
 * @brief Convenient aliases for GenericConcurrentEvent for users that prefer
 * the signal/emit terminology over Event.
 */

#include "concurrent_event_types.h"

namespace kmac {
namespace stellyra {

/**
 * @brief BasicConcurrentSignal<MutexType, Args...> - a resolving alias
 * matching BasicConcurrentEvent.
 *
 * @see BasicConcurrentEvent
 */
template< typename MutexType, typename... Args >
using BasicConcurrentSignal = BasicConcurrentEvent< MutexType, Args... >;

/**
 * @brief StandardConcurrentSignal<Args...> - a resolving alias matching
 * StandardConcurrentEvent.
 *
 * @see StandardConcurrentEvent
 */
template< typename... Args >
using StandardConcurrentSignal = StandardConcurrentEvent< Args... >;

/**
 * @brief SingleThreadedConcurrentSignal<Args...> - a resolving alias matching
 * SingleThreadedConcurrentEvent.
 *
 * @see SingleThreadedConcurrentEvent
 */
template< typename... Args >
using SingleThreadedConcurrentSignal = SingleThreadedConcurrentEvent< Args... >;

} // namespace stellyra
} // namespace kmac

#endif // KMAC_STELLYRA_CONCURRENT_SIGNAL_TYPES_H
