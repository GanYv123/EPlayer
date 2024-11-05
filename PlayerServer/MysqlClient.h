#pragma once

#include <mysql/mysql.h>
#include "DatabaseHelper.h"

class CMysqlClient
	:public CDatabaseClient
{
public:
	CMysqlClient(const CMysqlClient&) = delete;
	CMysqlClient& operator=(const CMysqlClient&) = delete;
public:
	CMysqlClient() {
		bzero(&m_db, sizeof(m_db));
		m_bInit = false;
	}

	~CMysqlClient() override {
		Close();
	}
public:
	//连接
	int Connect(const KeyValue& args) override;
	//执行
	int Exec(const Buffer& sql) override;
	//带结果的执行
	int Exec(const Buffer& sql, Result& result, const _Table_& table) override;
	//开启事务
	int StartTransaction() override;
	//提交事务
	int CommitTransaction() override;
	//回滚事务
	int RollbackTransaction() override;
	//关闭连接
	int Close() override;
	//是否连接 true表示连接中 false表示未连接
	bool IsConnected() override;
private:
	MYSQL m_db;
	bool m_bInit;//默认是false 表示没有初始化 初始化之后，则为true，表示已经连接
private:
	class ExecParam
	{
	public:
		ExecParam(CMysqlClient* obj, Result& result, const _Table_& table)
			:obj(obj), result(result), table(table)
		{}
		CMysqlClient* obj;
		Result& result;
		const _Table_& table;
	};
};

class _mysql_table_ :
	public _Table_
{
public:
	_mysql_table_() :_Table_() {}
	_mysql_table_(const _mysql_table_& table);
	~_mysql_table_() override;
	//返回创建的SQL语句
	Buffer Create()override;
	//删除表
	Buffer Drop()override;
	//增删改查
	//TODO:参数进行优化
	Buffer Insert(const _Table_& values)override;
	Buffer Delete(const _Table_& values)override;
	//TODO:参数进行优化
	Buffer Modify(const _Table_& values)override;
	Buffer Query() override;
	//创建一个基于表的对象
	PTable Copy()const override;
	void ClearFieldUsed()override;
public:
	//获取表的全名
	operator const Buffer() const override;
};

class _mysql_field_ :
	public _Field_
{
public:
	_mysql_field_();
	_mysql_field_(
		int ntype,
		const Buffer& name,
		unsigned attr,
		const Buffer& type,
		const Buffer& size,
		const Buffer& default_,
		const Buffer& check
	);
	_mysql_field_(const _mysql_field_& field);
	~_mysql_field_() override;
	Buffer Create()override;
	void LoadFromStr(const Buffer& str)override;
	//where 语句使用的
	Buffer toEqualExp() const override;
	Buffer toSqlStr() const override;
	//列的全名
	operator const Buffer() const override;
private:
	Buffer Str2Hex(const Buffer& data) const;
	union
	{
		bool Bool;
		int Integer;
		double Double;
		Buffer* String;

	}Value;
	int nType;
};

#ifdef DECLARE_TABLE_CLASS
#undef DECLARE_TABLE_CLASS
#endif


#define DECLARE_TABLE_CLASS(name, base) class name:public base { \
public: \
virtual PTable Copy() const {return PTable(new name(*this));} \
name():base(){Name=#name;

#define DECLARE_MYSQL_FIELD(ntype,name,attr,type,size,default_,check) \
{PField field(new _mysql_field_(ntype, #name, attr, type, size, default_, check));FieldDefine.push_back(field);Fields[#name] = field; }

#define DECLARE_TABLE_CLASS_EDN() }};