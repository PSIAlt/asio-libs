#include <iostream>
#include <boost/shared_array.hpp>
#include "graphite.hpp"

namespace ASIOLibs {

Graphite::Graphite(boost::asio::io_service &_io, const boost::asio::ip::udp::endpoint &_ep, const char *_prefix)
		: ep(_ep), sock(_io), prefix(_prefix) {
	sock.open( boost::asio::ip::udp::v4() );
}

Graphite::~Graphite() {
}

bool Graphite::writeStat(const char *name, value_type value) {
	int buf_sz = 15/*prefix*/ + 40/*name*/ + 10/*timestamp*/ + 4/*space,rn*/ + 10/*value*/;
	boost::shared_array< char > buf( new char[buf_sz+1]  );
	int wr = snprintf(buf.get(), buf_sz, "%s.%s %li %li\r\n", prefix.c_str(), name, value, static_cast<long int>(time(NULL)) ); //TODO optimize out time()
	assert( wr>0 && wr<buf_sz );

	sock.async_send_to(boost::asio::buffer(buf.get(), wr), ep, [buf](const boost::system::error_code &err, const long unsigned int &bytes){
		//Do nothing
	});
	return true;
}

};
