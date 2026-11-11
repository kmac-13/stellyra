#include "test_helpers.h"

#include <kmac/stellyra/event_loop.h>

// ---------------------------------------------------------------------------
// forwardFrom
//
// Destination-side counterpart to forwardTo (see test_forward_to.cpp):
// moved.forwardFrom( xChanged ) has the same net effect as
// xChanged.forwardTo( moved ), but is written on the destination and uses
// the destination's own trigger access to itself - so, unlike forwardTo(),
// it also works when the destination is a BasicPrivateEvent (see the
// PrivateEvent section below and test_private_event_multi_owner.cpp).
//
// Routes through the same createPartial machinery, so most of the
// argument-adaptation behaviour is already covered by test_partial_args.cpp
// and test_forward_to.cpp; this file focuses on the surface forwardFrom
// adds: the reversed tracker auto-sourcing, and the trigger-level access
// restriction on a private destination.
// ---------------------------------------------------------------------------

namespace {

template< typename MutexType >
class Widget : public stellyra::Trackable
{
public:
	stellyra::BasicEvent< MutexType, int, int > moveEv { this };  // (x, y)
	stellyra::BasicEvent< MutexType, int > xChanged { this };     // (x)
	stellyra::BasicEvent< MutexType > moved { this };             // ()
};

template< typename MutexType >
struct LoopedSource : public stellyra::Trackable
{
	stellyra::EventLoop loop;
	stellyra::BasicEvent< MutexType, int > xChanged { this };
	LoopedSource() { setEventLoop( &loop ); }
};

template< typename MutexType >
struct LoopedTarget : public stellyra::Trackable
{
	stellyra::EventLoop loop;
	stellyra::BasicEvent< MutexType > moved { this };
	LoopedTarget() { setEventLoop( &loop ); }
};

} // namespace

template< typename MutexType >
class ForwardFrom : public ::testing::Test {};

using MutexTypes = ::testing::Types<
	stellyra::platform::RecursiveMutex,
	stellyra::platform::SharedMutex,
	stellyra::platform::NullMutex >;
TYPED_TEST_SUITE( ForwardFrom, MutexTypes );

// ---------------------------------------------------------------------------
// Basic forwarding - partial and exact arity (mirrors forwardTo's own)
// ---------------------------------------------------------------------------

TYPED_TEST( ForwardFrom, DropsAllArgs )
{
	Widget< TypeParam > w;
	int calls = 0;
	w.moved.connectLambda( w, [ & ]() { calls++; } );

	w.moved.forwardFrom( w.xChanged );
	w.xChanged( 5 );

	EXPECT_EQ( calls, 1 );
}

TYPED_TEST( ForwardFrom, DropsTrailingArgFromTwoArgSource )
{
	Widget< TypeParam > w;
	int calls = 0;
	w.moved.connectLambda( w, [ & ]() { calls++; } );

	w.moved.forwardFrom( w.moveEv );
	w.moveEv( 1, 2 );

	EXPECT_EQ( calls, 1 );
}

TYPED_TEST( ForwardFrom, KeepsFirstNPrefix )
{
	Widget< TypeParam > w;
	stellyra::BasicEvent< TypeParam, int > firstOnly { &w };
	int lastX = -1;
	firstOnly.connectLambda( w, [ & ]( int x ) { lastX = x; } );

	firstOnly.forwardFrom( w.moveEv );
	w.moveEv( 7, 9 );

	EXPECT_EQ( lastX, 7 );
}

TYPED_TEST( ForwardFrom, ExactArityForwardsDirectly )
{
	Widget< TypeParam > w;
	stellyra::BasicEvent< TypeParam, int > mirror { &w };
	int last = -1;
	mirror.connectLambda( w, [ & ]( int x ) { last = x; } );

	mirror.forwardFrom( w.xChanged );
	w.xChanged( 99 );

	EXPECT_EQ( last, 99 );
}

TYPED_TEST( ForwardFrom, MultipleTriggersForwardEachTime )
{
	Widget< TypeParam > w;
	int calls = 0;
	w.moved.connectLambda( w, [ & ]() { calls++; } );

	w.moved.forwardFrom( w.xChanged );
	w.xChanged( 1 );
	w.xChanged( 2 );
	w.xChanged( 3 );

	EXPECT_EQ( calls, 3 );
}

// ---------------------------------------------------------------------------
// Reversed auto-Trackable pickup and lifetime: tracked from *this* event's
// (the destination's) own owner, the mirror image of forwardTo()'s
// target-owner tracking.
// ---------------------------------------------------------------------------

TYPED_TEST( ForwardFrom, TrackerIsAutoSourcedFromDestinationOwner )
{
	Widget< TypeParam > w;
	int calls = 0;
	w.moved.connectLambda( w, [ & ]() { calls++; } );

	stellyra::Connection c = w.moved.forwardFrom( w.xChanged );
	EXPECT_TRUE( c.isConnected() );
}

TYPED_TEST( ForwardFrom, DestroyingDestinationsOwnerSeversTheForward )
{
	Widget< TypeParam > source;
	{
		Widget< TypeParam > dest;
		dest.moved.forwardFrom( source.xChanged );

		int calls = 0;
		dest.moved.connectLambda( dest, [ & ]() { calls++; } );
		source.xChanged( 1 );
		EXPECT_EQ( calls, 1 );
	}
	// dest destroyed here - Trackable::disconnectAll() should sever the
	// forwarding connection it holds on source.xChanged

	// must not dangle-trigger into the destroyed destination
	source.xChanged( 2 );
}

TYPED_TEST( ForwardFrom, UntrackedWhenDestinationHasNoOwner )
{
	// a destination with no owner Trackable still forwards, just without automatic
	// lifetime binding - same trust model as connectFree() with no tracker
	Widget< TypeParam > w;
	stellyra::BasicEvent< TypeParam > orphanDest;  // no owner
	int calls = 0;
	orphanDest.connectLambda( [ & ]() { calls++; } );

	orphanDest.forwardFrom( w.xChanged );
	w.xChanged( 1 );

	EXPECT_EQ( calls, 1 );
}

// ---------------------------------------------------------------------------
// Returned Connection
// ---------------------------------------------------------------------------

TYPED_TEST( ForwardFrom, ReturnedConnectionExplicitlyDisconnects )
{
	Widget< TypeParam > w;
	int calls = 0;
	w.moved.connectLambda( w, [ & ]() { calls++; } );

	stellyra::Connection c = w.moved.forwardFrom( w.xChanged );
	w.xChanged( 1 );
	EXPECT_EQ( calls, 1 );

	c.disconnect();
	w.xChanged( 2 );
	EXPECT_EQ( calls, 1 );
}

// ---------------------------------------------------------------------------
// Cross-loop composition - same as forwardTo(), just initiated from the
// other side.
// ---------------------------------------------------------------------------

TYPED_TEST( ForwardFrom, CrossLoopForwardLandsInDestinationsContext )
{
	LoopedSource< TypeParam > src;
	LoopedTarget< TypeParam > tgt;

	int calls = 0;
	tgt.moved.connectLambda( tgt, [ & ]() { calls++; } );

	tgt.moved.forwardFrom( src.xChanged );

	src.xChanged( 1 );      // sender-side deferral: lands in src.loop first
	src.loop.drain();       // forwarding connection resolves + posts to tgt.loop
	EXPECT_EQ( calls, 0 );  // not yet delivered to target's context

	tgt.loop.drain();       // target's own trigger runs here, dispatching Direct
	EXPECT_EQ( calls, 1 );
}

// ---------------------------------------------------------------------------
// PrivateEvent: forwardFrom() is the one event-chaining form that actually
// works on a private destination - see test_private_event_multi_owner.cpp
// for the multi-friend EventFriendTypeList angle on this same surface.
// ---------------------------------------------------------------------------

namespace {

class SecureWidget : public stellyra::Trackable
{
public:
	stellyra::PEvent< SecureWidget, int > moved { this };

	// only SecureWidget's own code may wire a source into moved - that is
	// exactly what the trigger-level access restriction protects
	void wire( stellyra::BasicEvent< stellyra::platform::RecursiveMutex, int >& source )
	{
		moved.forwardFrom( source );
	}
};

} // namespace

TEST( ForwardFromPrivateEvent, WiresAndForwards )
{
	SecureWidget w;
	stellyra::Event< int > source;
	int last = -1;
	w.moved.connectLambda( w, [ & ]( int x ) { last = x; } );

	w.wire( source );
	source( 42 );

	EXPECT_EQ( last, 42 );
}
