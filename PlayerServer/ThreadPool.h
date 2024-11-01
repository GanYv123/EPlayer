#pragma once
#include <unistd.h>
#include "Epoll.h"
#include "Socket.h"
#include "Thread.h"


/**
 * 任务调度的完整流程:
 * 进程A 创建线程池对象是创建了一个server套接字（根据时间戳生成的唯一套接字）
 * 1.AddTask添加任务:通过创建的线程池的对象调用AddTask传入函数指针和参数
 * 为每个线程分派 TaskDispatch 作为线程执行函数，将本对象当作参数传入,之后启动线程
 * 这样开启的每个线程都有自己的一个独立的 local_socket
 *
 * 2.任务接收:线程池中维护一个 epoll TaskDispatch 利用c++11的新特性，将函数和参数绑定成一个对象
 * 序列化发送给线程，线程再调用这个函数
 *
 * 3.任务执行:获取到对象后直接调用即可
 */
class CThreadPool
{
public:
	//初始化线程池 
	CThreadPool() {
		m_server = nullptr;
		//根据时间戳创建本地套接字 供进程间通信
		timespec tp{ 0,0 };
		clock_gettime(CLOCK_REALTIME, &tp);
		char* buf{ nullptr };
		asprintf(&buf, "%ld.%ld.sock",tp.tv_sec%100000,tp.tv_nsec%1000000);
		if(buf!=nullptr){
			m_path = buf;
			free(buf);
		}
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
		if(m_server != nullptr) return -1;//已启动 防止重复启动
		if(m_path.empty()) return -2;//构造失败
		m_server = new CSocket(); //创建套接字
		if(m_server == nullptr) return -3;
		//根据获取到的时间戳来创建套接字，确保大概率的唯一性
		ret = m_server->Init(CSockParam(m_path,SOCK_IS_SERVER));
		if(ret != 0) return -4;
		m_epoll.Create(count);
		if(ret != 0) return -5;
		//将套接字添加到epoll监控中
		m_epoll.Add(*m_server, EpollData((void*)m_server));
		if(ret != 0) return -6;
		m_threads.resize(count);//设置存储线程对象的容器的大小(std::vector<CThread*>)
		for(unsigned i = 0; i < count; i++){
			//创建线程 为每个线程分派 TaskDispatch 作为线程执行函数
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
		for(const auto thread : m_threads){
			delete thread;
		}
		m_threads.clear();
		unlink(m_path);
	}

	//供外部模块 通过调用线程池对象 来添加任务 (函数指针 + 参数)
	template<typename _FUNCTION_,typename... _ARGS_>
	int AddTask(_FUNCTION_ func, _ARGS_... args) {
		//每个线程在访问这个变量时，都会有自己的独立副本。
		//这意味着一个线程对这个变量的修改不会影响到其他线程中的同名变量。
		static  thread_local CSocket client; //每个线程独立维护自己的 client
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

	size_t Size()const {
		return m_threads.size();
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
