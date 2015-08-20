#include <stdexcept>
#include <cstdarg>
#include "perf.hpp"
#include "utils.hpp"

namespace ASIOLibs {

std::string bin2hex(const std::string &in) {
	static const char table[] = "0123456789ABCDEF";
	std::string out;
	out.resize( in.size()*2 );

	int o=0;
	auto it = in.begin();
	for(size_t l=0; l<in.size(); l++) {
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
	for(size_t i=0, l=0; i<in.size(); i++) {
		char c = in[i];
		if     ( likely(c >= '0' && c <= '9') )
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
	size_t sz = 128;
	std::string ret(sz, 0);
	va_list args, args_copy;
	va_start(args, fmt);
	while( 1 ) {
		sz *= 2;
		if( unlikely(sz > 4096) ) {
			va_end(args);
			throw std::overflow_error("string_sprintf: Too long output");
		}
		ret.resize(sz);
		va_copy(args_copy, args);
		int r = vsnprintf(&ret[0], ret.size(), fmt, args_copy);
		va_end(args_copy);
		if( r>=0 && static_cast<unsigned>(r)<ret.size() ) {
			ret.resize( r );
			break;
		}
	}
	va_end (args);
	return ret;
}

};
