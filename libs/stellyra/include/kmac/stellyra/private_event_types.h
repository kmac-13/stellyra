#pragma once
#ifndef KMAC_STELLYRA_PRIVATE_EVENT_TYPES_H
#define KMAC_STELLYRA_PRIVATE_EVENT_TYPES_H

/**
 * @file private_event_types.h
 * @brief Convenient aliases over GenericPrivateEvent that support
 * function-style syntax with some specifying mutex types.
 *
 * BasicPrivateEvent is a thin alias over GenericPrivateEvent (see
 * generic_private_event.h) that supports the same template arguments and
 * signature as GenericPrivateEvent but adds a function-style signature form
 * (see event_signature.h) before resolving to GenericPrivateEvent.  The
 * other aliases - RecursivePrivateEvent, SharedPrivateEvent, and
 * SingleThreadedPrivateEvent - all resolve to GenericPrivateEvent with
 * specific mutex types (see platform.h) while also supporting the
 * function-style syntax option.
 */

#include "event_signature.h"
#include "generic_private_event.h"

namespace kmac {
namespace stellyra {

/**
 * @brief BasicPrivateEvent<FriendType, MutexType, Args...> - a resolving
 * alias over GenericPrivateEvent.
 *
 * Resolves the function-style signature form (see event_signature.h)
 * before naming GenericPrivateEvent, exactly the way BasicEvent resolves it
 * before naming GenericEvent - so BasicPrivateEvent<F, M, void(Args...)> and
 * BasicPrivateEvent<F, M, Args...> are always the exact same type.  For a
 * plain Args... pack the two names are interchangeable; use BasicPrivateEvent
 * when the function-style form is wanted, otherwise either option works.
 * `FriendType` may be a single class, or EventFriendTypeList<A, B, ...> for
 * the multi-friend form - unaffected by this alias layer either way.
 */
template< typename FriendType, typename MutexType, typename... Args >
using BasicPrivateEvent = typename detail::ResolvePrivateEventArgs< GenericPrivateEvent, FriendType, MutexType, Args... >::type;

/**
 * @brief Private event using the RecursiveMutex strategy.
 *
 * `FriendType` may be a single class, or EventFriendTypeList<A, B, ...> for
 * the multi-friend form.
 *
 * Accepts either the plain argument pack
 * (RecursivePrivateEvent<MyClass, int>) or the equivalent function-style
 * signature (RecursivePrivateEvent<MyClass, void(int)>) - see
 * event_signature.h.
 *
 * @code
 * RecursivePrivateEvent< MyClass, int > valueChanged { this };
 * RecursivePrivateEvent< EventFriendTypeList< MyClass, Setup >, int > valueChanged2 { this };
 * @endcode
 */
template< typename FriendType, typename... Args >
using RecursivePrivateEvent = typename detail::ResolvePrivateEventArgs< GenericPrivateEvent, FriendType, platform::RecursiveMutex, Args... >::type;

/**
 * @brief Private event using SharedMutex (concurrent reads, exclusive writes).
 *
 * Use when the event will be triggered from many threads simultaneously and
 * all connected handlers are Direct.
 *
 * Accepts the function-style signature form too - see RecursivePrivateEvent
 * above.
 */
template< typename FriendType, typename... Args >
using SharedPrivateEvent = typename detail::ResolvePrivateEventArgs< GenericPrivateEvent, FriendType, platform::SharedMutex, Args... >::type;

/**
 * @brief Private event using NullMutex (zero overhead, single-threaded only).
 *
 * Accepts the function-style signature form too - see RecursivePrivateEvent
 * above.
 */
template< typename FriendType, typename... Args >
using SingleThreadedPrivateEvent = typename detail::ResolvePrivateEventArgs< GenericPrivateEvent, FriendType, platform::NullMutex, Args... >::type;

} // namespace stellyra
} // namespace kmac

#endif // KMAC_STELLYRA_PRIVATE_EVENT_TYPES_H
