#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include "utils.hpp"
#include "http_conn.hpp"

namespace ASIOLibs {
namespace HTTP {

#pragma GCC visibility push(hidden)
extern "C" {
#include "picohttpparser.inc"
};
#pragma GCC visibility pop

enum {
	max_send_try = 5
};

Conn::Conn(boost::asio::yield_context &_yield, boost::asio::io_service &_io,
		boost::asio::ip::tcp::endpoint &_ep, long _conn_timeout, long _read_timeout)
		: yield(_yield), io(_io), ep(_ep), sock(_io), timer(_io), conn_timeout(_conn_timeout),
			read_timeout(_read_timeout), is_timeout(false), headers_cache_clear(true) {
	headers["User-Agent"] = "ASIOLibs " ASIOLIBS_VERSION;
	headers["Connection"] = "Keep-Alive";
}

void Conn::checkConnect() {
	if( !sock.is_open() ) {
		setupTimeout( conn_timeout );
		sock.async_connect(ep, yield);
		checkTimeout();
	}
}
void Conn::reconnect() {
	if( sock.is_open() ) sock.close();
	checkConnect();
}

void Conn::setupTimeout(long milliseconds) {
	is_timeout=false;
	timer.expires_from_now( boost::posix_time::milliseconds( milliseconds ) );
	timer.async_wait( boost::bind(&Conn::onTimeout, this, boost::asio::placeholders::error) );
}
void Conn::onTimeout(const boost::system::error_code &ec) {
	if( !ec && timer.expires_from_now() <= boost::posix_time::seconds(0) ) {
		if( sock.is_open() ) sock.close();
		is_timeout=true;
	}
}
void Conn::checkTimeout() {
	timer.cancel();
	if( is_timeout )
		throw Timeout( "ASIOLibs::HTTP::Conn: Timeout while requesting " + ep.address().to_string() );
}

void Conn::headersCacheCheck() {
	if( headers_cache_clear ) {
		headers_cache.clear();
		headers_cache_clear=false;
		for( const auto &i : headers ) {
			headers_cache += i.first;
			headers_cache += ": ";
			headers_cache += i.second;
			headers_cache += "\n";
		}
		headers_cache += "\n";
	}
}

std::unique_ptr< Response > Conn::GET(const std::string &uri) {
	headersCacheCheck();
	std::string req = ASIOLibs::string_sprintf("GET %s HTTP/1.1\n%s", uri.c_str(), headers_cache.c_str());
	writeRequest( req.data(), req.size(), true );
	return ReadAnswer(true);
}

void Conn::writeRequest(const char *buf, size_t sz, bool wait_read) {
	int try_count = 0;
	boost::system::error_code error_code;
	while( try_count++ < max_send_try ) {
		checkConnect();
		boost::asio::async_write(sock, boost::asio::buffer(buf, sz), yield[error_code]);
		if( !error_code ) {
			if( wait_read ) {
				setupTimeout( read_timeout );
				boost::asio::async_read(sock, boost::asio::null_buffers(), yield[error_code]);
				checkTimeout();
			}
			if( !error_code )
				return;
		}
		if( sock.is_open() ) //Dunno wtf happend, just reconnect
			reconnect();
	}
	throw boost::system::system_error(error_code, "Cannot write to socket");
}

std::unique_ptr< Response > Conn::ReadAnswer(bool read_body) {
	static const char hdr_end_pattern[] = "\r\n\r\n";
	static const char hdr_status_end_pattern[] = "\r\n";
	std::unique_ptr< Response > ret( new Response() );
	ret->ContentLength = -1;

	setupTimeout( read_timeout );
	boost::asio::async_read_until(sock, ret->read_buf, std::string(hdr_end_pattern), yield);
	checkTimeout();

	const char *data = boost::asio::buffer_cast<const char *>( ret->read_buf.data() );
	size_t sz = ret->read_buf.size();
	const char *hdr_end = (const char *)memmem(data, sz, hdr_status_end_pattern, sizeof(hdr_status_end_pattern)-1);
	if( !hdr_end )
		throw std::runtime_error("Cant find headers end");

	//Parse respnse status
	struct phr_header headers[100];
	size_t num_headers = sizeof(headers) / sizeof(headers[0]);
	int minor_version;
	const char *msg;
	size_t msg_len;
	int pret = phr_parse_response(data, hdr_end-data+sizeof(hdr_status_end_pattern)-1, &minor_version, &ret->status, &msg, &msg_len, headers, &num_headers, 0);

	if( pret != -2 && pret <= 0 )
		throw std::runtime_error("Cant parse http status");

	return ret;
}

};};
