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
	void Schedule(T &&h) { //Schedule handler to run in coroutine in future (as io_service can do a handler)
		if( shutdown )
			return;
		handler_queue.emplace_back( std::forward<T>(h) );
		ResumeCoroutine();
	}
	template < typename T >
	void Dispatch(T &&h) { //Same as Schedule+RunQueue but blocks until given handler is processed
		if( shutdown )
			return;
		handler_queue.emplace_back( std::forward<T>(h) );
		RunQueue( handler_queue.size() ); //Serve up to current handler
	}

	void Shutdown() { //Cancels all jobs if possible
		shutdown=true;
		RunQueue();
	}
	void RunQueue(size_t max_num=0); //Block for queue execution without shutdown (max_num can limit number of handlers to process)

	uint32_t GetWorkerCount() const { return poolsize; }
	uint32_t GetWorkersBusy() const { return poolsize-free_ctx.size(); }
	uint32_t GetHandlerQSize() const { return handler_queue.size(); }

private:
	void ResumeCoroutine(); //Find and resume any of coroutines from the pool
	void CtxHolder(boost::asio::yield_context yield);
	boost::asio::io_service &io;
	uint32_t poolsize;
	std::deque< handler_type > handler_queue;
	std::set< boost::asio::yield_context * > free_ctx;
	size_t handlers_processed=0;
	bool shutdown=false;
};

};

#endif
