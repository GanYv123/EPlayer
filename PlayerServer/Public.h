#pragma once
#include <string>
#include <memory.h>

class Buffer : public std::string
{
public:
	Buffer() : std::string() {}
	Buffer(const char* str, size_t length) :std::string() {
		resize(length);
		memcpy(const_cast<char*>(c_str()), str, length);
	}
	Buffer(const char* begin, const char* end) :std::string() {
		const std::ptrdiff_t len = end - begin;
		if(len > 0){
			resize(len);
			memcpy(const_cast<char*>(c_str()), begin, len);
		}
	}
	Buffer(size_t size) :std::string() { resize(size); }
	Buffer(const std::string& str) : std::string(str){}
	Buffer(const char* str) : std::string(str){}

public:
	// 返回一个非 const 的 char* 指针
	operator void* () { return const_cast<char*>(c_str()); }
	operator char* () { return const_cast<char*>(c_str()); }
	operator char* () const { return const_cast<char*>(c_str()); }
	operator unsigned char* (){ return (unsigned char*)c_str(); }
	operator const char* () const { return c_str(); }
	operator const void* () const { return c_str(); }
};
