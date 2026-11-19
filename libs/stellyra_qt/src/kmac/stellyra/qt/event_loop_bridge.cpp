#include "kmac/stellyra/qt/event_loop_bridge.h"

#include <kmac/stellyra/stellyra_assert.h>

#include <QThread>

namespace kmac {
namespace stellyra {
namespace qt {

EventLoopBridge::EventLoopBridge( QObject* parent /*= nullptr*/ )
	: QObject( parent )
{
	// installed once, here, before this object is handed to anyone else -
	// setPostNotify() is not safe to call concurrently with post() on
	// another thread, so this must happen before any other thread is given
	// a reference to _eventLoop (see EventLoop::setPostNotify()'s doc comment)
	_eventLoop.setPostNotify( Callable< void() >::create(
		[ this ]() { QMetaObject::invokeMethod( this, &EventLoopBridge::drainEventLoop, Qt::QueuedConnection ); } )
	);

	// by default, set the current thread as the drain thread
	updateDrainThread();
}

EventLoop& EventLoopBridge::eventLoop()
{
	return _eventLoop;
}

void EventLoopBridge::updateDrainThread()
{
	// a wrong registration here is silent and only surfaces later, as a
	// much harder to diagnose dispatch-on-the-wrong-thread bug - so a
	// caller getting this precondition wrong is a hard abort (every build,
	// not just debug), not a value this function could sensibly guess around
	// (see the doc comment: callers must use requestDrainThreadUpdate()
	// instead whenever they can't guarantee this)
	stellyra_assert_always( QThread::currentThread() == thread(),
		"EventLoopBridge::updateDrainThread() called from a thread other than this bridge's own Qt affinity "
		"thread - use requestDrainThreadUpdate() instead unless the calling code is genuinely known to be "
		"executing on the bridge's current thread" );

	_eventLoop.setDrainThread( platform::currentThreadId() );
}

void EventLoopBridge::requestDrainThreadUpdate()
{
	QMetaObject::invokeMethod( this, &EventLoopBridge::updateDrainThread, Qt::QueuedConnection );
}

void EventLoopBridge::drainEventLoop()
{
	_eventLoop.drain();
}

bool EventLoopBridge::event( QEvent* event )
{
	// NOTE that this method is called synchronously within the moveToThread
	// context BEFORE the QObject's affinity thread has actually been updated

	// check if the event type represents a thread change;
	if ( event->type() == QEvent::ThreadChange )
	{
		// queue the drain thread update
		requestDrainThreadUpdate();
	}

	return QObject::event( event );
}

} // namespace qt
} // namespace stellyra
} // namespace kmac
