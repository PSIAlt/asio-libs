#include <stdexcept>
#include <utility> //std::move
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

	return std::move(out);
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

	return std::move(out);
}

};
