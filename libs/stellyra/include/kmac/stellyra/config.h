#ifndef KMAC_STELLYRA_CONFIG_H
#define KMAC_STELLYRA_CONFIG_H

/**
 * @file config.h
 * @brief Stellyra library configuration: logging tags, macros, and optional Nova logging
 * integration.
 *
 * This header centralises all compile-time configuration for the Stellyra event
 * library.  Include it before any other Stellyraheader if you need to customise
 * logging behaviour; it is also included automatically by stellyra_fwd.h.
 *
 * @section nova Nova Integration
 *
 * To route Stellyra log messages through the Nova logging library, define
 * `STELLYRA_USE_NOVA` before including any Stellyra header (typically via
 * a compiler flag or your CMakeLists.txt):
 *
 * @code
 * // CMakeLists.txt
 * target_compile_definitions( app PRIVATE STELLYRA_USE_NOVA )
 * @endcode
 *
 * When Nova is not available, `STELLYRA_LOG` routes to `std::cerr` with a
 * `[TagName]` prefix.
 *
 * @section connection_logging Connection Issue Logging
 *
 * Connection logging is split into two independently-enabled domains, so a
 * general trace of every connect/disconnect and a warning about a probably-
 * wrong connection can be turned on separately rather than forced to share
 * one on/off switch:
 *
 * - STELLYRA_LOG_CONNECTION_ISSUES` - misconfiguration warnings: a Direct
 *   connection whose sender/receiver loops don't match, a Deferred
 *   connection with no receiver loop (at connect time or after a
 *   migration), and an object/method connection made with no `Trackable`
 *   to track it (so nothing will mark the slot inactive if the receiver is
 *   destroyed)
 * - `STELLYRA_LOG_CONNECTION_LIFECYCLE` - a line for every connect and
 *   disconnect, regardless of whether anything is wrong
 *
 * Both default to disabled.  Enable either independently by defining it
 * before including any Stellyra header:
 *
 * @code
 * // CMakeLists.txt
 * target_compile_definitions( app PRIVATE STELLYRA_LOG_CONNECTION_ISSUES )
 * target_compile_definitions( app PRIVATE STELLYRA_LOG_CONNECTION_LIFECYCLE )
 * @endcode
 *
 * When disabled, the corresponding logging compiles to nothing - zero
 * runtime overhead.
 *
 * @section tags Logging Tags
 *
 * Three logging domains are defined:
 *
 * - `StellyraTag` - general Stellyra library messages
 * - `StellyraConnectionTag` - general connection lifecycle logging
 *   (`STELLYRA_LOG_CONNECTION_LIFECYCLE`)
 * - `StellyraConnectionMisconfigTag` - connection misconfiguration warnings
 *   (`STELLYRA_LOG_CONNECTION_ISSUES`)
 *
 * With Nova, each tag can be bound to a different sink:
 * @code
 * kmac::nova::ScopedConfigurator config;
 * //config.bind< kmac::stellyra::StellyraTag >( &generalSink );  // unavailable
 * config.bind< kmac::stellyra::StellyraConnectionTag >( &lifecycleSink );
 * config.bind< kmac::stellyra::StellyraConnectionMisconfigTag >( &warningSink );
 * @endcode
 */

#include "platform.h"

// ============================================================================
// Connection issue logging - disabled by default
// ============================================================================

/**
 * @brief Define to enable connection misconfiguration warnings.
 *
 * When defined (typically via compiler flag), Stellyra will log warnings when:
 * - a Direct connection is made between objects on different event loops
 * - a Direct connection's loop state is mismatched at the time of a migration
 * - a Direct connection's loop state is matched at the time of a migration
 *   (logged the same as the mismatched case above - this flag reports the
 *   current state on every migration touching a Direct/Deferred connection,
 *   not just the moment it changes; see connectImpl()/updateSenderLoop()/
 *   updateHandlerLoop() for the call sites)
 * - a Deferred connection is made when the receiver has no event loop
 * - a Deferred connection has no receiver loop at the time of a migration
 * - a Deferred connection has a receiver loop at the time of a migration
 * - an object/method connection is made with no Trackable to track it (see
 *   EventStorage::connectImpl() - the receiver doesn't inherit Trackable and
 *   no Tracked<T> pairing was used, so nothing will mark the slot inactive
 *   if the receiver is destroyed)
 *
 * Defaults to disabled (0) if not defined externally.
 */
#ifndef STELLYRA_LOG_CONNECTION_ISSUES
#	define STELLYRA_LOG_CONNECTION_ISSUES 0
#endif

// convenience bool for use in Nova traits enabled field
#if STELLYRA_LOG_CONNECTION_ISSUES
#	define STELLYRA_CONNECTION_LOGGING_ENABLED true
#else
#	define STELLYRA_CONNECTION_LOGGING_ENABLED false
#endif

/**
 * @brief Define to enable general connection lifecycle logging: a line for
 * every connect and every disconnect, independent of
 * STELLYRA_LOG_CONNECTION_ISSUES above.  disconnectAll() logs a single
 * summary line (count of connections removed) rather than one line per
 * connection, to avoid flooding the log when an event is torn down in bulk.
 *
 * Defaults to disabled (0) if not defined externally.
 */
#ifndef STELLYRA_LOG_CONNECTION_LIFECYCLE
#	define STELLYRA_LOG_CONNECTION_LIFECYCLE 0
#endif

// convenience bool for use in Nova traits enabled field
#if STELLYRA_LOG_CONNECTION_LIFECYCLE
#	define STELLYRA_CONNECTION_LIFECYCLE_LOGGING_ENABLED true
#else
#	define STELLYRA_CONNECTION_LIFECYCLE_LOGGING_ENABLED false
#endif

// ============================================================================
// Logging tag types
// Always defined regardless of Nova availability, so call sites compile
// uniformly in both Nova and non-Nova builds.
// ============================================================================

namespace kmac {
namespace stellyra {

/**
 * @brief Nova logging domain tag for general Stellyra library messages.
 */
// struct StellyraTag {};

/**
 * @brief Nova logging domain tag for general connection lifecycle logging
 * (every connect/disconnect).
 *
 * Controlled by `STELLYRA_LOG_CONNECTION_LIFECYCLE`.  When that macro is
 * disabled, the Nova traits mark this tag as disabled so logging compiles
 * to nothing with zero overhead.
 */
struct StellyraConnectionTag {};

/**
 * @brief Nova logging domain tag for Stellyra connection misconfiguration
 * warnings.
 *
 * Controlled by `STELLYRA_LOG_CONNECTION_ISSUES`.  When that macro is
 * disabled, the Nova traits mark this tag as disabled so logging compiles
 * to nothing with zero overhead.
 */
struct StellyraConnectionMisconfigTag {};

} // namespace stellyra
} // namespace kmac

// ============================================================================
// Nova integration
// ============================================================================

#ifdef STELLYRA_USE_NOVA

#include <kmac/nova.h>

// register Nova logger traits for StellyraTag, always enabled when Nova is in use
// NOVA_LOGGER_TRAITS( kmac::stellyra::StellyraTag, STELLYRA, true, kmac::nova::TimestampHelper::systemNanosecs );

// register Nova logger traits for StellyraConnectionTag (general lifecycle),
// enabled field is driven by STELLYRA_LOG_CONNECTION_LIFECYCLE so that
// disabled logging compiles to nothing via Nova's if constexpr elimination
NOVA_LOGGER_TRAITS( kmac::stellyra::StellyraConnectionTag, STELLYRA_CONN, STELLYRA_CONNECTION_LIFECYCLE_LOGGING_ENABLED, kmac::nova::TimestampHelper::systemNanosecs );

// register Nova logger traits for StellyraConnectionMisconfigTag, enabled
// field is driven by STELLYRA_LOG_CONNECTION_ISSUES so that disabled
// logging compiles to nothing via Nova's if constexpr elimination
NOVA_LOGGER_TRAITS( kmac::stellyra::StellyraConnectionMisconfigTag, STELLYRA_CONN_MISCONFIG, STELLYRA_CONNECTION_LOGGING_ENABLED, kmac::nova::TimestampHelper::systemNanosecs );

/**
 * @brief Log a Stellyra message via Nova.
 *
 * Usage:
 * @code
 * STELLYRA_LOG( kmac::stellyra::StellyraConnectionMisconfigTag ) << "message\n";
 * @endcode
 */
#define STELLYRA_LOG( Tag ) NOVA_LOG( Tag )

#else // STELLYRA_USE_NOVA not defined - fall back to std::cerr

#include <iostream>

namespace kmac {
namespace stellyra {
namespace detail {

/**
 * @brief Write a `[TagName]` prefix to `std::cerr` and return the stream.
 *
 * Used by the `STELLYRA_LOG` fallback macro so that call sites can chain
 * `operator<<` naturally:
 * @code
 * STELLYRA_LOG( StellyraConnectionTag ) << "message\n";
 * // output: [StellyraConnectionTag] message
 * @endcode
 */
inline std::ostream& stellyraLogStream( const char* tag )
{
	return std::cerr << "[" << tag << "] ";
}

} // namespace detail
} // namespace stellyra
} // namespace kmac

/**
 * @brief Log a Stellyra message to `std::cerr` with a `[TagName]` prefix.
 *
 * The tag name is the stringified token passed as `Tag`, e.g.
 * `StellyraConnectionTag` becomes `"[StellyraConnectionTag]"`.
 *
 * For structured, routable logging, consider enabling Nova via
 * `STELLYRA_USE_NOVA`.
 */
#define STELLYRA_LOG( Tag ) kmac::stellyra::detail::stellyraLogStream( #Tag )

#endif // STELLYRA_USE_NOVA

// ============================================================================
// Convenience macros for connection logging at call sites.
// Each compiles to nothing when its own flag is 0, regardless of whether
// Nova is in use, and regardless of the other macro's setting.
// ============================================================================

/**
 * @brief Conditionally log a connection misconfiguration warning.
 *
 * Expands to a `STELLYRA_LOG( StellyraConnectionMisconfigTag )` stream
 * expression when `STELLYRA_LOG_CONNECTION_ISSUES` is non-zero, and to
 * nothing otherwise.
 *
 * Usage:
 * @code
 * STELLYRA_CONNECTION_MISCONFIG_LOG()
 *    << "Direct connection with mismatched loops: "
 *    << senderLoop << " -> " << receiverLoop << "\n";
 * @endcode
 */
#if STELLYRA_LOG_CONNECTION_ISSUES
	#define STELLYRA_CONNECTION_MISCONFIG_LOG() \
		STELLYRA_LOG( kmac::stellyra::StellyraConnectionMisconfigTag )
#else
	// expands to a discarded-value expression so call sites can chain <<
	// without referencing std::cerr or any other stream
	#define STELLYRA_CONNECTION_MISCONFIG_LOG() \
		if ( true ) { } else std::cerr
#endif

/**
 * @brief Conditionally log a general connection lifecycle event (connect,
 * disconnect, or a disconnectAll() summary).
 *
 * Expands to a `STELLYRA_LOG( StellyraConnectionTag )` stream expression
 * when `STELLYRA_LOG_CONNECTION_LIFECYCLE` is non-zero, and to nothing
 * otherwise.
 *
 * Usage:
 * @code
 * STELLYRA_CONNECTION_LIFECYCLE_LOG()
 *    << "connect: event=" << this << " slot=" << index << "\n";
 * @endcode
 */
#if STELLYRA_LOG_CONNECTION_LIFECYCLE
	#define STELLYRA_CONNECTION_LIFECYCLE_LOG() \
		STELLYRA_LOG( kmac::stellyra::StellyraConnectionTag )
#else
	// expands to a discarded-value expression so call sites can chain <<
	// without referencing std::cerr or any other stream
	#define STELLYRA_CONNECTION_LIFECYCLE_LOG() \
		if ( true ) { } else std::cerr
#endif

#endif // KMAC_STELLYRA_CONFIG_H
