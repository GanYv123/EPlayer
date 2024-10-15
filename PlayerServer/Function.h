#pragma once
#include <sys/types.h>
#include <unistd.h>
#include <functional>

class CFunctionBase //����ģ�崴����Ա����
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
		:m_binder(std::forward<_FUNCTION_>(func), std::forward<_ARGS_>(args)...) {
	}
	virtual ~CFunction() {
	}
	virtual int operator()() {
		return m_binder();// ���ð󶨵ĺ�������
	}
	typename std::_Bindres_helper<int, _FUNCTION_, _ARGS_...>::type m_binder;

protected:
private:
};
