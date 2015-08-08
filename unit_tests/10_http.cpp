#define CATCH_CONFIG_RUNNER
#include <iostream>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include "http_conn.hpp"
#include "catch.hpp"

using namespace std;
boost::asio::io_service io;
boost::asio::yield_context *yield;

TEST_CASE( "HTTP tests", "[main]" ) {
	boost::asio::ip::tcp::endpoint ep(boost::asio::ip::address::from_string("173.194.122.198"), 80); //google.com
	ASIOLibs::HTTP::Conn c( *yield, io, ep );
	auto r = c.GET("/");
	//cerr << r->Dump() << endl;
	REQUIRE( 1 == 1 );
	io.post( *yield );
}

int result=0;
void run_spawn(boost::asio::yield_context _yield, int argc, char* const argv[]) {
	yield = &_yield;
	result = Catch::Session().run( argc, argv );
}

int main( int argc, char* const argv[] ) {
	boost::asio::spawn( io, boost::bind(run_spawn, _1, argc, argv) );
	io.run();
	return result;
}
