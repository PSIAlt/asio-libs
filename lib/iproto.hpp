#pragma once
#ifndef _ASIO_LIBS_IPPROTO_HPP
#define _ASIO_LIBS_IPPROTO_HPP
#include <assert.h>
#include <cstring>
#include <tuple>
#include <stdexcept>
#include <string>

namespace IProto {

struct Header {
	Header() : msg(0), len(0), sync(0) {};
	Header(uint32_t _msg, uint32_t _sync) : msg(_msg), len(0), sync(_sync) {};
	Header(const Header &other) = default;
	Header &operator=(const Header &other) = default;
	Header(Header &&other) = default;
	uint32_t msg;
	uint32_t len;
	uint32_t sync;
};

struct PacketPtr {
	Header hdr;
	char data[];
};
struct Packet {
	Packet() : data(nullptr), ofs(0) {};
	~Packet() {
		free(data);
		data = nullptr;
	}
	Packet(Header &&_hdr) : hdr(_hdr), data(nullptr), ofs(0) {};
	Packet(const PacketPtr *buf); //Init from binary read buffer

	Packet(const Packet &other) = delete;
	const Packet &operator=(const Packet &other) = delete;
	Packet(Packet &&other) {
		data = other.data;
		other.data = nullptr;

		hdr = other.hdr;
		memset(&other, 0, sizeof(other));

		ofs = other.ofs;
		other.ofs=0;
	}
	void Reset() { //Read read cursor
		ofs = 0;
		memcpy(&hdr, data, sizeof(hdr));
	}
	uint32_t BytesLeft() const {
		return hdr.len - ofs;
	}
	std::string Dump() const;

	Header hdr;
	char *data;
	uint32_t ofs;
};

struct ByteBuffer {
	ByteBuffer(void *_buf, uint32_t _size) : buf(_buf), size(_size) {};
	ByteBuffer(const ByteBuffer &other) = default;
	ByteBuffer &operator=(const ByteBuffer &other) = default;

	void *buf;
	uint32_t size;
};

struct tuple_missmatch : public std::runtime_error {
	tuple_missmatch(const std::string &err) : runtime_error(err) {};
};
struct tuple_invalid : public tuple_missmatch {
	tuple_invalid(const std::string &err) : tuple_missmatch(err) {};
};

enum {
	BER_PACK = 1,
};

//Unpacker functions
template<typename T>
struct UnpackerGetValue { //instantiated in iproto.cpp
	UnpackerGetValue() {}
	T operator()(Packet &pkt, uint32_t flags);
};

template< typename T, typename ...Args >
struct UnpackerImpl {
	UnpackerImpl() {}
	std::tuple< T, Args... > operator()(Packet &pkt, uint32_t flags) {
		auto value_tuple = std::make_tuple( UnpackerGetValue<T>()(pkt, flags) );
		return std::tuple_cat(
			std::move(value_tuple),
			UnpackerImpl< Args... >()(pkt, flags)
		);
	}
};
template< typename T >
struct UnpackerImpl<T> {
	UnpackerImpl() {}
	std::tuple< T > operator()(Packet &pkt, uint32_t flags) {
		return std::make_tuple( UnpackerGetValue<T>()(pkt, flags) );
	}
};

template< typename ...Args >
std::tuple< Args... > Unpacker(Packet &pkt, uint32_t flags=0) {
	// Unpacker implementation
	assert( pkt.data != nullptr );
	if( pkt.ofs == 0 ) {
		//First call, setup hdr from raw buffer
		memcpy(&pkt.hdr, pkt.data, sizeof(pkt.hdr));
		pkt.ofs+=sizeof(pkt.hdr);
		pkt.hdr.len+=sizeof(pkt.hdr); //Set hacked length so calculations above should work
	}
	return UnpackerImpl< Args... >()(pkt, flags);
}

//Packer functions
template<typename T>
struct PackerSetValue { //instantiated in iproto.cpp
	PackerSetValue() {}
	void operator()(Packet &pkt, uint32_t flags, const T &val);
};
// TODO force to work string literal

template< typename T, typename ...Args >
struct PackerImpl {
	PackerImpl() {}
	void operator()(Packet &pkt, uint32_t flags, const T &val, const Args&... args) {
		PackerSetValue<T>()(pkt, flags, val);
		PackerImpl< Args... >()(pkt, flags, args...);
	}
};
template< typename T >
struct PackerImpl< T > {
	PackerImpl() {}
	void operator()(Packet &pkt, uint32_t flags, const T &val) {
		PackerSetValue<T>()(pkt, flags, val);
	}
};

extern std::atomic<uint32_t> packer_seq;
template< typename ...Args >
Packet Packer(uint32_t cmd, uint32_t flags, const Args&... args) {
	Packet pkt( {cmd, packer_seq++} );
	pkt.hdr.len = sizeof(pkt.hdr); //Skip bytes from begining
	pkt.ofs = sizeof(pkt.hdr) + sizeof...(args) * sizeof(uint32_t); //Hold capacity here for now
	pkt.data = static_cast<char*>( malloc(pkt.ofs) );
	PackerImpl< Args... >()(pkt, flags, args...);
	pkt.ofs = 0;
	pkt.hdr.len -= sizeof(pkt.hdr); //Set "normal" length
	memcpy(pkt.data, &pkt.hdr, sizeof(pkt.hdr)); //Prepare raw buffer for write
	return pkt;
}

//TODO pack from std::tuple
};
#endif
