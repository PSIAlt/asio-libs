#pragma once
#ifndef _ASIO_LIBS_COROUTINE_POOL_HPP_
#define _ASIO_LIBS_COROUTINE_POOL_HPP_
#include <utility>
#include <deque>
#include <set>
#include <functional>
#include <boost/asio/io_service.hpp>
#include <boost/asio/spawn.hpp>

namespace ASIOLibs {

struct CoroutinePool {
	typedef std::function< void(boost::asio::yield_context &yield) > handler_type;
	CoroutinePool( boost::asio::io_service &_io, uint32_t _poolsize );

	template < typename T >
	void Schedule(T &&h) {
		if( shutdown )
			return;
		handler_queue.emplace_back( std::forward<T>(h) );
		ResumeCoroutine();
	}

	void Shutdown() { //Cancels all jobs if possible
		shutdown=true;
		RunQueue();
	}
	void RunQueue(); //Block for queue execution without shutdown

private:
	void ResumeCoroutine(); //Find and resume any of coroutines from the pool
	void CtxHolder(boost::asio::yield_context yield);
	boost::asio::io_service &io;
	uint32_t poolsize;
	std::deque< handler_type > handler_queue;
	std::set< boost::asio::yield_context * > free_ctx;
	bool shutdown=false;
};

};

#endif
