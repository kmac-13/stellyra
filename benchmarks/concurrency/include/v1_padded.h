#pragma once
#ifndef BENCH_V1_PADDED_H
#define BENCH_V1_PADDED_H

/**
 * @file v1_padded.h
 * @brief Variant 1 - identical to v0, except each EpochDomain reader slot is
 * padded to its own cache line, eliminating false sharing between threads
 * claiming/releasing adjacent slots. Everything else (scan-from-zero,
 * fixed capacity, connect/disconnect/dispatch shape) is unchanged from v0,
 * so any throughput delta between v0 and v1 isolates the false-sharing cost.
 */

#include "common.h"

#include <array>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <mutex>
#include <new>
#include <thread>
#include <vector>

namespace bench {
namespace v1 {

#ifndef BENCH_MAX_CONCURRENT_TRIGGERS
#define BENCH_MAX_CONCURRENT_TRIGGERS 32
#endif

// Fixed rather than std::hardware_destructive_interference_size - avoids
// the -Winterference-size ABI-stability warning and keeps this constant
// (which only affects benchmark layout, not any real ABI) simple.
constexpr size_t kCacheLine = 64;

/// One reader slot, alone on its own cache line - the only change from v0.
struct alignas( kCacheLine ) PaddedSlot
{
	std::atomic< uint64_t > value{ UINT64_MAX };
};

class EpochDomainV1
{
public:
	static constexpr uint64_t FREE = UINT64_MAX;
	using Deleter = void (*)( void* );

private:
	struct RetiredItem
	{
		void* obj;
		Deleter deleter;
	};

	mutable std::array< PaddedSlot, BENCH_MAX_CONCURRENT_TRIGGERS > _readerSlots;
	mutable std::atomic< size_t > _highWaterMark{ 0 };
	std::atomic< uint64_t > _epoch{ 0 };
	std::vector< RetiredItem > _retireBins[ 3 ];

public:
	class Guard
	{
		friend class EpochDomainV1;

	private:
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
			if ( _slot != nullptr ) { _slot->store( EpochDomainV1::FREE, std::memory_order_release ); _slot = nullptr; }
		}
	};

	EpochDomainV1()
	{
		for ( auto& slot : _readerSlots ) slot.value.store( FREE, std::memory_order_relaxed );
	}

	~EpochDomainV1()
	{
		for ( auto& bin : _retireBins )
			for ( const RetiredItem& item : bin ) item.deleter( item.obj );
	}

	EpochDomainV1( const EpochDomainV1& ) = delete;
	EpochDomainV1& operator=( const EpochDomainV1& ) = delete;

	Guard enter() const
	{
		const uint64_t e = _epoch.load( std::memory_order_acquire );
		const size_t bound = _readerSlots.size();

		constexpr int MAX_SPIN_ROUNDS = 1000;
		for ( int round = 0; round < MAX_SPIN_ROUNDS; ++round )
		{
			for ( size_t i = 0; i < bound; ++i ) // still scan-from-zero, deliberately - isolates padding only
			{
				uint64_t expected = FREE;
				if ( _readerSlots[ i ].value.compare_exchange_strong( expected, e,
					std::memory_order_acq_rel, std::memory_order_relaxed ) )
				{
					return claim( i );
				}
			}
			std::this_thread::yield();
		}

		assert( false && "EpochDomainV1: reader slot pool exhausted" );
		return Guard();
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
		const uint64_t e = _epoch.load( std::memory_order_relaxed );
		_retireBins[ e % 3 ].push_back( RetiredItem{ obj, deleter } );
		tryAdvanceAndReclaim( e );
	}

private:
	Guard claim( size_t index ) const
	{
		size_t hwm = _highWaterMark.load( std::memory_order_relaxed );
		while ( hwm < index + 1
			&& ! _highWaterMark.compare_exchange_weak( hwm, index + 1,
				std::memory_order_relaxed, std::memory_order_relaxed ) )
		{
		}
		return Guard( &_readerSlots[ index ].value );
	}

	void tryAdvanceAndReclaim( uint64_t e )
	{
		const size_t bound = _highWaterMark.load( std::memory_order_relaxed );
		for ( size_t i = 0; i < bound; ++i )
		{
			const uint64_t v = _readerSlots[ i ].value.load( std::memory_order_acquire );
			if ( v != FREE && v != e ) return;
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
	static constexpr size_t kCapacity = 64;

	std::array< SlotArray, kCapacity > _slots;
	std::array< uint16_t, kCapacity > _nextGeneration{};
	std::atomic< uint32_t > _usedCount{ 0 };
	EpochDomainV1 _epoch;
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
};

} // namespace v1
} // namespace bench

#endif // BENCH_V1_PADDED_H
