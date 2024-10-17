#pragma once
#include "Function.h"
#include <cstdio>
#include <stdlib.h>
#include <memory.h>
#include <errno.h>    // 用于 errno
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

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
			ret = (*m_func)();
			exit(0);
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
		char buf[2][30] = { "edoyun" ,"zhaoxuyang" };
		iov[0].iov_base = buf[0];
		iov[0].iov_len = sizeof(buf[0]);
		iov[1].iov_base = buf[1];
		iov[1].iov_len = sizeof(buf[1]);
		msg.msg_iov = iov;
		msg.msg_iovlen = 2;
		//下面是要传入的数据
		const auto cmsg = static_cast<cmsghdr*>(calloc(1, CMSG_LEN(sizeof(int))));
		if (cmsg == nullptr) return -1;
		cmsg->cmsg_len = CMSG_LEN(sizeof(int));
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SCM_RIGHTS;
		*(int*)CMSG_DATA(cmsg) = fd;
		msg.msg_control = cmsg;
		msg.msg_controllen = cmsg->cmsg_len;

		printf("%s(%d):<%s> fd=%d\n", __FILE__, __LINE__, __FUNCTION__, fd);
		printf("%s(%d):<%s> pipes[1]=%d\n", __FILE__, __LINE__, __FUNCTION__, pipes[1]);
		printf("%s(%d):<%s> msg size:=%lu msg_addr:%8p\n", __FILE__, __LINE__, __FUNCTION__, sizeof(msg), &msg);
		const ssize_t ret = sendmsg(pipes[1], &msg, 0);
		printf("%s(%d):<%s> ret=%ld\n", __FILE__, __LINE__, __FUNCTION__, ret);

		free(cmsg);
		if (ret == -1) {
			return -2;
		}
		return 0;
	}

	int RecvFD(int& fd) {
		msghdr msg;
		iovec iov[2];
		char buf[2][30]{ "","" };
		iov[0].iov_base = buf[0];
		iov[0].iov_len = sizeof(buf[0]);
		iov[1].iov_base = buf[1];
		iov[1].iov_len = sizeof(buf[1]);
		msg.msg_iov = iov;
		msg.msg_iovlen = 2;

		const auto cmsg = static_cast<cmsghdr*>(calloc(1, CMSG_LEN(sizeof(int))));
		if (cmsg == nullptr) return -1;
		cmsg->cmsg_len = CMSG_LEN(sizeof(int));
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SCM_RIGHTS;
		msg.msg_control = cmsg;
		msg.msg_controllen = cmsg->cmsg_len;

		ssize_t ret = recvmsg(pipes[0], &msg, 0);
		if (ret == -1) {
			free(cmsg);
			printf("%s(%d):<%s> recvmsg:ret=%ld\n", __FILE__, __LINE__, __FUNCTION__, ret);
			return -2;
		}
		fd = *reinterpret_cast<int*>(CMSG_DATA(cmsg));
		free(cmsg);

		return 0;
	}

	static int SwitchDaemon() {
		// 1. Fork 子进程
		pid_t ret = fork();
		if (ret == -1) return -1;
		if (ret > 0) exit(0); // 2. 主进程到此退出
		// 3. 子进程 setsid 创建一个新的会话，并使子进程成为该会话的首进程
		ret = setsid();
		if (ret == -1) return -2;
		// 4. 立即再 fork 一次，确保子进程不能再获取控制终端
		ret = fork();
		if (ret == -1) return -3;
		if (ret > 0) exit(0); // 5. 第二次 fork 的子进程退出，孙进程继续
		// 6. 设置信号处理，忽略 SIGCHLD
		signal(SIGCHLD, SIG_IGN);
		// 7. 更改当前工作目录为根目录，避免阻止文件系统卸载
		/*
		if (chdir("/") == -1) {
			return -4;
		}
		//*/
		// 8. 关闭标准输入、输出和错误输出
		for (int i = 0; i < 3; ++i) close(i);
		// 9. 清除文件权限掩码
		umask(0);
		// 守护进程启动成功
		return 0;
	}

private:
	CFunctionBase* m_func;
	pid_t m_pid;
	int pipes[2];
};
