#define CATCH_CONFIG_RUNNER
#include "catch.hpp"
#include <iostream>
#include <condition_variable>
#include <mutex>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <yield_helper.hpp>

using namespace std;
boost::asio::io_service io;
boost::asio::yield_context *run_yield;

std::mutex m1;
std::condition_variable m1_cond_var;
std::mutex m2;
std::condition_variable m2_cond_var;

int enter_count = 0;
int leave_count = 0;
int nocallback_working = 0;

// Test onYield and onResume
void callbacks_worker(boost::asio::yield_context yield) {
	ASIOLibs::YieldHelper yh(yield);
	yh.onYield = []() { leave_count++; };
	yh.onResume = []() { enter_count++; };

	io.post( YIELDHELPER );
	assert( enter_count == 1 );
	assert( enter_count == leave_count );
	io.post( YIELDHELPER );
	io.post( YIELDHELPER );
	io.post( YIELDHELPER );
	io.post( YIELDHELPER );

	m1_cond_var.notify_one();
}
void callbacks_worker_nocallbacks(boost::asio::yield_context yield) {
	ASIOLibs::YieldHelper yh(yield);
	//Check it works without callbacks too
	io.post( YIELDHELPER );
	io.post( YIELDHELPER );
	io.post( YIELDHELPER );
	io.post( YIELDHELPER );
	io.post( YIELDHELPER );

	nocallback_working=1;
	m2_cond_var.notify_one();
}

TEST_CASE( "Callbacks working", "[callbacks]" ) {
	{
		std::unique_lock<std::mutex> lock(m1);
		while( enter_count != 5 ) {
			io.post( *run_yield );
			m1_cond_var.wait_for(lock, std::chrono::milliseconds(10));
			io.post( *run_yield );
		}
		REQUIRE( enter_count == leave_count );
		REQUIRE( enter_count == 5 );
	}
	{
		std::unique_lock<std::mutex> lock(m2);
		while( nocallback_working != 1 ) {
			io.post( *run_yield );
			m2_cond_var.wait_for(lock, std::chrono::milliseconds(10));
			io.post( *run_yield );
		}
		REQUIRE( nocallback_working == 1 );
	}
}


// A setup
int result=-1;
void run_tests(boost::asio::yield_context _yield, int argc, char* const argv[]) {
	run_yield = &_yield;
	boost::asio::spawn( io, boost::bind(callbacks_worker, _1) );
	boost::asio::spawn( io, boost::bind(callbacks_worker_nocallbacks, _1) );
	result = Catch::Session().run( argc, argv );
}

int main( int argc, char* const argv[] ) {
	boost::asio::spawn( io, boost::bind(run_tests, _1, argc, argv) );
	io.run();
	return result;
}
