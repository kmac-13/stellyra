#include "test_helpers.h"

#include <kmac/stellyra_extras.h>

#include <string>

// ---------------------------------------------------------------------------
// Connection misconfiguration warnings and general lifecycle logging
// (STELLYRA_LOG_CONNECTION_ISSUES / STELLYRA_LOG_CONNECTION_LIFECYCLE),
// added in the 2026-11-18/19 sessions - see TODO.md.
//
// Built as its own CMake target (test_connection_logging) with both flags
// forced on via target_compile_definitions - every other test target builds
// with both flags at their shipped-off default, so this is the only place
// the actual log-line gating and content is exercised against the real
// headers rather than a standalone smoke harness.  Each case captures
// stderr around the action under test (both macros write there - see
// config.h) and inspects the captured text directly.
//
// Covers both EventImpl backends where the logic is duplicated per-backend
// (updateSenderLoop()/updateHandlerLoop() in BasicEventImpl and
// BasicConcurrentEventImpl); the connect-time checks live once in
// EventStorage::connectImpl(), shared by both, so those are exercised with
// RecursiveEvent alone.
// ---------------------------------------------------------------------------

namespace {

// a receiver that inherits Trackable normally - the tracked case
struct TrackedReceiver : stellyra::Trackable
{
	void onInt( int ) {}
};

// deliberately does NOT inherit stellyra::Trackable, to exercise the
// untracked object/method connection warning
struct PlainReceiver
{
	void onInt( int ) {}
};

void freeFunc( int ) {}

} // namespace

// ---------------------------------------------------------------------------
// Connect-time checks (EventStorage::connectImpl) - backend-agnostic, so
// RecursiveEvent alone is enough to cover the shared code path.
// ---------------------------------------------------------------------------

TEST( ConnectionLogging, DirectMismatchWarnsAtConnectTime )
{
	stellyra::EventLoop senderLoop;
	stellyra::EventLoop receiverLoop;

	stellyra::Trackable senderOwner;
	senderOwner.setEventLoop( &senderLoop );

	TrackedReceiver receiver;
	receiver.setEventLoop( &receiverLoop );

	stellyra::RecursiveEvent< int > event( &senderOwner );

	testing::internal::CaptureStderr();
	auto conn = event.connect< &TrackedReceiver::onInt >( receiver, stellyra::ConnectionType::Direct );
	std::string output = testing::internal::GetCapturedStderr();

	EXPECT_NE( output.find( "Direct connection with mismatched loops" ), std::string::npos );
	EXPECT_TRUE( conn.isConnected() );
}

TEST( ConnectionLogging, DirectSameLoopStaysSilentAtConnectTime )
{
	stellyra::EventLoop loop;

	stellyra::Trackable senderOwner;
	senderOwner.setEventLoop( &loop );

	TrackedReceiver receiver;
	receiver.setEventLoop( &loop );

	stellyra::RecursiveEvent< int > event( &senderOwner );

	testing::internal::CaptureStderr();
	event.connect< &TrackedReceiver::onInt >( receiver, stellyra::ConnectionType::Direct );
	std::string output = testing::internal::GetCapturedStderr();

	// this target also has STELLYRA_LOG_CONNECTION_LIFECYCLE on, so a
	// "connect:" lifecycle line is expected here - only the misconfiguration
	// warning itself must be absent
	EXPECT_EQ( output.find( "Direct connection with mismatched loops" ), std::string::npos )
		<< "unexpected mismatch warning: " << output;
}

TEST( ConnectionLogging, DeferredWithNoReceiverLoopWarnsAtConnectTime )
{
	TrackedReceiver receiver;  // no setEventLoop() call - no receiver loop

	stellyra::RecursiveEvent< int > event;

	testing::internal::CaptureStderr();
	event.connect< &TrackedReceiver::onInt >( receiver, stellyra::ConnectionType::Deferred );
	std::string output = testing::internal::GetCapturedStderr();

	EXPECT_NE( output.find( "Deferred connection with no receiver loop" ), std::string::npos );
}

TEST( ConnectionLogging, UntrackedObjectMethodConnectionWarns )
{
	PlainReceiver receiver;  // does not inherit Trackable

	stellyra::RecursiveEvent< int > event;

	testing::internal::CaptureStderr();
	event.connect< &PlainReceiver::onInt >( receiver );
	std::string output = testing::internal::GetCapturedStderr();

	EXPECT_NE( output.find( "untracked object/method connection" ), std::string::npos );
}

TEST( ConnectionLogging, TrackedConnectionStaysSilentForUntrackedWarning )
{
	TrackedReceiver receiver;  // inherits Trackable - not untracked

	stellyra::RecursiveEvent< int > event;

	testing::internal::CaptureStderr();
	event.connect< &TrackedReceiver::onInt >( receiver );
	std::string output = testing::internal::GetCapturedStderr();

	EXPECT_EQ( output.find( "untracked object/method connection" ), std::string::npos );
}

TEST( ConnectionLogging, FreeFunctionConnectionStaysSilentForUntrackedWarning )
{
	stellyra::RecursiveEvent< int > event;

	testing::internal::CaptureStderr();
	event.connectFree( &freeFunc );
	std::string output = testing::internal::GetCapturedStderr();

	// free-function connections never set hasOwner, so the untracked-object/
	// method warning (which only fires for the object/method bucket) must
	// never fire for them, regardless of lifecycle logging noise
	EXPECT_EQ( output.find( "untracked object/method connection" ), std::string::npos );
}

TEST( ConnectionLogging, LambdaConnectionStaysSilentForUntrackedWarning )
{
	stellyra::RecursiveEvent< int > event;

	testing::internal::CaptureStderr();
	event.connectLambda( []( int ) {} );
	std::string output = testing::internal::GetCapturedStderr();

	EXPECT_EQ( output.find( "untracked object/method connection" ), std::string::npos );
}

// ---------------------------------------------------------------------------
// Connect/disconnect/disconnectAll general lifecycle logging - also a
// connectImpl()/disconnectSlotLocked()/preLockDisconnectAll() behavior, so
// RecursiveEvent alone covers the shared and per-backend disconnect paths
// are covered separately below.
// ---------------------------------------------------------------------------

TEST( ConnectionLogging, ConnectLogsLifecycleLine )
{
	TrackedReceiver receiver;
	stellyra::RecursiveEvent< int > event;

	testing::internal::CaptureStderr();
	event.connect< &TrackedReceiver::onInt >( receiver );
	std::string output = testing::internal::GetCapturedStderr();

	EXPECT_NE( output.find( "connect: event=" ), std::string::npos );
}

TEST( ConnectionLogging, DisconnectAllLogsOneSummaryLineNotPerConnection )
{
	TrackedReceiver r1, r2, r3;
	stellyra::RecursiveEvent< int > event;
	event.connect< &TrackedReceiver::onInt >( r1 );
	event.connect< &TrackedReceiver::onInt >( r2 );
	event.connect< &TrackedReceiver::onInt >( r3 );

	testing::internal::CaptureStderr();
	event.disconnectAll();
	std::string output = testing::internal::GetCapturedStderr();

	// exactly one "disconnectAll:" summary line, carrying the removed count -
	// not one "disconnect:" line per connection
	size_t summaryPos = output.find( "disconnectAll: event=" );
	ASSERT_NE( summaryPos, std::string::npos );
	EXPECT_NE( output.find( "removed=3", summaryPos ), std::string::npos );

	size_t secondSummary = output.find( "disconnectAll: event=", summaryPos + 1 );
	EXPECT_EQ( secondSummary, std::string::npos ) << "expected exactly one summary line";
}

// ---------------------------------------------------------------------------
// Migration-time checks - duplicated per-backend in updateSenderLoop() /
// updateHandlerLoop(); parametrized over both BasicEventImpl
// (RecursiveEvent) and BasicConcurrentEventImpl (StandardConcurrentEvent)
// so both copies of the logic are actually exercised here, not just one.
// ---------------------------------------------------------------------------

template< typename EventT >
class ConnectionLoggingMigration : public ::testing::Test {};

using MigrationEventTypes = ::testing::Types<
	stellyra::RecursiveEvent< int >,
	stellyra::StandardConcurrentEvent< int > >;
TYPED_TEST_SUITE( ConnectionLoggingMigration, MigrationEventTypes );

// The receiver migrates first (sender loop still null - mismatch, correctly
// warns), then the sender migrates to the *same* loop the receiver already
// independently moved to (the mismatch resolves and logs a "resolved" line),
// then the sender migrates again to a different loop (mismatch resumes, warns
// again).
TYPED_TEST( ConnectionLoggingMigration, DirectMismatchTracksCurrentReceiverLoopNotConnectTimeSnapshot )
{
	struct SenderOwner : stellyra::Trackable {};

	SenderOwner senderOwner;
	TrackedReceiver receiver;

	TypeParam event( &senderOwner );
	event.template connect< &TrackedReceiver::onInt >( receiver, stellyra::ConnectionType::Direct );

	stellyra::EventLoop loopA;
	stellyra::EventLoop loopB;

	// receiver migrates first - sender loop is still null, so this is a
	// mismatch (null != &loopA)
	testing::internal::CaptureStderr();
	receiver.setEventLoop( &loopA );
	std::string output1 = testing::internal::GetCapturedStderr();
	EXPECT_NE( output1.find( "Direct connection with mismatched loops" ), std::string::npos );

	// sender migrates to the SAME loop the receiver is already on - resolves
	// the mismatch (logs "resolved", not "mismatched loops")
	testing::internal::CaptureStderr();
	senderOwner.setEventLoop( &loopA );
	std::string output2 = testing::internal::GetCapturedStderr();
	EXPECT_EQ( output2.find( "Direct connection with mismatched loops" ), std::string::npos )
		<< "unexpected mismatch warning: " << output2;
	EXPECT_NE( output2.find( "Direct connection mismatch resolved" ), std::string::npos )
		<< "missing resolution line: " << output2;

	// sender migrates away again - mismatch resumes
	testing::internal::CaptureStderr();
	senderOwner.setEventLoop( &loopB );
	std::string output3 = testing::internal::GetCapturedStderr();
	EXPECT_NE( output3.find( "Direct connection with mismatched loops" ), std::string::npos );
}

TYPED_TEST( ConnectionLoggingMigration, DeferredWithNoReceiverLoopWarnsOnReceiverMigrationAway )
{
	TrackedReceiver receiver;
	stellyra::EventLoop loop;
	receiver.setEventLoop( &loop );

	TypeParam event;
	event.template connect< &TrackedReceiver::onInt >( receiver, stellyra::ConnectionType::Deferred );

	// migrating the receiver to no loop at all should warn
	testing::internal::CaptureStderr();
	receiver.setEventLoop( nullptr );
	std::string output = testing::internal::GetCapturedStderr();
	EXPECT_NE( output.find( "Deferred connection with no receiver loop" ), std::string::npos );
}

TYPED_TEST( ConnectionLoggingMigration, DisconnectLogsLifecycleLine )
{
	TrackedReceiver receiver;
	TypeParam event;
	auto conn = event.template connect< &TrackedReceiver::onInt >( receiver );

	testing::internal::CaptureStderr();
	conn.disconnect();
	std::string output = testing::internal::GetCapturedStderr();

	EXPECT_NE( output.find( "disconnect: event=" ), std::string::npos );
}

// ---------------------------------------------------------------------------
// Resolution logging - a migration that takes a Direct/Deferred connection
// from misconfigured back to properly configured must ALSO log, not just
// stay silent the way a good-to-good migration does.  See config.h and
// updateSenderLoop()/updateHandlerLoop() for the full semantics:
//   good -> good:   silent
//   good -> bad:    logs
//   bad  -> bad:    logs again (not edge-triggered within the bad state)
//   bad  -> good:   logs (the resolution itself)
// ---------------------------------------------------------------------------

TYPED_TEST( ConnectionLoggingMigration, DirectMismatchResolvedLogsOnSenderMigration )
{
	struct SenderOwner : stellyra::Trackable {};

	SenderOwner senderOwner;
	TrackedReceiver receiver;
	stellyra::EventLoop loopA;
	receiver.setEventLoop( &loopA );  // receiver on loopA, sender still null -> mismatched from connect

	TypeParam event( &senderOwner );
	event.template connect< &TrackedReceiver::onInt >( receiver, stellyra::ConnectionType::Direct );

	// sender migrates to the same loop the receiver is already on - resolves the mismatch
	testing::internal::CaptureStderr();
	senderOwner.setEventLoop( &loopA );
	std::string output = testing::internal::GetCapturedStderr();

	EXPECT_NE( output.find( "Direct connection mismatch resolved (sender migration)" ), std::string::npos )
		<< "missing resolution line: " << output;
}

TYPED_TEST( ConnectionLoggingMigration, DirectMismatchResolvedLogsOnReceiverMigration )
{
	struct SenderOwner : stellyra::Trackable {};

	SenderOwner senderOwner;
	TrackedReceiver receiver;
	stellyra::EventLoop loopA;
	senderOwner.setEventLoop( &loopA );  // sender on loopA, receiver still null -> mismatched from connect

	TypeParam event( &senderOwner );
	event.template connect< &TrackedReceiver::onInt >( receiver, stellyra::ConnectionType::Direct );

	// receiver migrates to the same loop the sender is already on - resolves the mismatch
	testing::internal::CaptureStderr();
	receiver.setEventLoop( &loopA );
	std::string output = testing::internal::GetCapturedStderr();

	EXPECT_NE( output.find( "Direct connection mismatch resolved (receiver migration)" ), std::string::npos )
		<< "missing resolution line: " << output;
}

TYPED_TEST( ConnectionLoggingMigration, DirectStillMismatchedLogsAgainNotJustOnce )
{
	struct SenderOwner : stellyra::Trackable {};

	SenderOwner senderOwner;
	TrackedReceiver receiver;
	stellyra::EventLoop loopA;
	stellyra::EventLoop loopB;
	receiver.setEventLoop( &loopA );

	TypeParam event( &senderOwner );
	event.template connect< &TrackedReceiver::onInt >( receiver, stellyra::ConnectionType::Direct );

	// sender migrates to loopB - still mismatched against receiver's loopA
	// (was already mismatched against null, stays mismatched against loopB)
	testing::internal::CaptureStderr();
	senderOwner.setEventLoop( &loopB );
	std::string output = testing::internal::GetCapturedStderr();

	EXPECT_NE( output.find( "Direct connection with mismatched loops (sender migration)" ), std::string::npos );
	EXPECT_EQ( output.find( "resolved" ), std::string::npos ) << "must not claim resolution here: " << output;
}

TYPED_TEST( ConnectionLoggingMigration, DirectAlreadyMatchedStaysSilentOnNoOpMigration )
{
	struct SenderOwner : stellyra::Trackable {};

	SenderOwner senderOwner;
	TrackedReceiver receiver;
	stellyra::EventLoop loopA;
	senderOwner.setEventLoop( &loopA );
	receiver.setEventLoop( &loopA );

	TypeParam event( &senderOwner );
	event.template connect< &TrackedReceiver::onInt >( receiver, stellyra::ConnectionType::Direct );

	// re-"migrating" to the SAME loop it's already on - a genuine good->good
	// case, since nothing about the topology changes
	testing::internal::CaptureStderr();
	senderOwner.setEventLoop( &loopA );
	std::string output = testing::internal::GetCapturedStderr();

	EXPECT_EQ( output.find( "Direct connection" ), std::string::npos ) << "unexpected output: " << output;
}

TYPED_TEST( ConnectionLoggingMigration, DeferredResolvedLogsOnReceiverMigration )
{
	TrackedReceiver receiver;  // no loop set - unrouted from connect

	TypeParam event;
	event.template connect< &TrackedReceiver::onInt >( receiver, stellyra::ConnectionType::Deferred );

	stellyra::EventLoop loop;

	testing::internal::CaptureStderr();
	receiver.setEventLoop( &loop );
	std::string output = testing::internal::GetCapturedStderr();

	EXPECT_NE( output.find( "Deferred connection now has a receiver loop (receiver migration)" ), std::string::npos )
		<< "missing resolution line: " << output;
}

TYPED_TEST( ConnectionLoggingMigration, DeferredAlreadyRoutedStaysSilentOnNoOpMigration )
{
	TrackedReceiver receiver;
	stellyra::EventLoop loop;
	receiver.setEventLoop( &loop );

	TypeParam event;
	event.template connect< &TrackedReceiver::onInt >( receiver, stellyra::ConnectionType::Deferred );

	testing::internal::CaptureStderr();
	receiver.setEventLoop( &loop );  // same loop - genuine good->good
	std::string output = testing::internal::GetCapturedStderr();

	EXPECT_EQ( output.find( "Deferred connection" ), std::string::npos ) << "unexpected output: " << output;
}
