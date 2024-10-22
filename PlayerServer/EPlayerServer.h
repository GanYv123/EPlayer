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
		int ret{ 0 };
		ret = m_epoll.Create(m_count);
		ERR_RETURN(ret, -1)
		ret = m_pool.Start(m_count);
		ERR_RETURN(ret, -2)
		for(unsigned i = 0; i < m_count; i++){
			ret = m_pool.AddTask(&CEPlayerServer::ThreadFunc,this);
			ERR_RETURN(ret, -3)
		}
		int sock = 0;
		while(m_epoll != -1){
			ret = proc->RecvFD(sock);
			if(ret < 0 || (sock == 0)) break;
			CSocketBase* pClient = new CSocket(sock);
			if(pClient == nullptr)continue;
			ret = m_epoll.Add(sock, EpollData((void*)pClient));
			if(m_connectedcallback){
				(*m_connectedcallback)();
			}
			WARN_CONTINUE(ret)
		}
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
								(*m_recvedcallback)();
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