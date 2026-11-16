#pragma once
#ifndef KMAC_STELLYRA_PRIVATE_SIGNAL_TYPES_H
#define KMAC_STELLYRA_PRIVATE_SIGNAL_TYPES_H

/**
 * @file private_signal_types.h
 * @brief Convenient aliases for GenericPrivateEvent for users that prefer the
 * signal/emit terminology over Event.
 */

#include "private_event_types.h"

namespace kmac {
namespace stellyra {

/**
 * @brief BasicPrivateSignal<FriendType, MutexType, Args...> - a resolving
 * alias matching BasicPrivateEvent.
 *
 * @see BasicPrivateEvent
 */
template< typename FriendType, typename MutexType, typename... Args >
using BasicPrivateSignal = BasicPrivateEvent< FriendType, MutexType, Args... >;

/**
 * @brief RecursivePrivateSignal<FriendType, Args...> - a resolving alias
 * matching RecursivePrivateEvent.
 *
 * @see RecursivePrivateEvent
 */
template< typename FriendType, typename... Args >
using RecursivePrivateSignal = RecursivePrivateEvent< FriendType, Args... >;

/**
 * @brief SharedPrivateSignal<FriendType, Args...> - a resolving alias
 * matching SharedPrivateEvent.
 *
 * @see SharedPrivateEvent
 */
template< typename FriendType, typename... Args >
using SharedPrivateSignal = SharedPrivateEvent< FriendType, Args... >;

/**
 * @brief SingleThreadedPrivateSignal<FriendType, Args...> - a resolving
 * alias matching SingleThreadedPrivateEvent.
 *
 * @see SingleThreadedPrivateEvent
 */
template< typename FriendType, typename... Args >
using SingleThreadedPrivateSignal = SingleThreadedPrivateEvent< FriendType, Args... >;

} // namespace stellyra
} // namespace kmac

#endif // KMAC_STELLYRA_PRIVATE_SIGNAL_TYPES_H
