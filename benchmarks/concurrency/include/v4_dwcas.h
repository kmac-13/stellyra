#pragma once
#ifndef BENCH_V4_DWCAS_H
#define BENCH_V4_DWCAS_H

/**
 * @file v4_dwcas.h
 * @brief Variant 4 - COW Snapshot behind a single {pointer, externalCount}
 * word, updated with one double-width CAS, using the standard "split
 * reference counting" protocol (as in the classic lock-free stack pop from
 * Williams, "C++ Concurrency in Action") rather than a naive separate
 * pointer+refcount pair.
 *
 * This is deliberately NOT the naive "load pointer, then increment its
 * refcount" design discussed earlier (that has a use-after-free race: the
 * increment can touch memory the pointer no longer owns). Here, acquiring a
 * reference and verifying the pointer hasn't moved happen as ONE atomic
 * compare_exchange on the combined {ptr, externalCount} word, so there is no
 * gap between "read the pointer" and "prove it's still valid" for another
 * thread to free it in.
 *
 * Protocol:
 *  - Snapshot::internalCount starts at 1 (bias: "the atomic slot itself
 *    holds a reference").
 *  - acquire(): CAS-increment externalCount on the CURRENT word. Succeeding
 *    means the ptr genuinely was current at that instant - impossible to
 *    increment a word that's already been swapped away, because the CAS's
 *    expected value includes the pointer itself.
 *  - release(p): plain fetch_sub(1) on p->internalCount; hitting exactly
 *    zero deletes p.
 *  - publish(next): exchange _current to {next, 0}, capturing the old word;
 *    fold the old word's accumulated externalCount into the old snapshot's
 *    internalCount in one fetch_add (net of the initial bias). Whichever of
 *    a release() or this fold is the operation that drives internalCount to
 *    exactly zero is the one that deletes - order-independent, since it's
 *    plain integer arithmetic summing to zero exactly once.
 *
 * The correctness of this hinges entirely on {ptr, externalCount} being
 * updated as a single indivisible unit - i.e. on 16-byte CAS actually being
 * available. It is NOT guaranteed lock-free on every target Stellyra cares
 * about (see the benchmark's printed is_lock_free() check) - this variant
 * exists to measure what it costs when it works, and to make that
 * measurement include whatever fallback libatomic actually uses when it
 * doesn't.
 *
 * SECOND, INDEPENDENT hazard found while building this (confirmed by
 * diagnostic, not just reasoned about): this scheme is vulnerable to
 * classic ABA via heap address reuse. Under rapid churn, a Snapshot is
 * freed and the allocator can hand the exact same address to the very next
 * `new Snapshot(...)`. A CAS loop in acquire()/release() that has been
 * holding a stale {oldAddress, oldCount} value in a register can then
 * succeed against the *new*, unrelated object at that address if its
 * externalCount happens to coincide - corrupting that new object's
 * accounting. Reproduced under ThreadSanitizer/AddressSanitizer as a
 * double-free; confirmed as ABA specifically by rerunning with delete
 * replaced by a deliberate leak, which survives the identical stress test
 * cleanly. A real fix needs a monotonic tag distinct from the address
 * (e.g. Boost's atomic_shared_ptr-style split counting, or a wider CAS than
 * 128 bits), not just {pointer, count}. This is a second, independent
 * reason (beyond lock-freedom) to be very cautious about this design for
 * Stellyra - both problems are avoided entirely by EpochDomain, which never
 * compares object identity by address at all.
 */

#include "common.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>

namespace bench {
namespace v4 {

class Store
{
	struct Snapshot
	{
		std::vector< HandlerFn > entries;
		std::atomic< int64_t > internalCount;

		explicit Snapshot( std::vector< HandlerFn > e ) : entries( std::move( e ) ), internalCount( 1 ) {}
	};

	struct TaggedPtr
	{
		Snapshot* ptr;
		uint64_t externalCount;
	};

	std::atomic< TaggedPtr > _current;
	mutable std::mutex _writeMutex; // serializes connect/disconnect/churn COW mutation only

	Snapshot* acquire()
	{
		TaggedPtr old = _current.load( std::memory_order_acquire );
		TaggedPtr next;
		do
		{
			next.ptr = old.ptr;
			next.externalCount = old.externalCount + 1;
		} while ( ! _current.compare_exchange_weak( old, next,
			std::memory_order_acq_rel, std::memory_order_acquire ) );
		return old.ptr;
	}

	/**
	 * @brief Give back the reference obtained from acquire().
	 *
	 * NOT a blind "always decrement internalCount" - that was the bug in an
	 * earlier draft of this file (caught by a single-threaded ASan run, no
	 * concurrency needed to hit it: dispatch() then churnOnce() was enough).
	 * A reference is only "folded" into p->internalCount once - at the
	 * moment publish() replaces _current out from under it. Until that
	 * happens, p is still the live, current snapshot, and undoing an
	 * acquire() means giving the increment back to the SAME word (CAS
	 * externalCount back down), not touching internalCount at all.
	 *
	 * If _current has already moved on to a different snapshot by the time
	 * we get here, publish()'s fold already transferred our contribution
	 * into p->internalCount (see publish()'s docs for why that ordering is
	 * guaranteed), so the correct thing here is exactly the direct
	 * fetch_sub the earlier draft did unconditionally.
	 */
	void release( Snapshot* p )
	{
		TaggedPtr old = _current.load( std::memory_order_acquire );
		TaggedPtr next;
		for ( ;; )
		{
			if ( old.ptr != p )
			{
				// already superseded - our increment was already folded into
				// p->internalCount by publish(); give it back there instead
				if ( p->internalCount.fetch_sub( 1, std::memory_order_acq_rel ) == 1 )
				{
					delete p;
				}
				return;
			}

			next.ptr = p;
			next.externalCount = old.externalCount - 1;
			if ( _current.compare_exchange_weak( old, next,
				std::memory_order_acq_rel, std::memory_order_acquire ) )
			{
				return; // handed back to the still-current word; nothing to delete
			}
			// old was refreshed by the failed CAS - loop and re-check old.ptr
		}
	}

	void publish( Snapshot* fresh )
	{
		TaggedPtr old = _current.exchange( TaggedPtr{ fresh, 0 }, std::memory_order_acq_rel );
		const int64_t extra = static_cast< int64_t >( old.externalCount );
		const int64_t prev = old.ptr->internalCount.fetch_add( extra - 1, std::memory_order_acq_rel );
		if ( prev == -( extra - 1 ) )
		{
			delete old.ptr;
		}
	}

public:
	/// Whether {Snapshot*, uint64_t} is actually lock-free on this build -
	/// print this in the benchmark output; if false, every number for this
	/// variant includes libatomic's fallback lock.
	static bool isLockFree()
	{
		std::atomic< TaggedPtr > probe;
		return probe.is_lock_free();
	}

	explicit Store( size_t initialHandlers )
	{
		std::vector< HandlerFn > entries;
		entries.reserve( initialHandlers );
		for ( size_t i = 0; i < initialHandlers; ++i ) entries.emplace_back( &handlerBody );
		_current.store( TaggedPtr{ new Snapshot( std::move( entries ) ), 0 }, std::memory_order_relaxed );
	}

	~Store()
	{
		TaggedPtr cur = _current.load( std::memory_order_relaxed );
		delete cur.ptr; // sole owner left at destruction time
	}

	void connect()
	{
		std::lock_guard< std::mutex > lock( _writeMutex );
		Snapshot* cur = acquire();
		auto entries = cur->entries; // O(n) clone - the cost being measured
		entries.emplace_back( &handlerBody );
		publish( new Snapshot( std::move( entries ) ) );
		release( cur );
	}

	/// Disconnect+reconnect one entry in a single clone - simulates steady
	/// churn without permanently growing, same intent as the other
	/// variants' churnOnce().
	void churnOnce()
	{
		std::lock_guard< std::mutex > lock( _writeMutex );
		Snapshot* cur = acquire();
		auto entries = cur->entries; // full O(n) clone every churn - this is the trade being measured
		if ( ! entries.empty() )
		{
			entries[ 0 ] = HandlerFn( &handlerBody );
		}
		publish( new Snapshot( std::move( entries ) ) );
		release( cur );
	}

	void dispatch()
	{
		Snapshot* snap = acquire();
		for ( auto& h : snap->entries ) h();
		release( snap );
	}
};

} // namespace v4
} // namespace bench

#endif // BENCH_V4_DWCAS_H
