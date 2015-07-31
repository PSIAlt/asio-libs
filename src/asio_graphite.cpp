#include <iostream>
#include <boost/shared_array.hpp>
#include "asio_graphite.hpp"
#define DEBUG(x) //std::cerr << x << std::endl;

namespace CPPTools {

ASIO_Graphite::ASIO_Graphite(boost::asio::io_service &_io, const boost::asio::ip::udp::endpoint &_ep, const char *_prefix)
		: ep(_ep), sock(_io), prefix(_prefix) {
	sock.open( boost::asio::ip::udp::v4() );
}

ASIO_Graphite::~ASIO_Graphite() {
	DEBUG("asio_graphite destructor");
}

bool ASIO_Graphite::writeStat(const char *name, value_type value) {
	DEBUG("asio_graphite writeStat");

	size_t buf_sz = 15/*prefix*/ + 40/*name*/ + 10/*timestamp*/ + 4/*space,rn*/ + 10/*value*/;
	boost::shared_array< char > buf( new char[buf_sz]  );
	int wr = snprintf(buf.get(), buf_sz, "%s.%s %li %li\r\n", prefix, name, value, static_cast<long int>(time(NULL)) ); //TODO optimize out time()
	assert( wr>0 );

	sock.async_send_to(boost::asio::buffer(buf.get(), wr), ep, [buf](const boost::system::error_code &err, const long unsigned int &bytes){
		//Do nothing
		DEBUG("asio_graphite wrote " << bytes << " bytes ec=" << err);
	});
	return true;
}

};
