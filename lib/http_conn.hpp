#pragma once
#ifndef _ASIO_LIBS_HTTP_CONN_HPP_
#define _ASIO_LIBS_HTTP_CONN_HPP_
#include <stdexcept>
#include <string>
#include <memory>
#include <map>
#include <string>
#include <functional>
#include <boost/asio/spawn.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <boost/system/error_code.hpp>

namespace ASIOLibs {
namespace HTTP {

struct Timeout : public std::runtime_error {
	Timeout(const std::string &s) : std::runtime_error(s) {}; 
};

struct Response {
	boost::asio::streambuf read_buf;
	ssize_t ContentLength;
	int status;
	std::map< std::string, std::string > headers;
	std::string Dump() const;
};

struct Conn {
	Conn(boost::asio::yield_context &_yield, boost::asio::io_service &_io,
		boost::asio::ip::tcp::endpoint &_ep, long _conn_timeout=1000, long _read_timeout=5000);

	void checkConnect();
	void reconnect();
	void close();

	std::unique_ptr< Response > DoSimpleRequest(const char *cmd, const std::string &uri);
	std::unique_ptr< Response > DoPostRequest(const char *cmd, const std::string &uri, size_t ContentLength,
		std::function< bool(const char **buf, size_t *len) > getDataCallback, bool can_recall=false);

	std::unique_ptr< Response > GET(const std::string &uri);
	std::unique_ptr< Response > HEAD(const std::string &uri);
	std::unique_ptr< Response > MOVE(const std::string &uri_from, const std::string &uri_to, bool allow_overwrite = true);

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

private:
	void setupTimeout(long milliseconds);
	void onTimeout(const boost::system::error_code &ec);
	void checkTimeout();

	void writeRequest(const char *buf, size_t sz, bool wait_read );
	std::unique_ptr< Response > ReadAnswer(bool read_body);

	void headersCacheCheck();

	boost::asio::yield_context &yield;
	boost::asio::ip::tcp::endpoint ep;
	boost::asio::ip::tcp::socket sock;
	boost::asio::deadline_timer timer;
	long conn_timeout, read_timeout, conn_count;
	bool is_timeout, headers_cache_clear, must_reconnect;

	std::map< std::string, std::string > headers;
	std::string headers_cache;
};

};};

#endif
