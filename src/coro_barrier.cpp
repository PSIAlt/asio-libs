#include "coro_barrier.hpp"

namespace ASIOLibs {

void CoroBarrier::done(boost::system::error_code _ec) {
	got_done=true;
	ec = _ec;
	if( !is_suspended )
		return; //Easy way

	//post() because proabaly we want to do something more in calling routine and probably calling routine do not run a yield_context
	io.post( [this]() {
		assert( is_suspended==true );
		auto coro = yield.coro_.lock();
		(*coro)(); //Resume
	});
}

boost::system::error_code CoroBarrier::wait() {
	assert( is_suspended == false );
	if( got_done ) {
		if( errorCheck )
			errorCheck(ec);
		return ec; //Easy way
	}

	while( !got_done ) {
		//Suspend this coroutine
		volatile auto coro = yield.coro_.lock(); //Prevent coroutine from forced_unwind since we have no active coro_handler actually
		is_suspended=true;
		yield.ca_(); //Yield coroutine
		//Now awoken
	}
	if( errorCheck )
		errorCheck(ec);
	return ec;
}

};
