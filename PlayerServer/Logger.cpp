#include "Logger.h"

LogInfo::LogInfo(
	const char* file, int line,
	const char* func,
	pid_t pid, pthread_t tid,
	const int level, const char* fmt, ...)
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
	m_buf += '\n';
	va_end(ap);
	//1
}

LogInfo::LogInfo(const char* file, int line,
	const char* func,
	pid_t pid, pthread_t tid,
	const int level)
{//自己主动发 Stream 式日志
	bAuto = true;

	constexpr char sLevel[][8] = {
		"INFO","DEBUG","WARNING","ERROR","FATAL"
	};
	char* buf = nullptr;
	const int count = asprintf(
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
	const int level, void* pData, const size_t nSize)
{
	constexpr char sLevel[][8] = {
		"INFO", "DEBUG", "WARNING", "ERROR", "FATAL"
	};

	bAuto = false;

	// 临时指针，用于接收 asprintf 动态分配的内存
	char* buf = nullptr;

	// 缓存时间字符串以避免重复调用
	auto timeStr = CLoggerServer::GetTimeStr();

	// 调用 asprintf 格式化日志头信息
	int count = asprintf(&buf, "%s(%d):[%s][%s]<%d-%d>(%s)\n",
		file, line, sLevel[level],
		static_cast<char*>(timeStr), pid, tid, func);

	if(count > 0){
		// 如果 asprintf 成功，将 buf 中的内容追加到 m_buf
		m_buf += buf;
		// 释放动态分配的内存
		free(buf);
	} else{
		// 如果 asprintf 失败，直接返回
		return;
	}

	auto Data = static_cast<char*>(pData);
	size_t bufSize = nSize * 4;  // 预估缓冲区大小，保证足够存储十六进制和可视化字符
	std::string hexBuffer;
	hexBuffer.reserve(bufSize);  // 避免多次内存重新分配

	for(size_t i = 0; i < nSize; i++){
		char hex[4];  // 仅需 3 个字符空间: "XX "
		snprintf(hex, sizeof(hex), "%02X ", Data[i] & 0xFF);
		hexBuffer += hex;

		if(0 == (i + 1) % 16){
			hexBuffer += "\t;";  // 行结束符

			char ascii[17] = { 0 };  // 用于存储 ASCII 表示的字符
			for(size_t j = 0; j < 16; j++){
				unsigned char ch = Data[i - 15 + j];
				ascii[j] = (ch >= 32 && ch <= 126) ? ch : '.';  // 可打印字符范围：32-126
			}
			hexBuffer += ascii;
			hexBuffer += '\n';
		}
	}

	// 如果有剩余未满16字节的数据，也要处理
	const size_t remainder = nSize % 16;
	if(remainder > 0){
		// 填充剩余部分
		for(size_t i = remainder; i < 16; i++){
			hexBuffer += "   ";  // 每个字节三个字符："XX "
		}
		hexBuffer += "\t;";
		for(size_t i = 0; i < remainder; i++){
			const unsigned char ch = Data[nSize - remainder + i];
			hexBuffer += (ch >= 32 && ch <= 126) ? ch : '.';
		}
		hexBuffer += '\n';
	}

	// 将格式化的 hexBuffer 附加到 m_buf
	m_buf += hexBuffer;
}


LogInfo::~LogInfo(){
	if(bAuto){
		m_buf += '\n';
		CLoggerServer::Trace(*this);
	}
}

CLoggerServer::CLoggerServer()
	: m_thread(&CLoggerServer::ThreadFunc, this), m_file(nullptr)
{
	m_server = nullptr;
	char curPath[256]{ "" };
	getcwd(curPath, sizeof(curPath));
	m_path = curPath;
	m_path += "/log/" + GetTimeStr() + ".log";
	printf("%s(%d):<%s> path=%s\n", __FILE__, __LINE__, __FUNCTION__, m_path.data());
}

int CLoggerServer::ThreadFunc() const {
	printf("%s(%d):<%s> IsValid:%d\n", __FILE__, __LINE__, __FUNCTION__,m_thread.IsValid());
	printf("%s(%d):<%s> m_epoll:%d\n", __FILE__, __LINE__, __FUNCTION__,(int)m_epoll);
	printf("%s(%d):<%s> m_server:%p\n", __FILE__, __LINE__, __FUNCTION__,m_server);

	EP_EVENTS events;
	std::map<int, CSocketBase*> mapClient;
	while(m_thread.IsValid() && (m_epoll != -1) && (m_server != nullptr)){
		const ssize_t ret = m_epoll.WaitEvents(events, 1000);
		printf("%s(%d):<%s> ret=%ld\n", __FILE__, __LINE__, __FUNCTION__, ret);
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
						printf("%s(%d):<%s> Link_r=%d\n", __FILE__, __LINE__, __FUNCTION__, r);

						if(r < 0) continue;
						r = m_epoll.Add(*pClient, EpollData((void*)pClient), EPOLLIN | EPOLLERR);
						printf("%s(%d):<%s> Add()_r=%d\n", __FILE__, __LINE__, __FUNCTION__, r);

						if(r < 0){
							delete pClient;
							mapClient[*pClient] = nullptr;
							continue;
						}
						const auto it = mapClient.find(*pClient);
						if(it != mapClient.end()){
							if(it->second)
								delete it->second;
						}
						mapClient[*pClient] = pClient;
						printf("%s(%d):<%s>\n", __FILE__, __LINE__, __FUNCTION__);

					} else{ //客户端
						printf("%s(%d):<%s> ptr=%p\n", __FILE__, __LINE__, __FUNCTION__, events[i].data.ptr);

						const auto pClient = static_cast<CSocketBase*>(events[i].data.ptr);
						if(pClient != nullptr){
							Buffer data(1024 * 1024);
							const int r = pClient->Recv(data);
							printf("%s(%d):<%s> Recv_r=%d\n", __FILE__, __LINE__, __FUNCTION__, r);

							if(r <= 0){ //失败
								mapClient[*pClient] = nullptr;
								delete pClient;
							} else{
								printf("%s(%d):<%s> WriteLog=%s\n", __FILE__, __LINE__, __FUNCTION__, static_cast<char*>(data));
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
