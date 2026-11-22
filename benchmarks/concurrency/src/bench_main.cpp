/**
 * @file bench_main.cpp
 * @brief Side-by-side throughput comparison of all 6 reclamation variants,
 * under pure dispatch and under dispatch-with-churn, across thread counts.
 *
 * Standalone: none of these variants depend on Stellyra's Trackable/EventLoop/
 * EventStorage/Callable - only the slot-storage + reclamation problem is
 * being isolated here, per the plan discussed with Jackie.
 */

/**
 * V4 (DWCAS refcount) is deliberately excluded from this executable: it has
 * a confirmed ABA hazard under rapid connect/disconnect churn (see
 * v4_dwcas.h's file docs and bench_v4.cpp), and a crash in one
 * benchmark::State would take the whole process - and every other variant's
 * numbers in this run - down with it. Its pure-dispatch (no churn) number
 * is measured separately in bench_v4, where the failure mode can't trigger.
 */

#include "../include/common.h"
#include "../include/v0_baseline.h"
#include "../include/v1_padded.h"
#include "../include/v2_dispersed.h"
#include "../include/v3_growable.h"
#include "../include/v5_mutex.h"
#include "../include/v6_gencounter.h"
#include "../include/v7_versioned_pool.h"

#include <benchmark/benchmark.h>

#include <cstdio>
#include <mutex>

namespace {

template< typename StoreT >
StoreT& sharedStore()
{
	static StoreT* store = nullptr;
	static std::once_flag onceFlag;
	std::call_once( onceFlag, [] { store = new StoreT( bench::kHandlerCount ); } );
	return *store;
}

template< typename StoreT >
void BM_Dispatch( benchmark::State& state )
{
	StoreT& store = sharedStore< StoreT >();
	for ( auto _ : state )
	{
		store.dispatch();
	}
}

template< typename StoreT >
void BM_DispatchWithChurn( benchmark::State& state )
{
	StoreT& store = sharedStore< StoreT >();
	uint64_t i = 0;
	for ( auto _ : state )
	{
		if ( state.thread_index() == 0 && ( i % bench::kChurnPeriod ) == 0 )
		{
			store.churnOnce();
		}
		else
		{
			store.dispatch();
		}
		++i;
	}
}

} // namespace

#define REGISTER_VARIANT( NS, LABEL ) \
	BENCHMARK_TEMPLATE( BM_Dispatch, NS::Store )->Name( LABEL "/Dispatch" )->ThreadRange( 1, 8 )->UseRealTime(); \
	BENCHMARK_TEMPLATE( BM_DispatchWithChurn, NS::Store )->Name( LABEL "/DispatchWithChurn" )->ThreadRange( 1, 8 )->UseRealTime();

REGISTER_VARIANT( bench::v0, "V0_Baseline_Epoch" )
REGISTER_VARIANT( bench::v1, "V1_Padded_Epoch" )
REGISTER_VARIANT( bench::v2, "V2_Dispersed_Epoch" )
REGISTER_VARIANT( bench::v3, "V3_Growable_Epoch" )
REGISTER_VARIANT( bench::v5, "V5_Mutex_Baseline" )
REGISTER_VARIANT( bench::v6, "V6_CowGenCounter" )
REGISTER_VARIANT( bench::v7, "V7_VersionedPool" )

int main( int argc, char** argv )
{
	std::fprintf( stderr,
		"hardware_concurrency() = %u\n"
		"(V4/DWCAS is benchmarked separately by bench_v4 - see its output for why)\n\n",
		std::thread::hardware_concurrency() );

	benchmark::Initialize( &argc, argv );
	if ( benchmark::ReportUnrecognizedArguments( argc, argv ) ) return 1;
	benchmark::RunSpecifiedBenchmarks();
	benchmark::Shutdown();

	auto& v3store = sharedStore< bench::v3::Store >();
	std::fprintf( stderr, "\nV3 EpochDomain generation count after run: %zu\n", v3store.epochGenerationCount() );

	auto& v6store = sharedStore< bench::v6::Store >();
	std::fprintf( stderr, "V6 pending-free queue depth after run: %zu\n", v6store.pendingFreeCount() );

	auto& v7store = sharedStore< bench::v7::Store >();
	std::fprintf( stderr, "V7 churnOnce() stall count after run: %lu\n",
		static_cast< unsigned long >( v7store.stallCount() ) );

	return 0;
}
