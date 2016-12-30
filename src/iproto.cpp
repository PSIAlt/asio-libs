#include <atomic>
#include <cctype>
#include "perf.hpp"
#include "iproto.hpp"

namespace IProto {

std::atomic<uint32_t> packer_seq(1);

Packet::Packet(const PacketPtr *buf) {
	data = static_cast<char*>( malloc(sizeof(hdr)+buf->hdr.len) );
	memcpy(data, buf, sizeof(hdr)+buf->hdr.len);
	memcpy(&hdr, buf, sizeof(hdr));
	ofs=0;
}

static std::string string_hex_dump(const std::string& input)
{
	static const char* const lut = "0123456789ABCDEF";
	size_t len = input.length();

	std::string output;
	output.reserve(3 * len);
	for(size_t j=0; j<len; j+=16 ) {
		size_t dsz=output.length();
		for (size_t i = j; i<j+16 && i < len; ++i) {
			const unsigned char c = input[i];
			if( i%4==0 ) output.push_back(' ');
			output.push_back(lut[(c >> 4)&0xf]);
			output.push_back(lut[c & 0xf]);
		}
		dsz = output.length() - dsz;
		while( dsz++ < 40 ) {
			output.push_back(' ');
		}
		output.push_back('\t');
		for(size_t i=j; i<j+16 && i<len; ++i) {
			output.push_back( isprint(input[i]) ? input[i] : '.' );
		}
		output.push_back('\n');
	}
	return output;
}

std::string Packet::Dump() const {
	if( !data )
		return "(Packet unititialized)";
	std::string out = "IProto packet msg=" + std::to_string(hdr.msg) + " sync=" +
		std::to_string(hdr.sync) + " len=" + std::to_string(hdr.len) + " cur_ofs=" + std::to_string(ofs) + " data:\n";
	out += string_hex_dump( std::string(data+sizeof(hdr), hdr.len) );
	return out;
}


//       Some implemetations/instantiations to speedup build
//Packer
template<typename T>
void PackerSetValue<T>::operator()(Packet &pkt, uint32_t flags, const T &val) {
	if( unlikely((pkt.hdr.len+sizeof(T)) > pkt.ofs) ) {
		pkt.ofs = pkt.ofs*2 + sizeof(T);
		pkt.data = static_cast<char*>( realloc(pkt.data, pkt.ofs) );
	}
	*reinterpret_cast< T* >(pkt.data+pkt.hdr.len) = val;
	pkt.hdr.len += sizeof(T);
}
template struct PackerSetValue< uint32_t >; //instantiate for uint32_t
template struct PackerSetValue< uint8_t >; //instantiate for uint8_t

static inline void pack_bersize(Packet &pkt, uint32_t flags, uint32_t sz) {
	if (sz >= (1 << 7)) {
		if (sz >= (1 << 14)) {
			if (sz >= (1 << 21)) {
				if (sz >= (1 << 28))
					PackerSetValue<uint8_t>()(pkt, flags, (sz >> 28) | 0x80);
				PackerSetValue<uint8_t>()(pkt, flags, (sz >> 21) | 0x80);
			}
			PackerSetValue<uint8_t>()(pkt, flags, (sz >> 14) | 0x80);
		}
		PackerSetValue<uint8_t>()(pkt, flags, (sz >> 7) | 0x80);
	}
	PackerSetValue<uint8_t>()(pkt, flags, sz & 0x7F);
}

template<> //instantiate for std::string
void PackerSetValue<std::string>::operator()(Packet &pkt, uint32_t flags, const std::string &val) {
	uint32_t sz = val.size();
	if (flags & BER_PACK) {
		pack_bersize(pkt, flags & ~BER_PACK, sz);
	} else {
		PackerSetValue<uint32_t>()(pkt, flags, sz);
	}
	if( likely((pkt.hdr.len+sz) > pkt.ofs) ) {
		pkt.ofs = pkt.ofs*2 + sz;
		pkt.data = static_cast<char*>( realloc(pkt.data, pkt.ofs) );
	}
	memcpy(pkt.data+pkt.hdr.len, val.data(), sz);
	pkt.hdr.len += sz;
}
template<> //instantiate for const char*
void PackerSetValue< char * >::operator()(Packet &pkt, uint32_t flags, char * const &val) {
	uint32_t sz = strlen(val);
	if (flags & BER_PACK) {
		pack_bersize(pkt, flags & ~BER_PACK, sz);
	} else {
		PackerSetValue<uint32_t>()(pkt, flags, sz);
	}
	if( (pkt.hdr.len+sz) > pkt.ofs ) {
		pkt.ofs = pkt.ofs*2 + sz;
		pkt.data = static_cast<char*>( realloc(pkt.data, pkt.ofs) );
	}
	memcpy(pkt.data+pkt.hdr.len, val, sz);
	pkt.hdr.len += sz;
}
template<> //instantiate for ByteBuffer
void PackerSetValue<ByteBuffer>::operator()(Packet &pkt, uint32_t flags, const ByteBuffer &val) {
	uint32_t sz = val.size;
	if( likely( (pkt.hdr.len+sz) > pkt.ofs ) ) {
		pkt.ofs = pkt.ofs*2 + sz;
		pkt.data = static_cast<char*>( realloc(pkt.data, pkt.ofs) );
	}
	memcpy(pkt.data+pkt.hdr.len, val.buf, sz);
	pkt.hdr.len += sz;
}

//Unpacker
template<typename T>
T UnpackerGetValue<T>::operator()(Packet &pkt, uint32_t flags) {
	if( unlikely( (pkt.ofs+sizeof(T)-sizeof(pkt.hdr)) > pkt.hdr.len ) )
		throw tuple_missmatch( "Tuple missmatch at offset " + std::to_string(pkt.ofs) );
	pkt.ofs += sizeof(T);
	return *reinterpret_cast<T*>( pkt.data + pkt.ofs - sizeof(T) );
}
template struct UnpackerGetValue< uint32_t >; //instantiate for uint32_t
template struct UnpackerGetValue< uint8_t >; //instantiate for uint8_t

template<> //instantiate for std::string
std::string UnpackerGetValue<std::string>::operator()(Packet &pkt, uint32_t flags) {
	std::string v;
	uint32_t v_len = UnpackerGetValue<uint32_t>()(pkt, flags);
	if( unlikely( (pkt.ofs+v_len-sizeof(pkt.hdr)) > pkt.hdr.len ) )
		throw tuple_missmatch( "Tuple missmatch inside std::string at offset " + std::to_string(pkt.ofs) + " v_len=" + std::to_string(v_len) );
	v.resize(v_len);
	memcpy(&v[0], pkt.data+pkt.ofs, v_len);
	pkt.ofs += v_len;
	return v;
}
template<> //instantiate for ByteBuffer
ByteBuffer UnpackerGetValue<ByteBuffer>::operator()(Packet &pkt, uint32_t flags) {
	uint32_t was_len = pkt.hdr.len - pkt.ofs;
	pkt.ofs += was_len;
	return ByteBuffer(pkt.data+pkt.ofs-was_len, was_len);
}
};
