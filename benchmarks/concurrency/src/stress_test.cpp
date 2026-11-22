/**
 * @file stress_test.cpp
 * @brief Correctness smoke test, meant to be built with
 * -fsanitize=thread,address,undefined and run BEFORE trusting any of
 * bench_main's throughput numbers - matches the project's own rule that
 * benchmark correctness is treated as rigorously as library correctness.
 *
 * Not a substitute for the real 200+-round Windows/MinGW Debug gate these
 * variants would need before going anywhere near Stellyra itself - this only
 * checks that none of the 6 standalone variants have an obvious data race
 * or use-after-free under concurrent dispatch + churn.
 */

#include "../include/v0_baseline.h"
#include "../include/v1_padded.h"
#include "../include/v2_dispersed.h"
#include "../include/v3_growable.h"
#include "../include/v5_mutex.h"
#include "../include/v6_gencounter.h"
#include "../include/v7_versioned_pool.h"
// v4_dwcas.h intentionally not run here under churn - see stress_test_v4.cpp

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

namespace {

template< typename StoreT >
void runStress( const char* name, int numThreads, int numChurnThreads, int msDuration )
{
	StoreT store( bench::kHandlerCount );
	std::atomic< bool > stop{ false };
	std::vector< std::thread > threads;

	for ( int i = 0; i < numThreads; ++i )
	{
		threads.emplace_back( [ &store, &stop ] {
			while ( ! stop.load( std::memory_order_relaxed ) )
			{
				store.dispatch();
			}
		} );
	}

	for ( int i = 0; i < numChurnThreads; ++i )
	{
		threads.emplace_back( [ &store, &stop ] {
			while ( ! stop.load( std::memory_order_relaxed ) )
			{
				store.churnOnce();
			}
		} );
	}

	std::this_thread::sleep_for( std::chrono::milliseconds( msDuration ) );
	stop.store( true, std::memory_order_relaxed );
	for ( auto& t : threads ) t.join();

	std::printf( "[OK] %s survived %d dispatch + %d churn threads for %dms\n",
		name, numThreads, numChurnThreads, msDuration );
}

} // namespace

int main()
{
	const int dispatchThreads = std::max( 2u, std::thread::hardware_concurrency() * 4 );
	const int churnThreads = 2;
	const int durationMs = 1500;

	runStress< bench::v0::Store >( "V0_Baseline_Epoch", dispatchThreads, churnThreads, durationMs );
	runStress< bench::v1::Store >( "V1_Padded_Epoch", dispatchThreads, churnThreads, durationMs );
	runStress< bench::v2::Store >( "V2_Dispersed_Epoch", dispatchThreads, churnThreads, durationMs );
	runStress< bench::v3::Store >( "V3_Growable_Epoch", dispatchThreads, churnThreads, durationMs );
	runStress< bench::v5::Store >( "V5_Mutex_Baseline", dispatchThreads, churnThreads, durationMs );
	runStress< bench::v6::Store >( "V6_CowGenCounter", dispatchThreads, churnThreads, durationMs );
	runStress< bench::v7::Store >( "V7_VersionedPool", dispatchThreads, churnThreads, durationMs );

	std::printf( "\nAll variants survived the stress smoke test.\n"
		"(V4/DWCAS is checked separately by stress_test_v4 - it is expected to fail; see v4_dwcas.h)\n" );
	return 0;
}
