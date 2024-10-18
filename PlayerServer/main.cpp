#include "Function.h"
#include "Process.h"
#include "Logger.h"
#include "ThreadPool.h"

int CreateLogServer(CProcess* proc) {
	CLoggerServer server;
	int ret = server.Start();
	if(ret != 0){
		printf("%s(%d):<%s> pid=%d ret=%d error(%d) msg:%s\n",
			__FILE__, __LINE__, __FUNCTION__, getpid(), ret, errno, strerror(errno));

	}
	int fd{ 0 };
	while(true){
		ret = proc->RecvFD(fd);
		printf("%s(%d):<%s> recvmsg:ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
		if(fd <= 0) break;
	}
	ret = server.Close();
	printf("%s(%d):<%s> recvmsg:ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);

	return 0;
}

int CreateClientServer(CProcess* proc) {
	printf("%s(%d):<%s> pid=%d\n", __FILE__, __LINE__, __FUNCTION__, getpid());
	int fd = -1;
	proc->RecvFD(fd);
	//printf("%s(%d):<%s> RecvFD ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
	//printf("%s(%d):<%s> fd=%d\n", __FILE__, __LINE__, __FUNCTION__, fd);
	sleep(1);
	char buf[30]{ "" };
	lseek(fd, 0, SEEK_SET);
	read(fd, buf, sizeof(buf));
	//printf("%s(%d):<%s> buf=%s\n", __FILE__, __LINE__, __FUNCTION__, buf);
	close(fd);
	return 0;
}

int LogTest() {
	char buffer[]{ "hello zxy!你好@世界" };
	usleep(1000 * 100);//100ms
	TRACEI("here is log %d %c %f %g %s ~你好~世界~!", 10, 'A', 1.0f, 2.0, buffer);
	DUMPD(static_cast<void*>(buffer), sizeof(buffer));
	LOGE << 100 << " " << 'S' << " " << 0.12345f << " " << 1.23456789 << " " << buffer << "你好世界！@";
	return 0;
}

int main() {
	//CProcess::SwitchDaemon();
	CProcess procLog, procClients;
	printf("%s(%d):<%s> pid=%d\n", __FILE__, __LINE__, __FUNCTION__, getpid());
	procLog.SetEntryFunction(CreateLogServer, &procLog);
	int ret = procLog.CreateSubProcess();
	if(ret != 0){
		printf("%s(%d):<%s> pid=%d\n", __FILE__, __LINE__, __FUNCTION__, getpid());
		return -1;
	}
	LogTest();
	printf("%s(%d):<%s> pid=%d\n", __FILE__, __LINE__, __FUNCTION__, getpid());
	//线程中测试日志
	CThread thread(LogTest);
	thread.Start();

	procClients.SetEntryFunction(CreateClientServer, &procClients);
	ret = procClients.CreateSubProcess();
	if(ret != 0){
		printf("%s(%d):<%s> pid=%d\n", __FILE__, __LINE__, __FUNCTION__, getpid());
		return -2;
	}
	printf("%s(%d):<%s> pid=%d\n", __FILE__, __LINE__, __FUNCTION__, getpid());
	usleep(100 * 000);
	int fd = open("./test.txt", O_RDWR | O_CREAT | O_APPEND);
	printf("%s(%d):<%s> fd=%d\n", __FILE__, __LINE__, __FUNCTION__, fd);
	if(fd == -1){
		return -3;
	}
	ret = procClients.SendFD(fd);
	printf("%s(%d):<%s> SendFD ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
	if(ret != 0)printf("errno:%d msg:%s\n", errno, strerror(errno));
	write(fd, "zxy0d000721", strlen("zxy0d000721"));
	close(fd);
	//test ThreadPoll>>
	CThreadPool pool;
	ret = pool.Start(4);
	if(ret != 0)printf("errno:%d msg:%s\n", errno, strerror(errno));
	ret = pool.AddTask(LogTest);
	if(ret != 0)printf("errno:%d msg:%s\n", errno, strerror(errno));
	ret = pool.AddTask(LogTest);
	if(ret != 0)printf("errno:%d msg:%s\n", errno, strerror(errno));
	ret = pool.AddTask(LogTest);
	if(ret != 0)printf("errno:%d msg:%s\n", errno, strerror(errno));
	ret = pool.AddTask(LogTest);
	if(ret != 0)printf("errno:%d msg:%s\n", errno, strerror(errno));
	(void)getchar();
	pool.Close();
	//<<end test ThreadPoll
	procLog.SendFD(-1);
	(void)getchar();
	return 0;
}