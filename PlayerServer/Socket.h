﻿// ReSharper disable CppClangTidyBugproneUnsafeFunctions
#pragma once
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>
#include <cstdint> // 包含 uint8_t 的定义
#include "Public.h"

enum SockAttr  // NOLINT(performance-enum-size)
{// NOLINT(performance-enum-size)
// 使用 int 作为底层类型  
	SOCK_IS_SERVER = 1,	// 是否服务器 1{服务器} 0{客户端}
	SOCK_IS_NON_BLOCK = 2,	// 是否阻塞 1{非阻塞} 0{阻塞}
	SOCK_IS_UDP = 4,	// 1{UDP} 0{TCP}
	SOCK_IS_IP = 8,		//是否为IP协议 1{IP协议} 0{本地套接字}
	SOCK_IS_REUSE = 16 //是否重用
};


class CSockParam
{
public:
	CSockParam()
		: port(-1), attr(0)	//默认是客户端、阻塞、tcp
	{
		bzero(&addr_in, sizeof(addr_in));
		bzero(&addr_un, sizeof(addr_un));
	}

	//构造网络套接字
	CSockParam(const Buffer& ip, const short port, const int attr)
		: addr_un()
	{
		this->ip = ip;
		this->port = port;
		this->attr = attr;
		addr_in.sin_family = AF_INET;
		addr_in.sin_port = htons(port);
		addr_in.sin_addr.s_addr = inet_addr(ip);
	}

	CSockParam(const sockaddr_in* addrin, const int attr)
		: addr_un(), port(0)
	{
		this->attr = attr;
		memcpy(&addr_in, addrin, sizeof(addr_in));
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
		if(this != &param){
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


/**
 * 套接字基类
 */
class CSocketBase
{
public:
	CSocketBase() {
		m_socket = -1;
		m_status = 0; //0{初始化未完成}
	}
	virtual ~CSocketBase() {
		CSocketBase::Close();
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
	virtual int Close() {
		m_status = 3; //设置为关闭状态
		if(m_socket != -1){
			if((m_param.attr & SOCK_IS_SERVER) && ((m_param.attr & SOCK_IS_IP) == 0))//非IP服务器
				unlink(m_param.ip);
			const int fd = m_socket;
			m_socket = -1;
			close(fd);
		}
		return  0;
	}
	virtual operator int() { return m_socket; }
	virtual operator int() const { return m_socket; }
	virtual operator const sockaddr_in* ()const{ return &m_param.addr_in; }
	virtual operator sockaddr_in* (){ return &m_param.addr_in; }
protected:
	//套接字描述符 默认-1
	int m_socket;
	//状态 0{初始化未完成} 1{初始化完成} 2{连接完成} 3{关闭状态}
	int m_status;
	//初始化的参数
	CSockParam m_param;

private:

};

/**
 * 套接字封装类
 */
class CSocket : public CSocketBase
{
public:
	CSocket() : CSocketBase() {}

	CSocket(const int sock) : CSocketBase() {
		m_socket = sock;
	}
	~CSocket() override {
		CSocket::Close();
	}

public:
	/*
	初始化 服务器 套接字创建 bind绑定、listen
	客户端 套接字
	*/
	int Init(const CSockParam& param) override {
		if(m_status != 0) return -1;
		m_param = param;
		const int type = (param.attr & SOCK_IS_UDP) ? SOCK_DGRAM : SOCK_STREAM;
		if(m_socket == -1){
			if(param.attr & SOCK_IS_IP)
				m_socket = socket(PF_INET, type, 0);
			else
				m_socket = socket(PF_LOCAL, type, 0);
		} else{
			m_status = 2; //accept来的套接字,已经处于连接状态
		}
		if(m_socket == -1) return -2;
		int ret{ 0 };
		if(m_param.attr & SOCK_IS_REUSE){
			int option = 1;
			ret = setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
			if(ret == -1) return -7;
		}
		if(m_param.attr & SOCK_IS_SERVER){
			if(m_param.attr & SOCK_IS_IP)
				ret = bind(m_socket, m_param.addrin(), sizeof(sockaddr_in));
			else
				ret = bind(m_socket, m_param.addrun(), sizeof(sockaddr_un));
			if(ret == -1) return -3;
			ret = listen(m_socket, 32);
			if(ret == -1) return -4;
		}
		if(m_param.attr & SOCK_IS_NON_BLOCK){
			int option = fcntl(m_socket, F_GETFL);
			if(option == -1) return -5;
			option |= O_NONBLOCK;
			ret = fcntl(m_socket, F_SETFL, option);
			if(ret == -1) return -6;
		}

		if(m_status == 0)m_status = 1;
		return 0;
	}
	//S C 连接 对于UDP可忽略
	int Link(CSocketBase** pClient = nullptr) override {
		if(m_status <= 0 || (m_socket == -1)) return -1;
		int ret{ 0 };
		if(m_param.attr & SOCK_IS_SERVER){
			if(pClient == nullptr) return -2;
			CSockParam param;
			int fd = -1;
			socklen_t len = 0;
			if(m_param.attr & SOCK_IS_IP){
				param.attr |= SOCK_IS_IP;
				len = sizeof(sockaddr_in);
				fd = accept(m_socket, param.addrin(), &len);
			} else{
				len = sizeof(sockaddr_un);
				fd = accept(m_socket, param.addrun(), &len);
			}
			if(fd == -1)return -3;
			*pClient = new CSocket(fd);
			if(*pClient == nullptr) return -4;
			ret = (*pClient)->Init(param);
			if(ret != 0){
				delete (*pClient);
				*pClient = nullptr;
				return -5;
			}
		} else{
			//客户端
			if(m_param.attr & SOCK_IS_IP)
				ret = connect(m_socket, m_param.addrin(), sizeof(sockaddr_in));
			else
				ret = connect(m_socket, m_param.addrun(), sizeof(sockaddr_un));

			if(ret != 0) return -6;
		}
		m_status = 2;//连接完成
		return 0;
	}
	//发送数据
	int Send(const Buffer& data) override {
		if(m_status < 2 || (m_socket == -1)) return -1;
		ssize_t index = 0;
		while(index < static_cast<ssize_t>(data.size())){
			const ssize_t len = write(m_socket, static_cast<char*>(data) + index, data.size() - index);
			if(len == 0) return -2;//closed
			if(len < 0) return -3;
			index += len;
		}
		return 0;
	}

	/**
	 * 接收数据
	 * @param data 缓冲区
	 * @return 大于0表示接收成功 小于等于0失败
	 *  0{没有收到数据}
	 * -2{发送错误}
	 * -3{套接字关闭}
	 */
	int Recv(Buffer& data) override {
		if(m_status < 2 || (m_socket == -1)) return -1;
		data.resize(1024 * 1024);
		const ssize_t len = read(m_socket, data, data.size());
		if(len > 0){
			data.resize(len);
			return static_cast<int>(len);
		}
		data.clear();
		if(len < 0){
			if(errno == EINTR || (errno == EAGAIN)){//非阻塞的情况
				data.clear();
				return 0;
			}
			return -2;//发送错误
		}
		return -3;//套接字被关闭
	}

	//关闭连接
	int Close() override {
		return CSocketBase::Close();
	}
private:
};