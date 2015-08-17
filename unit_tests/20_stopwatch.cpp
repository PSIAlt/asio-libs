#define CATCH_CONFIG_MAIN
#include <cstdlib>
#include "catch.hpp"
#include "stopwatch.hpp"

using namespace ASIOLibs;

TEST_CASE( "StopWatch basic", "[StopWatch]" ) {
	uint64_t t;
	StopWatch w0("w0");
	usleep(1000);
	t = w0.getElapsed();
	REQUIRE( t>1000 );
	REQUIRE( t<1500 );

	StopWatch w1("w1");
	usleep(10);
	t = w1.getElapsed();
	REQUIRE( t>10 );
	REQUIRE( t<100 );
}

TEST_CASE( "TimingStat collect", "[TimingStat]" ) {
	TimingStat ts;
	uint64_t t;
	{
		StopWatch w0( std::string("w0"), &ts);
		usleep(1000);
		t = w0.getElapsed();
		REQUIRE( t>1000 );
		REQUIRE( t<1500 );
	}
	{
		std::string n = "w1";
		StopWatch w1(n, &ts);
		usleep(10);
		t = w1.getElapsed();
		REQUIRE( t>10 );
		REQUIRE( t<100 );
	}
	{
		StopWatch w3("w0", &ts);
		usleep(1000);
		t = w3.getElapsed();
		REQUIRE( t>1000 );
		REQUIRE( t<1500 );
	}

	StopWatch w4("w2", &ts);
	usleep(1000);
	t = w4.FlushStat();

	REQUIRE( ts.Stat().at("w0") > 2000 );
	REQUIRE( ts.Stat().at("w0") < 3000 );
	REQUIRE( ts.Stat().at("w1") > 10 );
	REQUIRE( ts.Stat().at("w1") < 100 );
	REQUIRE( ts.Stat().at("w2") > 1000 );
	REQUIRE( ts.Stat().at("w2") < 2000 );
}

