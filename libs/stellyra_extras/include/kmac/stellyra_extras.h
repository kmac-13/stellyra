#pragma once
#ifndef KMAC_STELLYRA_EXTRAS_H
#define KMAC_STELLYRA_EXTRAS_H

/**
 * @file stellyra_extras.h
 * @brief Stellyra Extras - Extended functionality for the Stellyra event library
 *
 * Includes everything from <kmac/stellyra.h> plus extended features:
 * RAII connection helpers, combining events, inspection, and recording.
 *
 * Namespace: kmac::stellyra
 *
 * @section when_to_use When to Use
 *
 * Include <kmac/stellyra.h> for normal event-driven programming.
 * Include <kmac/stellyra_extras.h> when you need any of the following:
 *
 * - **ScopedConnection** / **ConnectionGroup** - RAII connection lifetime management
 * - **BasicCombiningEvent** - events that collect and aggregate handler return values
 * - **BasicConcurrentEvent** - lock-free-dispatch event for high-performance
 *   concurrent connect/disconnect/trigger from multiple threads
 * - **EventInspector** - runtime connection graph inspection and Graphviz DOT export
 * - **BasicRecordableEvent** - event emission recording, replay, and performance statistics
 * - **FastRecursiveMutex** / **SpinRecursiveMutex** - optional mutex types for
 *   BasicEvent<MutexType, ...>, alongside the core library's RecursiveEvent/SharedEvent/
 *   SingleThreadedEvent
 *
 * @section components Component Headers
 *
 * | Header               | Purpose                                             |
 * |----------------------|-----------------------------------------------------|
 * | connection_guard.h   | ConnectionGuard (thread-safe Connection access)     |
 * | connection_group.h   | ConnectionGroup (multi-connection RAII)             |
 * | generic_combining_event.h | GenericCombiningEvent<Combiner, ReturnType,     |
 * |                           | Args...> - the backing class                   |
 * | combining_event_types.h | BasicCombiningEvent<Combiner, ReturnType, Args...> - |
 * |                         | resolving alias over GenericCombiningEvent, also     |
 * |                         | accepting the function-style form                    |
 * |                         | BasicCombiningEvent<Combiner, ReturnType(Args...)>   |
 * |                         | via its own ResolveCombiningEventArgs mechanism (not |
 * |                         | shared with others due to return type requirement)   |
 * | combining_signal_types.h | BasicCombiningSignal<Combiner, ReturnType, Args...> - |
 * |                          | Signal alias matching BasicCombiningEvent; not       |
 * |                          | included by this umbrella - include directly if      |
 * |                          | needed                                                |
 * | combiners.h          | Built-in combiner functors (10+ provided)           |
 * | generic_concurrent_event.h | GenericConcurrentEvent<MutexType, Args...> -      |
 * |                            | the backing class, lock-free dispatch             |
 * | concurrent_event_types.h | Aliases, including mutex-based ones, over         |
 * |                          | GenericConcurrentEvent; BasicConcurrentEvent is   |
 * |                          | the function-style-aware alias over it            |
 * | concurrent_signal_types.h | Signal aliases matching concurrent_event_types.h; |
 * |                           | not included by this umbrella - include directly  |
 * |                           | if needed                                         |
 * | epoch_domain.h       | EpochDomain - lock-free reclamation backing         |
 * |                      | ConcurrentEvent's dispatch path; not included by    |
 * |                      | this umbrella directly - pulled in transitively via |
 * |                      | basic_concurrent_event_impl.h                       |
 * | event_inspector.h    | EventInspector<Args...>                             |
 * | generic_recordable_event.h | GenericRecordableEvent<MutexType, Args...> -      |
 * |                            | the backing class; also pulls in event_recorder.h |
 * | recordable_event_types.h | Aliases, including mutex-based ones, over         |
 * |                          | GenericRecordableEvent; BasicRecordableEvent is   |
 * |                          | the function-style-aware alias over it            |
 * | recordable_signal_types.h | Signal aliases matching recordable_event_types.h; |
 * |                           | not included by this umbrella - include directly  |
 * |                           | if needed                                         |
 * | event_recorder.h     | EventRecorder<Args...>                              |
 * |                      | (included transitively via generic_recordable_event.h) |
 * | property.h           | Property<T>, ReadOnlyProperty<Owner, T>,            |
 * |                      | ComputedProperty<Owner, Fn, T>, ConstProperty<T>    |
 * | mutex_types.h        | FastRecursiveMutex, SpinRecursiveMutex - optional   |
 * |                      | mutex types for BasicEvent<MutexType, ...>, not     |
 * |                      | used by any core-library Event alias, not held to   |
 * |                      | the same validation bar (see the header's own       |
 * |                      | doc comment)                                        |
 */

// core library
#include <kmac/stellyra.h>

// RAII connection helpers
#include <kmac/stellyra/connection_guard.h>
#include <kmac/stellyra/connection_group.h>

// combining events
#include <kmac/stellyra/combining_event_types.h>
#include <kmac/stellyra/combiners.h>

// concurrent event
#include <kmac/stellyra/concurrent_event_types.h>

// inspection
#include <kmac/stellyra/event_inspector.h>

// recording
#include <kmac/stellyra/recordable_event_types.h>

// properties
#include <kmac/stellyra/property.h>

// optional mutex types
#include <kmac/stellyra/mutex_types.h>

#endif // KMAC_STELLYRA_EXTRAS_H
