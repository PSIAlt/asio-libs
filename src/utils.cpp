#include <stdexcept>
#include "utils.hpp"

namespace ASIOLibs {

std::string bin2hex(const std::string &in) {
	static const char table[] = "0123456789ABCDEF";
	std::string out;
	out.resize( in.size()*2 );

	int o=0;
	auto it = in.begin();
	for(int l=0; l<in.size(); l++) {
		out[o++] = table[ (*it>>4) & 0xf ];
		out[o++] = table[ *(it++)  & 0xf ];
	}

	return out;
}

std::string hex2bin(const std::string &in) {
	std::string out;

	if( in.size()%2 != 0 )
		throw std::runtime_error("hex2bin: invalid input");

	out.resize( in.size()/2 );

	unsigned s=4;
	for(int i=0, l=0; i<in.size(); i++) {
		char c = in[i];
		if     ( c >= '0' && c <= '9' )
			out[l] |= (c-'0') << s;
		else if( c >= 'A' && c <= 'F' )
			out[l] |= (c-'A'+10) << s;
		else if( c >= 'a' && c <= 'f' )
			out[l] |= (c-'a'+10) << s;
		else
			throw std::runtime_error("hex2bin: invalid input");
		if( s == 0 )
			l++;
		s = (s == 4 ? 0 : 4);
	}

	return out;
}

std::string string_sprintf(const char *fmt, ...) {
	size_t sz = 16;
	std::string ret;
	va_list args;
	va_start(args, fmt);
	int r = -1;
	while( r == -1 ) {
		sz *= 2;
		if( sz > 4096 )
			throw std::overflow_error("string_sprintf: Too long output");
		ret.resize(sz);
		r = vsnprintf(&ret[0], ret.size(), fmt, args);
	}
	va_end (args);
	ret.resize( r );
	return ret;
}

};
