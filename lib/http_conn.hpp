#pragma once
#ifndef _ASIO_LIBS_HTTP_CONN_HPP_
#define _ASIO_LIBS_HTTP_CONN_HPP_
#include <stdexcept>
#include <string>
#include <memory>
#include <map>
#include <vector>
#include <string>
#include <functional>
#include <boost/asio/spawn.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/system/error_code.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/generic/stream_protocol.hpp>
#include "coro_barrier.hpp"

namespace ASIOLibs {

struct TimingStat;
namespace HTTP {

/*
	Asyncronous HTTP client
- POST/GET methods support
- headers support
- keep-alive support
- auto-rectonnection on keep-alive drop
- streaming POST support
- streaming GET support
*/

typedef boost::asio::generic::stream_protocol::socket socket_t;
typedef boost::asio::ip::tcp::endpoint endpoint_t;

struct Conn;

struct Response {
	ssize_t ContentLength;
	size_t ReadLeft;
	int status;
	std::vector< std::pair<std::string, std::string> > headers;
	const std::string *GetHeader(const std::string &name); //return nullptr when not found
	std::string Dump() const;
	std::string drainRead() const;
	size_t BytesBuffer() const { return read_buf.size(); }
	size_t BytesLeft() const { return ReadLeft; }
	bool isDrained() const { return BytesBuffer()==0 && BytesLeft()==0; }

private:
	friend struct Conn;
	mutable boost::asio::streambuf read_buf;
};

struct Conn {
	Conn(boost::asio::yield_context &_yield, boost::asio::io_service &_io,
		endpoint_t &_ep, long _conn_timeout=1000, long _read_timeout=5000);
	~Conn();

	void checkConnect();
	void reconnect();
	void close();

	std::unique_ptr< Response > DoSimpleRequest(const char *cmd, const std::string &uri, bool full_body_read);
	// Do a request with post data drained from callback getDataCallback. getDataCallback should return true when done.
	//Proxying finishes when *buf is NULL. can_recall indicates getDataCallback can be callaed again to get data one more time.
	std::unique_ptr< Response > DoPostRequest(const char *cmd, const std::string &uri, size_t ContentLength,
		std::function< bool(const char **buf, size_t *len) > getDataCallback, bool can_recall=false);

	std::unique_ptr< Response > GET(const std::string &uri, bool full_body_read = true);
	std::unique_ptr< Response > HEAD(const std::string &uri);
	std::unique_ptr< Response > MOVE(const std::string &uri_from, const std::string &uri_to, bool allow_overwrite);

	std::unique_ptr< Response > POST(const std::string &uri, const char *postdata, size_t postlen, const char *cmd = "POST");
	std::unique_ptr< Response > PUT(const std::string &uri, const char *postdata, size_t postlen) {
		return POST(uri, postdata, postlen, "PUT"); }

	std::map< std::string, std::string > &Headers() {
		headers_cache_clear=true;
		return headers;
	}
	const std::map< std::string, std::string > &Headers() const {
		return headers;
	}

	long getConnCount() const { return conn_count; }
	long getConnTimeout() const { return conn_timeout; }
	long getReadTimeout() const { return read_timeout; }
	void setConnTimeout(long _conn_timeout) { conn_timeout=_conn_timeout; }
	void setReadTimeout(long _read_timeout) { read_timeout=_read_timeout; }

	//Preload some number of bytes into resp. Useful when you want to read start of the response and then splice() or whatever. 0 for drain all
	void PrelaodBytes( std::unique_ptr< Response > &resp, size_t count );
	//Stream data from socket by chunks. Callback should return 0 when no need more data(rest of content will be dropped unless disable_drain set).
	void StreamReadData( std::unique_ptr< Response > &resp, std::function< size_t(const char *buf, size_t len) > dataCallback, bool disable_drain=false );
	//Proxy(splice) data to another socket
	void StreamSpliceData( std::unique_ptr< Response > &resp, socket_t &dest );


	void SetTimingStat( TimingStat *_stat ) { stat = _stat; }

	//Prefer not to use this unless you want some dangerous magic
	bool WriteRequestHeaders(const char *cmd, const std::string &uri, size_t ContentLength = 0);
	bool WriteRequestData(const void *buf, size_t len);
	std::shared_ptr<ASIOLibs::CoroBarrier> WriteRequestDataBarrier(const void *buf, size_t len);

	size_t WriteTee(socket_t &sock_from, size_t max_bytes);
	size_t WriteSplice(socket_t &sock_from, size_t max_bytes);
	std::unique_ptr< Response > ReadAnswer(bool read_body=true);
	const boost::asio::ip::tcp::endpoint &endpoint() const { return ep; }

	uint32_t max_read_transfer = 2048;

private:
	void setupTimeout(long milliseconds);
	void onTimeout(const boost::system::error_code &ec, std::shared_ptr<boost::asio::deadline_timer> timer_);
	void checkTimeout(const boost::system::error_code &ec, bool throw_other_errors = true);

	void writeRequest(const char *buf, size_t sz, bool wait_read );

	void headersCacheCheck();

	boost::asio::yield_context &yield;
	endpoint_t ep;
	socket_t sock;
	std::shared_ptr<boost::asio::deadline_timer> timer;
	long conn_timeout, read_timeout, conn_count;
	bool headers_cache_clear, must_reconnect;
	TimingStat *stat;

	std::map< std::string, std::string > headers;
	std::string headers_cache;
};

struct Error : public std::runtime_error {
	enum ErrorTypes {
		T_TIMEOUT,
		T_EXCEPTION,
		T_RESPONSE
	};
	Error(const std::string &s, Conn *_conn, ErrorTypes _t)
		: std::runtime_error(s), conn(_conn), t(_t) {
			if( conn )
				ep = conn->endpoint();
		};
	ErrorTypes getType() const { return t; }
	const Conn *getConn() const { return conn; } //Do not use if Conn is already destroyed
	const boost::asio::ip::tcp::endpoint &getEndpoint() const { return ep; }
private:
	Conn *conn;
	ErrorTypes t;
	boost::asio::ip::tcp::endpoint ep;
};

};};

#endif
