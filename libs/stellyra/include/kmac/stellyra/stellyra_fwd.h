#ifndef KMAC_STELLYRA_FORWARD_H
#define KMAC_STELLYRA_FORWARD_H

/**
 * @file stellyra_fwd.h
 * @brief Forward declarations for all Stellyra library classes, enums, and aliases.
 *
 * Include this header when you only need type names (e.g. in other library headers)
 * and want to avoid pulling in full definitions.  Full definitions are provided by
 * the individual component headers and the top-level <kmac/stellyra.h> and
 * <kmac/stellyra_extras.h>.
 */

namespace kmac {
namespace stellyra {

class Connection;        ///< see connection.h
class ConnectionGroup;   ///< see connection_group.h (in extras)
class EventLoop;         ///< see event_loop.h
class ScopedConnection;  ///< see connection.h
class Trackable;         ///< see trackable.h

/**
 * @brief Lifecycle state of an EventRecorder.
 */
enum class RecordingMode
{
	Disabled,   ///< No recording in progress; recorder is idle.
	Recording,  ///< Actively capturing emissions.
	Paused      ///< Recording was started but is temporarily suspended.
};

template< typename MutexType, typename... Args >
class GenericEvent;  ///< see basic_event.h - the underlying class behind the BasicEvent alias
///< NOTE: BasicEvent itself is an alias template (basic_event.h) and has no
///< incomplete form - it cannot be forward-declared here; include
///< basic_event.h wherever the name BasicEvent is needed.

template< typename FriendType, typename MutexType, typename... Args >
class GenericPrivateEvent;  ///< see private_event.h - the underlying class behind the BasicPrivateEvent alias
///< NOTE: BasicPrivateEvent itself is an alias template (private_event.h) and
///< has no incomplete form - it cannot be forward-declared here; include
///< private_event.h wherever the name BasicPrivateEvent is needed.

template< typename... Types >
struct EventFriendTypeList;  ///< see private_event.h - opt-in multi-friend GenericPrivateEvent/BasicPrivateEvent

// template< typename MutexType, typename... Args >
// class EventImpl;

template< typename MutexType, typename... Args >
class EventInspector;  ///< see event_inspector.h (stellyra_extras)

} // namespace stellyra
} // namespace kmac

#endif // KMAC_STELLYRA_FORWARD_H
