#pragma once
#include <stdio.h>
#include "Thread.h"
#include "Epoll.h"
#include "Socket.h"
#include <sys/timeb.h>
#include <stdarg.h>
#include <sstream>
#include <sys/stat.h>

enum LogLevel
{
	LOG_INFO,
	LOG_DEBUG,
	LOG_WARING,
	LOG_ERROR,
	LOG_FATAL
};

class CLoggerServer;
class LogInfo;

class LogInfo
{
public:
	LogInfo(
		const char* file, int line,
		const char* func,
		pid_t pid, pthread_t tid,
		int level, const char* fmt, ...);

	LogInfo(const char* file, int line,
		const char* func,
		pid_t pid, pthread_t tid,
		int level);

	LogInfo(const char* file, int line,
		const char* func,
		pid_t pid, pthread_t tid,
		int level, void* pData, size_t nSize);

	~LogInfo();

	operator Buffer() const{
		return m_buf;
	}

	template<typename T>
	LogInfo& operator<<(const T& data){
		std::stringstream stream;
		stream << data;
		m_buf += stream.str().c_str();
		return *this;
	}

private:
	bool bAuto;//默认false 流式日志{true}
	Buffer m_buf;
};

class CLoggerServer
{
public:
	CLoggerServer();

	~CLoggerServer(){
		Close();
	}

public:
	CLoggerServer(const CLoggerServer&) = delete;
	CLoggerServer& operator=(const CLoggerServer&) = delete;

public:
	/**
	 * 日志服务器的启动
	 * @return
	 */
	int Start(){
		if(m_server != nullptr) return -1;
		if(access("log", W_OK | R_OK) != 0){
			mkdir("log", S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
		}
		m_file = fopen(m_path, "w+");
		if(m_file == nullptr) return -2;

		int ret = m_epoll.Create(1);
		if(ret != 0) return -3;
		m_server = new CLocalSocket();
		if(m_server == nullptr){
			Close();
			return -4;
		}
		//本地套接字
		ret = m_server->Init(CSockParam("./log/server.sock", static_cast<int>(SOCK_IS_SERVER)));
		if(ret != 0){
			printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
			Close();
			return -5;
		}
		ret = m_epoll.Add(*m_server, EpollData((void*)m_server),EPOLLIN|EPOLLERR);
		if(ret != 0){
			Close();
			return -6;
		}
		ret = m_thread.Start();
		if(ret != 0){
			printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
			Close();
			return -7;
		}
		return 0;
	}


	int Close(){
		if(m_server != nullptr){
			const CSocketBase* p = m_server;
			m_server = nullptr;
			delete p;
		}
		m_epoll.Close();
		m_thread.Stop();
		return 0;
	}

	/**
	 * 给其他非日志进程和线程使用
	 */
	static void Trace(const LogInfo& info){
		int ret{ 0 };
		static thread_local CLocalSocket client;
		if(client == -1){
			ret = client.Init(CSockParam("./log/server.sock", 0));
			if(ret != 0){
#ifdef  _DEBUG
				printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
#endif
				return;
			}
			printf("%s(%d):<%s> ret=%d client:%d\n", __FILE__, __LINE__, __FUNCTION__, ret, (int)client);
			ret = client.Link();
			printf("%s(%d):<%s> ret=%d client:%d\n", __FILE__, __LINE__, __FUNCTION__, ret, (int)client);

		}
		ret = client.Send(info);
		printf("%s(%d):<%s> ret=%d client:%d\n", __FILE__, __LINE__, __FUNCTION__, ret, (int)client);

	}

	static Buffer GetTimeStr(){
		Buffer result(128);
		timeb tmb;
		ftime(&tmb);
		tm* pTm = localtime(&tmb.time);
		// 格式化时间戳和日志消息
		int nSize = snprintf(result, sizeof(result),
			"%04d-%02d-%02d_%02d-%02d-%02d.%03d",
			pTm->tm_year + 1900, pTm->tm_mon + 1, pTm->tm_mday,
			pTm->tm_hour, pTm->tm_min, pTm->tm_sec, tmb.millitm
		);
		result.resize(nSize);
		return result;
	}

protected:
	int ThreadFunc() const;

	/**
	 * 写日志
	 * @param data
	 */
	void WriteLog(const Buffer& data) const{
		if(m_file != nullptr){
			FILE* pFile = m_file;
			fwrite(static_cast<char*>(data), 1, data.size(), pFile);
			fflush(pFile);
#ifdef _DEBUG
			printf("%s\n", data.data());
#endif
		}
	}

private:
	CThread m_thread;
	CEpoll m_epoll;
	CSocketBase* m_server;
	Buffer m_path;
	FILE* m_file;
};

#ifndef TRACE

#define TRACEI(...) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_INFO,__VA_ARGS__))
#define TRACED(...) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_DEBUG,__VA_ARGS__))
#define TRACEW(...) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_WARING,__VA_ARGS__))
#define TRACEE(...) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_ERROR,__VA_ARGS__))
#define TRACEF(...) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_FATAL,__VA_ARGS__))

//LOGI<<"hello"<<"world";
#define LOGI LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_INFO)
#define LOGD LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_DEBUG)
#define LOGW LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_WARING)
#define LOGE LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_ERROR)
#define LOGF LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_FATAL)

//内存导出
//00 01 02 65 .... ;...a .
#define DUMPI(data, size) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(),  LOG_INFO, data, size))
#define DUMPD(data, size) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_DEBUG, data, size))
#define DUMPW(data, size) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_WARING, data, size))
#define DUMPE(data, size) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_ERROR, data, size))
#define DUMPF(data, size) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_FATAL, data, size))

#endif
