#pragma once
#include <functional>

#include "Socket.h"

class CFunctionBase //脱离模板创建成员变量
{
public:
	virtual ~CFunctionBase() = default;
	virtual int operator()(){ return -1; }
	virtual int operator()(CSocketBase*) { return -1; }
	virtual int operator()(CSocketBase*,const Buffer&) { return -1; }
protected:
private:
};

template<typename _FUNCTION_, typename... _ARGS_>
class CFunction : public CFunctionBase {
public:
	CFunction(_FUNCTION_ func, _ARGS_... args)
		:m_binder(std::forward<_FUNCTION_>(func), std::forward<_ARGS_>(args)...) {
	}
	~CFunction() override = default;

	int operator()() override {
		return m_binder();// 调用绑定的函数对象
	}
	typename std::_Bindres_helper<int, _FUNCTION_, _ARGS_...>::type m_binder;

protected:
private:
};