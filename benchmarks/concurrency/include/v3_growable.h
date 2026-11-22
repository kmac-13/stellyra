#pragma once
#ifndef BENCH_V3_GROWABLE_H
#define BENCH_V3_GROWABLE_H

/**
 * @file v3_growable.h
 * @brief Variant 3 - v2 (padded + dispersed-start) plus a growable
 * reader-slot pool, removing the fixed STELLYRA_MAX_CONCURRENT_TRIGGERS-style
 * compile-time cap.
 *
 * Key design constraint: a Guard holds a raw pointer straight into a slot
 * (exactly like the real EpochDomain), so growing can never reallocate or
 * free an array a live Guard might still point into. The fix used here:
 * growth only ever APPENDS a new, larger "generation" (capacity doubles,
 * same as Store's own growSlots() pattern) and every prior generation is
 * kept alive for the lifetime of the domain rather than reclaimed through
 * the epoch mechanism itself - reclaiming the reader-slot pool through its
 * own epoch is a self-referential problem (you'd need a second epoch domain
 * to protect the first one's array). Since growth events are expected to be
 * rare (bounded by log2(peak concurrent triggers)), retaining a handful of
 * superseded, now-unused arrays for the process lifetime is a fixed, small
 * cost - a deliberate, bounded trade against the previous hard failure mode
 * (assert-and-crash on overflow) and against unconditionally over-allocating
 * for a worst case that may never happen.
 *
 * enter() only ever touches the current generation and never takes a lock -
 * a pool run dry just grows and retries. retire()'s advance/reclaim check
 * does need to scan every generation's active readers (a reader that
 * claimed a slot in an older generation before a grow is just as "active"
 * as one in the newest), so it takes a mutex - consistent with the real
 * design, where retire() is already only ever called from the
 * mutex-protected connect/disconnect path, never from dispatch().
 */

#include "common.h"

#include <array>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace bench {
namespace v3 {

// Fixed rather than std::hardware_destructive_interference_size - avoids
// the -Winterference-size ABI-stability warning and keeps this constant
// (which only affects benchmark layout, not any real ABI) simple.
constexpr size_t kCacheLine = 64;

struct alignas( kCacheLine ) PaddedSlot
{
	std::atomic< uint64_t > value{ UINT64_MAX };
};

inline size_t threadScanStart( size_t bound )
{
	thread_local const size_t s_start = []( size_t b ) {
		const size_t h = std::hash< std::thread::id >{}( std::this_thread::get_id() );
		return b == 0 ? 0 : ( h % b );
	}( bound );
	return s_start;
}

class EpochDomainV3
{
public:
	static constexpr uint64_t FREE = UINT64_MAX;
	using Deleter = void (*)( void* );

private:
	struct RetiredItem { void* obj; Deleter deleter; };

	struct Generation
	{
		std::unique_ptr< PaddedSlot[] > slots;
		size_t capacity;
		std::atomic< size_t > highWaterMark{ 0 };

		explicit Generation( size_t cap ) : slots( new PaddedSlot[ cap ] ), capacity( cap )
		{
			for ( size_t i = 0; i < cap; ++i ) slots[ i ].value.store( FREE, std::memory_order_relaxed );
		}
	};

	static constexpr size_t kInitialCapacity = 8; // can start smaller now that it can grow

	std::atomic< Generation* > _current;
	std::vector< std::unique_ptr< Generation > > _allGenerations; // append-only, guarded by _mutex
	mutable std::mutex _mutex; // guards grow + retire-bin bookkeeping only; enter() never takes this
	std::atomic< uint64_t > _epoch{ 0 };
	std::vector< RetiredItem > _retireBins[ 3 ];

public:
	class Guard
	{
		friend class EpochDomainV3;
		std::atomic< uint64_t >* _slot = nullptr;
		explicit Guard( std::atomic< uint64_t >* slot ) : _slot( slot ) {}

	public:
		Guard() = default;
		Guard( Guard&& other ) noexcept : _slot( other._slot ) { other._slot = nullptr; }
		~Guard() { release(); }
		Guard( const Guard& ) = delete;
		Guard& operator=( const Guard& ) = delete;
		Guard& operator=( Guard&& other ) noexcept
		{
			if ( this != &other ) { release(); _slot = other._slot; other._slot = nullptr; }
			return *this;
		}

	private:
		void release()
		{
			if ( _slot != nullptr ) { _slot->store( EpochDomainV3::FREE, std::memory_order_release ); _slot = nullptr; }
		}
	};

	EpochDomainV3()
	{
		auto first = std::make_unique< Generation >( kInitialCapacity );
		_current.store( first.get(), std::memory_order_relaxed );
		_allGenerations.push_back( std::move( first ) );
	}

	~EpochDomainV3()
	{
		for ( auto& bin : _retireBins )
			for ( const RetiredItem& item : bin ) item.deleter( item.obj );
	}

	EpochDomainV3( const EpochDomainV3& ) = delete;
	EpochDomainV3& operator=( const EpochDomainV3& ) = delete;

	Guard enter()
	{
		Generation* gen = _current.load( std::memory_order_acquire );

		for ( ;; )
		{
			const uint64_t e = _epoch.load( std::memory_order_acquire );
			const size_t bound = gen->capacity;
			const size_t start = threadScanStart( bound );

			constexpr int MAX_SPIN_ROUNDS = 1000;
			for ( int round = 0; round < MAX_SPIN_ROUNDS; ++round )
			{
				for ( size_t k = 0; k < bound; ++k )
				{
					const size_t i = ( start + k ) % bound;
					uint64_t expected = FREE;
					if ( gen->slots[ i ].value.compare_exchange_strong( expected, e,
						std::memory_order_acq_rel, std::memory_order_relaxed ) )
					{
						size_t hwm = gen->highWaterMark.load( std::memory_order_relaxed );
						while ( hwm < i + 1
							&& ! gen->highWaterMark.compare_exchange_weak( hwm, i + 1,
								std::memory_order_relaxed, std::memory_order_relaxed ) )
						{
						}
						return Guard( &gen->slots[ i ].value );
					}
				}
				std::this_thread::yield();
			}

			// this generation is full - grow (or pick up a grow another
			// thread already published) and retry against the new one
			gen = growFrom( gen );
		}
	}

	template< typename T >
	void retire( T* obj )
	{
		if ( obj == nullptr ) return;
		retireErased( obj, []( void* p ) { delete static_cast< T* >( p ); } );
	}

	void retireErased( void* obj, Deleter deleter )
	{
		if ( obj == nullptr ) return;
		std::lock_guard< std::mutex > lock( _mutex );
		const uint64_t e = _epoch.load( std::memory_order_relaxed );
		_retireBins[ e % 3 ].push_back( RetiredItem{ obj, deleter } );
		tryAdvanceAndReclaimLocked( e );
	}

	/// Number of generations ever allocated - exposed only so the benchmark
	/// can report how many grow events actually happened.
	size_t generationCount() const
	{
		std::lock_guard< std::mutex > lock( _mutex );
		return _allGenerations.size();
	}

private:
	Generation* growFrom( Generation* full )
	{
		std::lock_guard< std::mutex > lock( _mutex );
		Generation* cur = _current.load( std::memory_order_relaxed );
		if ( cur != full )
		{
			return cur; // someone else already grew past `full`
		}

		auto next = std::make_unique< Generation >( cur->capacity * 2 );
		Generation* nextPtr = next.get();
		_allGenerations.push_back( std::move( next ) );
		_current.store( nextPtr, std::memory_order_release );
		return nextPtr;
	}

	// caller holds _mutex
	void tryAdvanceAndReclaimLocked( uint64_t e )
	{
		for ( const auto& genPtr : _allGenerations )
		{
			const size_t bound = genPtr->highWaterMark.load( std::memory_order_relaxed );
			for ( size_t i = 0; i < bound; ++i )
			{
				const uint64_t v = genPtr->slots[ i ].value.load( std::memory_order_acquire );
				if ( v != FREE && v != e )
				{
					return; // some reader, in any generation, is still behind
				}
			}
		}

		const uint64_t newEpoch = e + 1;
		_epoch.store( newEpoch, std::memory_order_release );

		auto& reclaimBin = _retireBins[ ( newEpoch + 1 ) % 3 ];
		for ( const RetiredItem& item : reclaimBin ) item.deleter( item.obj );
		reclaimBin.clear();
	}
};

class Store
{
	struct Entry
	{
		HandlerFn callable;
		uint16_t generation;
		explicit Entry( HandlerFn c, uint16_t gen ) : callable( std::move( c ) ), generation( gen ) {}
	};

	using SlotArray = std::atomic< Entry* >;
	static constexpr size_t kCapacity = 64; // connection-slot growth is a separate, already-solved concern

	std::array< SlotArray, kCapacity > _slots;
	std::array< uint16_t, kCapacity > _nextGeneration{};
	std::atomic< uint32_t > _usedCount{ 0 };
	EpochDomainV3 _epoch;
	std::mutex _mutex;

public:
	explicit Store( size_t initialHandlers )
	{
		for ( auto& s : _slots ) s.store( nullptr, std::memory_order_relaxed );
		for ( size_t i = 0; i < initialHandlers; ++i ) connect();
	}

	~Store()
	{
		const uint32_t used = _usedCount.load( std::memory_order_relaxed );
		for ( uint32_t i = 0; i < used; ++i ) delete _slots[ i ].load( std::memory_order_relaxed );
	}

	uint32_t connect()
	{
		std::lock_guard< std::mutex > lock( _mutex );
		const uint32_t used = _usedCount.load( std::memory_order_relaxed );
		uint32_t index = used;
		for ( uint32_t i = 0; i < used; ++i )
			if ( _slots[ i ].load( std::memory_order_relaxed ) == nullptr ) { index = i; break; }
		assert( index < kCapacity );
		if ( index == used ) _usedCount.store( used + 1, std::memory_order_release );
		Entry* entry = new Entry( HandlerFn( &handlerBody ), _nextGeneration[ index ] );
		_slots[ index ].store( entry, std::memory_order_release );
		return index;
	}

	void disconnect( uint32_t index )
	{
		std::lock_guard< std::mutex > lock( _mutex );
		Entry* old = _slots[ index ].exchange( nullptr, std::memory_order_release );
		if ( old != nullptr ) { ++_nextGeneration[ index ]; _epoch.retire( old ); }
	}

	void churnOnce()
	{
		std::lock_guard< std::mutex > lock( _mutex );
		const uint32_t used = _usedCount.load( std::memory_order_relaxed );
		for ( uint32_t i = 0; i < used; ++i )
		{
			Entry* old = _slots[ i ].load( std::memory_order_relaxed );
			if ( old != nullptr )
			{
				_slots[ i ].store( nullptr, std::memory_order_release );
				++_nextGeneration[ i ];
				_epoch.retire( old );
				Entry* fresh = new Entry( HandlerFn( &handlerBody ), _nextGeneration[ i ] );
				_slots[ i ].store( fresh, std::memory_order_release );
				return;
			}
		}
	}

	void dispatch()
	{
		auto guard = _epoch.enter();
		const uint32_t count = _usedCount.load( std::memory_order_acquire );
		for ( uint32_t i = 0; i < count; ++i )
		{
			Entry* entry = _slots[ i ].load( std::memory_order_acquire );
			if ( entry != nullptr ) entry->callable();
		}
	}

	size_t epochGenerationCount() const { return _epoch.generationCount(); }
};

} // namespace v3
} // namespace bench

#endif // BENCH_V3_GROWABLE_H
