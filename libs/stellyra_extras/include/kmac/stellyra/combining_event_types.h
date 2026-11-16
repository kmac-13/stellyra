#pragma once
#ifndef KMAC_STELLYRA_COMBINING_EVENT_TYPES_H
#define KMAC_STELLYRA_COMBINING_EVENT_TYPES_H

/**
 * @file combining_event_types.h
 * @brief Convenient aliases over GenericCombiningEvent that support
 * function-style syntax..
 *
 * BasicCombiningEvent is a thin alias over GenericCombiningEvent (see
 * generic_combining_event.h) that supports the same template arguments and
 * signature as GenericCombiningEvent but adds a function-style signature
 * form (see event_signature.h) before resolving to GenericCombiningEvent.
 *
 * Note that GenericCombiningEvent is synchronous only, so there are no
 * mutex-based options to worry about - only combiner and return types.
 * Also note that since non-void return types are allowed, the function-style
 * syntax resolving mechanism (see below) is a separate implementation from
 * the resolving mechanism used by all other event families.
 */

#include "generic_combining_event.h"

namespace kmac {
namespace stellyra {

namespace detail {

/**
 * @brief Resolves the trailing ReturnType/Args... for GenericCombiningEvent,
 * mirroring event_signature.h's ResolveEventArgs/ResolvePrivateEventArgs
 * for the one family where the return type is meaningful and must be kept,
 * not discarded: the plain pack form (Combiner, ReturnType, Args...) passes
 * through unchanged; the function-style form (Combiner, R(Args2...)) unpacks
 * the single trailing function-type argument into ReturnType = R,
 * Args... = Args2..., with no static_assert on R - unlike ResolveEventArgs,
 * which requires R to be void and throws it away, this resolver's entire
 * purpose is to capture R and forward it, since it is the per-handler value
 * GenericCombiningEvent's Combiner reduces.  See event_signature.h's "Why
 * Only void Returns" section for why that header's support deliberately does
 * not cover GenericCombiningEvent, and why this is a separate mechanism
 * rather  than a reuse of ResolveEventArgs.
 */
template<
	template< typename, typename, typename... > class EventTemplate,
	typename Combiner, typename ReturnType, typename... Args >
struct ResolveCombiningEventArgs
{
	using type = EventTemplate< Combiner, ReturnType, Args... >;
};

template<
	template< typename, typename, typename... > class EventTemplate,
	typename Combiner, typename R, typename... Args2 >
struct ResolveCombiningEventArgs< EventTemplate, Combiner, R( Args2... ) >
{
	using type = EventTemplate< Combiner, R, Args2... >;
};

} // namespace detail

/**
 * @brief BasicCombiningEvent<Combiner, ReturnType, Args...> - a resolving
 * alias over GenericCombiningEvent.
 *
 * Additionally accepts the function-style form
 * BasicCombiningEvent<Combiner, ReturnType(Args...)>, resolving it before
 * naming GenericCombiningEvent - so BasicCombiningEvent<C, R(Args...)> and
 * BasicCombiningEvent<C, R, Args...> are always the exact same type:
 *
 * @code
 * BasicCombiningEvent< Combiners::LogicalAnd<>, bool, std::string > validate;
 * BasicCombiningEvent< Combiners::LogicalAnd<>, bool( std::string ) > sameType;
 * static_assert( std::is_same_v< decltype( validate ), decltype( sameType ) > );
 * @endcode
 *
 * For a plain Args... pack, the two names (BasicCombiningEvent and
 * GenericCombiningEvent) are interchangeable; use BasicCombiningEvent when the
 * function-style form is required.
 */
template< typename Combiner, typename ReturnType, typename... Args >
using BasicCombiningEvent = typename detail::ResolveCombiningEventArgs< GenericCombiningEvent, Combiner, ReturnType, Args... >::type;

} // namespace stellyra
} // namespace kmac

#endif // KMAC_STELLYRA_COMBINING_EVENT_TYPES_H
