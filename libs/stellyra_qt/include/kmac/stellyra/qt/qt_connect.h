#pragma once
#ifndef KMAC_STELLYRA_QT_CONNECT_H
#define KMAC_STELLYRA_QT_CONNECT_H

/**
 * @file qt_connect.h
 * @brief Free functions bridging Qt signals and Stellyra events in each
 * direction, without requiring a hand-written lambda at every call site.
 */

// protect against conflict with Qt's emit keyword - this header is a valid
// standalone include point, so it can't rely on something upstream having
// already run this guard
#pragma push_macro("emit")
#undef emit
#include <kmac/stellyra.h>
#pragma pop_macro("emit")

#include <kmac/stellyra/qt/trackable_bridge.h>

#include <QObject>

#include <type_traits>
#include <utility>


namespace kmac {
namespace stellyra {
namespace qt {

/**
 * @brief Connects a Qt signal to call trigger() on a Stellyra event.
 *
 * Builds the forwarding lambda for you - `connectSignalToEvent(sender,
 * &Sender::signal, context, event)` in place of writing out
 * `QObject::connect(sender, &Sender::signal, context, [&event](auto&&...
 * args){ event.trigger(std::forward<decltype(args)>(args)...); })`
 * yourself.  `type` is forwarded to Qt's connect() unchanged (default
 * Qt::AutoConnection, same as Qt's own default).
 *
 * `EventT` is unconstrained - any event with a public trigger(Args...)
 * works (RecursiveEvent, SharedEvent, SingleThreadedEvent,
 * ConcurrentEvent, BasicRecordableEvent, BasicCombiningEvent, ...).
 * RecursiveEvent is worth defaulting to for a signal-fed event specifically
 * (not required, just a recommendation): Qt signal emission is routinely
 * reentrant - a slot can trigger further signals synchronously - and a
 * non-recursive mutex-backed event would deadlock, not just misbehave, if
 * this ever reenters trigger() on the same thread.
 *
 * @note Does NOT work with a private event (BasicPrivateEvent and its
 * aliases).  This is not a limitation this function could bypass by checking
 * who calls it - access control in C++ is decided by where the code that
 * names trigger() exists in, not by who calls that code, and the lambda this
 * function builds is written here, inside a free function that is never a
 * declared friend of any private event, regardless of who called
 * connectSignalToEvent.  If you need this for a private event:
 * the owning (friend) class can write its own small method that returns a
 * `[this](auto&&... args){ myPrivateEvent.trigger(args...); }` lambda - a
 * lambda defined inside a friend's own member keeps that friend's access
 * even when later called from completely unrelated code - and hand that
 * lambda straight to a plain QObject::connect(); no stellyra_qt helper is
 * needed for that case at all.
 */
template< typename Sender, typename Signal, typename EventT >
QMetaObject::Connection connectSignalToEvent(
	Sender* sender,
	Signal signal,
	const QObject* context,
	EventT& event,
	Qt::ConnectionType type = Qt::AutoConnection )
{
	return QObject::connect( sender, signal, context,
		[ &event ]( auto&&... args ) {
			event.trigger( std::forward< decltype( args ) >( args )... );
		},
		type );
}

/**
 * @brief Connects a Stellyra event to call a method on a TrackableBridge (or
 * a type that derives from one).
 *
 * `connectEventToTrackableBridge(event, bridge, &MyBridge::method)` in place
 * of writing out `event.connect(Tracked<MyBridge>{ bridge, bridge.trackable()
 * }, &MyBridge::method)` yourself.  `T` is deduced from `bridge` and `method`
 * together, so this works directly for a method declared on a subclass of
 * TrackableBridge (the usual reason to subclass it at all: to give the bridge
 * its own Qt signals/slots) and not only for the handful of methods
 * TrackableBridge itself declares.
 *
 * TrackableBridge does not inherit Trackable (see trackable_bridge.h), so
 * the plain, non-Tracked connect() overload would silently create an
 * untracked connection here instead of failing to compile - this function
 * exists specifically to make the self-referencing Tracked<T>{ bridge,
 * bridge.trackable() } form (receiver and tracker being the same object)
 * the default, rather than something easy to forget.
 *
 * @note Subclassing TrackableBridge to add real state and methods puts you
 * back in the ordinary "Trackable as base class" situation that
 * EagerDisconnector (trackable.h) exists for elsewhere in the library:
 * TrackableBridge's own _trackable member is destroyed as part of the
 * *base* class teardown, which runs after your subclass's own members are
 * already gone, so a dispatch racing in during that narrow window could
 * still observe a partially-destructed object.  TrackableBridge's
 * composition-over-inheritance only removes that risk for TrackableBridge
 * used plainly (unsubclassed) as a pure anchor alongside a separate,
 * unrelated receiver; it does not remove it for methods a subclass adds.
 * Treat those the same as any other Trackable-base-class method connected
 * from multiple threads.
 *
 * `method` must be void-returning, matching the one connect() overload this
 * forwards to - true of every Qt signal and slot in practice, so this
 * should never bite a real call site.  ConnParams (predicates, priority,
 * single-shot) are intentionally not exposed here; call event.connect()
 * directly if you need them.
 */
template< typename EventT, typename T, typename... MethodArgs >
Connection connectEventToTrackableBridge(
	EventT& event,
	T& bridge,
	void ( T::*method )( MethodArgs... ),
	ConnectionType type = ConnectionType::Auto )
{
	static_assert( std::is_base_of_v< TrackableBridge, T >,
		"connectEventToTrackableBridge requires bridge to be a TrackableBridge, or a type deriving from one" );
	return event.connect( Tracked< T >{ bridge, bridge.trackable() }, method, type );
}

} // namespace qt
} // namespace stellyra
} // namespace kmac

#endif // KMAC_STELLYRA_QT_CONNECT_H
