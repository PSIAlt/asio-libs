#define CATCH_CONFIG_RUNNER
#include "catch.hpp"
#include <iostream>
#include <condition_variable>
#include <mutex>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>

using namespace std;
boost::asio::io_service io;
boost::asio::yield_context *run_yield;

TEST_CASE( "Base cases", "[flowgraph]" ) {
}


// A setup
int result=-1;
void run_tests(boost::asio::yield_context _yield, int argc, char* const argv[]) {
	run_yield = &_yield;
	result = Catch::Session().run( argc, argv );
}

int main( int argc, char* const argv[] ) {
	boost::asio::spawn( io, boost::bind(run_tests, _1, argc, argv) );
	io.run();
	return result;
}
