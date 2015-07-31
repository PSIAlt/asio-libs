#include <iostream>
#include <tuple>
#include "assert.h"
#include "iproto.hpp"

int main() {
	using namespace IProto;
	using namespace std;
	int i=0;

	auto t0 = make_tuple( uint32_t(12) );
	auto p0 = Packer(123, 0, uint32_t(12) );
	assert( p0.hdr.len == sizeof(uint32_t) ); i++;
	assert( p0.hdr.sync == 1 ); i++;
	auto u0 = Unpacker< uint32_t >(p0);
	assert( u0 == t0 ); i++;
	assert( get<0>(u0) == 12 ); i++;
	assert( p0.hdr.msg == 123 ); i++;
	p0.Reset();
	assert( p0.hdr.len == sizeof(uint32_t) ); i++;
	assert( p0.hdr.sync == 1 ); i++;

	auto t1 = make_tuple( string("test") );
	auto p1 = Packer(123, 0, string("test"));
	assert( p1.hdr.len == sizeof(uint32_t)+4 ); i++;
	assert( p1.hdr.sync == 2 ); i++;
	auto u1 = Unpacker< string >(p1);
	assert( u1 == t1 ); i++;
	p1.Reset();
	assert( p1.hdr.len == sizeof(uint32_t)+4 ); i++;
	assert( p1.hdr.sync == 2 ); i++;

	auto t2 = make_tuple( uint32_t(12), string("test") );
	auto p2 = Packer(123, 0, uint32_t(12), string("test"));
	auto u2 = Unpacker< uint32_t, string >(p2);
	assert( u2 == t2 ); i++;

	auto t3 = make_tuple( string("test"), uint32_t(12) );
	auto p3 = Packer(123, 0, string("test"), uint32_t(12) );
	auto u3 = Unpacker< string, uint32_t >(p3);
	assert( u3 == t3 ); i++;

	auto t4 = make_tuple( uint32_t(12), string("test"), uint32_t(13), string("test") );
	auto p4 = Packer(123, 0, uint32_t(12), string("test"), uint32_t(13), string("test"));
	auto u4 = Unpacker< uint32_t, string, uint32_t, string >(p4);
	assert( u4 == t4 ); i++;

	auto t5 = make_tuple( uint32_t(12) );
	auto p5 = Packer(123, 0, uint32_t(13) );
	auto u5 = Unpacker< uint32_t >(p5);
	assert( u5 != t5 ); i++;

	auto t6 = make_tuple( "test2" );
	auto t6_1 = make_tuple( "test3" );
	auto p6 = Packer(123, 0, (char*)"test2" );
	auto u6 = Unpacker< std::string >(p6);
	assert( u6 == t6 ); i++;
	assert( get<0>(u6) == get<0>(t6) ); i++;
	assert( u6 != t6_1 ); i++;
	assert( get<0>(u6) != get<0>(t6_1) ); i++;

	{
		auto p6_2 = std::move(p6);
		assert( p6_2.data != nullptr ); i++;
		assert( p6.data == nullptr ); i++;
	}

	cerr << "IProto ok, " << i << " tests done" << endl;
	return 0;
}
