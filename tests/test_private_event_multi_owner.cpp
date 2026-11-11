#include "test_helpers.h"

// ---------------------------------------------------------------------------
// BasicPrivateEvent<EventFriendTypeList<Types...>, ...> - the multi-friend
// opt-in form.
//
// BasicPrivateEvent<FriendType, ...> (single, bare FriendType) restricts
// triggering to exactly one class.  EventFriendTypeList<Types...> lifts
// that to any fixed list of classes - the motivating case is a decoupling
// "Setup"-style mediator class that wires an otherwise-unrelated source event
// into a private destination via forwardFrom() (see test_forward_from.cpp),
// which needs friend access to the destination alongside its natural
// owner.
//
// Internally, both forms share one implementation
// (detail::BasicPrivateEventImpl, chained through detail::FriendChain);
// this file exercises the multi-friend surface specifically - the
// single-friend form's own behaviour (connect/disconnect/auto-disconnect/
// EventInspector) is already covered by the PrivateEvent section of
// test_coverage_gaps.cpp and is deliberately not re-tested here beyond a
// basic "existing call-site shape still compiles and runs" sanity check.
// ---------------------------------------------------------------------------

namespace {

class Receiver
{
public:
	void onValue( int x ) { lastX = x; ++callCount; }
	int lastX = -1;
	int callCount = 0;
};

class Setup;  // forward - the second, decoupling friend

class OwnerA : public stellyra::Trackable
{
public:
	stellyra::PrivateEvent< stellyra::EventFriendTypeList< OwnerA, Setup >, int > valueChanged { this };

	void setValue( int x ) { valueChanged( x ); }
};

class SourceB : public stellyra::Trackable
{
public:
	stellyra::Event< int > produced { this };

	void produce( int x ) { produced( x ); }
};

// the decoupling wiring class - neither OwnerA's natural owner, nor
// SourceB's - just a third party that knows about both and is listed as a
// second friend on OwnerA::valueChanged specifically so it can wire them
// together and, separately, trigger directly (if it needs to)
class Setup
{
public:
	static stellyra::Connection wire( OwnerA& a, SourceB& b )
	{
		return a.valueChanged.forwardFrom( b.produced );
	}

	static void triggerDirectly( OwnerA& a, int x )
	{
		a.valueChanged( x );
	}
};

} // namespace

// ---------------------------------------------------------------------------
// Single-friend form still works unchanged through the shared
// FriendChain-based implementation (call-site shape is documented as
// unaffected by the EventFriendTypeList specialization existing).
// ---------------------------------------------------------------------------

TEST( PrivateEventMultiOwner, SingleFriendFormUnaffected )
{
	class SecureButton : public stellyra::Trackable
	{
	public:
		stellyra::PEvent< SecureButton, int, int > clicked { this };
		void click( int x, int y ) { clicked( x, y ); }
	};

	SecureButton button;
	Receiver recv;
	button.clicked.connect( recv, &Receiver::onValue );  // partial-arity ok
	button.click( 5, 10 );

	EXPECT_EQ( recv.callCount, 1 );
	EXPECT_EQ( recv.lastX, 5 );
}

// ---------------------------------------------------------------------------
// Multi-friend form: natural owner retains trigger access
// ---------------------------------------------------------------------------

TEST( PrivateEventMultiOwner, NaturalOwnerCanStillTriggerDirectly )
{
	OwnerA a;
	Receiver recv;
	a.valueChanged.connect( recv, &Receiver::onValue );

	a.setValue( 7 );

	EXPECT_EQ( recv.callCount, 1 );
	EXPECT_EQ( recv.lastX, 7 );
}

// ---------------------------------------------------------------------------
// Multi-friend form: the second listed friend can also trigger directly
// ---------------------------------------------------------------------------

TEST( PrivateEventMultiOwner, SecondFriendCanTriggerDirectly )
{
	OwnerA a;
	Receiver recv;
	a.valueChanged.connect( recv, &Receiver::onValue );

	Setup::triggerDirectly( a, 3 );

	EXPECT_EQ( recv.callCount, 1 );
	EXPECT_EQ( recv.lastX, 3 );
}

// ---------------------------------------------------------------------------
// Multi-friend form: the second listed friend can wire an unrelated
// source into the private destination via forwardFrom() - the motivating
// use case from the design note.
// ---------------------------------------------------------------------------

TEST( PrivateEventMultiOwner, SecondFriendCanWireAnUnrelatedSourceViaForwardFrom )
{
	OwnerA a;
	SourceB b;
	Receiver recv;
	a.valueChanged.connect( recv, &Receiver::onValue );

	stellyra::Connection c = Setup::wire( a, b );
	EXPECT_TRUE( c.isConnected() );

	b.produce( 9 );

	EXPECT_EQ( recv.callCount, 1 );
	EXPECT_EQ( recv.lastX, 9 );
}

TEST( PrivateEventMultiOwner, WiredForwardFiresOnEveryProduce )
{
	OwnerA a;
	SourceB b;
	Receiver recv;
	a.valueChanged.connect( recv, &Receiver::onValue );

	Setup::wire( a, b );
	b.produce( 1 );
	b.produce( 2 );
	b.produce( 3 );

	EXPECT_EQ( recv.callCount, 3 );
	EXPECT_EQ( recv.lastX, 3 );
}

// ---------------------------------------------------------------------------
// Connection management (connect/disconnect/block) stays public and
// unrestricted for everyone, same as the single-friend form - only the
// trigger-level surface (operator()/trigger()/emit()/forwardFrom()) is
// friend-gated.
// ---------------------------------------------------------------------------

TEST( PrivateEventMultiOwner, ExternalCodeCanConnectAndDisconnect )
{
	OwnerA a;
	Receiver recv;

	stellyra::Connection c = a.valueChanged.connect( recv, &Receiver::onValue );
	Setup::triggerDirectly( a, 1 );
	EXPECT_EQ( recv.callCount, 1 );

	c.disconnect();
	Setup::triggerDirectly( a, 2 );
	EXPECT_EQ( recv.callCount, 1 );
}

TEST( PrivateEventMultiOwner, AutoDisconnectOnReceiverDestruction )
{
	OwnerA a;
	{
		Receiver recv;
		a.valueChanged.connect( recv, &Receiver::onValue );
		Setup::triggerDirectly( a, 1 );
	}
	// recv destroyed - must not dangle-trigger into it
	EXPECT_NO_THROW( Setup::triggerDirectly( a, 2 ) );
}

// ---------------------------------------------------------------------------
// EventInspector still works on the multi-friend form (it is granted
// friendship directly on both BasicPrivateEventImpl and the outer
// BasicPrivateEvent/EventFriendTypeList specialization - see the
// in-header comment on why both are needed).
// ---------------------------------------------------------------------------

TEST( PrivateEventMultiOwner, InspectorAccess )
{
	OwnerA a;
	Receiver recv;
	a.valueChanged.connect( recv, &Receiver::onValue );

	auto inspector = stellyra::EventInspector( a.valueChanged );
	auto info = inspector.getEventInfo();
	EXPECT_EQ( info.connectionCount, 1u );
	EXPECT_EQ( info.activeConnectionCount, 1u );
}
