// This test binary builds its own main() instead of linking GTest::gtest_main
// (see tests/CMakeLists.txt): queued Qt connections need a live
// QCoreApplication pumped via processEvents(), and that singleton has to be
// constructed exactly once, before any QObject in this binary is created.
//
// The Q_OBJECT fixtures (Sender, TestBridge) live in qt_test_fixtures.h
// rather than being defined inline here - see that header's comment for why.

#include "test_helpers.h"
#include "qt_test_fixtures.h"

#include <QCoreApplication>
#include <QThread>

#include <atomic>
#include <memory>
#include <thread>

namespace qt = kmac::stellyra::qt;

namespace {

// bounded poll, mirroring the idiom in test_sender_deferral.cpp - fails fast
// on a genuine regression rather than hanging, while also pumping the Qt
// event loop so a queued connection actually gets delivered
template< typename Predicate >
void pumpUntil( Predicate&& predicate, int timeoutMs = 1000 )
{
	auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds( timeoutMs );
	while ( ! predicate() )
	{
		ASSERT_LT( std::chrono::steady_clock::now(), deadline ) << "timed out waiting for condition";
		QCoreApplication::processEvents();
		msleep( 1 );
	}
}

} // namespace

// ---------------------------------------------------------------------------
// connectSignalToEvent
// ---------------------------------------------------------------------------

TEST( QtIntegration, CrossThreadSignalToEvent_DispatchesOnReceiverThread )
{
	Sender sender;
	qt::TrackableBridge bridge;
	stellyra::RecursiveEvent< int > event;

	auto connection = qt::connectSignalToEvent( &sender, &Sender::valueChanged, &bridge, event );
	ASSERT_TRUE( connection );

	DataReceiver receiver;
	std::thread::id receivedOnThread {};
	event.connectLambda( receiver, [ & ]( int v ) {
		receiver.processData( v );
		receivedOnThread = std::this_thread::get_id();
	} );

	const auto mainThread = std::this_thread::get_id();

	// emit from a different thread than bridge's (the context) - Auto
	// connection must queue onto bridge's thread, i.e. this (main) thread
	std::thread worker( [ & ]() { emit sender.valueChanged( 42 ); } );
	worker.join();

	pumpUntil( [ & ]() { return receiver.callCount == 1; } );

	EXPECT_EQ( receiver.lastValue, 42 );
	EXPECT_EQ( receivedOnThread, mainThread );
}

TEST( QtIntegration, AnchorDestroyedFirst_ConnectionInvalidated )
{
	Sender sender;
	auto bridge = std::make_unique< qt::TrackableBridge >();
	stellyra::RecursiveEvent< int > event;

	auto connection = qt::connectSignalToEvent( &sender, &Sender::valueChanged, bridge.get(), event );
	ASSERT_TRUE( connection );

	bridge.reset();

	EXPECT_FALSE( static_cast< bool >( connection ) )
		<< "Qt disconnects a functor connection automatically when its context object is destroyed";

	// emitting after the context is gone must not call the lambda (and
	// must not crash even though `event` is still referenced by it)
	int triggerCountBefore = 0;
	stellyra::Trackable anchor;
	event.connectLambda( anchor, [ & ]( int ) { ++triggerCountBefore; } );
	emit sender.valueChanged( 7 );
	EXPECT_EQ( triggerCountBefore, 0 );
}

TEST( QtIntegration, SignalSourceDestroyedFirst_AnchorDestroyedAfter_NoCrash )
{
	auto sender = std::make_unique< Sender >();
	auto bridge = std::make_unique< qt::TrackableBridge >();
	stellyra::RecursiveEvent< int > event;

	auto connection = qt::connectSignalToEvent( sender.get(), &Sender::valueChanged, bridge.get(), event );
	ASSERT_TRUE( connection );

	sender.reset();
	bridge.reset();
	// reaching here without crashing/UB (and cleanly under a sanitizer) is
	// the assertion: tearing down sender before its context is the ordinary
	// Qt teardown order and must not depend on `event` outliving either
	SUCCEED();
}

TEST( QtIntegration, ExplicitDirectConnectionIsHonored_RunsOnEmittingThread )
{
	Sender sender;
	qt::TrackableBridge bridge;
	stellyra::RecursiveEvent< int > event;

	// Direct, not the default Auto - must NOT be silently upgraded to a
	// queued cross-thread dispatch just because bridge lives on another thread
	auto connection = qt::connectSignalToEvent(
		&sender, &Sender::valueChanged, &bridge, event, Qt::DirectConnection );
	ASSERT_TRUE( connection );

	std::atomic< bool > fired { false };
	std::thread::id handlerThread {};
	stellyra::Trackable anchor;
	event.connectLambda( anchor, [ & ]( int ) {
		handlerThread = std::this_thread::get_id();
		fired = true;
	} );

	std::thread::id workerThreadId {};
	std::thread worker( [ & ]() {
		workerThreadId = std::this_thread::get_id();
		emit sender.valueChanged( 1 );
	} );
	worker.join();

	ASSERT_TRUE( fired.load() );
	EXPECT_EQ( handlerThread, workerThreadId )
		<< "Qt::DirectConnection must run synchronously on the emitting thread, not queue onto bridge's thread";
}

TEST( QtIntegration, ReentrantRecursiveEventFedBySignal_DoesNotDeadlock )
{
	Sender sender;
	qt::TrackableBridge bridge;
	stellyra::RecursiveEvent< int > event;

	// Direct keeps this same-thread, so the nested emission below is a
	// genuine same-thread reentrant trigger() call, not a cross-thread hop
	qt::connectSignalToEvent( &sender, &Sender::valueChanged, &bridge, event, Qt::DirectConnection );

	int depth = 0;
	int maxDepthSeen = 0;
	stellyra::Trackable anchor;
	event.connectLambda( anchor, [ & ]( int v ) {
		++depth;
		maxDepthSeen = std::max( maxDepthSeen, depth );
		if ( depth == 1 )
		{
			emit sender.valueChanged( v + 1 );  // reentrant - would deadlock on a non-recursive mutex
		}
		--depth;
	} );

	emit sender.valueChanged( 1 );

	EXPECT_EQ( maxDepthSeen, 2 );
}

// ---------------------------------------------------------------------------
// connectEventToTrackableBridge
// ---------------------------------------------------------------------------

TEST( QtIntegration, ConnectEventToTrackableBridge_InvokesMethodOnDerivedBridge )
{
	TestBridge bridge;
	stellyra::RecursiveEvent< int > event;

	auto connection = qt::connectEventToTrackableBridge( event, bridge, &TestBridge::onValue );
	ASSERT_TRUE( connection.isConnected() );

	event( 99 );

	EXPECT_EQ( bridge.callCount, 1 );
	EXPECT_EQ( bridge.lastValue, 99 );
}

TEST( QtIntegration, ConnectEventToTrackableBridge_DisconnectsWhenBridgeDestroyed )
{
	stellyra::RecursiveEvent< int > event;
	stellyra::Connection connection;

	{
		TestBridge bridge;
		connection = qt::connectEventToTrackableBridge( event, bridge, &TestBridge::onValue );
		ASSERT_TRUE( connection.isConnected() );
		event( 1 );
		EXPECT_EQ( bridge.callCount, 1 );
	} // bridge (and its composed Trackable) destroyed here

	EXPECT_FALSE( connection.isConnected() );
	event( 2 ); // must not call into the destroyed bridge
}

// ---------------------------------------------------------------------------
// EventLoopBridge - automatic drain-thread detection
//
// None of these call updateDrainThread() manually - that's the point:
// construction and moveToThread() alone must keep the owned EventLoop's
// drain thread correct with no other action.
// ---------------------------------------------------------------------------

TEST( QtIntegration, EventLoopBridge_ConstructionAloneRegistersDrainThreadImmediately )
{
	qt::EventLoopBridge bridge;

	ASSERT_TRUE( bridge.eventLoop().hasDrainThread() );
	EXPECT_EQ( bridge.eventLoop().drainThread(), std::this_thread::get_id() )
		<< "construction alone (no moveToThread, no manual call) must register the constructing thread";
}

TEST( QtIntegration, EventLoopBridge_MoveToThreadAutomaticallyUpdatesDrainThread )
{
	qt::EventLoopBridge bridge;
	ASSERT_EQ( bridge.eventLoop().drainThread(), std::this_thread::get_id() );

	QThread worker;
	std::atomic< bool > workerThreadIdKnown { false };
	std::thread::id workerThreadId {};
	QObject::connect( &worker, &QThread::started, &worker, [ & ]() {
		workerThreadId = std::this_thread::get_id();
		workerThreadIdKnown = true;
	}, Qt::DirectConnection );
	worker.start();
	pumpUntil( [ & ]() { return workerThreadIdKnown.load(); } );

	bridge.moveToThread( &worker );  // no manual updateDrainThread()/requestDrainThreadUpdate() call

	pumpUntil( [ & ]() { return bridge.eventLoop().drainThread() == workerThreadId; } );

	// detach affinity synchronously while worker's loop is still running -
	// moveToThread() may only be called from the object's own current thread,
	// so this must run ON worker (via a blocking queued call), not from main
	// after the fact; calling it from main once worker has already stopped is
	// refused by Qt (logs a warning, does nothing) and would leave bridge's
	// affinity dangling at worker's about-to-be-destroyed QThread object
	QMetaObject::invokeMethod( &bridge, [ & ]() { bridge.moveToThread( nullptr ); }, Qt::BlockingQueuedConnection );
	worker.quit();
	worker.wait();
}

TEST( QtIntegration, EventLoopBridge_TriggerAfterAutomaticMoveDispatchesOnNewThreadWithNoManualCall )
{
	qt::EventLoopBridge bridge;

	QThread worker;
	std::atomic< bool > workerThreadIdKnown { false };
	std::thread::id workerThreadId {};
	QObject::connect( &worker, &QThread::started, &worker, [ & ]() {
		workerThreadId = std::this_thread::get_id();
		workerThreadIdKnown = true;
	}, Qt::DirectConnection );
	worker.start();
	pumpUntil( [ & ]() { return workerThreadIdKnown.load(); } );

	bridge.moveToThread( &worker );  // no manual updateDrainThread()/requestDrainThreadUpdate() call

	// wait until the automatic registration has actually resolved before
	// triggering, so this test checks the resolved steady state deterministically
	// rather than racing the ThreadChange handler's own queued update
	pumpUntil( [ & ]() { return bridge.eventLoop().drainThread() == workerThreadId; } );

	stellyra::Trackable receiver;
	receiver.setEventLoop( &bridge.eventLoop() );

	stellyra::RecursiveEvent< int > event;
	std::atomic< bool > fired { false };
	std::thread::id handlerThread {};
	int lastValue = 0;
	event.connectLambda( receiver, [ & ]( int v ) {
		handlerThread = std::this_thread::get_id();
		lastValue = v;
		fired = true;
	} );

	event( 7 );  // triggered from this (main) thread, must defer onto bridge's (worker's) thread

	pumpUntil( [ & ]() { return fired.load(); } );

	EXPECT_EQ( lastValue, 7 );
	EXPECT_EQ( handlerThread, workerThreadId )
		<< "must have run on the automatically-registered drain thread (worker), not the triggering (main) thread";

	// see the previous test's cleanup comment for why this must run on
	// worker itself, via a blocking queued call, rather than from main
	QMetaObject::invokeMethod( &bridge, [ & ]() { bridge.moveToThread( nullptr ); }, Qt::BlockingQueuedConnection );
	worker.quit();
	worker.wait();
}

int main( int argc, char** argv )
{
	QCoreApplication app( argc, argv );
	::testing::InitGoogleTest( &argc, argv );
	return RUN_ALL_TESTS();
}
