#include "Server.h"
#include "Logger.h"

CServer::CServer(){
	m_server = nullptr;
	m_business = nullptr;
}

int CServer::Init(CBusiness* business, const Buffer& ip, short port) {
	TRACEI("CServer::Init");

	int ret{ 0 };
	if(business == nullptr) return -1;
	m_business = business;
	ret = m_process.SetEntryFunction(&CBusiness::BusinessProcess,m_business,&m_process);
	if(ret != 0)return -2;
	ret = m_process.CreateSubProcess();
	if(ret != 0)return -3;
	ret = m_pool.Start(8);
	if(ret != 0)return -4;
	ret = m_epoll.Create(8);
	if(ret != 0)return -5;

	// 初始化服务器 socket 对象
	m_server = new CSocket();
	if(m_server == nullptr) return -6;
	ret = m_server->Init(CSockParam(ip, port, SOCK_IS_SERVER | SOCK_IS_IP| SOCK_IS_REUSE));
	TRACEI("m_server->Init:%s",ret == 0 ? "success" : "error");

	if(ret != 0)return -7;
	ret = m_epoll.Add(*m_server,EpollData((void*)m_server));
	if(ret != 0)return -8;

	for(size_t i = 0; i < m_pool.Size(); i++){
		ret = m_pool.AddTask(&CServer::ThreadFunc, this);
		if(ret != 0) return -9;
	}
	TRACEI("m_pool.AddTask(&CServer::ThreadFunc):%s", ret == 0 ? "success" : "error");
	return 0;
}

int CServer::Run() const {
	TRACEI("<<<<<<< CServer::Run() >>>>>>>");

	while (m_server != nullptr){
		usleep(10);
	}
	return 0;
}

int CServer::Close() {
	TRACEI("<<<<<<< CServer::Close() >>>>>>>");

	if(m_server){
		CSocketBase* sock = m_server;
		m_server = nullptr;
		m_epoll.Del(*sock);
		delete sock;
	}
	m_epoll.Close();
	m_process.SendFD(-1);
	m_pool.Close();
	return 0;
}

//服务器处理 I/O 事件 的函数
int CServer::ThreadFunc() {
	TRACEI("server IO epoll:%d server:%p", (int)m_epoll, m_server);
	int ret{ 0 };
	EP_EVENTS events;
	while((m_epoll != -1) &&(m_server != nullptr)){
		const ssize_t size = m_epoll.WaitEvents(events,500);
		if(size < 0){
			break;
		}
		if(size > 0){
			TRACEI("I/O size:%d event:%08X", size,events[0].events);

			for(size_t i = 0; i < static_cast<size_t>(size); i++){
				if(events[i].events&EPOLLERR) break;
				if(events[i].events & EPOLLIN){ //有可读消息 表示有连接
					if(m_server){
						CSocketBase* pClient{ nullptr };
						ret = m_server->Link(&pClient);// 处理客户端连接
						TRACEI("**** m_server->Link(&pClient) ret:%d", ret);
						if(ret != 0) continue;
						//将接收的客户端 socket 通过子进程管道发送给业务模块。
						ret = m_process.SendSocket(*pClient,*pClient);
						TRACEI("**** SendSocket ret:%d", ret);
						if(ret != 0){
							TRACEE("**** send client %d failed",(int)*pClient);
							delete pClient;
							continue;
						}
						delete pClient;

					}
				}
			}
		}
	}
	TRACEI("服务器终止!");
	return 0;
}
