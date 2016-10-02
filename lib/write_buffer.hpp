#pragma once
#ifndef ASIOLIBS_WRITE_BUFFER
#define ASIOLIBS_WRITE_BUFFER
#include <queue>
#include <functional>
#include <memory>
#include <boost/asio/error.hpp>
#include <boost/asio/streambuf.hpp>

namespace ASIOLibs {

template <typename T>
struct WriteBuffer : std::enable_shared_from_this< WriteBuffer<T> > {
	typedef std::function< void(const boost::system::error_code &ec) > callback_func_t;
	WriteBuffer(T &_sock, bool __isSSL = false) : sock(_sock), _isSSL(__isSSL) {}
	WriteBuffer(WriteBuffer &) = delete;
	WriteBuffer(WriteBuffer &&) = delete;
	WriteBuffer &operator=(const WriteBuffer &) = delete;

	void Write(std::shared_ptr<boost::asio::streambuf> &buf, callback_func_t _cb = callback_func_t() );
	void Shutdown();

	bool isSSL() const { return _isSSL; }
	bool setSSL(bool v) { _isSSL=v; return _isSSL; }

private:
	void ensureWrite();
	void onWriteDone(const boost::system::error_code &ec);
	struct write_entry_t {
		write_entry_t(std::shared_ptr<boost::asio::streambuf> &_buf, callback_func_t _cb);
		write_entry_t(write_entry_t &) = delete;
		write_entry_t(write_entry_t &&) = default;
		write_entry_t &operator=(const write_entry_t &) = delete;

		std::shared_ptr<boost::asio::streambuf> buf;
		callback_func_t cb;
	};
	T &sock;
	std::queue< write_entry_t > write_queue;
	bool is_write_active = false, _isSSL = false;
};

};

#endif
