/**
 * @file stress_test_v4.cpp
 * @brief Isolated stress check for V4 (DWCAS refcount) ONLY. Expected to
 * fail (crash, or be flagged by a sanitizer) under real churn - this is
 * intentional documentation of the confirmed ABA hazard described in
 * v4_dwcas.h's file docs, kept as its own binary so that expected failure
 * never masks the other five variants passing in stress_test.cpp.
 */

#include "../include/v4_dwcas.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

int main()
{
	std::printf( "V4/DWCAS isolated churn stress - EXPECTED TO FAIL (confirmed ABA via address reuse; see v4_dwcas.h)\n" );

	bench::v4::Store store( bench::kHandlerCount );
	std::atomic< bool > stop{ false };
	std::vector< std::thread > threads;

	const int dispatchThreads = std::max( 2u, std::thread::hardware_concurrency() * 4 );
	for ( int i = 0; i < dispatchThreads; ++i )
	{
		threads.emplace_back( [ &store, &stop ] {
			while ( ! stop.load( std::memory_order_relaxed ) ) store.dispatch();
		} );
	}
	for ( int i = 0; i < 2; ++i )
	{
		threads.emplace_back( [ &store, &stop ] {
			while ( ! stop.load( std::memory_order_relaxed ) ) store.churnOnce();
		} );
	}

	std::this_thread::sleep_for( std::chrono::milliseconds( 1500 ) );
	stop.store( true, std::memory_order_relaxed );
	for ( auto& t : threads ) t.join();

	std::printf( "[UNEXPECTED] V4/DWCAS survived this run - ABA is a race, not a guarantee; "
		"re-run under ASan/TSan and/or longer duration before treating this as fixed.\n" );
	return 0;
}
