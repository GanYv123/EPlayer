#include "Logger.h"

LogInfo::LogInfo(
	const char* file, int line,
	const char* func,
	pid_t pid, pthread_t tid,
	int level, const char* fmt, ...)
{
	constexpr char sLevel[][8] = {
		"INFO","DEBUG","WARNING","ERROR","FATAL"
	};
	char* buf = nullptr;
	bAuto = false;

	int count = asprintf(
		&buf, "%s(%d):[%s][%s]<%d-%d>(%s) ",
		file, line, sLevel[level],
		static_cast<char*>(CLoggerServer::GetTimeStr()), pid, tid, func
	);
	if(count > 0){
		m_buf = buf;
		free(buf);
	} else return;

	va_list ap;
	va_start(ap, fmt);
	count = vasprintf(&buf, fmt, ap);
	if(count > 0){
		m_buf += buf;
		free(buf);
	}
	va_end(ap);
	//1
}

LogInfo::LogInfo(const char* file, int line,
	const char* func,
	pid_t pid, pthread_t tid,
	int level)
{//自己主动发 Stream 式日志
	bAuto = true;

	constexpr char sLevel[][8] = {
		"INFO","DEBUG","WARNING","ERROR","FATAL"
	};
	char* buf = nullptr;
	int count = asprintf(
		&buf, "%s(%d):[%s][%s]<%d-%d>(%s) ",
		file, line, sLevel[level],
		static_cast<char*>(CLoggerServer::GetTimeStr()), pid, tid, func
	);
	if(count > 0){
		m_buf = buf;
		free(buf);
	}
}

LogInfo::LogInfo(const char* file, int line,
	const char* func,
	pid_t pid, pthread_t tid,
	int level, void* pData, size_t nSize)
{
	constexpr char sLevel[][8] = {
		"INFO","DEBUG","WARNING","ERROR","FATAL"
	};
	char* buf = nullptr;
	bAuto = false;
	int count = asprintf(
		&buf, "%s(%d):[%s][%s]<%d-%d>(%s)\n",
		file, line, sLevel[level],
		static_cast<char*>(CLoggerServer::GetTimeStr()), pid, tid, func
	);
	if(count > 0){
		m_buf = buf;
		free(buf);
	} else return;

	Buffer out;
	size_t i = 0;
	auto Data = static_cast<char*>(pData);
	for(; i < nSize; i++){
		char buf[16]{ "" };
		snprintf(buf, sizeof(buf), "%02X ", Data[i] & 0xFF);
		m_buf += buf;
		if(0 == (i + 1) % 16){
			m_buf += "\t;";
			for(size_t j = i - 15; j <= i; j++){
				if(((Data[j] & 0xFF) > 31) && ((Data[j] & 0xFF) < 0x7F)){
					m_buf += Data[i];
				} else{
					m_buf += '.';
				}
			}
			m_buf += '\n';
		}
	}
	//处理尾巴
	const size_t k = i % 16;
	if(k != 0){
		for(size_t j = 0; j < 16 - k; j++) m_buf += "   ";
		m_buf += "\t;";
		for(size_t j = i - k; j <= i; j++){
			if(((Data[j] & 0xFF) > 31) && ((Data[j] & 0xFF) < 0x7F)){
				m_buf += Data[i];
			} else{
				m_buf += '.';
			}
		}
	}
}

LogInfo::~LogInfo(){
	if(bAuto){
		CLoggerServer::Trace(*this);
	}
}

CLoggerServer::CLoggerServer()
	: m_thread(&CLoggerServer::ThreadFunc, this)
{
	m_server = nullptr;
	m_path = "./log/" + GetTimeStr() + ".log";
	printf("%s(%d):<%s> path=%s\n", __FILE__, __LINE__, __FUNCTION__, m_path.data());
}

int CLoggerServer::ThreadFunc(){
	EP_EVENTS events;
	std::map<int, CSocketBase*> mapClient;
	while(m_thread.IsValid() && (m_epoll != -1) && (m_server != nullptr)){
		const ssize_t ret = m_epoll.WaitEvents(events, 1);
		if(ret < 0)break;
		if(ret > 0){
			ssize_t i = 0;
			for(; i < ret; i++){
				if(events[i].events & EPOLLERR){
					break;
				}
				if(events[i].events & EPOLLIN){
					if(events[i].data.ptr == m_server){
						//服务器收到输入请求 <有客户端要来连接>
						CSocketBase* pClient{ nullptr };
						int r = m_server->Link(&pClient);
						if(r < 0) continue;
						r = m_epoll.Add(*pClient, EpollData((void*)pClient), EPOLLIN | EPOLLERR);
						if(r < 0){
							delete pClient;
							mapClient[*pClient] = nullptr;
							continue;
						}
						const auto it = mapClient.find(*pClient);
						if(it != mapClient.end() && it->second != nullptr){
							delete it->second;
						}
						mapClient[*pClient] = pClient;
					} else{ //客户端
						const auto pClient = static_cast<CSocketBase*>(events[i].data.ptr);
						if(pClient != nullptr){
							Buffer data(1024 * 1024);
							const int r = pClient->Recv(data);
							if(r <= 0){ //失败
								delete pClient;
								mapClient[*pClient] = nullptr;
							} else{
								WriteLog(data);
							}
						}
					}
				}
			}
			if(i != ret){
				break;
			}
		}
	}
	for(auto it = mapClient.begin(); it != mapClient.end(); ++it){
		if(it->second){
			delete it->second;
		}
	}
	mapClient.clear();
	return 0;
}