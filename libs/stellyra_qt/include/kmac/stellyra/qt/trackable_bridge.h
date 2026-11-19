#pragma once
#ifndef KMAC_STELLYRA_QT_TRACKABLE_BRIDGE_H
#define KMAC_STELLYRA_QT_TRACKABLE_BRIDGE_H

/**
 * @file trackable_bridge.h
 * @brief A QObject that anchors a Trackable for a receiver that either
 * cannot or should not inherit Trackable directly.
 *
 * TrackableBridge composes (does not inherit) a Trackable, exposed via
 * trackable().  It exists to give Qt-signal connections and Stellyra
 * connections a single, shared lifetime anchor: hold one instance
 * alongside (or as a member of) the real receiver, pass this bridge as the
 * `context` to any Qt connect() wiring a signal into that receiver, and
 * pass trackable() as the tracker half of a Tracked<T>{ receiver, tracker }
 * when wiring that same receiver to a Stellyra event (see event_storage.h).
 * Destroying this bridge tears down both halves: QObject's own destructor
 * auto-disconnects every Qt connection where this object was used as
 * sender, receiver, or context, and the Trackable member's destructor (which
 * runs before the QObject destructor) calls disconnectAll() as part of the
 * same teardown.
 *
 * Composition over inheritance is deliberate here, beyond just letting this
 * anchor a receiver that is a different object entirely: inheriting
 * Trackable as a base (alongside some QObject-derived base) would reproduce
 * the same hazard EagerDisconnector (trackable.h) exists to close - base
 * class destructors run only after the most-derived class's own body and
 * members are already gone, so a dispatch racing in during that window
 * could observe a partially-destructed object.  A Trackable *member* here
 * has no such gap, provided this class itself is not further subclassed -
 * subclassing TrackableBridge to add real state and methods of your own
 * (the usual reason to do so: giving it its own Qt signals/slots) brings
 * that same gap back for those added members, same as inheriting Trackable
 * anywhere else (see the note on connectEventToTrackableBridge() in
 * qt_connect.h).
 *
 * Deliberately not coupled to EventLoopBridge (event_loop_bridge.h) - an
 * application that needs both holds one of each.  This bridge's own Qt
 * thread affinity (moveToThread(), same as any QObject) has no bearing on
 * which thread a Stellyra handler actually dispatches on - that is decided
 * entirely by the receiver's registered EventLoop and ConnectionType; it
 * only matters for Qt-side signal/slot resolution where this bridge is
 * used as the `context` argument (see connectSignalToEvent() in
 * qt_connect.h).
 */

// protect against conflict with Qt's emit keyword - this header is a valid
// standalone include point, so it can't rely on something upstream having
// already run this guard
#pragma push_macro("emit")
#undef emit
#include <kmac/stellyra.h>
#pragma pop_macro("emit")

#include <QObject>

namespace kmac {
namespace stellyra {
namespace qt {

class TrackableBridge : public QObject
{
	Q_OBJECT

private:
	Trackable _trackable;

public:
	/**
	 * @brief Constructs a new instance of TrackableBridge.
	 *
	 * @param parent the owning QObject, or nullptr for no parent
	 */
	explicit TrackableBridge( QObject* parent = nullptr );

	/**
	 * @brief The Trackable anchored by this bridge.  Pass this as the
	 * tracker half of a Tracked<T>{ receiver, tracker } when connecting a
	 * non-Trackable receiver to a Stellyra event, or as the tracker for
	 * this bridge itself (see connectEventToTrackableBridge() in
	 * qt_connect.h).
	 */
	Trackable& trackable();
};

} // namespace qt
} // namespace stellyra
} // namespace kmac

#endif // KMAC_STELLYRA_QT_TRACKABLE_BRIDGE_H
