#pragma once
#ifndef _ASIO_LIBS_CORO_BARRIER_HPP_
#define _ASIO_LIBS_CORO_BARRIER_HPP_
#include <functional>
#include <boost/asio/io_service.hpp>
#include <boost/asio/spawn.hpp>

namespace ASIOLibs {

// This concept allows ASIO coroutine to suspend until io operation is finished (a barrier)
struct CoroBarrier {
	CoroBarrier(boost::asio::io_service &_io, boost::asio::yield_context &_yield) : io(_io), yield(_yield) {};

	void done(boost::system::error_code _ec = boost::system::error_code() );
	boost::system::error_code wait();

	std::function< void(const boost::system::error_code &ec) > errorCheck;

private:
	boost::asio::io_service &io;
	boost::asio::yield_context &yield;
	boost::system::error_code ec;
	bool got_done=false;
	bool is_suspended=false;
};

};
#endif
