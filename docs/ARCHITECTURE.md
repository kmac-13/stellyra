# Stellyra Architecture

This document describes how Stellyra is built today.  It is not a design history or a migration guide - it describes the current source, file by file, so that anyone changing the library (or reviewing it, including for SOUP purposes) can find where a given behavior lives.

Stellyra is header-only, C++17, and split into two targets:

- **`stellyra`** (`libs/stellyra/include/kmac/stellyra/`) - the core library: events, connections, dispatch, lifetime tracking, event loops
- **`stellyra_extras`** (`libs/stellyra_extras/include/kmac/stellyra/`) - optional, separately-included features built on top of the core: a lock-free concurrent event, return-value-combining events, reactive properties, recording/replay, introspection, and a couple of alternative mutex types; nothing in `stellyra_extras` is required to use the core library, and nothing in the core library depends on `stellyra_extras`

## Events: `GenericEvent<MutexType, Args...>` and the `BasicEvent` alias

The underlying class is `GenericEvent<MutexType, Args...>` (`generic_event.h`) - the single implementation shared by the `BasicEvent`, `RecursiveEvent`, `SharedEvent`, and `SingleThreadedEvent` aliases.  It has no per-strategy counterpart: there is no `SharedGenericEvent` or `SingleThreadedGenericEvent` - one `GenericEvent` backs `BasicEvent`'s function-style form plus all three mutex strategies.  Calling code wanting the function-style signature form of `GenericEvent` will need to use the `BasicEvent` alias, which is a resolving alias that additionally accepts that form, with `BasicEvent<M, void(Args...)>` and `BasicEvent<M, Args...>` being the exact same type, `std::is_same_v` and all.  Using `GenericEvent` directly is equally fine when function-style syntax isn't needed; the two are interchangeable for a plain `Args...` pack - `BasicEvent<M, Args...>` and `GenericEvent<M, Args...>` are, again, the same type.  `BasicEvent` is, in turn, aliased for the three mutex strategies the library ships, as:

- `RecursiveEvent<Args...>` - `platform::RecursiveMutex` (the default)
- `SharedEvent<Args...>` - `platform::SharedMutex` (concurrent dispatch, exclusive connect/disconnect)
- `SingleThreadedEvent<Args...>` - `platform::NullMutex` (zero locking overhead; caller guarantees single-threaded use)

RecursiveEvent/SharedEvent/SingleThreadedEvent resolve the function-style form and name `GenericEvent` directly (not through `BasicEvent`), for the same reason as below: deduction and template-template-parameter matching want the underlying class template, not an alias defined via a metafunction.

The mutex strategy is a template parameter, so it is chosen per event instance, not via a single library-wide compile-time flag - a binary can mix `RecursiveEvent`, `SharedEvent`, and `SingleThreadedEvent` members freely.

`GenericEvent` itself is thin: it inherits connection management from `EventStorage` (below) and adds `operator()` / `trigger()` / `emit()` (all identical) in order to dispatch to connected handlers.

Everywhere in the library that needs to specify the underlying class rather than let calling code's chosen variant resolve on its own (i.e. friend grants, forward declarations (`stellyra_fwd.h`), inheritance, `EventInspector`'s constructor) references `GenericEvent` directly.  `BasicEvent` is an alias template with no incomplete form, so it can't be forward-declared or used as a template-template-parameter/friend-template target the way a class template can.

`BasicPrivateEvent` (`private_event_types.h`) is an alias naming `GenericPrivateEvent` directly, which wraps `GenericEvent` with private inheritance plus `friend FriendType`, so only `FriendType` can trigger it while every connect/disconnect/block method stays public.  The `RecordableEvent` family (in `stellyra_extras`, see below) is the same idea with recording and statistics bolted on.  `GenericPrivateEvent`/`BasicPrivateEvent`, `GenericRecordableEvent`/`BasicRecordableEvent`, and `GenericConcurrentEvent`/`BasicConcurrentEvent` each have the identical underlying-class/alias split that `GenericEvent`/`BasicEvent` has: an underlying class template (the `Generic*` name) plus a resolving alias (the short `Basic*` name) that additionally accepts the function-style signature form, with `std::is_same_v` true between the two variants of a plain pack.  As with `GenericEvent`, each `Generic*` class here is the one implementation shared by all of that family's mutex-strategy aliases - `GenericPrivateEvent` backs `RecursivePrivateEvent`, `SharedPrivateEvent`, and `SingleThreadedPrivateEvent` alike (likewise `GenericRecordableEvent` and `GenericConcurrentEvent`); none of these has, or needs, a per-strategy `Generic` class of its own.

### Dispatch deferral (sender side)

`GenericEvent::operator()` checks whether the event's owner (a `Trackable*`) has an `EventLoop`.  If it does, and the calling thread is not that loop's designated thread (via `EventLoop::shouldDispatchDirectlyOnThread()`), the *entire* dispatch - not just individual connections - is posted to that loop and runs whenever that loop is next drained.  Otherwise, dispatch happens synchronously on the calling thread.  This check is cheap when there is no loop at all (the common case): one pointer read, no capture, no lock.

## Connections and storage

### `EventStorage<ImplT, MutexType, Args...>` (`event_storage.h`)

This is the base class every event type inherits for its connect/disconnect/block surface.  It is parameterized on `ImplT` (the actual storage/dispatch engine - `BasicEventImpl` for `GenericEvent`, `BasicConcurrentEventImpl` for `GenericConcurrentEvent`, etc.) so the same connect-side API works across multiple backing implementations.

Connections are grouped into three named buckets, each with NTTP (compile-time) and runtime-pointer overloads:

- **`connect`** - object/method connections (member function pointer, bound to a receiver object)
- **`connectFree`** - free-function connections (including non-capturing lambdas)
- **`connectLambda`** - functor/lambda connections, either a raw functor (wrapped internally) or a pre-built `Callable`

Each bucket accepts an optional `Tracked<T>{ receiver, tracker }` for a receiver that does not itself inherit `Trackable` (see below), and an optional `ConnParams` for priority/predicate/single-shot (see below).  `forwardTo()` connects one event directly to another with matching signature.  `disconnect()` / `disconnectFree()` remove by receiver, by method/function identity, or via a `Trackable&`.  `block()` / `unblock()` / `blockGuard()` suspend dispatch for the whole event.

### Stable-slot storage: `HandlerEntry<Args...>` (`handler_entry.h`)

Each connection occupies one slot in a `std::vector<HandlerEntry<Args...>>` owned by `BasicEventImpl` - the impl behind `RecursiveEvent`, `SharedEvent`, and `SingleThreadedEvent`.  `BasicConcurrentEventImpl` (backing `ConcurrentEvent`) uses a different, atomically-swapped `Entry`/`SlotArray` design instead; see the `GenericConcurrentEvent` / `ConcurrentEvent` section below.  A disconnected slot becomes an inactive hole, tracked by a free list, and is reused by a later `connect()` rather than the vector being compacted.  `HandlerEntry` is 48 bytes on 64-bit and holds the handler `Callable`, a `Trackable` migration tag, `EventFlags` (see below), declared+resolved `ConnectionTypes`, a priority, a generation counter, and the connection's `EventLoop*` (for Deferred dispatch).

Because slots never move, a `Connection` handle (next) can refer to one directly by index - there is no separate heap-allocated "connection object" distinct from the `HandlerEntry` itself (beyond whatever the `Callable` it holds needs - see below).

### `Connection` (`connection.h`)

A `Connection` is a 24-byte value: `WeakPtr<EventImplBase>` + a `uint32_t` slot index + a `uint16_t` generation. `isConnected()` resolves the weak pointer and compares the stored generation against the slot's current one - each time a slot is freed, its generation is bumped, so a `Connection` to a disconnected-and-reused slot reads as not connected rather than addressing the wrong handler.  `GenData` (`gen_data.h`) is the same (index, generation) pair used internally for bulk operations.

`ScopedConnection` wraps a `Connection` and disconnects on destruction (move-only).  `BlockGuard` is a type-erased RAII block guard (blocks on construction, unblocks on destruction) - deliberately not templated, so it can be returned from `blockGuard()` regardless of the event's `Args...`.

### `Callable<ReturnType(Args...)>` (`callable.h`)

The type-erased callable stored in every `HandlerEntry` (and used as the predicate type too).  A flat `(void* obj, stub function pointer, optional destructor function pointer)` triple - no virtual dispatch, and no heap allocation except when wrapping an owning functor (`create(F&&)`, typically a capturing lambda), which is heap-allocated via `new` and freed in `Callable`'s destructor or on overwrite. Free functions and pointer-to-member-functions are non-owning and allocation-free.  `createPartial<Method>(obj)` / `createPartial<Func>()` support connecting a handler that takes *fewer* arguments than the event provides - the trailing arguments are dropped at the call site, at zero runtime cost (`MethodArity`/`FunctionArity` resolve this at compile time in `event_detail.h`).  This is how partial-arity method/free-function connections work; lambdas are expected to just omit trailing parameters themselves.

### `ConnParams<MutexType, Args...>` (`conn_params.h`)

The single bundle for a connection's optional settings: priority (`.prio()`), a conditional predicate (constructor or `.when()`, with a `PredicateContext` of `Sender` or `Receiver`), and single-shot (`.once()`).  This approach helps avoid `connect` overload explosion, incluing having separate `connectIf()`/`connectOnce()`/`connectWithPriority()` entry points - any combination is one `ConnParams` value passed to the normal `connect()`/`connectFree()`/`connectLambda()` overloads.

Priority is a simple integer, with higher-priority connections being evaluated before lower-priority connections.  Note that Deferred connections with higher priority are still deferred and potentially executed later than Direct connections with lower priority.

`.once()` is a `static_assert` failure on `SharedEvent` (a single-shot handler disconnecting itself while a shared lock is held would deadlock); the assertion only fires when `.once()` is actually called, not merely when a `ConnParams` of that `MutexType` is instantiated.

`PredicateContext::Sender` evaluates the predicate at emission time, before a Deferred connection's task is even posted - always synchronously inside `trigger()`.  `PredicateContext::Receiver` (the default) means the predicate is evaluated immediately before the handler runs - synchronously inside `trigger()` for a Direct connection, or on the receiver's loop at drain time for a Deferred one.  

### Connection type resolution (`connection_type.h`, `event_detail.h`)

A connection declares `ConnectionType::Direct`, `Deferred`, or `Auto` (default) at connect time.  This resolves to a `ResolvedConnectionType` of `Direct`, `Deferred`, or `None` (topology doesn't support either - the connection stays live but is silently skipped) via `detail::resolveConnectionType()`:

- `Direct` always resolves `Direct`
- `Deferred` resolves `Deferred` if the receiver has an `EventLoop`, else `None`
- `Auto` resolves `Direct` if sender and receiver loops match (including both loops being null), `Deferred` if the receiver has a non-null loop and the sender has a loop different from receiver (including a null one), else `None`

Resolution is cached in the `HandlerEntry` at connect time and re-run whenever either side's loop changes via `Trackable::setEventLoop()`.

## Dispatch (`basic_event_impl.h`)

`BasicEventImpl<MutexType, Args...>` is the storage/dispatch engine behind `GenericEvent`.  It owns the `handlers` vector, a free list (`nextFree`), an optional priority-ordering index (`order`, built and maintained only while
at least one non-default-priority connection exists - the common all-default-priority case pays nothing for this), and whole-event block depth.

`dispatch(Args...)` has two paths:

- **Fast path**, used when every connection is Direct with no predicates (`nonDirectCount == 0 && predicateCount == 0`): a single lock, a straight walk over active slots, no branching per connection
- **Slow path**, used otherwise: per-handler predicate evaluation (in the right context), resolved-type dispatch (Direct now, or post to the receiver's loop for Deferred), and single-shot cleanup immediately after that handler returns — a same-thread call into `disconnectHandler()`, which takes the exclusive lock - this is exactly why `.once()` is rejected on `SharedEvent` (see `ConnParams` above): dispatch there holds only a shared lock, so that immediate disconnect call would deadlock against itself

Argument capture for Deferred posting is lazy: the `shared_ptr<tuple<...>>` used to carry arguments across the loop boundary is only allocated once the first Deferred connection is actually encountered during a given dispatch (`ensureCapture()`), not unconditionally.

**Reentrancy**: a handler may legitimately connect to, disconnect from, or re-trigger the *same* event it is being called from (this is why `RecursiveEvent`'s default mutex is recursive).  A `triggerDepth` counter plus a `hadReentrantActivity` flag defer slot-reuse and order-rebuild bookkeeping while a reentrant call is in progress, reconciling once the outermost dispatch call unwinds (`reconcileAfterTrigger()`).  This is **not** supported on `SharedEvent` - reentering while holding a shared lock can deadlock.

## Lifetime tracking: `Trackable` (`trackable.h`)

`Trackable` can be used through inheritance or composition/aggregation.

Through inheritance, a class derives from `Trackable` to get (a) automatic disconnection of every connection where it is the receiver, when it is destroyed, and (b) an `EventLoop` association (`setEventLoop()`) that both re-resolves its own events' sender-side topology and every tracked receiver-side connection.

For a receiver that cannot or should not inherit `Trackable`, `Tracked<T>` (in `event_storage.h`) is a two-field aggregate - `{ T& receiver, Trackable& tracker }` - passed to any `connect`/`connectFree`/`connectLambda` overload in place of a bare receiver reference, so a separate, explicitly-owned `Trackable` (aliased as `Anchor`) does the tracking instead.

Every `Trackable` has one `migrationTag`, shared by every connection where it is the receiver, so when it moves to a different `EventLoop`, all of its pending Deferred tasks migrate together in a single call (`EventLoop::migratePendingTo()`), regardless of which event or connection produced each one.

`Trackable`'s `disconnectAll` - whether run implicitly via `~Trackable()` or explicitly - only takes effect once destruction actually reaches it.  For an inheriting receiver, that's after the derived object's own destructor body and members are already destroyed, since base subobjects always destruct last; a dispatch already in flight when destruction begins can observe the receiver mid-destruction (see `KNOWN_ISSUES.md`).  `EagerDisconnector`, also in `trackable.h`, closes this by inverting the relationship - the wrapper inherits from the receiver's implementation class rather than the other way around, so its destructor (and the `disconnectAll` it runs) is most-derived and executes first.

## `EventLoop` and `AutoDrainThread` (`event_loop.h`, `auto_drain_thread.h`)

`EventLoop` is a manually-drained task queue (`Callable<void()>` tasks).  `post()` enqueues; `drain()` processes everything queued, including tasks posted during the drain itself.  `shouldDispatchDirectlyOnThread()` is the gate `GenericEvent::operator()` (and friends) use to decide whether to dispatch now or defer to the loop - true if the calling thread is the loop's registered drain thread, or is, right now, the thread actively draining it.  `AutoDrainThread` is an optional helper that spawns a dedicated background thread which sleeps until `post()` wakes it, drains, and sleeps again - no polling.

Note that `EventLoop` supports a single listener to be notified when a task has been posted to the loop.  This is how `AutoDrainThread` gets notified in order to drain the loop, and is how using code can implement auto-draining without the use of `AutoDrainThread`.

## `platform.h`: thread-safety abstraction

All mutex and atomic types used throughout the library go through `kmac::stellyra::platform` aliases (`Mutex`, `RecursiveMutex`, `SharedMutex`, `Atomic<T>`, `SharedPtr<T>`, `WeakPtr<T>`, `ThreadId`).  When `STELLYRA_THREAD_SAFE=0` (opt-in, for bare-metal/single-core targets), these resolve to zero-overhead no-op stubs and `<mutex>`/`<shared_mutex>`/ `<atomic>` are not even included; `platform::Atomic<T>` falls back to a `volatile`-based implementation (single-core safe only, with a documented hook - `STELLYRA_CUSTOM_ATOMIC` - for supplying a real multi-core atomic on such a target).  `NullMutex` (used by `SingleThreadedEvent`) is always a no-op regardless of this flag.

## `stellyra_extras`

### `GenericConcurrentEvent` / `BasicConcurrentEvent` / `ConcurrentEvent` (`generic_concurrent_event.h`, `concurrent_event_types.h`)

A second, lock-free primary event type.  The underlying class is `GenericConcurrentEvent<MutexType, Args...>`; `BasicConcurrentEvent<MutexType, Args...>` is a resolving alias over it (same underlying-class/alias split as `GenericEvent`/`BasicEvent`), and `ConcurrentEvent`/`SingleThreadedConcurrentEvent` (`concurrent_event_types.h`) resolve the function-style form and name `GenericConcurrentEvent` directly, for the same template-template-parameter reason as `RecursiveEvent`/`SharedEvent`/`SingleThreadedEvent`.  Each slot is a `std::atomic<Entry*>`, reclaimed through `EpochDomain` (below) rather than protected by a lock - `dispatch()` never takes the event's `mutex` at all; that mutex guards only connect/disconnect/priority-rebuild.  This gives `GenericConcurrentEvent` genuinely concurrent, non-serialized dispatch across threads, at the cost of being only *eventually consistent* (not linearizable) against a concurrent `updateSenderLoop()`/`updateHandlerLoop()` call - a documented, deliberate trade for lock-free dispatch.  Supports the same Direct/Deferred/Auto connection model, predicates, priority, and single-shot as `GenericEvent`; does not support reentrant dispatch the way `RecursiveEvent`'s recursive mutex does (there is no mutex held during dispatch to reenter in the first place, but the slot array can still be mutated concurrently with an in-flight dispatch by design - see the file's own documentation for the consistency model).

### `EpochDomain` (`epoch_domain.h`)

A small epoch-based-reclamation (EBR) utility backing `GenericConcurrentEvent`: readers announce "active" for the duration of a whole `dispatch()` call (not per-object), and a writer's `retire()` defers freeing until no reader could still be inside a critical section that started before the retired object was removed.  Reader-slot pool size is fixed at compile time via `STELLYRA_MAX_CONCURRENT_TRIGGERS` (default 32); exceeding it fails loudly via `stellyra_assert_always` (never compiled out by NDEBUG) rather than corrupting anything.

### `GenericCombiningEvent` / `BasicCombiningEvent<Combiner, ReturnType, Args...>` (`generic_combining_event.h`, `combining_event_types.h`, `combiners.h`)

Fires all connected handlers synchronously and reduces their return values through a `Combiner` functor (`Combiners::LogicalAnd`, `LogicalOr`, `Sum`, `Product`, `Mean`/`Average`, `Maximum`, `Minimum`, `CountTrue`, `First`, `FirstNonDefault`, `Last`, `LastNonDefault`).  Always Direct - there is no Deferred variant, since the combined result must be available synchronously - and its mutex is non-recursive, so reentrant connect/disconnect/trigger from inside a handler is not supported.

The underlying class is `GenericCombiningEvent<Combiner, ReturnType, Args...>`; `BasicCombiningEvent<Combiner, ReturnType, Args...>` is a resolving alias over it, the same underlying-class/alias split as the other four event families, additionally accepting the function-style form `BasicCombiningEvent<Combiner, ReturnType(Args...)>`.  This is through a *separate* mechanism, `detail::ResolveCombiningEventArgs`, confined to `combining_event_types.h` rather than shared with `event_signature.h`'s `ResolveEventArgs`/`ResolvePrivateEventArgs`: those two require the return type be `void` and discard it, where here the return type is the whole point - it's the per-handler value the `Combiner` reduces - so this resolver captures `R` and forwards it into the `ReturnType` slot instead.  `BasicCombiningSignal` (the signal/emit-terminology alias) targets `BasicCombiningEvent` directly, same as the higher-level aliases in the other four families.

### Properties (`property.h`)

Property types:

- `Property<T>`: read/write by anyone
- `ReadOnlyProperty<Owner, T>`: read externally, write only by `Owner`
- `ComputedProperty<Owner, T>`: derived via an `Owner` member function
- `ComputedPropertyFn<Owner, T>`: derived via a capturing lambda, using one `Callable<T()>`
- `ConstProperty<T>`: immutable; `changed` exists for API uniformity but never fires

All expose the same read surface (implicit conversion, `get()`, a `changed` event) so calling code doesn't need to know which variant it's reading.

### Introspection: `EventInspector<MutexType, Args...>` (`event_inspector.h`, `inspection_info.h`)

Read-only, lock-respecting inspection of a live event's connection table - per-connection snapshots (`ConnectionInfo`), aggregate stats (`EventInfo`), and human-readable or Graphviz DOT dumps (`toDot()`/`toDotString()`, renderable with `dot -Tpng` or any DOT viewer).  Works on `GenericPrivateEvent` too (declared a friend specifically so it can inspect past the private trigger methods).  Per-connection receiver address/type is not available - only "method" vs. "function/lambda" (`EventFlags::hasOwner()`), since `Callable` does not expose its bound object pointer generically.

### Recording and replay: `EventRecorder<Args...>` / `GenericRecordableEvent` / `BasicRecordableEvent` (`event_recorder.h`, `generic_recordable_event.h`, `recordable_event_types.h`)

`EventRecorder` captures a bounded or unbounded sequence of emissions (arguments + timestamps) with start/pause/resume/stop, replay (immediate, original-timing, or speed-scaled), range replay, timing statistics, and CSV export.  The underlying class is `GenericRecordableEvent<MutexType, Args...>`; `BasicRecordableEvent<MutexType, Args...>` is a resolving alias over it, and `RecursiveRecordableEvent`/`SharedRecordableEvent`/`SingleThreadedRecordableEvent` (`recordable_event_types.h`) resolve the function-style form and name `GenericRecordableEvent` directly.  `GenericRecordableEvent` wires a recorder (and optional cumulative emission-time statistics) around the same dispatch path as `GenericEvent`, wiring the recorder's replay target back through its own `operator()`.  Both are opt-in and add memory/per-emission overhead relative to plain `RecursiveEvent` - intended for development/debugging/testing/simulation, not as the default production event type.

### `ConnectionGroup` / `ConnectionGuard` (`connection_group.h`, `connection_guard.h`)

`ConnectionGroup` owns a `std::vector<Connection>` so a set of connections can be handled as a unit (e.g. when swapping a model) without destroying the owning object; move-only, `+=` to add.  `ConnectionGuard` wraps a single `Connection` behind a mutex for cases where the handle itself (not just the underlying connection) is shared and mutated across threads, serializing `isConnected()`, `disconnect()`, and `block()`/`unblock()`/`isBlocked()` — the same surface `Connection` itself has, made safe for concurrent access to one shared handle.

### Optional mutex types (`mutex_types.h`)

`FastRecursiveMutex` and `SpinRecursiveMutex`: experimental, not used by any shipped event alias, provided for benchmarking/comparison against `std::recursive_mutex` on a given target.  Both use the same atomic-owner + depth-counter reentrancy design; `SpinRecursiveMutex` spins (bounded-then-yield) instead of blocking on an inner `std::mutex`.

## `stellyra_qt`

Qt integration support is the one non-header-only module in the repo:

### Umbrella Include

`stellyra_qt.h` is the umbrella include - it wraps `stellyra.h`'s own include in the same `push_macro`/`undef`/`pop_macro("emit")` guard `event_loop_bridge.h` itself uses, protecting against Qt's `emit` macro (which is defined unless `QT_NO_KEYWORDS` is set) breaking the token `emit` wherever `BasicEvent` (or related) declares its `emit()` method.  The other types and utility functions defined in this section - `EventLoopBridge`, `TrackableBridge`, `qt_connect.h` - live in `kmac::stellyra::qt`, separate from the rest of the Stellyra library both in namespace and in build (opt-in `STELLYRA_BUILD_QT`, linked against Qt5 or Qt6's `Core` module) since it's the only piece that takes a Qt dependency.  

### `EventLoopBridge` (`event_loop_bridge.h`, `event_loop_bridge.cpp`)

`EventLoopBridge` is a `QObject` (needs moc), owning an `EventLoop` and draining it on whatever thread the bridge has Qt affinity with rather than a dedicated background thread the way `AutoDrainThread` does.  This lets Stellyra receivers run on the Qt GUI/main thread without a second drain thread competing with Qt's own event loop.  The owned `EventLoop`'s drain-thread registration tracks the bridge's own Qt thread affinity automatically - registered at construction (the constructing thread), and re-registered on every later `moveToThread()` call via an `event()` override reacting to `QEvent::ThreadChange` - so ordinary `moveToThread()` usage, exactly as for any other `QObject`, is enough; no caller action is required.

`updateDrainThread()` is the method that registers the current thread as the `EventLoop`'s drain-thread.  For most cases, it is unnecessary to call directly since the bridge handles thread tracking automatically.  However, there is a small window between when the bridge's affinity thread is set (via `moveToThread()`) and when it actually gets applied to `EventLoop`.  For cases where this window needs to be closed immediately, `updateDrainThread()` can be called manually at the top of the thread's own run()/init code as this method updates the drain-thread immediately to the current executing thread.  This could be useful for cases where the bridge instance is initialized outside its eventual affinity thread, with `moveToThread()` being called somewhere followed by an explicit `updateDrainThread()` inside the affinity thread.  Otherwise, drain-thread registration is queued to the Qt thread via `QMetaObject::invokeMethod(..., Qt::QueuedConnection)`.  Calling `updateDrainThread()` from the wrong thread, though, is a hard abort (`stellyra_assert_always`, every build) rather than a silent wrong registration - a wrong registration here has no well-defined way to continue, and would otherwise surface later as a much harder-to-diagnose dispatch-on-the-wrong-thread bug.

`drainEventLoop()` is available to drain the `EventLoop`, but, again, it is not required to call explicitly in most cases as `EventLoopBridge` automatically drains the `EventLoop` whenever a task is posted to it.  Draining is queued to the Qt thread via `QMetaObject::invokeMethod(..., Qt::QueuedConnection)`.  For cases where the `EventLoop` needs to be drained outside of the Qt thread context - such as on application shutdown when the `EventLoopBridge` survives the destruction of the affinity thread - `drainEventLoop()` can be called explicitly.

### `TrackableBridge` (`trackable_bridge.h`)

`TrackableBridge` is a `QObject` that *composes* (not inherits) a `Trackable`, exposed via `trackable()`.  It's the shared lifetime anchor: pass it as the `context` to a Qt `connect()` wiring a signal into a Stellyra event, and pass `trackable()` as the tracker half of a `Tracked<T>{ receiver, tracker }` wiring a Stellyra event to a non-`Trackable` receiver (e.g. a method defined in a `QObject`-inherited class).  Destroying `TrackableBridge` tears down both halves - Qt's own auto-disconnect on context destruction, and the composed `Trackable`'s `disconnectAll()` as part of the same teardown, member before base.  `Trackable` is composed rather than inherited to avoid issues with multiple inheritance.  `TrackableBridge` can be subclassed, but exposes those subclasses to the same base-class-destroyed-before-its-dispatch-finishes hazard `EagerDisconnector` (`trackable.h`) exists to support closing in user code (and one ofthe reasons `QObject::deleteLater()` exists).

### Utility Functions (`qt_connect.h`)

`connectSignalToEvent(sender, signal, context, event, type = Qt::AutoConnection)` builds the forwarding lambda for a Qt-signal-to-Stellyra-event connection, forwarding `type` unchanged.  Works with any non-private event type; does *not* work with a private event (`BasicPrivateEvent` family) regardless of who calls it - access control in C++ is decided by where the code that writes `.trigger()` is implemented, not by who calls that code, and the lambda this function builds is written inside a free function that is never a declared friend of any private event.  For cases where forwarding to a private event is needed, a friend/owner class can supply a `[this](auto&&... args){ myPrivateEvent.trigger(args...); }` lambda scoped to the friend/owner and pass that directly to a plain `QObject::connect()`.

`connectEventToTrackableBridge(event, bridge, method, type = ConnectionType::Auto)` is the Stellyra-event-to-bridge-method direction: `Tracked<T>{ bridge, bridge.trackable() }` (receiver and tracker being the same object) rather than the plain non-`Tracked` overload, which would otherwise silently create an untracked connection here instead of failing to compile.  `T` is deduced from `bridge` and `method` together (`static_assert`-checked as a `TrackableBridge` or a type deriving from one), so it works directly for a method declared on a `TrackableBridge` subclass, not only the handful `TrackableBridge` itself declares.

## Other core headers

- `event_flags.h` - `EventFlags` (active/predicate/single-shot/owner/blocked bits) and `ConnectionTypes` (declared + resolved type), both packed into one byte each, underlying `HandlerEntry`
- `event_impl_base.h` - `EventImplBase`, the type-erased base every event's impl object inherits, giving `Connection`/`Trackable` a common interface independent of `Args...`, plus a globally unique per-instance `id`
- `generic_private_event.h` / `private_event_types.h` - see above
- `stellyra_fwd.h` - forward declarations only, for headers that need type names without full definitions
- `config.h` - logging tags and the optional Nova logging integration (`STELLYRA_USE_NOVA`), plus two independently-enabled compile flags for connection logging, each off by default with zero overhead when off:
  - `STELLYRA_LOG_CONNECTION_ISSUES` for misconfiguration warnings (mismatched-loop Direct connections, no-loop Deferred connections, untracked object/method connections) and `STELLYRA_LOG_CONNECTION_LIFECYCLE` for a general trace of every connect/disconnect
  - wired up at `EventStorage::connectImpl()` (connect time) and `updateSenderLoop()`/`updateHandlerLoop()`/`disconnectSlotLocked()`/`preLockDisconnectAll()` in both `BasicEventImpl` and `BasicConcurrentEventImpl` (migration and disconnect time)
- `version.h` - compile-time version macros/constants
- `stellyra_assert.h` - `stellyra_assert_always`, a hard assertion that is never compiled out by `NDEBUG` (unlike plain `assert`), for conditions indicating genuine API misuse with no well-defined way to continue
- `event_signature.h` - resolves the function-style signature syntax shared by every `Args...`-based alias family: `BasicEvent`, `BasicConcurrentEvent`, `BasicRecordableEvent`, and `BasicPrivateEvent` (plus all mutex-specific variants), all of which are restricted to `void` returns (a `static_assert` is generated for any non-`void` return values); for legitimate non-`void` return value needs, use `BasicCombiningEvent` (`combining_event_types.h`), which has its own custom resolver
