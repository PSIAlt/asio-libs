#define CATCH_CONFIG_RUNNER
#include "catch.hpp"
#include <iostream>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/asio/deadline_timer.hpp>
#include "coroutine_pool.hpp"

using namespace std;
boost::asio::io_service io;
int counter = 0;
boost::asio::yield_context *yield;

TEST_CASE( "From a coroutine", "[coroutine_pool]" ) {
	counter = 0;
	ASIOLibs::CoroutinePool p(io, 10);

	for(int i=0; i<100; i++) {
		p.Schedule([&](boost::asio::yield_context &y) {
			counter++;
			io.post(y);
			counter++;
		});
	}

	p.RunQueue();
	REQUIRE( counter==100*2 );
}

TEST_CASE( "1 worker; many job", "[coroutine_pool]" ) {
	counter = 0;
	ASIOLibs::CoroutinePool p(io, 1);

	for(int i=0; i<100; i++) {
		p.Schedule([&](boost::asio::yield_context &y) {
			counter++;
			io.post(y);
			counter++;
		});
	}

	p.RunQueue();
	REQUIRE( counter==100*2 );
}

TEST_CASE( "10 worker; 0 job", "[coroutine_pool]" ) {
	counter = 0;
	ASIOLibs::CoroutinePool p(io, 10);

	p.RunQueue();
	REQUIRE( counter==0 );
}

TEST_CASE( "3 worker; 11 jobs; no RunQueue", "[coroutine_pool]" ) {
	counter = 0;
	ASIOLibs::CoroutinePool p(io, 3);

	for(int i=0; i<11; i++) {
		p.Schedule([&](boost::asio::yield_context &y) {
			counter++;
			io.post(y);
			counter++;
		});
	}

	//Instead RunQueue do a timed wait
	for(int i=0; i<100 && counter!=22; i++) {
		boost::asio::deadline_timer t(io);
		t.expires_from_now( boost::posix_time::milliseconds(10) );
		//Yield this coroutine so posted work can be processed with io.run()
		t.async_wait( *yield );
	}

	REQUIRE( counter==22 );
}

TEST_CASE( "1 worker; post job from handler", "[coroutine_pool]" ) {
	counter = 0;
	ASIOLibs::CoroutinePool p(io, 1);

	p.Schedule([&](boost::asio::yield_context &y) {
		p.Schedule([&](boost::asio::yield_context &y2) {
			counter++;
			io.post(y2);
			counter++;
		});
		counter++;
		io.post(y);
		counter++;
	});

	p.RunQueue();
	REQUIRE( counter==4 );
}

TEST_CASE( "Dispatch() test", "[coroutine_pool]" ) {
	counter = 0;
	ASIOLibs::CoroutinePool p(io, 3);

	for(int i=0; i<11; i++) {
		p.Schedule([&](boost::asio::yield_context &y) {
			counter++;
			io.post(y);
			counter++;
		});
		p.Dispatch([&](boost::asio::yield_context &y) {
			counter++;
			io.post(y);
			counter++;
		});
		REQUIRE( counter == (i+1)*4 );
	}

	p.RunQueue();
	REQUIRE( counter==11*4 );
}

// A setup
int result=-1;
void run_tests(boost::asio::yield_context _yield, int argc, char* const argv[]) {
	yield = &_yield;
	result = Catch::Session().run( argc, argv );
}

int main( int argc, char* const argv[] ) {
	boost::asio::spawn( io, boost::bind(run_tests, _1, argc, argv) );
	io.run();
	return result;
}
