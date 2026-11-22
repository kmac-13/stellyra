cmake_minimum_required( VERSION 3.16 )
project( concurrent_event_bench CXX )

set( CMAKE_CXX_STANDARD 17 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )

if( NOT CMAKE_BUILD_TYPE )
	set( CMAKE_BUILD_TYPE Release )
endif()

find_package( Threads REQUIRED )

# Google Benchmark: three ways to get it, tried in order.  Whichever path
# runs, it must actually produce the benchmark::benchmark target - setting
# BENCHMARK_ROOT alone does nothing, since nothing has told CMake to process
# that directory's CMakeLists.txt yet.
#
# 1. A local checkout, via BENCHMARK_ROOT (set this to your existing
#    google/benchmark clone - e.g. a sibling checkout next to Stellyra).
# 2. An already-installed/system benchmark, via find_package.
# 3. Fetched and built as part of this project (needs network access to
#    github.com) - the original fallback, kept for a machine with neither
#    of the above.
set( BENCHMARK_ROOT "../../../../googlebenchmark" CACHE PATH "Root of an existing google/benchmark checkout (optional)" )
# set( BENCHMARK_ROOT "" CACHE PATH "Root of an existing google/benchmark checkout (optional)" )

set( BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE )
set( BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE )
set( BENCHMARK_ENABLE_WERROR OFF CACHE BOOL "" FORCE )
set( BENCHMARK_DOWNLOAD_DEPENDENCIES ON CACHE BOOL "" FORCE )

if( BENCHMARK_ROOT AND EXISTS "${BENCHMARK_ROOT}/CMakeLists.txt" )
	message( STATUS "Using local Google Benchmark checkout at ${BENCHMARK_ROOT}" )
	add_subdirectory( ${BENCHMARK_ROOT} ${CMAKE_BINARY_DIR}/googlebenchmark-build )
else()
	find_package( benchmark QUIET )
	if( NOT benchmark_FOUND )
		message( STATUS "benchmark not found locally or via find_package - fetching it via CMake instead" )
		include( FetchContent )
		FetchContent_Declare(
			googlebenchmark
			GIT_REPOSITORY https://github.com/google/benchmark.git
			GIT_TAG v1.9.1
		)
		FetchContent_MakeAvailable( googlebenchmark )
	endif()
endif()

# add_subdirectory() on a bare benchmark checkout defines the `benchmark`
# target but not the `benchmark::benchmark` alias (that alias is only added
# by benchmark's own top-level CMakeLists when it's the top-level project,
# not always guaranteed when pulled in as a subdirectory of another one) -
# add it ourselves if it's still missing so the target_link_libraries calls
# below work the same regardless of which of the three paths above ran.
if( TARGET benchmark AND NOT TARGET benchmark::benchmark )
	add_library( benchmark::benchmark ALIAS benchmark )
endif()

# libatomic is needed for the 16-byte atomic in v4_dwcas.h on GCC/Clang
# (MinGW included); MSVC has no equivalent separate library to link.
if( CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" )
	set( MAYBE_ATOMIC_LIB atomic )
else()
	set( MAYBE_ATOMIC_LIB )
endif()

add_executable( bench_main src/bench_main.cpp )
target_link_libraries( bench_main PRIVATE benchmark::benchmark Threads::Threads ${MAYBE_ATOMIC_LIB} )
target_compile_options( bench_main PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/O2> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-O3> )

# V4 (DWCAS) measured in its own process - see bench_main.cpp's file docs.
add_executable( bench_v4 src/bench_v4.cpp )
target_link_libraries( bench_v4 PRIVATE benchmark::benchmark Threads::Threads ${MAYBE_ATOMIC_LIB} )
target_compile_options( bench_v4 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/O2> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-O3> )

add_executable( stress_test src/stress_test.cpp )
target_link_libraries( stress_test PRIVATE Threads::Threads ${MAYBE_ATOMIC_LIB} )

# V4's known-failing churn check, isolated so its expected crash never masks
# the other five variants passing - see stress_test_v4.cpp's file docs.
add_executable( stress_test_v4 src/stress_test_v4.cpp )
target_link_libraries( stress_test_v4 PRIVATE Threads::Threads ${MAYBE_ATOMIC_LIB} )

# ThreadSanitizer/AddressSanitizer variants of the stress tests.  Neither
# ships with Qt's bundled MinGW (mingw1310_64 here has no libasan/libubsan,
# and MinGW-w64 has no TSan runtime at all) or with MSVC in the plain
# -fsanitize= form - if you're configuring this on Windows, both targets are
# skipped there; run them on Linux (WSL is fine) instead, the same way this
# project already treats Linux as a correctness pre-check for everything
# else.  The plain (non-sanitized) stress_test/stress_test_v4 above still
# build and run fine on Windows/MinGW - it's only these two extra targets
# that need a toolchain with sanitizer runtimes.
if( NOT (MINGW OR MSVC) )
	add_executable( stress_test_tsan src/stress_test.cpp )
	target_link_libraries( stress_test_tsan PRIVATE Threads::Threads ${MAYBE_ATOMIC_LIB} )
	target_compile_options( stress_test_tsan PRIVATE -O1 -g -fsanitize=thread )
	target_link_options( stress_test_tsan PRIVATE -fsanitize=thread )

	add_executable( stress_test_v4_asan src/stress_test_v4.cpp )
	target_link_libraries( stress_test_v4_asan PRIVATE Threads::Threads ${MAYBE_ATOMIC_LIB} )
	target_compile_options( stress_test_v4_asan PRIVATE -O1 -g -fsanitize=address,undefined )
	target_link_options( stress_test_v4_asan PRIVATE -fsanitize=address,undefined )
else()
	message( STATUS "Skipping stress_test_tsan/stress_test_v4_asan on this toolchain (no sanitizer runtimes on MinGW/MSVC) - run them on Linux instead" )
endif()
