#include <iostream>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include "utils.hpp"
#include "http_conn.hpp"

#define DEBUG(x) //std::cerr << x << std::endl;

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
		: yield(_yield), ep(_ep), sock(_io), timer(_io), conn_timeout(_conn_timeout),
			read_timeout(_read_timeout), conn_count(0), is_timeout(false), headers_cache_clear(true), must_reconnect(true) {
	headers["User-Agent"] = "ASIOLibs " ASIOLIBS_VERSION;
	headers["Connection"] = "Keep-Alive";
}

void Conn::checkConnect() {
	if( !sock.is_open() || must_reconnect ) {
		if( sock.is_open() ) sock.close();
		setupTimeout( conn_timeout );
		sock.async_connect(ep, yield);
		checkTimeout();
		conn_count++;
		must_reconnect=false;
	}
}
void Conn::reconnect() {
	if( sock.is_open() ) sock.close();
	checkConnect();
}
void Conn::close() {
	if( sock.is_open() ) sock.close();
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

std::unique_ptr< Response > Conn::DoSimpleRequest(const char *cmd, const std::string &uri) {
	headersCacheCheck();
	std::string req = ASIOLibs::string_sprintf("%s %s HTTP/1.1\n%s", cmd, uri.c_str(), headers_cache.c_str());
	writeRequest( req.data(), req.size(), true );
	return ReadAnswer(true);
}

std::unique_ptr< Response > Conn::GET(const std::string &uri) {
	return DoSimpleRequest("GET", uri);
}
std::unique_ptr< Response > Conn::HEAD(const std::string &uri) {
	return DoSimpleRequest("HEAD", uri);
}
std::unique_ptr< Response > Conn::MOVE(const std::string &uri_from, const std::string &uri_to, bool allow_overwrite) {
	headersCacheCheck();
	std::string req = ASIOLibs::string_sprintf("MOVE %s HTTP/1.1\nDestination: %s\nOverwrite: %s\n%s",
		uri_from.c_str(), uri_to.c_str(), (allow_overwrite ? "F" : "T"), headers_cache.c_str());
	writeRequest( req.data(), req.size(), true );
	return ReadAnswer(true);
}

std::unique_ptr< Response > Conn::DoPostRequest(const char *cmd, const std::string &uri, size_t ContentLength,
		std::function< bool(const char **buf, size_t *len) > getDataCallback, bool can_recall) {
	headersCacheCheck();
	std::string req = ASIOLibs::string_sprintf("%s %s HTTP/1.1\nContent-Length: %zu\n%s", cmd, uri.c_str(), ContentLength, headers_cache.c_str());

	int try_count = 0;
	boost::system::error_code error_code;
	while( try_count++ < max_send_try ) {
		checkConnect();
		writeRequest( req.data(), req.size(), false );
		bool is_done = false;
		while( !is_done ) {
			const char *postdata;
			size_t postlen;
			is_done = getDataCallback(&postdata, &postlen);
			if( postdata == NULL )
				ReadAnswer(true);
			boost::asio::async_write(sock, boost::asio::buffer(postdata, postlen), yield[error_code]);
			if( !can_recall && error_code )
				throw boost::system::system_error(error_code);
			if( error_code )
				break;
		}
		if( error_code == boost::asio::error::operation_aborted )
			throw boost::system::system_error(error_code);
		if( !error_code )
			return ReadAnswer(true);
		if( sock.is_open() ) //Dunno wtf happend, just reconnect
			reconnect();
	}
	throw boost::system::system_error(error_code);
}

std::unique_ptr< Response > Conn::POST(const std::string &uri, const char *postdata, size_t postlen, const char *cmd) {
	auto cb = [&postdata, &postlen](const char **buf, size_t *len) {
		*buf = postdata;
		*len = postlen;
		return true;
	};
	return DoPostRequest(cmd, uri, postlen, cb, true);
}

void Conn::writeRequest(const char *buf, size_t sz, bool wait_read) {
	int try_count = 0;
	boost::system::error_code error_code;
	DEBUG( "writeRequest " << std::string(buf, sz) );
	while( try_count++ < max_send_try ) {
		checkConnect();
		boost::asio::async_write(sock, boost::asio::buffer(buf, sz), yield[error_code]);
		if( !error_code ) {
			if( wait_read ) {
				setupTimeout( read_timeout );
				boost::asio::async_read(sock, boost::asio::null_buffers(), yield[error_code]);
				checkTimeout();
				if( error_code == boost::asio::error::operation_aborted )
					throw boost::system::system_error(error_code);
			}
			if( !error_code )
				return;
		}
		if( sock.is_open() ) //Dunno wtf happend, just reconnect
			reconnect();
	}
	throw boost::system::system_error(error_code);
}

std::unique_ptr< Response > Conn::ReadAnswer(bool read_body) {
	static const char hdr_end_pattern[] = "\r\n\r\n";
	std::unique_ptr< Response > ret( new Response() );
	ret->ContentLength = -1;

	setupTimeout( read_timeout );
	boost::asio::async_read_until(sock, ret->read_buf, std::string(hdr_end_pattern), yield);
	checkTimeout();

	const char *data = boost::asio::buffer_cast<const char *>( ret->read_buf.data() );
	size_t sz = ret->read_buf.size();
	DEBUG("ReadAnswer: " << std::string(data, sz));
	const char *hdr_end = (const char *)memmem(data, sz, hdr_end_pattern, sizeof(hdr_end_pattern)-1);
	if( !hdr_end )
		throw std::runtime_error("Cant find headers end");

	//Parse respnse status
	struct phr_header headers[100];
	size_t num_headers = sizeof(headers) / sizeof(headers[0]);
	int minor_version;
	const char *msg;
	size_t msg_len;
	int pret = phr_parse_response(data, hdr_end-data+sizeof(hdr_end_pattern)-1, &minor_version, &ret->status, &msg, &msg_len, headers, &num_headers, 0);

	if( pret != -2 && pret <= 0 )
		throw std::runtime_error("Cant parse http response");

	bool l_must_reconnect = true;
	for(int i=0; i < num_headers; i++) {
		//mllog::debug("Response header '%.*s' = '%.*s'", headers[i].name_len, headers[i].name, headers[i].value_len, headers[i].value);
		if( !strncmp(headers[i].name, "Content-Length", headers[i].name_len) ) {
			ret->ContentLength = strtol(headers[i].value, NULL, 0);
		}else if( !strncmp(headers[i].name, "Connection", headers[i].name_len) && !strncasecmp(headers[i].value, "Keep-Alive", headers[i].value_len) ) {
			l_must_reconnect = false;
		}
		ret->headers.emplace( std::make_pair(std::string(headers[i].name, headers[i].name_len), std::string(headers[i].value, headers[i].value_len)) );
	}
	must_reconnect = must_reconnect || l_must_reconnect;

	return ret;
}

std::string Response::Dump() const {
	ASIOLibs::StrFormatter s;
	s << "HTTP status=" << status << "; ContentLength=" << ContentLength << "\n";
	return s.str();
}

};};
