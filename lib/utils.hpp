#pragma once
#ifndef _ASIO_LIBS_UTILS_HPP_
#define _ASIO_LIBS_UTILS_HPP_
#include <string>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <utility> //forward

namespace ASIOLibs {

#define ASIOLIBS_VERSION "1.1"

struct ScopeGuard {
	typedef std::function< void() > func_type;
	//TODO make perfect forwarding?
	explicit ScopeGuard(func_type _func) : func(_func), isReleased(false) {}
	~ScopeGuard() {
		if( !isReleased && func ) try {
			func();
		}catch(...) {};
	}
	void Forget() { isReleased=true; }

	//noncopyable
	ScopeGuard(const ScopeGuard&) = delete;
	ScopeGuard& operator=(const ScopeGuard&) = delete;

private:
	func_type func;
	bool isReleased;
};

template<typename T>
void ForceFree(T &obj) {
	T().swap(obj);
}

class StrFormatter
{
public:
	StrFormatter() {}
	~StrFormatter() {}

	template <typename Type>
	StrFormatter & operator << (const Type & value)
	{
		stream_ << value;
		return *this;
	}

	std::string str() const         { return stream_.str(); }
	operator std::string () const   { return stream_.str(); }

	enum ConvertToString 
	{
		to_str
	};
	std::string operator >> (ConvertToString) { return stream_.str(); }

	StrFormatter(const StrFormatter &) = delete;
	StrFormatter &operator=(StrFormatter &) = delete;
	StrFormatter(StrFormatter &&) = default;

private:
	std::stringstream stream_;
};

template <typename T>
struct Optional {
	Optional() : has_value(false) {}
	Optional(T &&t) : value( std::forward<T>(t) ), has_value(true) {}

	Optional(Optional &&other) = default;
	Optional(const Optional &other) = default;
	Optional & operator=(const Optional &other) = default;
	Optional & operator=(Optional &&other) = default;

	T &operator*() {
		if( !has_value )
			throw std::runtime_error("Optional has no value");
		return value;
	}
	bool hasValue() const { return has_value; }

private:
	T value;
	bool has_value;
};

std::string string_sprintf(const char *fmt, ...);
std::string bin2hex(const std::string &in);
Optional<std::string> hex2bin(const std::string &in);



};

#endif
