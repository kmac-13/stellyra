#pragma once
#ifndef KMAC_STELLYRA_COMBINING_SIGNAL_TYPES_H
#define KMAC_STELLYRA_COMBINING_SIGNAL_TYPES_H

/**
 * @file combining_signal_types.h
 * @brief Convenient aliases for GenericCombiningEvent for users that prefer
 * the signal/emit terminology over Event.
 */

#include "combining_event_types.h"

namespace kmac {
namespace stellyra {

/**
 * @brief BasicCombiningSignal<Args...> - a resolving alias matching
 * BasicCombiningEvent.
 *
 * @see BasicCombiningEvent
 */
template< typename Combiner, typename ReturnType, typename... Args >
using BasicCombiningSignal = BasicCombiningEvent< Combiner, ReturnType, Args... >;

} // namespace stellyra
} // namespace kmac

#endif // KMAC_STELLYRA_COMBINING_SIGNAL_TYPES_H
