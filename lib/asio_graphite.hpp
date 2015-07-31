#pragma once
#ifndef _ASIO_GRAPHITE_HPP
#define _ASIO_GRAPHITE_HPP
#include <string>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/noncopyable.hpp>

namespace CPPTools {

class ASIO_Graphite :
	public boost::noncopyable {
public:
	typedef long value_type;

	ASIO_Graphite(boost::asio::io_service &_io, const boost::asio::ip::udp::endpoint &_ep, const char *prefix);
	~ASIO_Graphite();

	bool writeStat(const char *name, value_type value=1);

protected:
	boost::asio::ip::udp::endpoint ep;
	boost::asio::ip::udp::socket sock;
	const char *prefix;
};

};


#endif
