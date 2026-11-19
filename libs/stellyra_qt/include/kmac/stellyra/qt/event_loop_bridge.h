#pragma once
#ifndef KMAC_STELLYRA_QT_EVENT_LOOP_BRIDGE_H
#define KMAC_STELLYRA_QT_EVENT_LOOP_BRIDGE_H

/**
 * @file event_loop_bridge.h
 * @brief Bridges the gap between Stellyra's events and the Qt framework.
 *
 * EventLoopBridge owns a Stellyra EventLoop and will drain that loop
 * whenever a task gets posted to the loop.  This allows Stellyra events to
 * be processed within a Qt application's threading model without requiring
 * manual implementation of event-processing.  The owned EventLoop's drain
 * thread tracks this object's own Qt thread affinity automatically - it is
 * registered at construction (the constructing thread), and re-registered
 * whenever something later calls moveToThread() on this object, with no
 * further action required: just construct an EventLoopBridge and use
 * moveToThread() exactly as you would for any other QObject.  Pass the
 * owned EventLoop to `Trackable::setEventLoop()` for any Stellyra receiver
 * whose connections should be deferred onto this bridge's thread.
 *
 * updateDrainThread() is the only other way the drain-thread registration
 * is ever touched: a manual escape hatch for code that is genuinely known
 * to already be executing on the bridge's target thread (e.g. the top of
 * that thread's own run()/init code), to close the small window that the
 * automatic queued update otherwise leaves open between moveToThread()
 * returning and that update actually running (see the method's doc comment,
 * including the hard-abort guard against calling it from the wrong  thread
 * by mistake).  drainEventLoop() does not touch the registration at  all
 * (see the method's doc comment for why).
 */

// protect against conflict with Qt's emit keyword - this header is a valid
// standalone include point (not just via the stellyra_qt.h umbrella), so it
// can't rely on something upstream having already run this guard
#pragma push_macro("emit")
#undef emit
#include <kmac/stellyra.h>
#pragma pop_macro("emit")

#include <QEvent>
#include <QObject>

namespace kmac {
namespace stellyra {
namespace qt {

class EventLoopBridge : public QObject
{
	Q_OBJECT

private:
	EventLoop _eventLoop;

public:
	/**
	 * @brief Constructs a new instance of EventLoopBridge, initializing the
	 * owned EventLoop, registering the constructing thread as its drain
	 * thread immediately (safe here specifically because construction is
	 * always synchronous on the real target thread at that moment - no
	 * moveToThread() race to worry about yet), and wiring it to drain
	 * automatically - via a queued call to drainEventLoop() - whenever a
	 * task is posted to it.
	 *
	 * @param parent the owning QObject, or nullptr for no parent
	 */
	explicit EventLoopBridge( QObject* parent = nullptr );

	/**
	 * @brief The EventLoop owned by this bridge.  Pass this to
	 * Trackable::setEventLoop() on any receiver whose connections should be
	 * deferred onto this bridge's thread.
	 */
	EventLoop& eventLoop();

	/**
	 * @brief Registers the calling thread as the owned EventLoop's drain
	 * thread.
	 *
	 * Called automatically - at construction, and again (via
	 * requestDrainThreadUpdate(), queued) on every moveToThread() - so this
	 * normally never needs to be called directly.  It exists publicly for
	 * the one case the automatic handling doesn't close instantly: that
	 * update is queued, so there's a small window between moveToThread()
	 * returning and that queued update actually running.  Code that is
	 * genuinely known to already be executing on the bridge's target thread
	 * (e.g. the top of that thread's own run()/init code, after
	 * moveToThread() was called before that thread started) can call this
	 * directly to close that window immediately instead of waiting on it.
	 *
	 * @warning Only call this from code that is genuinely executing on the
	 * thread this bridge is meant to operate on.  This is checked - calling
	 * it from any other thread is a hard abort (stellyra_assert_always, in
	 * every build, not just debug) rather than a silent wrong registration:
	 * a wrong registration here has no well-defined way to continue, and
	 * surfacing it immediately beats it quietly causing a much
	 * harder-to-diagnose dispatch-on-the-wrong-thread bug later.  If you
	 * can't guarantee this precondition, you don't need to do anything -
	 * the automatic handling above already covers you, just not instantly.
	 */
	void updateDrainThread();

	/**
	 * @brief Drains the owned Stellyra EventLoop.
	 *
	 * Call via Qt signal-slot connection or QMetaObject::invokeMethod(
	 * bridge, &EventLoopBridge::drainEventLoop, Qt::QueuedConnection ) from any
	 * background thread after posting work; this is also what the
	 * EventLoop's own post-notification hook does automatically (see the
	 * constructor), so this normally doesn't need to be called directly.  To
	 * force a flush before tearing down this bridge's own thread (so
	 * nothing already posted gets silently dropped), call it instead via
	 * QMetaObject::invokeMethod( bridge, &EventLoopBridge::drainEventLoop,
	 * Qt::BlockingQueuedConnection ) - from any thread, but only while this
	 * bridge's own thread's event loop is still running to receive it, i.e.
	 * before telling that thread to quit.
	 */
	void drainEventLoop();

protected:
	/**
	 * @brief Reacts to QEvent::ThreadChange by requesting a drain-thread
	 * update, so moveToThread() alone is enough to keep the owned
	 * EventLoop's drain thread correct with no other action required.
	 *
	 * Deliberately uses requestDrainThreadUpdate() (queued), not
	 * updateDrainThread() (immediate) - this override runs synchronously,
	 * during moveToThread() itself, on whatever thread called
	 * moveToThread(), which is not necessarily this bridge's new thread (it
	 * could be either the old thread, the new one, or a third thread
	 * entirely).  Worse, thread() itself has not yet been updated to the new
	 * value at the point this runs, so even reading thread() here to decide
	 * what to register would give the wrong answer.
	 * requestDrainThreadUpdate()'s queuing sidesteps this correctly (see its
	 * own doc comment for why), which is why this override forwards to it
	 * rather than registering anything itself.
	 */
	bool event( QEvent* event ) override;

private:
	/**
	 * @brief Schedules updateDrainThread() to run on whatever thread this
	 * bridge actually has affinity with, without the caller having to
	 * reason about where it's currently executing or when moveToThread()
	 * has taken effect.  Safe to call from any thread, at any time -
	 * unlike updateDrainThread() itself, this has no thread precondition.
	 *
	 * Used only by the event() override above, in response to
	 * QEvent::ThreadChange.
	 *
	 * Internally this is just QMetaObject::invokeMethod( this,
	 * &EventLoopBridge::updateDrainThread, Qt::QueuedConnection ).  This is
	 * safe to call even from directly inside a QEvent::ThreadChange handler
	 * while thread() itself still (misleadingly) reports the OLD thread:
	 * the delivery still correctly lands on the object's real new thread
	 * once it actually starts running there, even though thread() only
	 * catches up to the new value after the event has already been
	 * dispatched.
	 */
	void requestDrainThreadUpdate();
};

} // namespace qt
} // namespace stellyra
} // namespace kmac

#endif // KMAC_STELLYRA_QT_EVENT_LOOP_BRIDGE_H
