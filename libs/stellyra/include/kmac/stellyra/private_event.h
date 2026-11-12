#pragma once
#ifndef KMAC_STELLYRA_PRIVATE_EVENT_H
#define KMAC_STELLYRA_PRIVATE_EVENT_H

/**
 * @file private_event.h
 * @brief PrivateEvent - Event with compile-time restricted triggering.
 *
 * GenericPrivateEvent<FriendType, Args...> inherits privately from
 * GenericEvent<MutexType, Args...> so that only `FriendType` can call the
 * triggering methods (operator(), trigger(), emit(), forwardFrom()), while
 * all connection and disconnect methods remain publicly accessible.
 *
 * This enforces the principle that only the class that owns an event should
 * be able to fire it, while any code can observe it by connecting handlers.
 *
 * @code
 * class Button : public Trackable
 * {
 * public:
 *     // anyone can connect, but only Button can trigger
 *     RecursivePrivateEvent< Button, int, int > clicked { this };
 *
 *     void click( int x, int y )
 *     {
 *         clicked( x, y );   // OK - Button is the friend type
 *     }
 * };
 *
 * Button button;
 * button.clicked.connect< &Handler::onClick >( handler );  // OK
 * button.clicked( 10, 20 );  // compile error - triggering is private
 * @endcode
 *
 * GenericPrivateEvent<EventFriendTypeList<Types...>, Args...> is the opt-in
 * multi-friend form, for the case where more than one class legitimately
 * needs trigger access - e.g. a decoupling `Setup`-style class that wires
 * two otherwise-unrelated owning classes' events together via forwardFrom()
 * needs friend access to the destination event it is wiring into, alongside
 * that event's natural owner:
 *
 * @code
 * class Button : public Trackable
 * {
 * public:
 *     GenericPrivateEvent< EventFriendTypeList< Button, Setup >, int, int > clicked { this };
 * };
 * @endcode
 *
 * @warning Private inheritance means PrivateEvent cannot be used where an
 *   GenericEvent& is expected.
 *
 * @warning The access restriction applies to the type: any instance of a
 *   listed friend type may trigger any PrivateEvent<..., ...> sharing that
 *   friend, not just the one declared on the same instance.  This is a
 *   standard C++ friend limitation.
 *
 * @warning Because Stellyra is header-only, the real (necessarily
 *   header-visible) base event type backing a PrivateEvent can still be
 *   reached via reinterpret_cast, bypassing private/protected/friend
 *   entirely.  This is a structural ceiling on any header-only C++
 *   access-control scheme, not specific to this type - the "private" in
 *   PrivateEvent means "not accidentally reachable," not "cryptographically
 *   enforced."
 */

#include "event.h"

namespace kmac {
namespace stellyra {

/**
 * @brief Tag type naming more than one FriendType for GenericPrivateEvent /
 * BasicPrivateEvent.
 *
 * GenericPrivateEvent<EventFriendTypeList<A, B, ...>, MutexType, Args...> (or
 * the equivalent BasicPrivateEvent option) is the multi-friend opt-in form;
 * the single, bare FriendType form remains the common, single-friend form
 * and is unaffected by this type's existence.  EventFriendTypeList is never
 * instantiated - it exists purely to be named as GenericPrivateEvent's (or
 * BasicPrivateEvent's) first template argument.
 */
template< typename... Types >
struct EventFriendTypeList
{
};

namespace detail {

/**
 * @brief Pure tag bundling a PrivateEvent's MutexType and Args... into a
 * single type, so FriendChain (below) can carry them alongside its own,
 * separate Friends... pack - a template cannot otherwise declare two
 * independent parameter packs.
 */
template< typename MutexType, typename... Args >
struct PrivateEventSignature
{
};

/**
 * @brief Shared implementation base for every BasicPrivateEvent form (both
 * the single-FriendType primary template and the EventFriendTypeList
 * multi-friend specialization) - holds every using-declaration exactly
 * once, so neither form has to repeat the full connect/disconnect/trigger
 * surface.
 *
 * Privately inherits GenericEvent so that, absent an explicit using-
 * declaration, nothing is accidentally exposed.  The connection-management
 * surface (connect/disconnect/block/...) is re-exposed `public` - anyone
 * may observe or wire up a private event.  The trigger-level surface
 * (operator()/trigger()/emit()/forwardFrom()) is re-exposed only
 * `protected` - FriendChain (below) grants exactly the listed friend types
 * access to that protected surface, through ordinary C++ friendship plus
 * public inheritance back up to the outermost GenericPrivateEvent type.
 *
 * forwardFrom() sits with the trigger-level surface, not the connection-
 * management one, even though it looks like a connect-style setup call:
 * wiring a source event to forward into this one hands whoever can fire
 * that source the power to fire this event too, so it carries the same
 * access restriction as calling operator() directly would.
 */
template< typename MutexType, typename... Args >
class BasicPrivateEventImpl : private GenericEvent< MutexType, Args... >
{
	// qualified deliberately: an unqualified `friend class EventInspector;`
	// here would not bind to kmac::stellyra::EventInspector (the one
	// forward-declared in stellyra_fwd.h) at all - a bare, "first-declaring"
	// friend name is injected into the *nearest enclosing namespace*
	// instead of being looked up through enclosing scopes, so inside this
	// `detail` namespace it would silently create a distinct, useless
	// detail::EventInspector friend declaration instead of granting
	// anything to the real one
	template< typename M, typename... A >
	friend class kmac::stellyra::EventInspector;

	// see the forwardTo()-to-a-private-target gap this avoids: GenericEvent
	// is not otherwise granted anything here, this is kept for parity with
	// any internal code that already relies on it
	friend GenericEvent< MutexType, Args... >;

public:
	// inherit constructors (no-owner and owner forms)
	using GenericEvent< MutexType, Args... >::GenericEvent;

	~BasicPrivateEventImpl() = default;

	// =========================================================================
	// expose the full public connect / disconnect / observation API since
	// private inheritance would normally hide everything; each using-
	// declaration explicitly opts a name back into the public interface
	// =========================================================================

	// --- object / method connections ---
	using GenericEvent< MutexType, Args... >::connect;

	// --- free-function connections (separate names; not covered above) ---
	using GenericEvent< MutexType, Args... >::connectFree;

	// --- lambda / functor connections (separate names; not covered above) ---
	using GenericEvent< MutexType, Args... >::connectLambda;

	// --- event forwarding (source-side only - see forwardFrom() below for
	// the destination-side, friend-gated counterpart) ---
	using GenericEvent< MutexType, Args... >::forwardTo;

	// --- disconnect ---
	using GenericEvent< MutexType, Args... >::disconnect;
	using GenericEvent< MutexType, Args... >::disconnectFree;
	using GenericEvent< MutexType, Args... >::disconnectAll;

	// --- block / unblock ---
	using GenericEvent< MutexType, Args... >::block;
	using GenericEvent< MutexType, Args... >::unblock;
	using GenericEvent< MutexType, Args... >::blockGuard;

	// --- owner introspection (needed by forwardTo()'s auto-tracker lookup
	// when *this* is used as someone else's forwardTo() target) ---
	using GenericEvent< MutexType, Args... >::owner;

protected:
	// maintain restricted visibility for the trigger-level surface so it is
	// inaccessible outside a listed friend type; private inheritance
	// already hides these, but the other using-declarations make the
	// intent explicit and guard against accidental exposure via ADL or
	// template argument deduction; `protected` (not `private`) so that
	// FriendChain's layered public inheritance can still see these names
	// to re-grant access to each listed friend (see FriendChain below)
	using GenericEvent< MutexType, Args... >::operator();
	using GenericEvent< MutexType, Args... >::trigger;
	using GenericEvent< MutexType, Args... >::emit;
	using GenericEvent< MutexType, Args... >::forwardFrom;
};

/**
 * @brief Recursive friend-granting chain: FriendChain<Signature, A, B, C>
 * publicly inherits FriendChain<Signature, B, C> and additionally declares
 * `friend A;`, so each layer grants exactly one friend and the chain
 * bottoms out, after the last friend is peeled off, at a layer with no
 * Friends... left at all.
 *
 * That zero-friends base case is the one below (a full specialization on
 * an empty trailing pack) - it is what actually inherits
 * BasicPrivateEventImpl.  It is never named directly with zero friends in
 * practice (BasicPrivateEvent always supplies at least one), but exists so
 * the recursion has a well-formed, unambiguous place to stop.
 *
 * Every intermediate FriendChain layer and BasicPrivateEventImpl itself are
 * deliberately left inheritable - the recursion depends on it - and live in
 * this `detail` namespace, not the public API; only the outermost
 * GenericPrivateEvent (both forms, below) is sealed `final`.  Without that
 * seal, an unrelated class could derive from a FriendChain/
 * BasicPrivateEventImpl layer directly and bypass the intended friend list;
 * sealing only the outer type is still sufficient against that, because
 * C++'s protected-member-access rule requires going through an expression
 * of the attacker's own derived type, so such a derivation could only ever
 * trigger a disconnected decoy instance of its own, never an existing,
 * already-wired-up event belonging to someone else.
 */
template< typename Signature, typename... Friends >
class FriendChain;

template< typename MutexType, typename... Args >
class FriendChain< PrivateEventSignature< MutexType, Args... > >
	: public BasicPrivateEventImpl< MutexType, Args... >
{
public:
	using BasicPrivateEventImpl< MutexType, Args... >::BasicPrivateEventImpl;
};

template< typename MutexType, typename... Args, typename First, typename... Rest >
class FriendChain< PrivateEventSignature< MutexType, Args... >, First, Rest... >
	: public FriendChain< PrivateEventSignature< MutexType, Args... >, Rest... >
{
	using Base = FriendChain< PrivateEventSignature< MutexType, Args... >, Rest... >;

	friend First;

public:
	using Base::Base;
};

} // namespace detail

/**
 * @brief Event with triggering access restricted to `FriendType`.
 *
 * This is the underlying class; BasicPrivateEvent (below) is the resolving
 * alias calling code should name instead when the function-style signature
 * form is wanted - for a plain Args... pack the two names are
 * interchangeable.
 *
 * @tparam FriendType the class (and only that class) allowed to call
 *   operator()(), trigger(), emit(), and forwardFrom()
 * @tparam MutexType synchronisation strategy
 * @tparam Args argument types forwarded to connected handlers
 */
template< typename FriendType, typename MutexType, typename... Args >
class GenericPrivateEvent final
	: public detail::FriendChain< detail::PrivateEventSignature< MutexType, Args... >, FriendType >
{
	using Base = detail::FriendChain< detail::PrivateEventSignature< MutexType, Args... >, FriendType >;

	// access to a private base several levels down a hierarchy must be
	// granted on the exact most-derived type named at the cast's call
	// site, not merely on the intermediate class that declares the
	// private link - EventInspector's cast lives in event_inspector.h and
	// names GenericPrivateEvent<FriendType, ...> directly, so the grant has
	// to be repeated here even though BasicPrivateEventImpl already
	// grants it for its own, closer callers
	template< typename M, typename... A >
	friend class EventInspector;

public:
	using Base::Base;

	~GenericPrivateEvent() = default;
};

/**
 * @brief Event with triggering access restricted to every type listed in
 * `EventFriendTypeList<Types...>`.
 *
 * Call-site shape for the common, single-friend case
 * (GenericPrivateEvent<ClassA, Mutex, Args...>) is unaffected by this
 * specialization; this is the opt-in form for when more than one class
 * needs trigger access.  See the file-level @code block above for an
 * example.
 *
 * @tparam Types every class allowed to call operator()(), trigger(),
 *   emit(), and forwardFrom()
 * @tparam MutexType synchronisation strategy
 * @tparam Args argument types forwarded to connected handlers
 */
template< typename... Types, typename MutexType, typename... Args >
class GenericPrivateEvent< EventFriendTypeList< Types... >, MutexType, Args... > final
	: public detail::FriendChain< detail::PrivateEventSignature< MutexType, Args... >, Types... >
{
	using Base = detail::FriendChain< detail::PrivateEventSignature< MutexType, Args... >, Types... >;

	// see the primary template's identical declaration above for why this
	// has to be repeated here rather than relying on BasicPrivateEventImpl's
	template< typename M, typename... A >
	friend class EventInspector;

public:
	using Base::Base;

	~GenericPrivateEvent() = default;
};

/**
 * @brief BasicPrivateEvent<FriendType, MutexType, Args...> - a resolving
 * alias over GenericPrivateEvent.
 *
 * Resolves the function-style signature form (see event_signature.h)
 * before naming GenericPrivateEvent, exactly the way BasicEvent resolves it
 * before naming GenericEvent - so BasicPrivateEvent<F, M, void(Args...)> and
 * BasicPrivateEvent<F, M, Args...> are always the exact same type.  For a
 * plain Args... pack the two names are interchangeable; use BasicPrivateEvent
 * when the function-style form is wanted, otherwise either option works.
 * `FriendType` may be a single class, or EventFriendTypeList<A, B, ...> for
 * the multi-friend form - unaffected by this alias layer either way.
 */
template< typename FriendType, typename MutexType, typename... Args >
using BasicPrivateEvent = typename detail::ResolvePrivateEventArgs< GenericPrivateEvent, FriendType, MutexType, Args... >::type;

// ============================================================================
// Convenience aliases
// ============================================================================

/**
 * @brief Private event using the RecursiveMutex strategy.
 *
 * `FriendType` may be a single class, or EventFriendTypeList<A, B, ...> for
 * the multi-friend form.
 *
 * Accepts either the plain argument pack
 * (RecursivePrivateEvent<MyClass, int>) or the equivalent function-style
 * signature (RecursivePrivateEvent<MyClass, void(int)>) - see
 * event_signature.h.
 *
 * @code
 * RecursivePrivateEvent< MyClass, int > valueChanged { this };
 * RecursivePrivateEvent< EventFriendTypeList< MyClass, Setup >, int > valueChanged2 { this };
 * @endcode
 */
template< typename FriendType, typename... Args >
using RecursivePrivateEvent = typename detail::ResolvePrivateEventArgs< GenericPrivateEvent, FriendType, platform::RecursiveMutex, Args... >::type;

/**
 * @brief Signal-terminology alias for RecursivePrivateEvent.
 */
template< typename FriendType, typename... Args >
using RecursivePrivateSignal = RecursivePrivateEvent< FriendType, Args... >;

/**
 * @brief Private event using SharedMutex (concurrent reads, exclusive writes).
 *
 * Use when the event will be triggered from many threads simultaneously and
 * all connected handlers are Direct.
 *
 * Accepts the function-style signature form too - see RecursivePrivateEvent
 * above.
 */
template< typename FriendType, typename... Args >
using SharedPrivateEvent = typename detail::ResolvePrivateEventArgs< GenericPrivateEvent, FriendType, platform::SharedMutex, Args... >::type;

/**
 * @brief Signal-terminology alias for SharedPrivateEvent.
 */
template< typename FriendType, typename... Args >
using SharedPrivateSignal = SharedPrivateEvent< FriendType, Args... >;

/**
 * @brief Private event using NullMutex (zero overhead, single-threaded only).
 *
 * Accepts the function-style signature form too - see RecursivePrivateEvent
 * above.
 */
template< typename FriendType, typename... Args >
using SingleThreadedPrivateEvent = typename detail::ResolvePrivateEventArgs< GenericPrivateEvent, FriendType, platform::NullMutex, Args... >::type;

/**
 * @brief Signal-terminology alias for SingleThreadedPrivateEvent.
 */
template< typename FriendType, typename... Args >
using SingleThreadedPrivateSignal = SingleThreadedPrivateEvent< FriendType, Args... >;

} // namespace stellyra
} // namespace kmac

#endif // KMAC_STELLYRA_PRIVATE_EVENT_H
