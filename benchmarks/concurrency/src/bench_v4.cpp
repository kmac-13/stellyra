/**
 * @file bench_v4.cpp
 * @brief V4 (DWCAS refcount) measured in isolation, pure dispatch only - no
 * churn thread. Churn is what triggers this variant's confirmed ABA hazard
 * (rapid free+reallocate at the same address - see v4_dwcas.h's file docs),
 * so it's deliberately not exercised here. This gives V4 its best-case
 * number (no writer contention at all) without risking the crash taking out
 * the other five variants' results in bench_main.
 */

#include "../include/common.h"
#include "../include/v4_dwcas.h"

#include <benchmark/benchmark.h>

#include <cstdio>
#include <mutex>
#include <thread>

namespace {

bench::v4::Store& sharedStore()
{
	static bench::v4::Store* store = nullptr;
	static std::once_flag onceFlag;
	std::call_once( onceFlag, [] { store = new bench::v4::Store( bench::kHandlerCount ); } );
	return *store;
}

void BM_Dispatch( benchmark::State& state )
{
	bench::v4::Store& store = sharedStore();
	for ( auto _ : state )
	{
		store.dispatch();
	}
}

} // namespace

BENCHMARK( BM_Dispatch )->Name( "V4_DWCAS_Refcount/Dispatch_NoChurn" )->ThreadRange( 1, 8 )->UseRealTime();

int main( int argc, char** argv )
{
	std::fprintf( stderr,
		"hardware_concurrency() = %u\n"
		"{Snapshot*, uint64_t} is_lock_free() = %s  <-- if false, every number below includes libatomic's fallback lock\n"
		"NOTE: churn intentionally not exercised here - see v4_dwcas.h docs for the confirmed ABA hazard under rapid free+realloc.\n\n",
		std::thread::hardware_concurrency(),
		bench::v4::Store::isLockFree() ? "true" : "false" );

	benchmark::Initialize( &argc, argv );
	if ( benchmark::ReportUnrecognizedArguments( argc, argv ) ) return 1;
	benchmark::RunSpecifiedBenchmarks();
	benchmark::Shutdown();
	return 0;
}
