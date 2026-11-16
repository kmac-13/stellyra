#include "test_helpers.h"

#include <kmac/stellyra/recordable_signal_types.h>
#include <kmac/stellyra/private_signal_types.h>
#include <kmac/stellyra/signal_types.h>
#include <kmac/stellyra_extras.h>

#include <mutex>
#include <string>
#include <type_traits>

// ---------------------------------------------------------------------------
// Function-style signature support (event_signature.h):
//
//   Event< int, std::string >          // plain pack form
//   Event< void( int, std::string ) >  // equivalent function-style form
//
// Both forms must resolve to the exact same type, across every
// Args...-based alias family: Event/SharedEvent/SingleThreadedEvent,
// ConcurrentEvent/SingleThreadedConcurrentEvent, the *RecordableEvent
// family, and PrivateEvent/PEvent/SharedPrivateEvent/SingleThreadedPrivateEvent
// (including the multi-friend EventFriendTypeList form).  Non-void returns
// must be rejected at compile time rather than silently accepted.
//
// BasicEvent/BasicConcurrentEvent/BasicRecordableEvent/BasicPrivateEvent
// themselves (not just the Event/ConcurrentEvent/RecordableEvent/
// PrivateEvent aliases) also understand the function-style form directly -
// this is what lets a user with their own MutexType (not one of the ones
// each alias family's own aliases cover) get the same convenience, with
// the same "exact same type" guarantee the alias forms give: each resolves
// the function-style form before naming its underlying class (GenericEvent,
// GenericConcurrentEvent, GenericRecordableEvent, GenericPrivateEvent
// respectively).  See the "direct usage" sections below.
//
// BasicCombiningEvent (combining_event_types.h) gets the same function-style
// convenience through a separate mechanism (ResolveCombiningEventArgs,
// confined to that header) - rather than requiring R to be void and
// discarding it the way every family above does, it captures R and forwards
// it as BasicCombiningEvent's own ReturnType, since that return value is
// meaningful there.  See the "BasicCombiningEvent direct usage" section below.
// ---------------------------------------------------------------------------

namespace {

class Owner : public stellyra::Trackable
{
};

class OtherFriend : public stellyra::Trackable
{
};

class Receiver : public stellyra::Trackable
{
public:
	void onValue( int, const std::string& ) { ++callCount; }
	int callCount = 0;
};

} // namespace

// ---------------------------------------------------------------------------
// Core Event family (event.h)
// ---------------------------------------------------------------------------

static_assert( std::is_same_v<
		stellyra::RecursiveEvent< int, std::string >,
		stellyra::RecursiveEvent< void( int, std::string ) >
	>,
	"Event<Args...> and Event<void(Args...)> must be the same type" );

static_assert( std::is_same_v<
		stellyra::SharedEvent< int, std::string >,
		stellyra::SharedEvent< void( int, std::string ) >
	> );

static_assert( std::is_same_v<
		stellyra::SingleThreadedEvent< int, std::string >,
		stellyra::SingleThreadedEvent< void( int, std::string ) >
	> );

// zero-argument and single-argument forms, and named parameters (purely
// documentation - the compiler discards the names, so this must be the
// same type as the unnamed spelling)
static_assert( std::is_same_v< stellyra::RecursiveEvent<>, stellyra::RecursiveEvent< void() > > );
static_assert( std::is_same_v< stellyra::RecursiveEvent< int >, stellyra::RecursiveEvent< void( int ) > > );
static_assert( std::is_same_v<
		stellyra::RecursiveEvent< void( int employeeId, const std::string& newTitle ) >,
		stellyra::RecursiveEvent< void( int, const std::string& ) >
	> );

// ---------------------------------------------------------------------------
// ConcurrentEvent family (concurrent_event.h)
// ---------------------------------------------------------------------------

static_assert( std::is_same_v<
		stellyra::StandardConcurrentEvent< int, std::string >,
		stellyra::StandardConcurrentEvent< void( int, std::string ) >
	> );

static_assert( std::is_same_v<
		stellyra::SingleThreadedConcurrentEvent< int >,
		stellyra::SingleThreadedConcurrentEvent< void( int ) >
	> );

// ---------------------------------------------------------------------------
// RecordableEvent family (recordable_event.h)
// ---------------------------------------------------------------------------

static_assert( std::is_same_v<
		stellyra::RecursiveRecordableEvent< int >,
		stellyra::RecursiveRecordableEvent< void( int ) >
	> );

static_assert( std::is_same_v<
		stellyra::SharedRecordableEvent< int >,
		stellyra::SharedRecordableEvent< void( int ) >
	> );

static_assert( std::is_same_v<
		stellyra::SingleThreadedRecordableEvent< int >,
		stellyra::SingleThreadedRecordableEvent< void( int ) >
	> );

static_assert( std::is_same_v<
		stellyra::RecursiveRecordableSignal< int >,
		stellyra::RecursiveRecordableSignal< void( int ) >
	> );

// ---------------------------------------------------------------------------
// PrivateEvent family (private_event.h) - FriendType stays a fixed leading
// parameter; only the trailing Args... pack gets the function-style form.
// ---------------------------------------------------------------------------

static_assert( std::is_same_v<
		stellyra::RecursivePrivateEvent< Owner, int, std::string >,
		stellyra::RecursivePrivateEvent< Owner, void( int, std::string ) >
	> );

static_assert( std::is_same_v<
		stellyra::RecursivePrivateSignal< Owner, int >,
		stellyra::RecursivePrivateSignal< Owner, void( int ) >
	> );

static_assert( std::is_same_v<
		stellyra::SharedPrivateEvent< Owner, int >,
		stellyra::SharedPrivateEvent< Owner, void( int ) >
	> );

static_assert( std::is_same_v<
		stellyra::SingleThreadedPrivateEvent< Owner, int >,
		stellyra::SingleThreadedPrivateEvent< Owner, void( int ) >
	> );

// multi-friend (EventFriendTypeList) form, through the function-style spelling
static_assert( std::is_same_v<
		stellyra::RecursivePrivateEvent< stellyra::EventFriendTypeList< Owner, OtherFriend >, int, std::string >,
		stellyra::RecursivePrivateEvent< stellyra::EventFriendTypeList< Owner, OtherFriend >, void( int, std::string ) >
	> );

// ---------------------------------------------------------------------------
// Runtime behaviour: a function-style event connects and dispatches exactly
// like its plain-pack equivalent - this option changes nothing at runtime.
// ---------------------------------------------------------------------------

TEST( EventSignature, FunctionStyleEventConnectsAndDispatches )
{
	Owner owner;
	Receiver receiver;

	stellyra::RecursiveEvent< void( int, std::string ) > valueChanged { &owner };
	valueChanged.connect( receiver, &Receiver::onValue );

	valueChanged( 42, "hello" );

	EXPECT_EQ( receiver.callCount, 1 );
}

TEST( EventSignature, FunctionStyleConcurrentEventConnectsAndDispatches )
{
	Owner owner;
	Receiver receiver;

	stellyra::StandardConcurrentEvent< void( int, std::string ) > valueChanged { &owner };
	valueChanged.connect( receiver, &Receiver::onValue );

	valueChanged( 7, "world" );

	EXPECT_EQ( receiver.callCount, 1 );
}

TEST( EventSignature, FunctionStylePrivateEventRestrictsTriggering )
{
	class RestrictedOwner : public stellyra::Trackable
	{
	public:
		stellyra::RecursivePrivateEvent< RestrictedOwner, void( int, std::string ) > valueChanged { this };

		void fire( int x, const std::string& s ) { valueChanged( x, s ); }
	};

	RestrictedOwner owner;
	Receiver receiver;

	owner.valueChanged.connect( receiver, &Receiver::onValue );
	owner.fire( 1, "a" );

	EXPECT_EQ( receiver.callCount, 1 );
}

// ---------------------------------------------------------------------------
// BasicEvent direct usage - a custom MutexType, not one of the three the
// Event/SharedEvent/SingleThreadedEvent aliases cover, also gets the
// function-style form by naming BasicEvent directly.
// ---------------------------------------------------------------------------

namespace {

/// Deliberately not one of platform::* - proves this isn't piggybacking on
/// the alias machinery, and that a user's own mutex type works unmodified.
struct CustomMutex
{
	std::recursive_mutex _mutex;
	void lock() { _mutex.lock(); }
	void unlock() { _mutex.unlock(); }
	bool try_lock() { return _mutex.try_lock(); }
};

} // namespace

// The function-style and plain-pack spellings of BasicEvent itself are the
// exact same type, just like the alias forms above - BasicEvent resolves
// the function-style form before naming GenericEvent (see basic_event.h),
// the same way Event/SharedEvent/SingleThreadedEvent resolve it before
// naming GenericEvent.
static_assert( std::is_same_v<
		stellyra::BasicEvent< CustomMutex, void( int, std::string ) >,
		stellyra::BasicEvent< CustomMutex, int, std::string >
	> );

TEST( EventSignature, BasicEventWithCustomMutexConnectsAndDispatches )
{
	Owner owner;
	Receiver receiver;

	stellyra::BasicEvent< CustomMutex, void( int, std::string ) > valueChanged { &owner };
	valueChanged.connect( receiver, &Receiver::onValue );

	valueChanged( 1, "a" );
	valueChanged.trigger( 2, "b" );
	valueChanged.emit( 3, "c" );

	EXPECT_EQ( receiver.callCount, 3 );
}

TEST( EventSignature, EventInspectorCtadSeesThroughFunctionStyleBasicEvent )
{
	Owner owner;
	Receiver receiver;

	stellyra::BasicEvent< CustomMutex, void( int, std::string ) > valueChanged { &owner };
	valueChanged.connect( receiver, &Receiver::onValue );

	// class template argument deduction (CTAD) deduces
	// EventInspector<CustomMutex, int, std::string> here - valueChanged's
	// type IS EventInspector<CustomMutex, int, std::string>'s expected
	// argument type exactly, BasicEvent's void(Args...) and Args...
	// spellings being the same type, so no deduction guide is needed
	auto inspector = stellyra::EventInspector( valueChanged );
	static_assert( std::is_same_v<
			decltype( inspector ),
			stellyra::EventInspector< CustomMutex, int, std::string >
		> );

	EXPECT_EQ( inspector.connectionCount(), 1u );
}

TEST( EventSignature, FunctionStyleBasicEventForwardsBothDirections )
{
	Owner owner;

	stellyra::BasicEvent< CustomMutex, void( int, std::string ) > source { &owner };
	stellyra::BasicEvent< CustomMutex, int, std::string > plainTarget { &owner };
	int plainTargetCalls = 0;
	plainTarget.connectLambda( owner, [ &plainTargetCalls ]( int, std::string ) { ++plainTargetCalls; } );

	source.forwardTo( plainTarget );
	source( 9, "fwd" );
	EXPECT_EQ( plainTargetCalls, 1 );

	stellyra::BasicEvent< CustomMutex, int, std::string > plainSource { &owner };
	stellyra::BasicEvent< CustomMutex, void( int, std::string ) > funcStyleDest { &owner };
	int destCalls = 0;
	funcStyleDest.connectLambda( owner, [ &destCalls ]( int, std::string ) { ++destCalls; } );

	funcStyleDest.forwardFrom( plainSource );
	plainSource( 5, "back" );
	EXPECT_EQ( destCalls, 1 );
}

TEST( EventSignature, FunctionPointerArgumentIsNotMistakenForFuncStyleSyntax )
{
	// a single function-POINTER argument (a genuine use case: an event
	// representing "the active callback changed") must NOT be swallowed by
	// the function-style specialization - R(Args...) and R(*)(Args...) are
	// distinct C++ types
	using Callback = void ( * )( int );

	static_assert( std::is_same_v<
			stellyra::RecursiveEvent< Callback >,
			stellyra::BasicEvent< stellyra::platform::RecursiveMutex, Callback >
		> );
	static_assert( ! std::is_same_v< stellyra::RecursiveEvent< Callback >, stellyra::RecursiveEvent< int > > );

	static int sLastSeen = 0;
	struct Local
	{
		static void callback( int x ) { sLastSeen = x; }
	};

	Owner owner;
	stellyra::RecursiveEvent< Callback > callbackChanged { &owner };
	callbackChanged.connectLambda( owner, []( Callback cb ) { cb( 99 ); } );
	callbackChanged( &Local::callback );

	EXPECT_EQ( sLastSeen, 99 );
}

// ---------------------------------------------------------------------------
// BasicConcurrentEvent direct usage - same treatment as BasicEvent: a
// custom MutexType gets the function-style form with the exact-same-type
// guarantee, by resolving before naming GenericConcurrentEvent (see
// basic_concurrent_event.h).
// ---------------------------------------------------------------------------

static_assert( std::is_same_v<
		stellyra::BasicConcurrentEvent< CustomMutex, int, std::string >,
		stellyra::BasicConcurrentEvent< CustomMutex, void( int, std::string ) >
	> );

static_assert( std::is_same_v<
		stellyra::GenericConcurrentEvent< CustomMutex, int, std::string >,
		stellyra::BasicConcurrentEvent< CustomMutex, int, std::string >
	> );

TEST( EventSignature, BasicConcurrentEventWithCustomMutexConnectsAndDispatches )
{
	Owner owner;
	Receiver receiver;

	stellyra::BasicConcurrentEvent< CustomMutex, void( int, std::string ) > valueChanged { &owner };
	valueChanged.connect( receiver, &Receiver::onValue );

	valueChanged( 1, "a" );
	valueChanged.trigger( 2, "b" );
	valueChanged.emit( 3, "c" );

	EXPECT_EQ( receiver.callCount, 3 );
}

// ---------------------------------------------------------------------------
// BasicRecordableEvent direct usage - same treatment, resolving before
// naming GenericRecordableEvent (see basic_recordable_event.h).
// ---------------------------------------------------------------------------

static_assert( std::is_same_v<
		stellyra::BasicRecordableEvent< CustomMutex, int, std::string >,
		stellyra::BasicRecordableEvent< CustomMutex, void( int, std::string ) >
	> );

static_assert( std::is_same_v<
		stellyra::GenericRecordableEvent< CustomMutex, int, std::string >,
		stellyra::BasicRecordableEvent< CustomMutex, int, std::string >
	> );

TEST( EventSignature, BasicRecordableEventWithCustomMutexConnectsAndDispatches )
{
	Owner owner;
	Receiver receiver;

	stellyra::BasicRecordableEvent< CustomMutex, void( int, std::string ) > valueChanged { &owner };
	valueChanged.connect( receiver, &Receiver::onValue );
	valueChanged.recorder().startRecording();

	valueChanged( 1, "a" );

	EXPECT_EQ( receiver.callCount, 1 );
	EXPECT_EQ( valueChanged.recorder().recordingCount(), 1u );
}

// ---------------------------------------------------------------------------
// BasicPrivateEvent direct usage - same treatment, resolving before naming
// GenericPrivateEvent (see private_event.h), for both the single-friend and
// multi-friend EventFriendTypeList forms.  EventInspector's private-event
// constructor takes GenericPrivateEvent directly (same CTAD-deduction reason
// as the primary constructor and GenericEvent), so it's exercised here too.
// ---------------------------------------------------------------------------

static_assert( std::is_same_v<
		stellyra::BasicPrivateEvent< Owner, CustomMutex, int, std::string >,
		stellyra::BasicPrivateEvent< Owner, CustomMutex, void( int, std::string ) >
	> );

static_assert( std::is_same_v<
		stellyra::GenericPrivateEvent< Owner, CustomMutex, int, std::string >,
		stellyra::BasicPrivateEvent< Owner, CustomMutex, int, std::string >
	> );

static_assert( std::is_same_v<
		stellyra::BasicPrivateEvent< stellyra::EventFriendTypeList< Owner, OtherFriend >, CustomMutex, int >,
		stellyra::BasicPrivateEvent< stellyra::EventFriendTypeList< Owner, OtherFriend >, CustomMutex, void( int ) >
	> );

TEST( EventSignature, BasicPrivateEventWithCustomMutexRestrictsTriggering )
{
	class RestrictedOwner : public stellyra::Trackable
	{
	public:
		stellyra::BasicPrivateEvent< RestrictedOwner, CustomMutex, void( int, std::string ) > valueChanged { this };

		void fire( int x, const std::string& s ) { valueChanged( x, s ); }
	};

	RestrictedOwner owner;
	Receiver receiver;

	owner.valueChanged.connect( receiver, &Receiver::onValue );
	owner.fire( 1, "a" );

	EXPECT_EQ( receiver.callCount, 1 );
}

TEST( EventSignature, EventInspectorCtadSeesThroughFunctionStyleBasicPrivateEvent )
{
	class RestrictedOwner : public stellyra::Trackable
	{
	public:
		stellyra::BasicPrivateEvent< RestrictedOwner, CustomMutex, void( int, std::string ) > valueChanged { this };
	};

	RestrictedOwner owner;
	Receiver receiver;

	owner.valueChanged.connect( receiver, &Receiver::onValue );

	auto inspector = stellyra::EventInspector( owner.valueChanged );
	static_assert( std::is_same_v<
			decltype( inspector ),
			stellyra::EventInspector< CustomMutex, int, std::string >
		> );

	EXPECT_EQ( inspector.connectionCount(), 1u );
}

// ---------------------------------------------------------------------------
// BasicCombiningEvent direct usage - a separate resolving mechanism
// (ResolveCombiningEventArgs, confined to combining_event_types.h) from the
// ResolveEventArgs/ResolvePrivateEventArgs families above: BasicCombiningEvent's
// return type is meaningful (the per-handler value its Combiner reduces),
// so unlike every alias above, the function-style form here is NOT
// restricted to void - the resolver captures and forwards R rather than
// asserting it away.  BasicCombiningEvent<Combiner, R(Args...)> and
// BasicCombiningEvent<Combiner, R, Args...> must still be the exact same type.
// ---------------------------------------------------------------------------

static_assert( std::is_same_v<
		stellyra::BasicCombiningEvent< stellyra::Combiners::LogicalAnd<>, bool, std::string >,
		stellyra::BasicCombiningEvent< stellyra::Combiners::LogicalAnd<>, bool( std::string ) >
	> );

static_assert( std::is_same_v<
		stellyra::GenericCombiningEvent< stellyra::Combiners::LogicalAnd<>, bool, std::string >,
		stellyra::BasicCombiningEvent< stellyra::Combiners::LogicalAnd<>, bool, std::string >
	> );

TEST( EventSignature, CombiningEventFunctionStyleConnectsAndCombines )
{
	stellyra::BasicCombiningEvent< stellyra::Combiners::LogicalAnd<>, bool( const std::string& ) > validate;

	validate.connectLambda( []( const std::string& s ) { return ! s.empty(); } );
	validate.connectLambda( []( const std::string& s ) { return s != "bad"; } );

	EXPECT_TRUE( validate( "ok" ) );
	EXPECT_FALSE( validate( "bad" ) );
}
