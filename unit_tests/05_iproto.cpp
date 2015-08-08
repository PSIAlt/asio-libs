#define CATCH_CONFIG_MAIN
#include <iostream>
#include <tuple>
#include "iproto.hpp"
#include "catch.hpp"

TEST_CASE( "IProto tests", "[main]" ) {
	using namespace IProto;
	using namespace std;
	auto t0 = make_tuple( uint32_t(12) );
	auto p0 = Packer(123, 0, uint32_t(12) );
	REQUIRE( p0.hdr.len == sizeof(uint32_t) );
	REQUIRE( p0.hdr.sync == 1 );
	auto u0 = Unpacker< uint32_t >(p0);
	REQUIRE( u0 == t0 );
	REQUIRE( get<0>(u0) == 12 );
	REQUIRE( p0.hdr.msg == 123 );
	p0.Reset();
	REQUIRE( p0.hdr.len == sizeof(uint32_t) );
	REQUIRE( p0.hdr.sync == 1 );

	auto t1 = make_tuple( string("test") );
	auto p1 = Packer(123, 0, string("test"));
	REQUIRE( p1.hdr.len == sizeof(uint32_t)+4 );
	REQUIRE( p1.hdr.sync == 2 );
	auto u1 = Unpacker< string >(p1);
	REQUIRE( u1 == t1 );
	p1.Reset();
	REQUIRE( p1.hdr.len == sizeof(uint32_t)+4 );
	REQUIRE( p1.hdr.sync == 2 );

	auto t2 = make_tuple( uint32_t(12), string("test") );
	auto p2 = Packer(123, 0, uint32_t(12), string("test"));
	auto u2 = Unpacker< uint32_t, string >(p2);
	REQUIRE( u2 == t2 );

	auto t3 = make_tuple( string("test"), uint32_t(12) );
	auto p3 = Packer(123, 0, string("test"), uint32_t(12) );
	auto u3 = Unpacker< string, uint32_t >(p3);
	REQUIRE( u3 == t3 );

	auto t4 = make_tuple( uint32_t(12), string("test"), uint32_t(13), string("test") );
	auto p4 = Packer(123, 0, uint32_t(12), string("test"), uint32_t(13), string("test"));
	auto u4 = Unpacker< uint32_t, string, uint32_t, string >(p4);
	REQUIRE( u4 == t4 );

	auto t5 = make_tuple( uint32_t(12) );
	auto p5 = Packer(123, 0, uint32_t(13) );
	auto u5 = Unpacker< uint32_t >(p5);
	REQUIRE( u5 != t5 );

	auto t6 = make_tuple( "test2" );
	auto t6_1 = make_tuple( "test3" );
	auto p6 = Packer(123, 0, (char*)"test2" );
	auto u6 = Unpacker< std::string >(p6);
	REQUIRE( u6 == t6 );
	REQUIRE( get<0>(u6) == get<0>(t6) );
	REQUIRE( u6 != t6_1 );
	REQUIRE( get<0>(u6) != get<0>(t6_1) );

	{
		auto p6_2 = std::move(p6);
		REQUIRE( p6_2.data != nullptr );
		REQUIRE( p6.data == nullptr );
	}

	uint32_t simplebuf = 0x1234;
	auto p7 = Packer(123, 0, ByteBuffer( &simplebuf, sizeof(simplebuf)) );
	auto u7 = Unpacker< ByteBuffer >(p7);
	REQUIRE( memcmp(&simplebuf, get<0>(u7).buf, sizeof(simplebuf)) == 0 );
}
