#pragma once
#include "Process.h"
#include "Thread.h"
#include "ThreadPool.h"

class CBusiness
{
public:
	CBusiness():
		m_connectedcallback(nullptr),
		m_recvedcallback(nullptr)
	{}
	virtual int BusinessProcess(CProcess* proc) = 0;
	template<typename _FUNCTION_, typename... _ARGS_>
	int SetConnectCallback(_FUNCTION_ func, _ARGS_... args) {
		m_connectedcallback = new CFunction<_FUNCTION_, _ARGS_...>(func, args...);
		if(m_connectedcallback == nullptr) return -1;
		return 0;
	}


	template<typename _FUNCTION_, typename... _ARGS_>
	int SetRecvCallback(_FUNCTION_ func, _ARGS_... args) {
		m_recvedcallback = new CFunction<_FUNCTION_, _ARGS_...>(func, args...);
		if(m_recvedcallback == nullptr) return -1;
		return 0;
	}

protected:
	CFunctionBase* m_connectedcallback;
	CFunctionBase* m_recvedcallback;
};

class CServer
{
public:
	CServer();
	~CServer(){ Close(); }
	CServer(const CServer&) = delete;
	CServer& operator=(const CServer&) = delete;

public:
	int Init(CBusiness* business,const Buffer& ip = "127.0.0.1",short port = 9999);
	int Run() const;
	int Close();
protected:
	int ThreadFunc();

private:
	CThreadPool m_pool;
	CSocketBase* m_server;
	CEpoll m_epoll;
	CProcess m_process;
	CBusiness* m_business; //业务模块 手动delete
};

