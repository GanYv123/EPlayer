#pragma once
#include <unistd.h>

#include "Epoll.h"
#include "Socket.h"
#include "Thread.h"

class CThreadPool
{
public:
	CThreadPool() {
		m_server = nullptr;
		timespec tp{ 0,0 };
		clock_gettime(CLOCK_REALTIME, &tp);
		char* buf{ nullptr };
		asprintf(&buf, "%ld.%ld.sock",tp.tv_sec%100000,tp.tv_nsec%1000000);
		if(buf!=nullptr){
			m_path = buf;
			free(buf);
		}
		//todo:问题处理在start接口里面判断m_path来解决问题
		usleep(1);
	}
	~CThreadPool() {
		Close();
	}
	CThreadPool(const CThreadPool&) = delete;
	CThreadPool& operator=(const CThreadPool&) = delete;

public:
	int Start(const unsigned count) {
		int ret{ 0 };
		if(m_server != nullptr) return -1;//已经初始化
		if(m_path.empty()) return -2;//构造失败
		m_server = new CLocalSocket();
		if(m_server == nullptr) return -3;
		ret = m_server->Init(CSockParam(m_path,SOCK_IS_SERVER));
		if(ret != 0) return -4;
		m_epoll.Create(count);
		if(ret != 0) return -5;
		m_epoll.Add(*m_server, EpollData((void*)m_server));
		if(ret != 0) return -6;
		m_threads.resize(count);
		for(unsigned i = 0; i < count; i++){
			m_threads[i] = new CThread(&CThreadPool::TaskDispatch, this);
			if(m_threads[i] == nullptr) return -7;
			ret = m_threads[i]->Start();
			if(ret != 0) return -8;
		}
		return 0;
	}
	void Close() {
		m_epoll.Close();
		if(m_server){
			const CSocketBase* p = m_server;
			m_server = nullptr;
			delete p;
		}
		for(auto thread : m_threads){
			delete thread;
		}
		m_threads.clear();
		unlink(m_path);
	}
	template<typename _FUNCTION_,typename... _ARGS_>
	int AddTask(_FUNCTION_ func, _ARGS_... args) {
		//每个线程在访问这个变量时，都会有自己的独立副本。
		//这意味着一个线程对这个变量的修改不会影响到其他线程中的同名变量。
		static  thread_local CLocalSocket client;
		int ret{ 0 };
		if(client == -1){
			ret = client.Init(CSockParam(m_path, 0));
			if(ret != 0) return  -1;
			ret = client.Link();
			if(ret != 0) return -2;
		}
		const CFunctionBase* base = new CFunction<_FUNCTION_, _ARGS_...>(func, args...);
		if(base == nullptr) return -3;
		Buffer data(sizeof(CFunctionBase*));
		memcpy(data, &base, sizeof(CFunctionBase*));
		ret = client.Send(data);
		if(ret != 0){
			delete base;
			return -4;
		}
		return 0;
	}

protected:
	int TaskDispatch() {
		while (m_epoll != -1){
			EP_EVENTS EpEvents;
			int ret{ 0 };
			ssize_t eSize = m_epoll.WaitEvents(EpEvents);
			if(eSize > 0){
				for (ssize_t i = 0;i < eSize;i++){
					if(EpEvents[i].events & EPOLLIN){
						CSocketBase* pClient{ nullptr };
						if(EpEvents[i].data.ptr == m_server){
							//服务器套接字收到输入（客户端来连接）
							ret = m_server->Link(&pClient);
							if(ret != 0) continue;
							m_epoll.Add(*pClient, EpollData((void*)pClient));
							if(ret != 0){//添加失败
								delete pClient;
								continue;
							}
						}else{//客户端接收到数据
							pClient = static_cast<CSocketBase*>(EpEvents[i].data.ptr);
							if(pClient){
								CFunctionBase* base{ nullptr };
								Buffer data(sizeof(CFunctionBase*));
								ret = pClient->Recv(data);
								if(ret <= 0){
									// ReSharper disable once CppExpressionWithoutSideEffects
									m_epoll.Del(*pClient);
									delete pClient;
									continue;
								}
								//ret > 0
								memcpy(&base, (char*)data, sizeof(CSocketBase*));
								if(base != nullptr){
									(*base)();
									delete base;
								}
							}
						}
					}
				}
			}
		}

		return 0;
	}

private:
	CEpoll m_epoll;
	std::vector<CThread*> m_threads;
	CSocketBase* m_server;
	Buffer m_path;
};
