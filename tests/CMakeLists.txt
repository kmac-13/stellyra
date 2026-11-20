cmake_minimum_required( VERSION 3.16 )
project( StellyraTests )

set( CMAKE_CXX_STANDARD 17 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )
set( CMAKE_CXX_EXTENSIONS OFF )

if( WIN32 )
	add_definitions( -DNOMINMAX -DWIN32_LEAN_AND_MEAN )
endif()

# Enables EventLoop's STELLYRA_TEST_INJECT hook points (currently just
# migratePendingTo()'s), used to deterministically reproduce concurrent
# lock-contention scenarios instead of hoping the scheduler interleaves
# things a particular way.  Compiles to nothing (no member, no call site)
# in any build where this isn't defined, so this is safe to enable for the
# whole test suite - only tests that explicitly install a hook are affected.
add_definitions( -DSTELLYRA_TEST_INJECT )

# ---------------------------------------------------------------------------
# GoogleTest
# Fetched from source so there is no external dependency to install.
#
# Local development builds use an already-checked-out sibling googletest
# directory (fast, no network dependency).  CI runners have no such sibling
# checkout, so they pass -DSTELLYRA_FETCH_GOOGLETEST=ON to pull it via
# FetchContent instead - the local default is unchanged either way.
# ---------------------------------------------------------------------------
option( STELLYRA_FETCH_GOOGLETEST "Fetch GoogleTest via FetchContent instead of the local ../../../googletest checkout" OFF )

if( STELLYRA_FETCH_GOOGLETEST )
	include( FetchContent )

	FetchContent_Declare(
		googletest
		GIT_REPOSITORY https://github.com/google/googletest.git
		GIT_TAG        v1.14.0
	)

	# prevent GTest from overriding our compiler/linker settings on Windows
	set( gtest_force_shared_crt ON CACHE BOOL "" FORCE )
	FetchContent_MakeAvailable( googletest )
else()
	add_subdirectory( "../../../googletest/" ${CMAKE_CURRENT_BINARY_DIR}/googletest )

	# prevent GTest from overriding our compiler/linker settings on Windows
	set( gtest_force_shared_crt ON CACHE BOOL "" FORCE )
endif()

include( GoogleTest )
enable_testing()

# Collect Stellyra files
file( GLOB STELLYRA_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/../libs/stellyra/include/kmac/*.h
	${CMAKE_CURRENT_SOURCE_DIR}/../libs/stellyra/include/kmac/stellyra/*.h
	${CMAKE_CURRENT_SOURCE_DIR}/../libs/stellyra_extras/include/kmac/*.h
	${CMAKE_CURRENT_SOURCE_DIR}/../libs/stellyra_extras/include/kmac/stellyra/*.h
)

# Collect Stellyra documents
file( GLOB STELLYRA_DOCS
	${CMAKE_CURRENT_SOURCE_DIR}/../*.md
	${CMAKE_CURRENT_SOURCE_DIR}/../docs/*.md
)


# ---------------------------------------------------------------------------
# Helper function - creates a GTest executable and registers it with CTest
# ---------------------------------------------------------------------------
function( add_stellyra_test test_name source_file )
	add_executable( ${test_name} ${source_file} ${STELLYRA_SOURCES} ${STELLYRA_DOCS} )

	target_include_directories( ${test_name} PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/../libs/stellyra/include
		${CMAKE_CURRENT_SOURCE_DIR}/../libs/stellyra_extras/include
		${CMAKE_CURRENT_SOURCE_DIR}/..
	)

	target_link_libraries( ${test_name} PRIVATE GTest::gtest GTest::gtest_main )

	if( UNIX )
		target_link_libraries( ${test_name} PRIVATE pthread )
	endif()

	target_compile_features( ${test_name} PRIVATE cxx_std_17 )

	if( CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" )
		target_compile_options( ${test_name} PRIVATE -Wall -Wextra -Wpedantic )
	elseif( MSVC )
		target_compile_options( ${test_name} PRIVATE /W4 /wd4127 )
	endif()

	gtest_discover_tests( ${test_name} )
endfunction()

# ---------------------------------------------------------------------------
# Test targets
# ---------------------------------------------------------------------------

# building blocks
add_stellyra_test( test_callable               test_callable.cpp )

# event handling
add_stellyra_test( test_basic_connections      test_basic_connections.cpp )
add_stellyra_test( test_free_functions         test_free_functions.cpp )
add_stellyra_test( test_advanced_connections   test_advanced_connections.cpp )
add_stellyra_test( test_partial_args           test_partial_args.cpp )
add_stellyra_test( test_invokers               test_invokers.cpp )
add_stellyra_test( test_sender_deferral        test_sender_deferral.cpp )
add_stellyra_test( test_thread_safety          test_thread_safety.cpp )
add_stellyra_test( test_reentrancy             test_reentrancy.cpp )
add_stellyra_test( test_predicate_context      test_predicate_context.cpp )
add_stellyra_test( test_connection_lifecycle   test_connection_lifecycle.cpp )
add_stellyra_test( test_disconnect_by_target   test_disconnect_by_target.cpp )
add_stellyra_test( test_stable_slots           test_stable_slots.cpp )
add_stellyra_test( test_receiver_lifetime_anchor test_receiver_lifetime_anchor.cpp )
add_stellyra_test( test_pending_removal_stress test_pending_removal_stress.cpp )
add_stellyra_test( test_trackconnection_race   test_trackconnection_race.cpp )
add_stellyra_test( test_forward_to             test_forward_to.cpp )
add_stellyra_test( test_forward_from           test_forward_from.cpp )
add_stellyra_test( test_private_event_multi_owner test_private_event_multi_owner.cpp )
add_stellyra_test( test_event_signature         test_event_signature.cpp )

# event loop
add_stellyra_test( test_event_loops            test_event_loops.cpp )
add_stellyra_test( test_external_event_loop    test_external_event_loop.cpp )
add_stellyra_test( test_event_loop_migration   test_event_loop_migration.cpp )
add_stellyra_test( test_event_loop_direct_api  test_event_loop_direct_api.cpp )

# extras
add_stellyra_test( test_combining_and_groups   test_combining_and_groups.cpp )
add_stellyra_test( test_combiners              test_combiners.cpp )
add_stellyra_test( test_recordable_event       test_recordable_event.cpp )
add_stellyra_test( test_concurrent_event       test_concurrent_event.cpp )
add_stellyra_test( test_property               test_property.cpp )

# misc
add_stellyra_test( test_block_guard            test_block_guard.cpp )
add_stellyra_test( test_deferred_teardown_order test_deferred_teardown_order.cpp )
add_stellyra_test( test_coverage_gaps          test_coverage_gaps.cpp )
add_stellyra_test( test_edge_cases             test_edge_cases.cpp )
add_stellyra_test( test_sense_plan_act         test_sense_plan_act.cpp )

# Connection misconfiguration/lifecycle logging - the only target built with
# both logging flags forced on, so the actual log-line gating and content
# (not just the "it compiles to nothing when off" smoke-tested elsewhere) is
# exercised against the real headers.  Every other test target above builds
# with both flags at their shipped-off default.
add_stellyra_test( test_connection_logging     test_connection_logging.cpp )
target_compile_definitions( test_connection_logging PRIVATE
	STELLYRA_LOG_CONNECTION_ISSUES=1
	STELLYRA_LOG_CONNECTION_LIFECYCLE=1 )

# ---------------------------------------------------------------------------
# Qt integration test - only when the Qt module itself is being built.
#
# Does its own find_package( Qt ) and compiles the Qt module's source files
# directly into this target rather than linking the KMac_StellyraQt target
# the root CMakeLists.txt defines (same as every add_stellyra_test() target
# above builds Stellyra's own headers directly via relative include paths
# instead of linking KMac_Stellyra/KMac_StellyraExtras.  This file is also a
# valid standalone CMake project for dealing with just tests/ directly in an
# IDE without configuring the whole repo.
#
# Not routed through add_stellyra_test(): this target needs AUTOMOC (the
# test fixtures are Q_OBJECT classes), and its own main() that constructs a
# QCoreApplication before any QObject in the binary, so it links
# GTest::gtest directly rather than GTest::gtest_main.
# ---------------------------------------------------------------------------

# when used as a stand-alone project needing Qt test, uncomment this
# set( STELLYRA_BUILD_QT ON )
if( STELLYRA_BUILD_QT )
	find_package( Qt6 QUIET COMPONENTS Core )
	if( Qt6_FOUND )
		set( _stellyra_test_qt_package Qt6 )
	else()
		find_package( Qt5 REQUIRED COMPONENTS Core )
		set( _stellyra_test_qt_package Qt5 )
	endif()

	# all Qt-dependent code must be listed explicitly for AUTOMOC to moc them
	add_executable( test_qt_integration
		qt_test_fixtures.h
		test_qt_integration.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/../libs/stellyra_qt/include/kmac/stellyra/qt/qt_connect.h
		${CMAKE_CURRENT_SOURCE_DIR}/../libs/stellyra_qt/include/kmac/stellyra/qt/event_loop_bridge.h
		${CMAKE_CURRENT_SOURCE_DIR}/../libs/stellyra_qt/include/kmac/stellyra/qt/trackable_bridge.h
		${CMAKE_CURRENT_SOURCE_DIR}/../libs/stellyra_qt/src/kmac/stellyra/qt/event_loop_bridge.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/../libs/stellyra_qt/src/kmac/stellyra/qt/trackable_bridge.cpp
	)

	target_include_directories( test_qt_integration PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/../libs/stellyra/include
		${CMAKE_CURRENT_SOURCE_DIR}/../libs/stellyra_extras/include
		${CMAKE_CURRENT_SOURCE_DIR}/../libs/stellyra_qt/include
		${CMAKE_CURRENT_SOURCE_DIR}/..
	)

	target_link_libraries( test_qt_integration PRIVATE ${_stellyra_test_qt_package}::Core GTest::gtest )

	if( UNIX )
		target_link_libraries( test_qt_integration PRIVATE pthread )
	endif()

	target_compile_features( test_qt_integration PRIVATE cxx_std_17 )

	if( CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" )
		target_compile_options( test_qt_integration PRIVATE -Wall -Wextra -Wpedantic )
	elseif( MSVC )
		target_compile_options( test_qt_integration PRIVATE /W4 /wd4127 )
	endif()

	set_target_properties( test_qt_integration PROPERTIES AUTOMOC ON )

	if( WIN32 )
		# test_qt_integration is the only target here that dynamically
		# links Qt; none of the other other tests need this
		gtest_discover_tests( test_qt_integration
			DISCOVERY_MODE PRE_TEST
			PROPERTIES ENVIRONMENT "PATH=$<TARGET_FILE_DIR:${_stellyra_test_qt_package}::Core>;$ENV{PATH}"
		)
	else()
		gtest_discover_tests( test_qt_integration )
	endif()
endif()
