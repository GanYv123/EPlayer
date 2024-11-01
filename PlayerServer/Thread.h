#pragma once
#include <cstdio>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <asm-generic/errno.h>
#include <csignal>
#include <map>
#include "Function.h"

class CThread
{
public:
	CThread() {
		m_function = nullptr;
		m_thread = 0;
		m_bPaused = false;
	}
	template<typename _FUNCTION_, typename... _ARGS_>
	CThread(_FUNCTION_ func,_ARGS_... args)
		:m_function(new CFunction<_FUNCTION_,_ARGS_...>(func,args...))
	{
		m_thread = 0;
		m_bPaused = false;
	}
	~CThread() = default;

public:
	CThread(const CThread&) = delete;
	CThread& operator=(const CThread&) = delete;
public:
	template<typename _FUNCTION_, typename... _ARGS_>
	int SetThreadFunc(_FUNCTION_ func, _ARGS_... args) {
		m_function(new CFunction<_FUNCTION_, _ARGS_...>(func, args...));
		if (m_function == nullptr) return -1;
		return 0;
	}
	int Start() {
		pthread_attr_t attr;
		int ret{ -1 };
		ret = pthread_attr_init(&attr);
		if (ret != 0) return -1;
		ret = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
		if (ret != 0) return -2;
		//ret = pthread_attr_setscope(&attr, PTHREAD_SCOPE_PROCESS);
		//if (ret != 0) return -3;
		ret = pthread_create(&m_thread, &attr, &CThread::ThreadEntry, this);
		if (ret != 0) return -4;
		m_mapThread[m_thread] = this;
		ret = pthread_attr_destroy(&attr);
		if (ret != 0) return -5;
		return 0;
	}
	int Pause() {
		if (m_thread != 0)return -1;
		if (m_bPaused){
			m_bPaused = false;
			return 0;
		}
		m_bPaused = true;
		const int ret = pthread_kill(m_thread, SIGUSR1);
		if (ret != 0){
			m_bPaused = false;
			return -2;
		}
		return 0;
	}
	int Stop() {
		if(m_thread != 0){
			pthread_t thread = m_thread;
			m_thread = 0;
			timespec ts;
			ts.tv_sec = 0;
			ts.tv_nsec = 100 * 100000;//100ms
			const int ret = pthread_timedjoin_np(thread, nullptr, &ts);//等待线程 thread 的结束
			if(ret == ETIMEDOUT){
				pthread_detach(thread);
				pthread_kill(thread, SIGUSR2);
			}
		}
		return 0;
	}
	//判断线程是否有效
	bool IsValid() const { return m_thread != 0; }
protected:
	//__stdcall
	static void* ThreadEntry(void* arg) {
		const auto thiz = static_cast<CThread*>(arg);
		struct sigaction act = { 0 };
		sigemptyset(&act.sa_mask);
		act.sa_flags = SA_SIGINFO;
		act.sa_sigaction = &CThread::Sigaction;
		sigaction(SIGUSR1, &act, nullptr);
		sigaction(SIGUSR2, &act, nullptr);

		thiz->EntryThread();
		if (thiz->m_thread) thiz->m_thread = 0;

		const pthread_t thread = pthread_self(); //不是冗余 有可能被stop函数把m_thread清零
		const auto it = m_mapThread.find(thread);
		if(it != m_mapThread.end()){
			m_mapThread[thread] = nullptr;
		}
		pthread_detach(thread);
		pthread_exit(nullptr);
	}
	
	static void Sigaction(int signo,siginfo_t* info, void*context) {
		if (signo == SIGUSR1) {
			const pthread_t thread = pthread_self();
			auto it = m_mapThread.find(thread);
			if(it != m_mapThread.end()){
				if(it->second){
					while (it->second->m_bPaused){
						if (it->second->m_thread == 0) pthread_exit(nullptr);
						usleep(1000);//1ms
					}
				}
			}
		}
		else if(signo == SIGUSR2){
			//todo:线程退出
			pthread_exit(nullptr);
		}
	}

	void EntryThread() const {//__thiscall
		if(m_function!=nullptr){
			const int ret = (*m_function)();
			if(ret != 0){
				printf("%s(%d):<%s>ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
			}
		}
	}
private:
	CFunctionBase* m_function;
	pthread_t m_thread;
	bool m_bPaused;//true{暂停} false{运行中}
	static std::map<pthread_t, CThread*> m_mapThread;
};

/*
 *AI 对该代码的分析：
 *
 *std::map的线程安全问题：m_mapThread 是静态的，而静态成员变量在多个线程中是共享的。你在多个线程中对 m_mapThread 进行插入、查找和删除操作，这会导致数据竞争问题。需要使用互斥锁（如 pthread_mutex_t 或 C++11 的 std::mutex）来保护对 m_mapThread 的访问。

pthread_t 比较问题：pthread_t 类型在不同的平台上可能是不同的类型（如整型或指针类型）。直接将 pthread_t 作为键放在 std::map 中可能导致兼容性问题，最好明确类型转换或者在特定平台上测试其可行性。

资源释放问题：你在 CThread 构造函数和 SetThreadFunc 中创建了一个 CFunction 对象（使用 new），但没有在析构函数或其他地方释放它，可能会导致内存泄漏。需要在析构函数中加上相应的清理逻辑。

信号处理的线程安全：你使用 sigaction 注册信号处理函数，在处理信号时需要确保对共享资源的访问是线程安全的。信号处理函数通常非常简单，避免调用非异步信号安全的函数如 printf 或访问共享资源。

Pause 函数的逻辑问题：你在 Pause 函数中调用 pthread_kill 来发送信号暂停线程，但如果线程已暂停（m_bPaused 为 true），你直接返回 0。你可能需要考虑在这种情况下再次启动线程的逻辑。

IsValid 函数的逻辑：你的 IsValid 函数返回 m_thread == 0 来判断线程是否有效，但从逻辑上看，当线程为 0 时，应该是无效的。你可能想反转返回值。
*/