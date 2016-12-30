#include <iostream>
#include <algorithm>
#include <stdexcept>
#include <system_error>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <fcntl.h>
#include "perf.hpp"
#include "utils.hpp"
#include "http_conn.hpp"
#include "stopwatch.hpp"

#define DEBUG(x) //std::cerr << x << std::endl;

namespace ASIOLibs {
namespace HTTP {

extern "C" {
#include "picohttpparser.h"
};

#define IS_TIMER_TIMEOUT(t) (t->expires_from_now() <= boost::posix_time::milliseconds(0))

#define TIMEOUT_START(x) \
	boost::system::error_code error_code; \
	setupTimeout( x ); \
	ASIOLibs::ScopeGuard _timer_guard( [this]{ \
		setupTimeout(0); \
	});

#define TIMEOUT_END() checkTimeout(error_code);
#define TIMEOUT_END_NOTHROW() checkTimeout(error_code, false);

#define TIMING_STAT_START(name) \
	{ \
	std::unique_ptr<StopWatch> sw; \
	if( stat ) \
		sw.reset( new StopWatch(name, stat) );

#define TIMING_STAT_END() \
	}

enum {
	max_send_try = 5
};

static inline void cork_set(int fd) {
#ifdef TCP_CORK
	int state = 1;
	setsockopt(fd, IPPROTO_TCP, TCP_CORK, &state, sizeof(state));
#elif defined(TCP_NOPUSH)
	int state = 1;
	setsockopt(fd, IPPROTO_TCP, TCP_NOPUSH, &state, sizeof(state));
#else
#endif
}
static inline void cork_clear(int fd) {
#ifdef TCP_CORK
	int state = 0;
	setsockopt(fd, IPPROTO_TCP, TCP_CORK, &state, sizeof(state));
#elif defined(TCP_NOPUSH)
	int state = 0;
	setsockopt(fd, IPPROTO_TCP, TCP_NOPUSH, &state, sizeof(state));
#else
#endif
}

Conn::Conn(boost::asio::yield_context &_yield, boost::asio::io_service &_io,
		endpoint_t &_ep, long _conn_timeout, long _read_timeout)
		: yield(_yield), ep(_ep), sock(_io), timer( std::make_shared<boost::asio::deadline_timer>(_io) ), conn_timeout(_conn_timeout),
			read_timeout(_read_timeout), conn_count(0), headers_cache_clear(true),
			must_reconnect(true), stat(nullptr) {
	headers["User-Agent"] = "ASIOLibs " ASIOLIBS_VERSION;
	headers["Connection"] = "Keep-Alive";
}

Conn::~Conn() {
	setupTimeout(0);
}

void Conn::checkConnect() {
	if( unlikely(!sock.is_open() || must_reconnect) ) {
		close();
		try {
			TIMEOUT_START( conn_timeout );
			TIMING_STAT_START("http_connect");
			sock.async_connect(ep, yield[error_code]);
			TIMING_STAT_END();
			TIMEOUT_END();
		}catch( Error &e ) {
			if( e.getType() == Error::ErrorTypes::T_TIMEOUT )
				throw Error( "ASIOLibs::HTTP::Conn: Connect timeout", this, Error::ErrorTypes::T_TIMEOUT );
			throw;
		}
		//sock.set_option( boost::asio::ip::tcp::no_delay(true) );
		conn_count++;
		must_reconnect=false;
	}
}
void Conn::reconnect() {
	close();
	checkConnect();
}
void Conn::close() {
	boost::system::error_code ec = boost::asio::error::interrupted;
	for(int i=0; i<3 && sock.is_open() && ec == boost::asio::error::interrupted; ++i)
		sock.close(ec);
}
void Conn::setupTimeout(long milliseconds) {
	if( milliseconds==0 ) {
		// set big time to prevent IS_TIMER_TIMEOUT from give us true value on handler races
		timer->expires_from_now( boost::posix_time::seconds(120) );
		timer->cancel();
	}else{
		timer->expires_from_now( boost::posix_time::milliseconds( milliseconds ) );
		timer->async_wait( boost::bind(&Conn::onTimeout, this, boost::asio::placeholders::error, timer) );
	}
}
void Conn::onTimeout(const boost::system::error_code &ec, std::shared_ptr<boost::asio::deadline_timer> timer_) {
	/* Check ec for error & check expires_from_now() with IS_TIMER_TIMEOUT macro
	to ensure its expired, not canceled: there is possible RC when timeout was queued into asio,
	and then Conn destroyed for different reason. This is possible UB because timer already destroyed(and Conn too),
	but handler with good error is alive. For this cases each cancel() and ~Conn do call setupTimeout(0)
	to make IS_TIMER_TIMEOUT detect forced cancel
	*/
	if( likely(!ec && IS_TIMER_TIMEOUT(timer_)) )
		close();
}
void Conn::checkTimeout(const boost::system::error_code &ec, bool throw_other_errors) {
	if( unlikely(ec) ) {
		if( !IS_TIMER_TIMEOUT(timer) && ec != boost::asio::error::operation_aborted ) {
			if( throw_other_errors )
				throw Error(boost::system::system_error(ec).what(), this, Error::ErrorTypes::T_EXCEPTION);
			else {
				setupTimeout(0);
				return;
			}
		}
		close();
		throw Error( "ASIOLibs::HTTP::Conn: Timeout", this, Error::ErrorTypes::T_TIMEOUT );
	}
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

std::unique_ptr< Response > Conn::DoSimpleRequest(const char *cmd, const std::string &uri, bool full_body_read) {
	headersCacheCheck();
	std::string req = ASIOLibs::string_sprintf("%s %s HTTP/1.1\n%s", cmd, uri.c_str(), headers_cache.c_str());
	writeRequest( req.data(), req.size(), true );
	return ReadAnswer(full_body_read);
}

std::unique_ptr< Response > Conn::GET(const std::string &uri, bool full_body_read) {
	return DoSimpleRequest("GET", uri, full_body_read);
}
std::unique_ptr< Response > Conn::HEAD(const std::string &uri) {
	auto ret = DoSimpleRequest("HEAD", uri, false);
	ret->ReadLeft = 0; //Body wont follow
	return ret;
}
std::unique_ptr< Response > Conn::MOVE(const std::string &uri_from, const std::string &uri_to, bool allow_overwrite) {
	headersCacheCheck();
	std::string req = ASIOLibs::string_sprintf("MOVE %s HTTP/1.1\nDestination: %s\nOverwrite: %s\n%s",
		uri_from.c_str(), uri_to.c_str(), (allow_overwrite ? "T" : "F"), headers_cache.c_str());
	writeRequest( req.data(), req.size(), true );
	return ReadAnswer(true);
}

std::unique_ptr< Response > Conn::DoPostRequest(const char *cmd, const std::string &uri, size_t ContentLength,
		std::function< bool(const char **buf, size_t *len) > getDataCallback, bool can_recall) {
	headersCacheCheck();
	std::string req = ASIOLibs::string_sprintf("%s %s HTTP/1.1\nContent-Length: %zu\n%s", cmd, uri.c_str(), ContentLength, headers_cache.c_str());

	int try_count = 0;
	boost::system::error_code error_code;

	{
		cork_set( sock.native_handle() );
		ASIOLibs::ScopeGuard _cork_sg([&]() {
			cork_clear( sock.native_handle() );
		});
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
				setupTimeout( read_timeout );
				TIMING_STAT_START("http_write");
				boost::asio::async_write(sock, boost::asio::buffer(postdata, postlen), yield[error_code]);
				TIMING_STAT_END();
				TIMEOUT_END_NOTHROW();
				if( unlikely(!can_recall && error_code) )
					throw Error(boost::system::system_error(error_code).what(), this, Error::ErrorTypes::T_EXCEPTION);
				if( unlikely(error_code) )
					break;
			}
			if( unlikely(error_code == boost::asio::error::operation_aborted) )
				throw Error(boost::system::system_error(error_code).what(), this, Error::ErrorTypes::T_EXCEPTION);
			if( likely(!error_code) ) {
				_cork_sg.Forget();
				return ReadAnswer(true);
			}
			if( unlikely(sock.is_open()) ) //Dunno wtf happend, just reconnect
				reconnect();
		}
	}
	throw Error(boost::system::system_error(error_code).what(), this, Error::ErrorTypes::T_EXCEPTION);
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
		setupTimeout( read_timeout );
		TIMING_STAT_START("http_write");
		boost::asio::async_write(sock, boost::asio::buffer(buf, sz), yield[error_code]);
		TIMING_STAT_END();
		TIMEOUT_END_NOTHROW();
		if( likely(!error_code) ) {
			if( wait_read ) {
				setupTimeout( read_timeout );
				TIMING_STAT_START("http_read");
				sock.async_read_some(boost::asio::null_buffers(), yield[error_code]);
				TIMING_STAT_END();
				TIMEOUT_END();
			}
			if( likely(!error_code) )
				return;
		}
		if( unlikely(sock.is_open()) ) //Dunno wtf happend, just reconnect
			reconnect();
	}
	throw Error(boost::system::system_error(error_code).what(), this, Error::ErrorTypes::T_EXCEPTION);
}

std::unique_ptr< Response > Conn::ReadAnswer(bool read_body) {
	static const char hdr_end_pattern[] = "\r\n\r\n";
	std::unique_ptr< Response > ret( new Response() );
	ret->ContentLength = -1;
	ret->ReadLeft = 0;

	cork_clear( sock.native_handle() ); //Flush request if tcp_cork is set
	{
		TIMEOUT_START( read_timeout );
		TIMING_STAT_START("http_read");
		boost::asio::async_read_until(sock, ret->read_buf, std::string(hdr_end_pattern), yield[error_code]);
		TIMING_STAT_END();
		TIMEOUT_END();
	}

	const char *data = boost::asio::buffer_cast<const char *>( ret->read_buf.data() );
	size_t sz = ret->read_buf.size();
	DEBUG("ReadAnswer: " << std::string(data, sz));
	const char *hdr_end = (const char *)memmem(data, sz, hdr_end_pattern, sizeof(hdr_end_pattern)-1);
	if( unlikely(!hdr_end) )
		throw Error("Cant find headers end", this, Error::ErrorTypes::T_RESPONSE);

	//Parse respnse status
	struct phr_header headers[100];
	size_t num_headers = sizeof(headers) / sizeof(headers[0]);
	int minor_version;
	const char *msg;
	size_t msg_len;
	int pret = phr_parse_response(data, hdr_end-data+sizeof(hdr_end_pattern)-1, &minor_version, &ret->status, &msg, &msg_len, headers, &num_headers, 0);

	if( unlikely(pret != -2 && pret <= 0) )
		throw Error("Cant parse http response: " + std::to_string(pret), this, Error::ErrorTypes::T_RESPONSE);

	bool l_must_reconnect = true;
	for(size_t i=0; i < num_headers; i++) {
		std::string hdr_name(headers[i].name, headers[i].name_len);
		std::transform(hdr_name.begin(), hdr_name.end(), hdr_name.begin(), ::toupper);

		if( hdr_name == "CONTENT-LENGTH" ) {
			ret->ContentLength = strtol(headers[i].value, NULL, 0);
		}else if( hdr_name == "CONNECTION" && !strncasecmp(headers[i].value, "Keep-Alive", headers[i].value_len) ) {
			l_must_reconnect = false;
		}
		ret->headers.emplace_back( std::move(hdr_name), std::string(headers[i].value, headers[i].value_len) );
	}
	must_reconnect = must_reconnect || l_must_reconnect;
	std::sort(ret->headers.begin(), ret->headers.end());

	ret->read_buf.consume( hdr_end - data + sizeof(hdr_end_pattern)-1 ); //Remove headers from input stream

	if( likely(ret->ContentLength>=0) ) {
		assert( static_cast<size_t>(ret->ContentLength) >= ret->read_buf.size() );
		ret->ReadLeft = ret->ContentLength - ret->read_buf.size();
	}
	if( read_body && ret->ContentLength>0 ) {
		while( ret->ReadLeft > 0 ) {
			size_t rd=0;
			{
				TIMEOUT_START( read_timeout );
				TIMING_STAT_START("http_read");
				rd = boost::asio::async_read(sock, ret->read_buf,
					boost::asio::transfer_exactly(ret->ReadLeft>max_read_transfer ? max_read_transfer : ret->ReadLeft), yield[error_code]);
				TIMING_STAT_END();
				TIMEOUT_END();
			}
			ret->ReadLeft -= rd;
		}
	}

	return ret;
}

void Conn::PrelaodBytes( std::unique_ptr< Response > &resp, size_t count ) {
	size_t len = resp->read_buf.size();
	if( len >= count && count != 0 )
		return; //Already have this count

	if( unlikely(count == 0 || count > (resp->ReadLeft + len)) )
		count = (resp->ReadLeft + len);
	len = count - len;
	size_t rd=0;
	{
		TIMEOUT_START( read_timeout );
		TIMING_STAT_START("http_read");
		rd = boost::asio::async_read(sock, resp->read_buf, boost::asio::transfer_exactly(len), yield[error_code]);
		TIMING_STAT_END();
		TIMEOUT_END();
	}
	resp->ReadLeft -= rd;
}

void Conn::StreamReadData( std::unique_ptr< Response > &resp, std::function< size_t(const char *buf, size_t len) > dataCallback, bool disable_drain ) {
	bool interrupt = false, disable_callback=false;
	const char *buf;
	size_t len;
	while( !interrupt ) {
		len = resp->read_buf.size();
		if( likely(len>0) ) {
			buf = boost::asio::buffer_cast<const char *>( resp->read_buf.data() );
			size_t consume_len = len;
			if( ! disable_callback ) {
				consume_len = dataCallback(buf, len);
				if( consume_len==0 ) disable_callback=true;
			}
			resp->read_buf.consume(consume_len);

			if( unlikely(disable_callback && disable_drain) )
				return; //Leave socket unread
			if( consume_len < len )
				continue; // Finish writing this chunk
		}
		if( likely(resp->ReadLeft > 0) ) {
			size_t rd=0;
			{
				TIMEOUT_START( read_timeout );
				TIMING_STAT_START("http_read");
				rd = boost::asio::async_read(sock, resp->read_buf,
					boost::asio::transfer_exactly(resp->ReadLeft>max_read_transfer ? max_read_transfer : resp->ReadLeft), yield[error_code]);
				TIMING_STAT_END();
				TIMEOUT_END();
			}
			resp->ReadLeft -= rd;
		}else{
			interrupt = true;
		}
	}
}

void Conn::StreamSpliceData( std::unique_ptr< Response > &resp, socket_t &dest ) {
#ifndef SPLICE_F_MOVE
	assert( !"Cant call ASIOLibs::HTTP::Conn::StreamSpliceData: splice(2) is linux-only call" );
	abort();
#else
	if( resp->read_buf.size() ) {
		TIMING_STAT_START("client_write");
		boost::asio::async_write(dest, resp->read_buf, yield);
		TIMING_STAT_END();
	}
	if( resp->ReadLeft > 0 ) {
		int pipefd[2];
		if ( pipe(pipefd) < 0 )
			throw Error(std::string("pipe() failed: ") + strerror(errno), this, Error::ErrorTypes::T_EXCEPTION);
		ASIOLibs::ScopeGuard _close_guard( [&]{
			::close(pipefd[0]);
			::close(pipefd[1]);
		});

		enum {
			SPLICE_FULL_HINT = 16*1448, //From haproxy src: A pipe contains 16 segments max, and it's common to see segments of 1448 bytes
			MAX_SPLICE_AT_ONCE = 1<<30
		};
		size_t rd_pipe=0; //Bytes pipe contains atm
		while( resp->ReadLeft > 0 ) {
			//Move to pipe

			while( resp->ReadLeft > 0 && rd_pipe < SPLICE_FULL_HINT ) {
				TIMEOUT_START( read_timeout );
				TIMING_STAT_START("http_read");
				sock.async_read_some(boost::asio::null_buffers(), yield[error_code]); //Have somthing to read
				TIMING_STAT_END();
				TIMEOUT_END();

				size_t rd_once = resp->ReadLeft > MAX_SPLICE_AT_ONCE ? MAX_SPLICE_AT_ONCE : resp->ReadLeft;
				ssize_t rd = splice( sock.native_handle(), NULL, pipefd[1], NULL, rd_once, SPLICE_F_MOVE | SPLICE_F_NONBLOCK);
				if( unlikely( rd < 0) ) {
					if( rd<0 && errno != EAGAIN && errno != EINTR )
						throw Error(std::string("splice() #1 failed: ") + strerror(errno), this, Error::ErrorTypes::T_EXCEPTION);

					if( rd<0 && errno == EAGAIN ) {
						// no input in sock or sock closed -- already checked by async_read
						// or pipe is full
						break;
					}
					continue;
				} else if (unlikely(!rd))
					throw Error(std::string("splice() #1 failed: EOF"), this, Error::ErrorTypes::T_EXCEPTION);
				rd_pipe += rd;
				resp->ReadLeft -= rd;
			}


			//Move from pipe
			while( rd_pipe > 0 ) {
				TIMING_STAT_START("client_write");
				dest.async_write_some(boost::asio::null_buffers(), yield); //Can write
				TIMING_STAT_END();

				ssize_t wr = splice( pipefd[0], NULL, dest.native_handle(), NULL, rd_pipe, SPLICE_F_MOVE | SPLICE_F_NONBLOCK);
				if( unlikely( wr <= 0) ) {
					// Throw exceptions to indicate client error, not backend.
					if( wr<0 && errno == EAGAIN ) {
						if( rd_pipe < resp->ReadLeft )
							break; //Exit write loop & read next data to pipe
						continue;
					} else if( wr<0 && errno != EINTR )
						throw std::system_error(errno, std::system_category(), "splice() #2 failed");
					else if( wr==0 )
						throw std::system_error(0, std::system_category(), "splice() #2 failed: wr==0");
					continue;
				}
				rd_pipe -= wr;
			}
		} //while( resp->ReadLeft > 0 )
		assert( rd_pipe==0 );
	}//if( resp->ReadLeft > 0 )
#endif
}

bool Conn::WriteRequestHeaders(const char *cmd, const std::string &uri, size_t ContentLength) try {
	headersCacheCheck();
	std::string req = ASIOLibs::string_sprintf("%s %s HTTP/1.1\nContent-Length: %zu\n%s",
		cmd, uri.c_str(), ContentLength, headers_cache.c_str());
	writeRequest( req.data(), req.size(), false );
	return true;
} catch (std::exception &e) {
	close();
	throw;
}

bool Conn::WriteRequestData(const void *buf, size_t len) try {
	size_t wr=0;
	TIMEOUT_START( read_timeout );
	TIMING_STAT_START("http_write");
	wr = boost::asio::async_write(sock, boost::asio::buffer(buf, len), yield[error_code]);
	TIMING_STAT_END();
	TIMEOUT_END();
	assert( wr == len );
	return true;
} catch (std::exception &e) {
	close();
	throw;
}

std::shared_ptr<ASIOLibs::CoroBarrier> Conn::WriteRequestDataBarrier(const void *buf, size_t len) try {
	auto r = std::make_shared<ASIOLibs::CoroBarrier>(sock.get_io_service(), yield);
	setupTimeout( read_timeout );
	auto t = std::make_shared<StopWatch>("http_write", stat);

	r->errorCheck = [this](const boost::system::error_code &ec) -> void {
		checkTimeout(ec);
	};
	auto completion_handler = [this, t, r](const boost::system::error_code &ec, size_t sz) mutable -> void {
		t.reset();
		setupTimeout(0);
		r->done(ec);
	};

	boost::asio::async_write(sock, boost::asio::buffer(buf, len), completion_handler);
	return r;
} catch (std::exception &e) {
	close();
	throw;
}

size_t Conn::WriteTee(socket_t &sock_from, size_t max_bytes) {
#ifndef SPLICE_F_MOVE
	assert( !"Cant call ASIOLibs::HTTP::Conn::WriteTee: tee(2) is linux-only call" );
	abort();
#else
	{
		TIMEOUT_START( read_timeout );
		TIMING_STAT_START("http_write");
		sock.async_write_some(boost::asio::null_buffers(), yield[error_code]); //Have somthing to write
		TIMING_STAT_END();
		TIMEOUT_END();
	}
	TIMING_STAT_START("client_read");
	sock_from.async_read_some(boost::asio::null_buffers(), yield); //Can read
	TIMING_STAT_END();
	ssize_t wr = tee(sock_from.native_handle(), sock.native_handle(), max_bytes, SPLICE_F_MORE | SPLICE_F_NONBLOCK);
	if( unlikely(wr == -1) )
		throw Error(std::string("tee() failed: ") + strerror(errno), this, Error::ErrorTypes::T_EXCEPTION);
	return wr;
#endif
}

size_t Conn::WriteSplice(socket_t &sock_from, size_t max_bytes) {
#ifndef SPLICE_F_MOVE
	assert( !"Cant call ASIOLibs::HTTP::Conn::WriteSplice: splice(2) is linux-only call" );
	abort();
#else
	size_t wr_total=0;
	while( max_bytes > wr_total ) {
		ssize_t wr = splice( sock_from.native_handle(), NULL, sock.native_handle(), NULL, max_bytes-wr_total, SPLICE_F_MOVE | SPLICE_F_NONBLOCK | SPLICE_F_MORE);
		if( unlikely(wr == -1 && errno != EAGAIN) )
			throw Error(std::string("splice() failed: ") + strerror(errno), this, Error::ErrorTypes::T_EXCEPTION);
		else if( wr < 1 ) {
			{
				TIMEOUT_START( read_timeout );
				TIMING_STAT_START("http_write");
				sock.async_write_some(boost::asio::null_buffers(), yield[error_code]); //Have somthing to write
				TIMING_STAT_END();
				TIMEOUT_END();
			}
			TIMING_STAT_START("client_read");
			sock_from.async_read_some(boost::asio::null_buffers(), yield); //Can read
			TIMING_STAT_END();
		} else
			wr_total += wr;
	}
	return wr_total;
#endif
}


std::string Response::Dump() const {
	ASIOLibs::StrFormatter s;
	s << "HTTP status=" << status << "; ContentLength=" << ContentLength << "; ReadLeft=" << ReadLeft << " Headers:\n";
	for( auto &i : headers ) {
		s << "'" << i.first << "': '" << i.second << "'\n";
	}
	return s.str();
}
std::string Response::drainRead() const {
	const char *data = boost::asio::buffer_cast<const char *>( read_buf.data() );
	size_t sz = read_buf.size();
	std::string ret(data, sz);
	read_buf.consume(sz);
	return ret;
}

const std::string *Response::GetHeader(const std::string &name) {
	auto it = std::lower_bound(headers.begin(), headers.end(), std::make_pair(name, std::string()) );
	if( likely(it != headers.end() && it->first == name) )
		return &it->second;
	return nullptr;
}

};};
