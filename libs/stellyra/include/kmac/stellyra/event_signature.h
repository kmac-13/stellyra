#pragma once
#ifndef KMAC_STELLYRA_EVENT_SIGNATURE_H
#define KMAC_STELLYRA_EVENT_SIGNATURE_H

/**
 * @file event_signature.h
 * @brief Function-style signature support for the BasicEvent/
 * BasicConcurrentEvent/BasicRecordableEvent/BasicPrivateEvent alias
 * families, e.g. RecursiveEvent<void(int, string)> as an alternative to
 * RecursiveEvent<int, string>.
 *
 * Every Args...-based alias in this library (BasicEvent,
 * BasicConcurrentEvent, BasicRecordableEvent, BasicPrivateEvent, and their
 * MutexType variants) is a thin alias over a class template that takes a
 * MutexType (and, for the private-event family, a FriendType ahead of that)
 * followed by the handler argument pack: GenericEvent for RecursiveEvent/
 * SharedEvent/SingleThreadedEvent (see basic_event.h), GenericConcurrentEvent
 * for RecursiveConcurrentEvent/SingleThreadedConcurrentEvent
 * (basic_concurrent_event.h), GenericRecordableEvent for the *RecordableEvent
 * family (basic_recordable_event.h), and GenericPrivateEvent for the
 * PrivateEvent family (private_event.h).  This header lets each of those
 * aliases additionally accept a single function-type argument instead of the
 * raw pack:
 *
 * @code
 * RecursiveEvent< int, std::string >          // plain pack form
 * RecursiveEvent< void( int, std::string ) >  // equivalent function-style form
 * @endcode
 *
 * Both forms produce the exact same type, for every alias in this header's
 * title - resolution happens once, here, before the underlying class
 * template (GenericEvent, or whichever other family member it is) is even
 * named, so nothing elsewhere in the library (HandlerEntry, EventStorage,
 * BasicEventImpl, connect()/trigger(), ...) is aware this syntax exists.
 *
 * BasicEvent itself also understands the function-style form directly, for
 * a MutexType the RecursiveEvent/SharedEvent/SingleThreadedEvent aliases
 * above don't cover - it resolves it the same way, before naming GenericEvent
 * (see basic_event.h), so `BasicEvent<M, void(Args...)>` and
 * `BasicEvent<M, Args...>` are the exact same type too, not merely
 * equivalent.
 *
 * @section named_params Named Parameters
 *
 * The parameter names in a function type are ordinary C++ grammar and are
 * discarded by the compiler like any other function type's parameter names
 * - they carry no run-time or type-level meaning, only documentation value
 * at the declaration site:
 *
 * @code
 * RecursiveEvent< void( int employeeId, const std::string& newTitle ) > titleChanged;
 * @endcode
 *
 * is exactly RecursiveEvent< int, const std::string& >; nothing needs to
 * know the names exist, because the compiler discards them by the time any
 * template here sees the type.  Naming the parameters this way is encouraged
 * wherever it helps a reader at the declaration site - it is free, in both
 * compile time and runtime terms.
 *
 * @section non_void Why Only void Returns
 *
 * A multicast event has no single well-defined return value without a
 * reduction strategy - that's what GenericCombiningEvent exists for.  Writing
 * RecursiveEvent< int( Args... ) > would promise a return value operator()
 * does not actually give, so the function-style form is restricted, via
 * partial specialization, to void(Args...) only; a non-void return fails to
 * compile with a message pointing at CombiningEvent instead of silently
 * discarding the return type.  This syntax deliberately does not extend to
 * CombiningEvent - its return type is meaningful there (the per-handler
 * value the combiner reduces), which is a separate question from this
 * alias-only convenience and is not addressed by this header.
 */

#include <type_traits>

namespace kmac {
namespace stellyra {
namespace detail {

/**
 * @brief Resolves the trailing Args... for an alias over a
 * MutexType-then-Args... class template (BasicEvent, BasicConcurrentEvent,
 * BasicRecordableEvent): the plain pack form passes Args... through
 * unchanged; the function-style form (a single R(Args2...) argument)
 * unpacks to Args2..., after static_asserting that R is void.
 */
template< template< typename, typename... > class EventTemplate, typename MutexType, typename... Args >
struct ResolveEventArgs
{
	using type = EventTemplate< MutexType, Args... >;
};

template< template< typename, typename... > class EventTemplate, typename MutexType, typename R, typename... Args2 >
struct ResolveEventArgs< EventTemplate, MutexType, R( Args2... ) >
{
	static_assert( std::is_void_v< R >,
		"BasicEvent<R(Args...)> (and the associated aliases) require a void "
		"return type - a multicast event has no single well-defined return "
		"value without a reduction strategy (see CombiningEvent if you need one)" );

	using type = EventTemplate< MutexType, Args2... >;
};

/**
 * @brief Same resolution as ResolveEventArgs, for the private-event alias
 * family (BasicPrivateEvent), whose class template has an extra leading
 * FriendType parameter ahead of MutexType.
 */
template< template< typename, typename, typename... > class PrivateEventTemplate, typename FriendType, typename MutexType, typename... Args >
struct ResolvePrivateEventArgs
{
	using type = PrivateEventTemplate< FriendType, MutexType, Args... >;
};

template< template< typename, typename, typename... > class PrivateEventTemplate, typename FriendType, typename MutexType, typename R, typename... Args2 >
struct ResolvePrivateEventArgs< PrivateEventTemplate, FriendType, MutexType, R( Args2... ) >
{
	static_assert( std::is_void_v< R >,
		"BasicPrivateEvent<Friend, R(Args...)> (and the associated aliases) "
		"require a void return type - see ResolveEventArgs's documentation "
		"(in event_signature.h) for why" );

	using type = PrivateEventTemplate< FriendType, MutexType, Args2... >;
};

} // namespace detail
} // namespace stellyra
} // namespace kmac

#endif // KMAC_STELLYRA_EVENT_SIGNATURE_H
