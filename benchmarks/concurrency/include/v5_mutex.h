#pragma once
#ifndef BENCH_V5_MUTEX_H
#define BENCH_V5_MUTEX_H

/**
 * @file v5_mutex.h
 * @brief Variant 5 - the thing all the others are trying to beat: a plain
 * recursive-mutex-guarded vector, dispatch and connect/disconnect all under
 * the same lock. Stands in for Stellyra's existing recursive-mutex-based
 * `Event`.
 */

#include "common.h"

#include <mutex>
#include <vector>

namespace bench {
namespace v5 {

class Store
{
	std::recursive_mutex _mutex;
	std::vector< HandlerFn > _entries;

public:
	explicit Store( size_t initialHandlers )
	{
		_entries.reserve( initialHandlers );
		for ( size_t i = 0; i < initialHandlers; ++i ) _entries.emplace_back( &handlerBody );
	}

	void connect()
	{
		std::lock_guard< std::recursive_mutex > lock( _mutex );
		_entries.emplace_back( &handlerBody );
	}

	void churnOnce()
	{
		std::lock_guard< std::recursive_mutex > lock( _mutex );
		if ( ! _entries.empty() )
		{
			_entries[ 0 ] = HandlerFn( &handlerBody );
		}
	}

	void dispatch()
	{
		std::lock_guard< std::recursive_mutex > lock( _mutex );
		for ( auto& h : _entries ) h();
	}
};

} // namespace v5
} // namespace bench

#endif // BENCH_V5_MUTEX_H
