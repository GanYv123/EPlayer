#include <cstdio>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <functional>
#include <memory.h>
#include <sys/socket.h>

class CFunctionBase //脱离模板创建成员变量
{
public:
	virtual ~CFunctionBase() {}
	virtual int operator()() = 0;
protected:
private:
};

template<typename _FUNCTION_, typename... _ARGS_>
class CFunction : public CFunctionBase {
public:
	CFunction(_FUNCTION_ func, _ARGS_... args) 
		:m_binder(std::forward<_FUNCTION_>(func), std::forward<_ARGS_>(args)...)
	{
	}
	virtual ~CFunction() {
	}
	virtual int operator()() {
		return m_binder();// 调用绑定的函数对象
	}
	typename std::_Bindres_helper<int, _FUNCTION_, _ARGS_...>::type m_binder;

protected:
private:
};

class CProcess {
public:
	CProcess() {
		m_func = nullptr;
		memset(pipes, 0, sizeof(pipes));
	}
	~CProcess() {
		if (m_func != nullptr) {
			delete m_func;
			m_func = nullptr;
		}
	}

	//为 CProcess 对象指定不同的入口函数
	template<typename _FUNCTION_, typename... _ARGS_>
	int SetEntryFunction(_FUNCTION_ func, _ARGS_... args) {
		m_func = new CFunction<_FUNCTION_, _ARGS_...>(func, args...);

		return 0;
	}

	//启动子进程执行该函数
	int CreateSubProcess() {
		if (m_func == nullptr) return -1;
		int ret = socketpair(AF_LOCAL, SOCK_STREAM, 0, pipes);
		if (ret == -1) return -2;
		pid_t pid = fork();
		if (pid == -1) return -3;
		if (pid == 0) {//子进程
			close(pipes[1]);//关闭 写
			pipes[1] = 0;
			return (*m_func)();
		}
		//主进程
		close(pipes[0]);//关闭 读
		pipes[0] = 0;
		m_pid = pid;//记录主进程pid
		return 0;
	}

	int SendFD(int fd) {//主进程完成
		struct msghdr msg;
		iovec iov[2];
		iov[0].iov_base = (char*)"edoyun";
		iov[0].iov_len = 7;
		iov[1].iov_base = (char*)"zhaoxuyang";
		iov[1].iov_len = 11;
		msg.msg_iov = iov;
		msg.msg_iovlen = 2;
		//下面是要传入的数据
		cmsghdr* cmsg = (cmsghdr*)calloc(1, CMSG_LEN(sizeof(int)));
		if (cmsg == nullptr) return -1;
		cmsg->cmsg_len = CMSG_LEN(sizeof(int));
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SCM_RIGHTS;
		*(int*)CMSG_DATA(cmsg) = fd;
		msg.msg_control = cmsg;
		msg.msg_controllen = cmsg->cmsg_len;
		ssize_t ret = sendmsg(pipes[1], &msg, 0);

		free(cmsg);
		if (ret == -1) {
			return -2;
		}
		return 0;
	}

	int RecvFD(int& fd) {
		msghdr msg;
		iovec iov[2];
		char buf[][10]{ "","" };
		iov[0].iov_base = buf[0];
		iov[0].iov_len = sizeof(buf[0]);
		iov[1].iov_base = buf[1];
		iov[1].iov_len = sizeof(buf[1]);
		msg.msg_iov = iov;
		msg.msg_iovlen = 2;

		cmsghdr* cmsg = (cmsghdr*)calloc(1, CMSG_LEN(sizeof(int)));
		if (cmsg == nullptr) return -1;
		cmsg->cmsg_len = CMSG_LEN(sizeof(int));
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SCM_RIGHTS;
		msg.msg_control = cmsg;
		msg.msg_controllen = CMSG_LEN(sizeof(int));
		ssize_t ret = recvmsg(pipes[0], &msg, 0);
		free(cmsg);
		if (ret == -1) {
			return -2;
		}
		fd = *(int*)CMSG_DATA(cmsg);
		return 0;
	}

private:
	CFunctionBase* m_func;
	pid_t m_pid;
	int pipes[2];
};

int CreateLogServer(CProcess* proc) {
	return 0;
}

int CreateClientSerevr(CProcess* proc) {
	return 0;
}

int main() {
	CProcess procLog, procClients;
	procLog.SetEntryFunction(CreateLogServer, &procLog);
	int ret = procLog.CreateSubProcess();
	if (ret !=0 ) {
		return -1;
	}

	procClients.SetEntryFunction(CreateClientSerevr, &procClients);
	int ret = procClients.CreateSubProcess();
	if (ret != 0) {
		return -2;
	}
	return 0;
}