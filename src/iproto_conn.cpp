#include <cstdlib>
#include <cstdarg>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include "perf.hpp"
#include "iproto.hpp"
#include "iproto_conn.hpp"

#define LOG_DEBUG 0

namespace IProto {

int stderr_printf(char const* format, ...) {
	va_list args;
	va_start(args, format);
	int result = vfprintf(stderr, format, args);
	va_end(args);
	fprintf(stderr, "\n");
	return result;
}

Conn::Conn(boost::asio::io_service &_io, const boost::asio::ip::tcp::endpoint &_ep, uint32_t _connect_timeout, uint32_t _read_timeout)
	: io(_io), ep(_ep), sock(_io), timer(_io), ping_timer(_io) {
		if( _read_timeout==0 ) _read_timeout=_connect_timeout;
		connect_timeout=_connect_timeout;
		read_timeout=_read_timeout;
		write_queue_len=0;
		write_is_active=ping_enabled=true;
		log_func = stderr_printf;
}

Conn::~Conn() {
	if( LOG_DEBUG )
		log_func("[iproto_conn] %s:%u destructor", ep.address().to_string().c_str(), ep.port());

	close();
	dismissCallbacks(CB_ERR);
}

void Conn::close() {
	boost::system::error_code ec = boost::asio::error::interrupted;
	for(int i=0; i<3 && sock.is_open() && ec == boost::asio::error::interrupted; ++i)
		sock.close(ec);
}

void Conn::disablePing() {
	ping_enabled=false;
	ping_timer.cancel();
}

void Conn::setupPing(const boost::system::error_code& error) {
	if( error || !ping_enabled )
		return;
	if( !sock.is_open() )
		beginConnect();
	//Write a ping packet into socket
	static const IProto::Header hdr{0xff00, 0};
	static const IProto::PacketPtr pkt{hdr};
	Write(IProto::Packet(&pkt), boost::bind(&Conn::pingCb, shared_from_this(), _1)  );
}

void Conn::pingCb(RequestResult res) {
	if( unlikely(res.code != CB_OK) ) {
		//Reconnect
		log_func("[iproto_conn] %s:%u ping failed, code %u", ep.address().to_string().c_str(), ep.port(), res.code);
		if( res.code == CB_TIMEOUT ) {
			reconnect();
			return;
		}
	}
	ping_timer.expires_from_now( boost::posix_time::milliseconds(300) );
	ping_timer.async_wait( boost::bind(&Conn::setupPing, shared_from_this(), boost::asio::placeholders::error) );
}

bool Conn::checkConnect() {
	if( unlikely(!sock.is_open()) ) {
		beginConnect();
		return true;
	}
	return false;
}
void Conn::beginConnect() {
	write_is_active=false;
	ping_timer.cancel();
	//Limit connection time
	timer.expires_from_now( boost::posix_time::milliseconds(connect_timeout) );
	timer.async_wait( boost::bind(&Conn::reconnectByTimer, shared_from_this(), boost::asio::placeholders::error) );

	sock.async_connect( ep, boost::bind(&Conn::onConnect, shared_from_this(), boost::asio::placeholders::error) );
	if( LOG_DEBUG )
		log_func("[iproto_conn] Connecting to %s:%d", ep.address().to_string().c_str(), ep.port() );
}
void Conn::reconnect() {
	close();
	if( LOG_DEBUG )
		log_func("[iproto_conn] %s:%u reconnect", ep.address().to_string().c_str(), ep.port());
	beginConnect();
}
void Conn::dismissCallbacks(CB_Result res) {
	if( LOG_DEBUG )
		log_func("[iproto_conn] %s:%u dismissCallbacks", ep.address().to_string().c_str(), ep.port());
	ping_timer.cancel();
	for(auto it=callbacks_map.begin(); it!=callbacks_map.end(); ) {
		it = invokeCallback(it, RequestResult(res));
	}
	callbacks_map.clear();
}
void Conn::reconnectByTimer(const boost::system::error_code& error) {
	if( !error )
		reconnect();
}
void Conn::onConnect(const boost::system::error_code& error) {
	if( likely(!error) ) {
		if( LOG_DEBUG )
			log_func("[iproto_conn] %s:%u connected", ep.address().to_string().c_str(), ep.port());
		timer.cancel();
		setupReadHandler();
		sock.set_option( boost::asio::ip::tcp::no_delay(true) );
		if( ping_enabled )
			setupPing( boost::system::error_code() );
		else
			ensureWriteBuffer( boost::system::error_code() );
	}else
		log_func("[iproto_conn] %s:%u connect failed: %s", ep.address().to_string().c_str(), ep.port(), error.message().c_str() );
}

void Conn::setupReadHandler() {
	if( likely(!rd_buf || rd_buf->size()==0) )
		rd_buf.reset(new boost::asio::streambuf);
	onRead( boost::system::error_code() );
}
void Conn::onRead(const boost::system::error_code& error) {
	if( unlikely(error) ) {
		log_func("[iproto_conn] %s:%u read error: %s", ep.address().to_string().c_str(), ep.port(), error.message().c_str() );
		dismissCallbacks(CB_ERR);
		if( error != boost::asio::error::operation_aborted ) {
			reconnect();
		}
		return;
	}
	if( LOG_DEBUG )
		log_func("[iproto_conn] %s:%u onRead rd_buf->size=%zu", ep.address().to_string().c_str(), ep.port(), rd_buf->size());
	while( rd_buf->size() >= sizeof(Header) ) {
		const PacketPtr *buf = boost::asio::buffer_cast< const PacketPtr * >( rd_buf->data() );
		size_t want_read = sizeof(Header)+buf->hdr.len;
		if( want_read <= rd_buf->size() ) {
			invokeCallback(buf->hdr.sync, RequestResult(CB_OK, Packet(buf)) );
			rd_buf->consume( sizeof(Header) + buf->hdr.len );
		}else{
			boost::asio::async_read(sock, *rd_buf, boost::asio::transfer_at_least(want_read - rd_buf->size()),
				boost::bind(&Conn::onRead, shared_from_this(), boost::asio::placeholders::error) );
			return;
		}
	}
	if( likely(sock.is_open()) )
		boost::asio::async_read(sock, *rd_buf, boost::asio::transfer_at_least(sizeof(Header)-rd_buf->size()),
			boost::bind(&Conn::onRead, shared_from_this(), boost::asio::placeholders::error) );
}
void Conn::ensureWriteBuffer(const boost::system::error_code& error, const char *wr_buf) {
	if( unlikely(error) ) {
		log_func("[iproto_conn] %s:%u write error: %s", ep.address().to_string().c_str(), ep.port(), error.message().c_str() );
		if( error != boost::asio::error::operation_aborted ) {
			if( error == boost::asio::error::broken_pipe ) {
				//Packet was not completely transfered, we can do a retry
				write_queue.push_back( wr_buf );
				write_queue_len++;
				wr_buf=nullptr;//Prevent free
			}
			reconnect();
		}else{
			dismissCallbacks(CB_ERR);
		}
		if( likely(wr_buf != nullptr) )
			::free((void*)wr_buf);
		return;
	}
	if( likely(wr_buf != nullptr) )
		::free((void*)wr_buf);

	if( likely(write_queue_len>0 && (wr_buf || !write_is_active)) ) {
		const char *wr = write_queue.front();
		write_queue.pop_front();
		write_queue_len--;
		const Header *hdr = reinterpret_cast< const Header * >(wr);
		boost::asio::async_write(sock, boost::asio::buffer(wr, sizeof(*hdr)+hdr->len),
			boost::bind(&Conn::ensureWriteBuffer, shared_from_this(), boost::asio::placeholders::error, wr) );
		if( LOG_DEBUG )
			log_func("[iproto_conn] %s:%u write packet sync=%u len=%u", ep.address().to_string().c_str(), ep.port(), hdr->sync, hdr->len);
		write_is_active=true;
	} else
		write_is_active=false;
}
bool Conn::dropPacketWrite(Packet &&pkt) {
	write_queue.push_back( pkt.data );
	pkt.data=nullptr;
	write_queue_len++;

	if( unlikely(checkConnect()) ) {
		log_func("[iproto_conn] %s:%u dropPacketWrite deferred (no connect)", ep.address().to_string().c_str(), ep.port());
		return false;
	}

	ensureWriteBuffer( boost::system::error_code() );
	return true;
}
bool Conn::Write(Packet &&pkt, callbacks_func_type &&cb) {
	auto timer = new boost::asio::deadline_timer(io);
	timer->expires_from_now( boost::posix_time::milliseconds(read_timeout) );
	timer->async_wait( boost::bind(&Conn::onTimeout, shared_from_this(), boost::asio::placeholders::error, pkt.hdr.sync, timer) );

	callbacks_map[pkt.hdr.sync] = std::make_pair(timer, std::forward<callbacks_func_type>(cb));
	return dropPacketWrite( std::forward<Packet>(pkt) );
}
void Conn::Shutdown() {
	if( LOG_DEBUG )
		log_func("[iproto_conn] %s:%u shutdown", ep.address().to_string().c_str(), ep.port());
	close();
	dismissCallbacks(CB_ERR);
}
bool Conn::GentleShutdown() {
	if( LOG_DEBUG )
		log_func("[iproto_conn] %s:%u GentleShutdown", ep.address().to_string().c_str(), ep.port());

	if( callbacks_map.empty() && !write_is_active ) {
		Shutdown();
		return true;
	}
	return false;
}
void Conn::onTimeout(const boost::system::error_code& error, uint32_t sync, boost::asio::deadline_timer *timer) {
	if( unlikely(!error) ) {
		if( LOG_DEBUG )
			log_func("[iproto_conn] %s:%u Packet with sync=%u timed out", ep.address().to_string().c_str(), ep.port(), sync);
		invokeCallback(sync, RequestResult(CB_TIMEOUT));
	}
	delete timer;
}
void Conn::invokeCallback(uint32_t sync, RequestResult &&req_res) {
	auto it = callbacks_map.find(sync);
	if( it != callbacks_map.end() )
		invokeCallback(it, std::forward<RequestResult>(req_res));
}
Conn::callbacks_map_type::iterator Conn::invokeCallback(Conn::callbacks_map_type::iterator &it, RequestResult &&req_res) try {
	if( LOG_DEBUG )
		log_func("[iproto_conn] %s:%u invokeCallback sync=%u res.code=%u", ep.address().to_string().c_str(), ep.port(), it->first, req_res.code);

	callbacks_map_type::iterator ret_it;
	auto timer_and_cb = std::move(it->second); //Move to reduce inc/dec of shared_ptrs
	ret_it = callbacks_map.erase(it);
	if( likely(timer_and_cb.first && req_res.code!=CB_TIMEOUT) )
		timer_and_cb.first->cancel();

	if( likely(timer_and_cb.second) )
		//Post in separate job to avoid call in current coroutine and/or other shitty accidents
		io.post( boost::bind(std::move(timer_and_cb.second), std::forward<RequestResult>(req_res)) );
	return ret_it;
} catch(std::exception &e) {
	log_func("[iproto_conn] %s:%u invokeCallback uncatched exception: %s", ep.address().to_string().c_str(), ep.port(), e.what() );
	abort(); //It's your guilt
}

};
