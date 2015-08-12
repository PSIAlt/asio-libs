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
boost::asio::ip::tcp::endpoint ep;

TEST_CASE( "HTTP GET tests", "[get]" ) {
	ASIOLibs::HTTP::Conn c( *yield, io, ep );
	c.Headers()["Host"] = "forsakens.ru";
	auto r0 = c.GET("/");
	REQUIRE( r0->status == 200 );
	REQUIRE( r0->ContentLength > 0 );
	REQUIRE( c.getConnCount() == 1 );
	auto r1 = c.GET("/test/503");
	REQUIRE( r1->status == 503 );
	REQUIRE( r1->ContentLength > 0 );
	REQUIRE( c.getConnCount() == 1 );
	auto r2 = c.GET("/test/400");
	REQUIRE( r2->status == 400 );
	REQUIRE( r2->ContentLength > 0 );
	REQUIRE( c.getConnCount() == 1 );
	auto r3 = c.GET("/Pg-hstore-1.01.tar.gz");
	REQUIRE( r3->status == 200 );
	REQUIRE( r3->ContentLength == 123048 );
	REQUIRE( c.getConnCount() == 2 ); //Keepalive drop after code 400
	auto r4 = c.GET("/");
	REQUIRE( r4->status == 200 );
	REQUIRE( c.getConnCount() == 2 );
}
TEST_CASE( "HTTP HEAD tests", "[head]" ) {
	ASIOLibs::HTTP::Conn c( *yield, io, ep );
	c.Headers()["Host"] = "forsakens.ru";
	auto r0 = c.HEAD("/");
	REQUIRE( r0->status == 200 );
	REQUIRE( r0->ContentLength > 0 );
	REQUIRE( c.getConnCount() == 1 );
	c.close();
	auto r1 = c.HEAD("/test/503");
	REQUIRE( r1->status == 503 );
	REQUIRE( r1->ContentLength > 0 );
	REQUIRE( c.getConnCount() == 2 );
	c.close();
	auto r2 = c.HEAD("/test/400");
	REQUIRE( r2->status == 400 );
	REQUIRE( r2->ContentLength > 0 );
	REQUIRE( c.getConnCount() == 3 );
	c.close();
	auto r3 = c.HEAD("/Pg-hstore-1.01.tar.gz");
	REQUIRE( r3->status == 200 );
	REQUIRE( r3->ContentLength == 123048 );
	REQUIRE( c.getConnCount() == 4 );
	c.close();
}
TEST_CASE( "HTTP GET stream tests", "[get]" ) {
	ASIOLibs::HTTP::Conn c( *yield, io, ep );
	c.Headers()["Host"] = "forsakens.ru";

	//Request streaming
	auto r0 = c.GET("/Pg-hstore-1.01.tar.gz", false);
	REQUIRE( r0->status == 200 );
	REQUIRE( r0->ContentLength == 123048 );
	REQUIRE( c.getConnCount() == 1 );
	size_t cb_read=0;
	std::string cb_read_str;
	c.StreamReadData(r0, [&cb_read_str, &cb_read](const char *buf, size_t len) -> size_t {
		cb_read += len;
		cb_read_str += std::string(buf, len);
		return len;
	});
	REQUIRE( cb_read == 123048 );

	//Request normal
	auto r1 = c.GET("/Pg-hstore-1.01.tar.gz");
	REQUIRE( r1->status == 200 );
	REQUIRE( r1->ContentLength == 123048 );
	REQUIRE( c.getConnCount() == 1 );
	std::string drain_read_str = r1->drainRead();

	//Compare output
	REQUIRE( cb_read_str == drain_read_str );

	//Check partial drain
	auto r2 = c.GET("/Pg-hstore-1.01.tar.gz", false);
	REQUIRE( r2->status == 200 );
	REQUIRE( r2->ContentLength == 123048 );
	REQUIRE( c.getConnCount() == 1 );
	size_t r2_cb_read=0;
	std::string r2_cb_read_str;
	c.StreamReadData(r2, [&r2_cb_read_str, &r2_cb_read](const char *buf, size_t len) -> size_t {
		if( r2_cb_read>0 )
			return 0; /* "gimme back control flow" */
		r2_cb_read += len;
		r2_cb_read_str += std::string(buf, len);
		return len;
	}, true/*disable full content drain*/);
	REQUIRE( r2_cb_read > 0 );
	REQUIRE( r2_cb_read < 123048 ); //This is faulty assertion (depends on how nginx will answer)
	c.StreamReadData(r2, [&r2_cb_read_str, &r2_cb_read](const char *buf, size_t len) -> size_t { //Continue read
		r2_cb_read += len;
		r2_cb_read_str += std::string(buf, len);
		return len;/*continue until end*/
	});
	REQUIRE( r2_cb_read == 123048 );
	REQUIRE( r2_cb_read_str == drain_read_str );

	//PreloadBytes
	auto r3 = c.GET("/Pg-hstore-1.01.tar.gz", false);
	REQUIRE( r3->status == 200 );
	REQUIRE( r3->ContentLength == 123048 );
	REQUIRE( c.getConnCount() == 1 );
	REQUIRE( r3->BytesBuffer() < 123048 );
	REQUIRE( r3->ContentLength == (r3->BytesBuffer() + r3->BytesLeft()) );
	c.PrelaodBytes(r3, 12345); //preload 12345 bytes from socket
	REQUIRE( r3->BytesBuffer() == 12345 );
	REQUIRE( r3->BytesLeft() == (123048 - r3->BytesBuffer()) );
	c.PrelaodBytes(r3, 0); //preload all
	REQUIRE( r3->BytesBuffer() == 123048 );
	std::string r3_drain_read_str = r3->drainRead();
	REQUIRE( r3_drain_read_str == drain_read_str );

	//Check connection is not screwed
	auto r99 = c.GET("/Pg-hstore-1.01.tar.gz");
	REQUIRE( r99->status == 200 );
	REQUIRE( r99->ContentLength == 123048 );
	REQUIRE( c.getConnCount() == 1 );
}
TEST_CASE( "HTTP GET timeouts tests", "[get]" ) {
	{
		ASIOLibs::HTTP::Conn c( *yield, io, ep, 1, 100000 ); //Should be connect timeout
		c.Headers()["Host"] = "forsakens.ru";
		bool was_timeout = false;
		try {
			auto r0 = c.GET("/Pg-hstore-1.01.tar.gz");
		}catch(ASIOLibs::HTTP::Timeout &e) {
			was_timeout = true;
		}
		REQUIRE( was_timeout == true );
	}
	{
		ASIOLibs::HTTP::Conn c( *yield, io, ep, 100000, 1 ); //Should be read timeout
		c.Headers()["Host"] = "forsakens.ru";
		bool was_timeout = false;
		try {
			auto r0 = c.GET("/Pg-hstore-1.01.tar.gz");
		}catch(ASIOLibs::HTTP::Timeout &e) {
			was_timeout = true;
		}
		REQUIRE( was_timeout == true );
	}
}
TEST_CASE( "HTTP lowlevel api", "[get]" ) {
	ASIOLibs::HTTP::Conn c( *yield, io, ep ); //Should be connect timeout
	c.Headers()["Host"] = "forsakens.ru";
	c.WriteRequestHeaders("GET", "/test/503");
	auto r0 = c.ReadAnswer();
	REQUIRE( r0->status == 503 );
	REQUIRE( r0->ReadLeft == 0 );

	c.WriteRequestHeaders("GET", "/");
	auto r1 = c.ReadAnswer();
	REQUIRE( r1->status == 200 );
	REQUIRE( r1->ContentLength > 0 );
	REQUIRE( r1->ReadLeft == 0 );

	c.WriteRequestHeaders("GET", "/");
	auto r2 = c.ReadAnswer();
	REQUIRE( r2->status == 200 );
	REQUIRE( r2->ContentLength > 0 );
	REQUIRE( r2->ReadLeft == 0 );

	c.WriteRequestHeaders("GET", "/Pg-hstore-1.01.tar.gz");
	auto r3 = c.ReadAnswer(false);
	REQUIRE( r3->status == 200 );
	REQUIRE( r3->ContentLength == 123048 );
	REQUIRE( r3->ReadLeft > 0 );
	c.PrelaodBytes(r3, 0);
	REQUIRE( r3->ReadLeft == 0 );
}

int result=0;
void run_spawn(boost::asio::yield_context _yield, int argc, char* const argv[]) {
	yield = &_yield;
	result = Catch::Session().run( argc, argv );
}

int main( int argc, char* const argv[] ) {
	boost::asio::ip::tcp::resolver resolver(io);
	boost::asio::ip::tcp::resolver::query query("forsakens.ru", "80");
	boost::asio::ip::tcp::resolver::iterator iter = resolver.resolve(query);
	ep = *iter;
	boost::asio::spawn( io, boost::bind(run_spawn, _1, argc, argv) );
	io.run();
	return result;
}
