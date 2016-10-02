#include <utility>
#include "write_buffer.hpp"
#include <boost/bind.hpp>
#include <boost/asio/placeholders.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>

namespace ASIOLibs {


template <typename T>
WriteBuffer<T>::write_entry_t::write_entry_t(std::shared_ptr<boost::asio::streambuf> &_buf, callback_func_t _cb)
	: buf(_buf), cb(_cb) {

}

template <typename T>
void WriteBuffer<T>::Write(std::shared_ptr<boost::asio::streambuf> &buf, callback_func_t _cb ) {
	write_queue.emplace( buf, _cb );
	ensureWrite();
}

template <typename T>
void WriteBuffer<T>::Shutdown() {
	decltype(write_queue) t;
	t.swap(write_queue);
	//TODO something else?
	is_write_active = false;
}

template <typename T>
void WriteBuffer<T>::ensureWrite() {
	if( is_write_active || write_queue.empty() ) return;
	boost::asio::async_write(sock, *write_queue.front().buf, boost::bind(&WriteBuffer::onWriteDone, this->shared_from_this(), boost::asio::placeholders::error) );
	is_write_active = true;
}

template <>
void WriteBuffer< boost::asio::ssl::stream<boost::asio::ip::tcp::socket> >::ensureWrite() {
	if( is_write_active || write_queue.empty() ) return;
	if( isSSL() )
		boost::asio::async_write(sock, *write_queue.front().buf,
			boost::bind(&WriteBuffer::onWriteDone, this->shared_from_this(), boost::asio::placeholders::error) );
	else
		boost::asio::async_write(sock.next_layer(), *write_queue.front().buf,
			boost::bind(&WriteBuffer::onWriteDone, this->shared_from_this(), boost::asio::placeholders::error) );
	is_write_active = true;
}

template <typename T>
void WriteBuffer<T>::onWriteDone(const boost::system::error_code &ec) {
	is_write_active = false;
	if( !write_queue.empty() ) {
		if( write_queue.front().cb )
			write_queue.front().cb( ec );
		write_queue.pop();
	}
	if( ec )
		Shutdown();
	else
		ensureWrite();
}

template struct WriteBuffer< boost::asio::ip::tcp::socket >; //instantiate
template struct WriteBuffer< boost::asio::ssl::stream<boost::asio::ip::tcp::socket> >; //instantiate

};
