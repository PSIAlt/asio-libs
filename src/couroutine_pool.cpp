#include <stdio.h>
#include <stdexcept>
#include <boost/bind.hpp>
#include "coroutine_pool.hpp"

#define VERY_DEBUG(...) ;
// static inline void VERY_DEBUG(const char *fmt, ...) {
// 	va_list args;
// 	va_start(args, fmt);
// 	vfprintf(stderr, fmt, args);
// 	va_end(args);
// 	fprintf(stderr, "\n");
// }

namespace ASIOLibs {

CoroutinePool::CoroutinePool( boost::asio::io_service &_io, uint32_t _poolsize ) : io(_io), poolsize(_poolsize) {
	for(uint32_t i=0; i<poolsize; i++) {
		VERY_DEBUG("Spawning CtxHolder");
		boost::asio::spawn(io, boost::bind(&CoroutinePool::CtxHolder, this, _1) );
	}
}

void CoroutinePool::ResumeCoroutine() {
	if( free_ctx.empty() ) {
		VERY_DEBUG("ResumeCoroutine no free ctx");
		return; //No need to resume cus we had control and when we yield a coroutine will take control and check handler_queue
	}

	VERY_DEBUG("ResumeCoroutine enter");
	volatile auto y = *free_ctx.begin();
	free_ctx.erase( free_ctx.begin() );
	//post() because proabaly we want to do something more in calling routine and probably calling routine do not run a yield_context
	io.post( [y]() {
		VERY_DEBUG("Resuming a coroutine");
		auto coro = y->coro_.lock();
		//if( coro )
			(*coro)(); //Resume
	});
}

void CoroutinePool::CtxHolder(boost::asio::yield_context yield) {
	// Prepare this coroutine
	while( !shutdown ) {
		//There is a possibility need to skip yield if we are just started and there is job awaiting
		if( handler_queue.empty() ) {
			free_ctx.insert( &yield ); //Add pointer so ResumeCoroutine can resume us
			VERY_DEBUG("CtxHolder %p: going to sleep", &yield);
			volatile auto coro = yield.coro_.lock(); //Prevent coroutine from forced_unwind since we have no active coro_handler actually
			yield.ca_(); //Yield coroutine
			VERY_DEBUG("CtxHolder %p: resumed", &yield);
		}

		// Awoken, now check for job
		
		if( shutdown ) break;
		while( !handler_queue.empty() ) {
			auto h = std::move( handler_queue.front() );
			handler_queue.pop_front();
			VERY_DEBUG("CtxHolder %p: running a job", &yield);
			h(yield); //Let exceptions crash to see bt
			handlers_processed++;
		}
		//Queue is done, wait for next resume or shutdown
	}
}

void CoroutinePool::RunQueue(size_t max_num) {
	VERY_DEBUG("CoroutinePool::RunQueue begin");
	size_t want_handlers_processed = handlers_processed + max_num;
	while( !handler_queue.empty() ) {
		ResumeCoroutine();
		io.run_one();
		io.poll();
		if( want_handlers_processed == handlers_processed )
			break;
	}
	VERY_DEBUG("CoroutinePool::RunQueue end");
}

};
