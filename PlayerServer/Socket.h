// ReSharper disable CppClangTidyBugproneUnsafeFunctions
#pragma once
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>

class Buffer : public std::string {
public:
	Buffer() : std::string() {}
	Buffer(size_t size) :std::string() { resize(size); }

public:
	// 返回一个非 const 的 char* 指针
	operator char* () { return const_cast<char*>(c_str()); }
	operator char* () const { return const_cast<char*>(c_str()); }
	operator const char* () const { return c_str(); }
};

#include <cstdint> // 包含 uint8_t 的定义

enum class SockAttr : int {// NOLINT(performance-enum-size)
	// 使用 int 作为底层类型  
	SOCK_IS_SERVER = 1, // 是否服务器 1{服务器} 0{客户端}
	SOCK_IS_BLOCK = 2    // 是否阻塞 1{阻塞} 0{非阻塞}
};


class CSockParam {
public:
	CSockParam()
		: port(-1), attr() {
		bzero(&addr_in, sizeof(addr_in));
		bzero(&addr_un, sizeof(addr_un));
	}

	//构造网络套接字
	CSockParam(const Buffer& ip, const short port, const int attr)
		: addr_un() {
		this->ip = ip;
		this->port = port;
		this->attr = attr;
		addr_in.sin_family = AF_INET;
		addr_in.sin_port = port;
		addr_in.sin_addr.s_addr = inet_addr(ip);
	}
	//构造本地套接字
	CSockParam(const Buffer& path, const int attr)
		: addr_in(), port(-1) {
		ip = path;
		addr_un.sun_family = AF_UNIX;
		strcpy(addr_un.sun_path, path);
		this->attr = attr;
	}

	~CSockParam() = default;

	CSockParam(const CSockParam& param) {
		ip = param.ip;
		port = param.port;
		attr = param.attr;
		memcpy(&addr_in, &param.addr_in, sizeof(addr_in));
		memcpy(&addr_un, &param.addr_un, sizeof(addr_un));
	}
public:
	CSockParam& operator=(const CSockParam& param) {
		if (this != &param) {
			ip = param.ip;
			port = param.port;
			attr = param.attr;
			memcpy(&addr_in, &param.addr_in, sizeof(addr_in));
			memcpy(&addr_un, &param.addr_un, sizeof(addr_un));
		}
		return *this;
	}
	sockaddr* addrin() { return reinterpret_cast<sockaddr*>(&addr_in); }
	sockaddr* addrun() { return reinterpret_cast<sockaddr*>(&addr_un); }

public:
	//地址
	sockaddr_in addr_in;
	sockaddr_un addr_un;
	//ip
	Buffer ip;
	//端口
	short port;
	//参考套接字属性 SockAttr
	int	attr;
};

class CSocketBase {
public:
	virtual ~CSocketBase() {
		m_status = 3; //设置为关闭状态
		if(m_socket != -1){
			int fd = m_socket;
			m_socket = -1;
			close(fd);
		}
	}

public:
	/*初始化 服务器 套接字创建 bind绑定、listen
	客户端 套接字*/
	virtual int Init(const CSockParam& param) = 0;
	//S C 连接 对于UDP可忽略
	virtual int Link(CSocketBase** pClient = nullptr) = 0;
	//发送数据
	virtual int Send(const Buffer& data) = 0;
	//接收数据
	// ReSharper disable once IdentifierTypo
	virtual int Recv(Buffer& data) = 0;
	//关闭连接
	virtual int Close() = 0;
protected:
	//套接字描述符 默认-1
	int m_socket = -1;
	//状态 0{初始化未完成} 1{初始化完成} 2{连接完成} 3{关闭状态}
	int m_status = -1;
private:

};