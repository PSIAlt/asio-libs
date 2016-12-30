#pragma once
#ifndef _ASIO_LIBS_IPPROTO_CONN_HPP
#define _ASIO_LIBS_IPPROTO_CONN_HPP
#include <cstdio>
#include <memory>//uniqe_ptr
#include <list>
#include <utility>//forward
#include <functional>
#include <unordered_map>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/placeholders.hpp>
#include <boost/asio/streambuf.hpp>
#include "iproto.hpp"

namespace boost {
namespace asio {
class io_service;
};};

namespace IProto {

enum CB_Result {
	CB_OK=0,
	CB_ERR,
	CB_TIMEOUT
};
struct RequestResult {
	RequestResult() : code(CB_ERR) {}
	RequestResult(CB_Result _code) : code(_code) {}
	RequestResult(CB_Result _code, Packet &&_pkt) : code(_code), pkt{ new Packet(std::move(_pkt)) } {}
	CB_Result code;
	std::shared_ptr< Packet > pkt;

	RequestResult(RequestResult &&other) = default;
	RequestResult(const RequestResult &other) = default;
	RequestResult& operator=(const RequestResult &other) = default;
};

struct Conn : public std::enable_shared_from_this<Conn> {
	typedef boost::asio::ip::tcp::socket socktype;
	typedef std::function< void(RequestResult res) > callbacks_func_type;
	typedef std::unordered_map< uint32_t, std::pair< std::shared_ptr<boost::asio::deadline_timer>, callbacks_func_type> > callbacks_map_type;

	Conn(boost::asio::io_service &_io, const boost::asio::ip::tcp::endpoint &_ep,
		uint32_t _connect_timeout=1000, uint32_t _read_timeout=0); //timeouts are in milliseconds
	Conn(const Conn &) = delete;
	Conn &operator=(const Conn &) = delete;
	Conn(Conn &&) = delete;
	~Conn();

	const boost::asio::ip::tcp::endpoint &get_endpoint() { return ep; }
	boost::asio::io_service &get_io_service() { return io; }
	bool dropPacketWrite(Packet &&pkt);
	bool Write(Packet &&pkt, callbacks_func_type &&cb);
	void Shutdown();
	bool GentleShutdown();
	bool checkConnect();
	void reconnect();
	void disablePing();
	void close();

	template <class CompletionToken>
	RequestResult WriteYield(Packet &&pkt, CompletionToken &token) { //Template for yield_context
		typename boost::asio::handler_type< CompletionToken, void(RequestResult) >::type
			handler( token );

		boost::asio::async_result<decltype(handler)> result(handler);
		Write( std::forward<Packet>(pkt), handler);
		return result.get();
	}

	decltype(std::printf) *log_func;

private:
	void beginConnect();
	void setupPing(const boost::system::error_code& error);
	void pingCb(RequestResult res);
	void dismissCallbacks(CB_Result res);
	void reconnectByTimer(const boost::system::error_code& error);
	void onConnect(const boost::system::error_code& error);
	void setupReadHandler();
	void ensureWriteBuffer(const boost::system::error_code& error, const char *wr_buf = nullptr);
	void onRead(const boost::system::error_code& error, std::shared_ptr< boost::asio::streambuf > rd_buf);
	void onTimeout(const boost::system::error_code& error, uint32_t sync, std::shared_ptr< boost::asio::deadline_timer > timer);
	void invokeCallback(uint32_t sync, RequestResult &&req_res);
	callbacks_map_type::iterator invokeCallback(callbacks_map_type::iterator &it, RequestResult &&req_res);

	boost::asio::io_service &io;
	boost::asio::ip::tcp::endpoint ep;
	socktype sock;
	boost::asio::deadline_timer timer, ping_timer; //Timer for various delays etc
	uint32_t connect_timeout, read_timeout, write_queue_len;

	std::list< const char * > write_queue;
	bool write_is_active, ping_enabled;

	callbacks_map_type callbacks_map;
};

};

#endif
