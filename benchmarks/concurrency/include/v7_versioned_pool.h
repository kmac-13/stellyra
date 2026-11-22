#pragma once
#ifndef BENCH_V7_VERSIONED_POOL_H
#define BENCH_V7_VERSIONED_POOL_H

/**
 * @file v7_versioned_pool.h
 * @brief Variant 7 - copy-on-write Snapshot dispatch backed by a small fixed
 * pool of reusable slots, each with its own refcount, addressed through a
 * version-tagged index instead of a raw pointer.
 *
 * Storage: `kPoolSize` slots are allocated once, for the lifetime of the
 * `Store`, and never freed or reallocated individually. Each slot holds one
 * `Snapshot` (a flat `std::vector<HandlerFn>`) and its own
 * `std::atomic<uint32_t> refCount`. `_current` is a single
 * `std::atomic<uint64_t>` packing a slot index and a version number that
 * increases by one on every swap.
 *
 * Reading (`dispatch()`): load `_current` to get a slot index and version,
 * increment that slot's own refcount, then re-check `_current` still holds
 * the exact same index-and-version pair. A match means the increment landed
 * before anyone could recycle the slot, so it's safe to run its handlers. A
 * mismatch means the slot was recycled in the gap between the load and the
 * increment, so the increment is undone and the whole thing is retried
 * against whatever is current now. Because the version never repeats, two
 * different generations can never be mistaken for each other even if they
 * later reuse the same slot index.
 *
 * Writing (`churnOnce()`): find a slot that is neither the current one nor
 * referenced by anyone (refcount zero), clone the current snapshot into it,
 * publish it by storing the new index-and-version pair into `_current`, then
 * drop the outgoing slot's own reference (the one that represented `_current`
 * itself pointing at it). If every slot is either current or still
 * referenced, this waits (a brief spin) for one to free up rather than
 * allocating a new one - `stallCount()` reports how often that actually
 * happens.
 *
 * Because no slot is ever individually freed and reallocated, and a slot is
 * only ever reused once its own refcount reads zero, this needs no separate
 * retirement queue and no reclamation pass of any kind.
 */

#include "common.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

namespace bench {
namespace v7 {

class Store
{
	struct Snapshot
	{
		std::vector< HandlerFn > entries;
	};

	struct Slot
	{
		Snapshot snapshot;
		std::atomic< uint32_t > refCount{ 0 };
	};

	static constexpr size_t kPoolSize = 2;
	static constexpr int kIndexBits = 3; // room for up to 8 slots
	static constexpr uint64_t kIndexMask = ( uint64_t( 1 ) << kIndexBits ) - 1;

	std::atomic< uint64_t > _current;
	std::array< Slot, kPoolSize > _pool;
	std::mutex _mutex; // serializes churnOnce and the slot search it does
	std::atomic< uint64_t > _stallCount{ 0 };

	static uint64_t pack( size_t index, uint64_t version )
	{
		return ( version << kIndexBits ) | ( index & kIndexMask );
	}

	static size_t unpackIndex( uint64_t tagged )
	{
		return static_cast< size_t >( tagged & kIndexMask );
	}

	static uint64_t unpackVersion( uint64_t tagged )
	{
		return tagged >> kIndexBits;
	}

public:
	explicit Store( size_t initialHandlers )
	{
		Slot& first = _pool[ 0 ];
		first.snapshot.entries.reserve( initialHandlers );
		for ( size_t i = 0; i < initialHandlers; ++i )
		{
			first.snapshot.entries.emplace_back( &handlerBody );
		}
		first.refCount.store( 1, std::memory_order_relaxed ); // _current's own reference
		_current.store( pack( 0, 0 ), std::memory_order_relaxed );
	}

	void dispatch()
	{
		for ( ;; )
		{
			const uint64_t tagged = _current.load( std::memory_order_acquire );
			Slot& slot = _pool[ unpackIndex( tagged ) ];

			slot.refCount.fetch_add( 1, std::memory_order_acq_rel );

			if ( _current.load( std::memory_order_acquire ) == tagged )
			{
				for ( auto& handler : slot.snapshot.entries )
				{
					handler();
				}
				slot.refCount.fetch_sub( 1, std::memory_order_acq_rel );
				return;
			}

			// slot was recycled between the load and the increment above -
			// this increment isn't protecting the generation we meant to
			// read, so undo it and retry against whatever is current now
			slot.refCount.fetch_sub( 1, std::memory_order_acq_rel );
		}
	}

	/// Clones the current snapshot into a free slot, replaces one handler
	/// with a fresh one, and publishes it - exercises the same
	/// connect+disconnect cost as a real churn cycle without permanently
	/// growing the handler count.
	void churnOnce()
	{
		std::lock_guard< std::mutex > lock( _mutex );

		const uint64_t oldTagged = _current.load( std::memory_order_relaxed );
		const size_t oldIndex = unpackIndex( oldTagged );
		const uint64_t oldVersion = unpackVersion( oldTagged );

		const size_t newIndex = acquireFreeSlotLocked( oldIndex );
		Slot& newSlot = _pool[ newIndex ];

		newSlot.snapshot.entries = _pool[ oldIndex ].snapshot.entries;
		if ( ! newSlot.snapshot.entries.empty() )
		{
			newSlot.snapshot.entries[ 0 ] = HandlerFn( &handlerBody );
		}
		newSlot.refCount.store( 1, std::memory_order_release ); // this becomes _current's reference

		_current.store( pack( newIndex, oldVersion + 1 ), std::memory_order_release );

		_pool[ oldIndex ].refCount.fetch_sub( 1, std::memory_order_acq_rel ); // drop the old pointer's reference
	}

	/// Number of times churnOnce() had to wait for a pool slot to free up
	/// rather than finding one immediately - exposed so the benchmark can
	/// report whether this pool size ever actually runs dry at the
	/// harness's churn rate.
	uint64_t stallCount() const
	{
		return _stallCount.load( std::memory_order_relaxed );
	}

private:
	// caller holds _mutex
	size_t acquireFreeSlotLocked( size_t excludeIndex )
	{
		bool countedStall = false;
		for ( ;; )
		{
			for ( size_t i = 0; i < kPoolSize; ++i )
			{
				if ( i == excludeIndex ) continue;
				if ( _pool[ i ].refCount.load( std::memory_order_acquire ) == 0 )
				{
					return i;
				}
			}
			if ( ! countedStall )
			{
				_stallCount.fetch_add( 1, std::memory_order_relaxed );
				countedStall = true;
			}
			std::this_thread::yield();
		}
	}
};

} // namespace v7
} // namespace bench

#endif // BENCH_V7_VERSIONED_POOL_H
