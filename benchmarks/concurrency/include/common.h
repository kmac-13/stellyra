#pragma once
#ifndef BENCH_COMMON_H
#define BENCH_COMMON_H

/**
 * @file common.h
 * @brief Shared pieces for all reclamation-strategy variants: the Entry type
 * each variant stores, and the handler body every variant invokes. Kept
 * identical across variants so the benchmark measures the reclamation
 * strategy, not differences in handler cost.
 *
 * Deliberately has zero dependency on Stellyra's Trackable/EventLoop/
 * EventStorage/Callable machinery - only the slot-storage + reclamation
 * problem is being isolated here.
 */

#include <atomic>
#include <cstdint>
#include <functional>

namespace bench {

/// Sink every handler touches, so the compiler can't optimize the call away
/// and so every variant pays the same "real work" cost. One counter per
/// thread: each dispatching thread only ever touches its own, so this adds
/// real, unelidable work without adding any contention of its own between
/// threads that a shared counter would.
inline std::atomic< uint64_t >& sink()
{
	thread_local std::atomic< uint64_t > s_sink{ 0 };
	return s_sink;
}

/// The handler body used by every variant: cheap, but a real atomic RMW so
/// it can't be elided and so its cost is representative of a trivial
/// slot/signal handler.
inline void handlerBody()
{
	sink().fetch_add( 1, std::memory_order_relaxed );
}

using HandlerFn = std::function< void() >;

/// Number of handlers connected to the store at steady state - fixed across
/// variants and across the run, matching how a typical Stellyra event has a
/// small, fairly static set of connections that gets dispatched to
/// frequently, with only occasional connect/disconnect churn.
constexpr size_t kHandlerCount = 16;

/// Every Nth trigger call issued by the designated churn thread instead does
/// a disconnect+connect instead of a dispatch, to exercise each variant's
/// reclamation path under concurrent read load rather than only measuring
/// steady-state dispatch.
constexpr int kChurnPeriod = 20;

} // namespace bench

#endif // BENCH_COMMON_H
