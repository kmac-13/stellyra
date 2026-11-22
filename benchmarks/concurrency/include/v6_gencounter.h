#pragma once
#ifndef BENCH_V6_GENCOUNTER_H
#define BENCH_V6_GENCOUNTER_H

/**
 * @file v6_gencounter.h
 * @brief Variant 6 - copy-on-write Snapshot dispatch, reclaimed with a single
 * packed two-generation counter instead of a reader-slot table.
 *
 * Storage: `Store` holds one `std::atomic<Snapshot*>`, where `Snapshot` owns
 * a flat `std::vector<HandlerFn>`. `dispatch()` reads the pointer once and
 * iterates the vector directly - contiguous, no pointer-chasing between
 * entries.
 *
 * Reclamation: one `std::atomic<uint64_t>` (`_inFlight`) holds two 32-bit
 * counters packed side by side, one per "generation". A second flag
 * (`_currentGeneration`) says which of the two is currently being counted
 * into by new dispatches. `dispatch()` reads that flag once, adds its unit
 * to `_inFlight`, then loads the `Snapshot` pointer and runs it; on exit it
 * subtracts the same unit. Because the counter is bumped *before* the
 * pointer is loaded, any `Snapshot` swapped out after the bump is guaranteed
 * to still be covered by that dispatch's presence in the counter.
 *
 * When a `Snapshot` is swapped out, it's tagged with whichever generation is
 * current at that moment. If that generation's half of `_inFlight` is
 * already zero, the old `Snapshot` is freed immediately - the common case,
 * with no queuing at all. Otherwise it's queued, and whichever dispatch call
 * later drains that generation's half to exactly zero flips the current
 * generation and frees everything tagged with the drained one, using a
 * non-blocking `try_lock` so a dispatch call never waits on this. If the
 * lock isn't free at that moment, the queued `Snapshot` simply waits for the
 * next time its generation drains to zero - later dispatch traffic keeps
 * producing more chances for that to happen.
 *
 * `_inFlight` and `_currentGeneration` never grow: however many threads
 * dispatch concurrently, this remains two fixed atomics on the `Store`
 * itself, not one entry per thread and not a fixed-capacity table with a
 * scan-to-claim step.
 */

#include "common.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <utility>
#include <vector>

namespace bench {
namespace v6 {

class Store
{
	struct Snapshot
	{
		std::vector< HandlerFn > entries;
	};

	static constexpr int kGenerationBits = 32;
	static constexpr uint64_t kGenerationAUnit = uint64_t( 1 );
	static constexpr uint64_t kGenerationBUnit = uint64_t( 1 ) << kGenerationBits;
	static constexpr uint64_t kGenerationAMask = ( uint64_t( 1 ) << kGenerationBits ) - 1;
	static constexpr uint64_t kGenerationBMask = kGenerationAMask << kGenerationBits;

	std::atomic< Snapshot* > _current;
	std::atomic< uint64_t > _inFlight{ 0 };
	std::atomic< bool > _currentGeneration{ true }; // true = generation A is the one being counted into
	std::vector< std::pair< Snapshot*, bool > > _pendingFree; // { snapshot, generation it was retired under }
	std::mutex _mutex; // guards churnOnce, _pendingFree, and the generation flip

public:
	explicit Store( size_t initialHandlers )
	{
		auto* snap = new Snapshot();
		snap->entries.reserve( initialHandlers );
		for ( size_t i = 0; i < initialHandlers; ++i )
		{
			snap->entries.emplace_back( &handlerBody );
		}
		_current.store( snap, std::memory_order_relaxed );
	}

	~Store()
	{
		delete _current.load( std::memory_order_relaxed );
		for ( auto& pending : _pendingFree )
		{
			delete pending.first;
		}
	}

	void dispatch()
	{
		const bool generation = _currentGeneration.load( std::memory_order_acquire );
		const uint64_t unit = generation ? kGenerationAUnit : kGenerationBUnit;

		_inFlight.fetch_add( unit, std::memory_order_acq_rel );
		Snapshot* snap = _current.load( std::memory_order_acquire );
		for ( auto& handler : snap->entries )
		{
			handler();
		}
		const uint64_t after = _inFlight.fetch_sub( unit, std::memory_order_acq_rel ) - unit;

		tryReclaim( generation, after );
	}

	/// Clones the current snapshot, replaces one handler with a fresh one,
	/// and swaps it in - exercises the same connect+disconnect cost as a
	/// real churn cycle without permanently growing the handler count.
	void churnOnce()
	{
		std::lock_guard< std::mutex > lock( _mutex );

		Snapshot* oldSnap = _current.load( std::memory_order_relaxed );
		auto* newSnap = new Snapshot( *oldSnap );
		if ( ! newSnap->entries.empty() )
		{
			newSnap->entries[ 0 ] = HandlerFn( &handlerBody );
		}

		_current.store( newSnap, std::memory_order_release );
		retireLocked( oldSnap );
	}

	/// Number of retired snapshots still waiting on their generation to
	/// drain - exposed only so the benchmark can report how deep this queue
	/// actually gets under load.
	size_t pendingFreeCount()
	{
		std::lock_guard< std::mutex > lock( _mutex );
		return _pendingFree.size();
	}

private:
	// caller holds _mutex
	void retireLocked( Snapshot* oldSnap )
	{
		const bool generation = _currentGeneration.load( std::memory_order_acquire );
		const uint64_t mask = generation ? kGenerationAMask : kGenerationBMask;

		if ( ( _inFlight.load( std::memory_order_acquire ) & mask ) == 0 )
		{
			delete oldSnap;
			return;
		}
		_pendingFree.emplace_back( oldSnap, generation );
	}

	void tryReclaim( bool generation, uint64_t inFlightAfterExit )
	{
		const uint64_t mask = generation ? kGenerationAMask : kGenerationBMask;
		if ( ( inFlightAfterExit & mask ) != 0 )
		{
			return; // this generation hasn't drained to zero yet
		}

		std::unique_lock< std::mutex > lock( _mutex, std::try_to_lock );
		if ( ! lock.owns_lock() )
		{
			return; // another dispatch or churnOnce call will get the next chance
		}

		if ( _currentGeneration.load( std::memory_order_relaxed ) == generation )
		{
			_currentGeneration.store( ! generation, std::memory_order_release );
		}

		for ( size_t i = 0; i < _pendingFree.size(); )
		{
			if ( _pendingFree[ i ].second == generation )
			{
				delete _pendingFree[ i ].first;
				_pendingFree[ i ] = _pendingFree.back();
				_pendingFree.pop_back();
			}
			else
			{
				++i;
			}
		}
	}
};

} // namespace v6
} // namespace bench

#endif // BENCH_V6_GENCOUNTER_H
