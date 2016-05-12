#pragma once
#ifndef _ASIO_LIBS_YIELD_HELPER_HPP_
#define _ASIO_LIBS_YIELD_HELPER_HPP_
#include <functional>
#include <boost/asio/spawn.hpp>

/*
	Usage:
	your_function(boost::asio::yield_context yield) {
		ASIOLibs::YieldHelper yh(yield);
		yh.onYield = []() {
			clearLogPrefix();
		};
		yh.onResume = []() {
			setLogPrefix();
		};
		//....
		async_write(sock, buf, YIELDHELPER);
		//....
	}
*/

namespace ASIOLibs {

struct YieldHelper;
extern YieldHelper *currentYieldHelper;

struct YieldHelper {
	YieldHelper(boost::asio::yield_context &_yield) : yield(_yield) {
		currentYieldHelper = this;
	}
	~YieldHelper() {
		currentYieldHelper = nullptr;
	}
	std::function< void() > onYield;
	std::function< void() > onResume;

private:
	friend struct YRHelper;
	boost::asio::yield_context &yield;

	void beforeYield() {
		if( onYield ) onYield();
		currentYieldHelper = nullptr;
	}
	void afterResume() {
		if( onResume ) onResume();
		currentYieldHelper = this;
	}
};

struct YRHelper { // A proxy object which do callbacks before&after line its used at.
	YRHelper(YieldHelper *_y) : y(_y) {
		assert( _y != nullptr );
		y->beforeYield();
	}

	~YRHelper() {
		y->afterResume();
	} 

	operator boost::asio::yield_context&() const {
		return y->yield;
	}
private:
	YieldHelper *y;
};


};

#define YIELDHELPER static_cast<boost::asio::yield_context&>( ASIOLibs::YRHelper(ASIOLibs::currentYieldHelper) )


#endif
