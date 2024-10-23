#pragma once
#include "Logger.h"
#include "Server.h"
#include <map>

#define ERR_RETURN(ret,err) if(ret != 0){TRACEE("ret=%d errno=%d msg=<%s>", ret, errno, strerror(errno));return err;}

#define WARN_CONTINUE(ret) if(ret != 0){TRACEW("ret=%d errno=%d msg=<%s>", ret, errno, strerror(errno));continue;}

class CEPlayerServer : public CBusiness
{
public:
	CEPlayerServer(const unsigned count) :CBusiness() {
		m_count = count;
	}
	~CEPlayerServer() {
		m_epoll.Close();
		m_pool.Close();
		for (const auto it : m_mapClients){
			delete it.second;
		}
		m_mapClients.clear();
	}

	int BusinessProcess(CProcess* proc) override {
		using  namespace std::placeholders;
		int ret{ 0 };
		ret = SetConnectCallback(&CEPlayerServer::Connected, this, _1);
		ERR_RETURN(ret, -1)
			ret = SetRecvCallback(&CEPlayerServer::Received, this, _1, _2);
		ret = m_epoll.Create(m_count);
		ERR_RETURN(ret, -2)
		ret = m_pool.Start(m_count);
		ERR_RETURN(ret, -3)
		for(unsigned i = 0; i < m_count; i++){
			ret = m_pool.AddTask(&CEPlayerServer::ThreadFunc,this);
			ERR_RETURN(ret, -4)
		}
		int sock = 0;
		sockaddr_in addrin;
		while(m_epoll != -1){
			ret = proc->RecvSocket(sock,&addrin);
			if(ret < 0 || (sock == 0)) break;
			CSocketBase* pClient = new CSocket(sock);
			if(pClient == nullptr)continue;
			ret = pClient->Init(CSockParam(&addrin,SOCK_IS_IP));
			WARN_CONTINUE(ret)

			ret = m_epoll.Add(sock, EpollData((void*)pClient));

			if(m_connectedcallback){
				(*m_connectedcallback)(pClient);
			}
			WARN_CONTINUE(ret)
		}
		return 0;
	}

private:
	int Connected(CSocketBase* pClient) {

		return 0;
	}
	int Received(CSocketBase* pClient,const Buffer& data) {

		return 0;
	}
private:
	int ThreadFunc() {
		int ret{ 0 };
		EP_EVENTS events;
		while(m_epoll != -1){
			const ssize_t size = m_epoll.WaitEvents(events);
			if(size < 0){
				break;
			}
			if(size > 0){
				for(size_t i = 0; i < static_cast<size_t>(size); i++){
					if(events[i].events & EPOLLERR) break;
					if(events[i].events & EPOLLIN){
						const auto pClient{ static_cast<CSocketBase*>(events[i].data.ptr)};
						if(pClient){
							Buffer data;
							ret = pClient->Recv(data);
							WARN_CONTINUE(ret)
							if(m_recvedcallback){
								(*m_recvedcallback)(pClient,data);
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
	CThreadPool m_pool;
	std::map<int, CSocketBase*>m_mapClients;
	unsigned m_count;
};