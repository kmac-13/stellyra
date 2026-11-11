#pragma once
#ifndef KMAC_STELLYRA_PRIVATE_EVENT_H
#define KMAC_STELLYRA_PRIVATE_EVENT_H

/**
 * @file private_event.h
 * @brief PrivateEvent - Event with compile-time restricted triggering.
 *
 * PrivateEvent<FriendType, Args...> inherits privately from
 * BasicEvent<MutexType, Args...> (defaulting to Event<Args...>) so that
 * only `FriendType` can call the triggering methods (operator(), trigger(),
 * emit(), forwardFrom()), while all connection and disconnect methods remain
 * publicly accessible.
 *
 * This enforces the principle that only the class that owns an event should
 * be able to fire it, while any code can observe it by connecting handlers.
 *
 * @code
 * class Button : public Trackable
 * {
 * public:
 *     // anyone can connect, but only Button can trigger
 *     PEvent< Button, int, int > clicked { this };
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
 * PrivateEvent<EventFriendTypeList<Types...>, Args...> is the opt-in
 * multi-friend form, for the case where more than one class legitimately
 * needs trigger access - e.g. a decoupling `Setup`-style class that wires
 * two otherwise-unrelated owning classes' events together via
 * forwardFrom() needs friend access to the destination event it is wiring
 * into, alongside that event's natural owner:
 *
 * @code
 * class Button : public Trackable
 * {
 * public:
 *     PrivateEvent< EventFriendTypeList< Button, Setup >, int, int > clicked { this };
 * };
 * @endcode
 *
 * @warning Private inheritance means PrivateEvent cannot be used where a
 *   BasicEvent& is expected.
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
 *
 * @see PEvent for the short aliases.
 */

#include "event.h"

namespace kmac {
namespace stellyra {

/**
 * @brief Tag type naming more than one FriendType for BasicPrivateEvent.
 *
 * BasicPrivateEvent<EventFriendTypeList<A, B, ...>, MutexType, Args...> is
 * the multi-friend opt-in form; BasicPrivateEvent<A, MutexType, Args...>
 * (a single, bare FriendType) remains the common, single-friend form and is
 * unaffected by this type's existence.  EventFriendTypeList is never
 * instantiated - it exists purely to be named as BasicPrivateEvent's first
 * template argument.
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
 * Privately inherits BasicEvent so that, absent an explicit using-
 * declaration, nothing is accidentally exposed.  The connection-management
 * surface (connect/disconnect/block/...) is re-exposed `public` - anyone
 * may observe or wire up a private event.  The trigger-level surface
 * (operator()/trigger()/emit()/forwardFrom()) is re-exposed only
 * `protected` - FriendChain (below) grants exactly the listed friend types
 * access to that protected surface, through ordinary C++ friendship plus
 * public inheritance back up to the outermost BasicPrivateEvent type.
 *
 * forwardFrom() sits with the trigger-level surface, not the connection-
 * management one, even though it looks like a connect-style setup call:
 * wiring a source event to forward into this one hands whoever can fire
 * that source the power to fire this event too, so it carries the same
 * access restriction as calling operator() directly would.
 */
template< typename MutexType, typename... Args >
class BasicPrivateEventImpl : private BasicEvent< MutexType, Args... >
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

	// see the forwardTo()-to-a-private-target gap this avoids: BasicEvent
	// is not otherwise granted anything here, this is kept for parity with
	// any internal code that already relies on it
	friend BasicEvent< MutexType, Args... >;

public:
	// inherit constructors (no-owner and owner forms)
	using BasicEvent< MutexType, Args... >::BasicEvent;

	~BasicPrivateEventImpl() = default;

	// =========================================================================
	// expose the full public connect / disconnect / observation API since
	// private inheritance would normally hide everything; each using-
	// declaration explicitly opts a name back into the public interface
	// =========================================================================

	// --- object / method connections ---
	using BasicEvent< MutexType, Args... >::connect;

	// --- free-function connections (separate names; not covered above) ---
	using BasicEvent< MutexType, Args... >::connectFree;

	// --- lambda / functor connections (separate names; not covered above) ---
	using BasicEvent< MutexType, Args... >::connectLambda;

	// --- event forwarding (source-side only - see forwardFrom() below for
	// the destination-side, friend-gated counterpart) ---
	using BasicEvent< MutexType, Args... >::forwardTo;

	// --- disconnect ---
	using BasicEvent< MutexType, Args... >::disconnect;
	using BasicEvent< MutexType, Args... >::disconnectFree;
	using BasicEvent< MutexType, Args... >::disconnectAll;

	// --- block / unblock ---
	using BasicEvent< MutexType, Args... >::block;
	using BasicEvent< MutexType, Args... >::unblock;
	using BasicEvent< MutexType, Args... >::blockGuard;

	// --- owner introspection (needed by forwardTo()'s auto-tracker lookup
	// when *this* is used as someone else's forwardTo() target) ---
	using BasicEvent< MutexType, Args... >::owner;

protected:
	// maintain restricted visibility for the trigger-level surface so it is
	// inaccessible outside a listed friend type; private inheritance
	// already hides these, but the other using-declarations make the
	// intent explicit and guard against accidental exposure via ADL or
	// template argument deduction; `protected` (not `private`) so that
	// FriendChain's layered public inheritance can still see these names
	// to re-grant access to each listed friend (see FriendChain below)
	using BasicEvent< MutexType, Args... >::operator();
	using BasicEvent< MutexType, Args... >::trigger;
	using BasicEvent< MutexType, Args... >::emit;
	using BasicEvent< MutexType, Args... >::forwardFrom;
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
 * BasicPrivateEvent (both forms, below) is sealed `final`.  Without that
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
 * @tparam FriendType the class (and only that class) allowed to call
 *   operator()(), trigger(), emit(), and forwardFrom()
 * @tparam MutexType synchronisation strategy
 * @tparam Args argument types forwarded to connected handlers
 */
template< typename FriendType, typename MutexType, typename... Args >
class BasicPrivateEvent final
	: public detail::FriendChain< detail::PrivateEventSignature< MutexType, Args... >, FriendType >
{
	using Base = detail::FriendChain< detail::PrivateEventSignature< MutexType, Args... >, FriendType >;

	// access to a private base several levels down a hierarchy must be
	// granted on the exact most-derived type named at the cast's call
	// site, not merely on the intermediate class that declares the
	// private link - EventInspector's cast lives in event_inspector.h and
	// names BasicPrivateEvent<FriendType, ...> directly, so the grant has
	// to be repeated here even though BasicPrivateEventImpl already
	// grants it for its own, closer callers
	template< typename M, typename... A >
	friend class EventInspector;

public:
	using Base::Base;

	~BasicPrivateEvent() = default;
};

/**
 * @brief Event with triggering access restricted to every type listed in
 * `EventFriendTypeList<Types...>`.
 *
 * Call-site shape for the common, single-friend case
 * (BasicPrivateEvent<ClassA, Mutex, Args...>) is unaffected by this
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
class BasicPrivateEvent< EventFriendTypeList< Types... >, MutexType, Args... > final
	: public detail::FriendChain< detail::PrivateEventSignature< MutexType, Args... >, Types... >
{
	using Base = detail::FriendChain< detail::PrivateEventSignature< MutexType, Args... >, Types... >;

	// see the primary template's identical declaration above for why this
	// has to be repeated here rather than relying on BasicPrivateEventImpl's
	template< typename M, typename... A >
	friend class EventInspector;

public:
	using Base::Base;

	~BasicPrivateEvent() = default;
};

// ============================================================================
// Convenience aliases
// ============================================================================

/**
 * @brief PrivateEvent using the default RecursiveMutex strategy.
 *
 * The most common form: use this unless you need SharedEvent or
 * SingleThreadedEvent semantics with trigger-access restriction.
 *
 * `FriendType` may be a single class, or EventFriendTypeList<A, B, ...> for
 * the multi-friend form.
 *
 * @code
 * PrivateEvent< MyClass, int > valueChanged { this };
 * PrivateEvent< EventFriendTypeList< MyClass, Setup >, int > valueChanged2 { this };
 * @endcode
 */
template< typename FriendType, typename... Args >
using PrivateEvent = BasicPrivateEvent< FriendType, platform::RecursiveMutex, Args... >;

/**
 * @brief Shorter alias for PrivateEvent.
 */
template< typename FriendType, typename... Args >
using PEvent = PrivateEvent< FriendType, Args... >;

/**
 * @brief Signal-terminology alias for PrivateEvent.
 */
template< typename FriendType, typename... Args >
using PrivateSignal = PrivateEvent< FriendType, Args... >;

/**
 * @brief Signal-terminology short alias for PrivateEvent.
 */
template< typename FriendType, typename... Args >
using PSignal = PrivateSignal< FriendType, Args... >;

/**
 * @brief PrivateEvent using SharedMutex (concurrent reads, exclusive writes).
 *
 * Use when the event will be triggered from many threads simultaneously and
 * all connected handlers are Direct.
 */
template< typename FriendType, typename... Args >
using SharedPrivateEvent = BasicPrivateEvent< FriendType, platform::SharedMutex, Args... >;

/**
 * @brief Shorter alias of SharedPrivateEvent.
 */
template< typename FriendType, typename... Args >
using SharedPEvent = SharedPrivateEvent< FriendType, Args... >;

/**
 * @brief PrivateEvent using NullMutex (zero overhead, single-threaded only).
 */
template< typename FriendType, typename... Args >
using SingleThreadedPrivateEvent = BasicPrivateEvent< FriendType, platform::NullMutex, Args... >;

/**
 * @brief Shorter alias of SingleThreadedPrivateEvent.
 */
template< typename FriendType, typename... Args >
using SingleThreadedPEvent = SingleThreadedPrivateEvent< FriendType, Args... >;

} // namespace stellyra
} // namespace kmac

#endif // KMAC_STELLYRA_PRIVATE_EVENT_H
