#pragma once
#include "Public.h"
#include "DatabaseHelper.h"
#include "sqlite3/sqlite3.h"

class CSqlite3Client
	:public CDatabaseClient
{
public:
	CSqlite3Client() {
		m_db = nullptr;
		m_stmt = nullptr;
	}
	~CSqlite3Client() override { Close(); }
public:
	CSqlite3Client(const CDatabaseClient&) = delete;
	CSqlite3Client& operator=(const CSqlite3Client&) = delete;
public:
	//连接数据库
	int Connect(const KeyValue& args)override;
	//执行
	int Exec(const Buffer& sql)override;
	//执行带结果
	int Exec(const Buffer& sql, Result& result, const _Table_& table)override;
	//开启事物
	int StartTransaction()override;
	//提交事物
	int CommitTransaction() override;
	//回滚事物
	int RollbackTransaction()override;
	//关闭数据库
	int Close() override;
	//是否连接 true{连接中} false{未连接}
	bool IsConnected() override;
private:
	static int ExecCallback(void* arg, const int count, char** names, char** values);
	int ExecCallback(Result& result, const _Table_& table, int count, char** names, char** values);
private:
	sqlite3_stmt* m_stmt;
	sqlite3* m_db;

	class ExecParam
	{
	public:
		ExecParam(CSqlite3Client* obj, Result& result, const _Table_& table)
			:obj(obj), result(result), table(table)
		{

		}
		CSqlite3Client* obj;
		Result& result;
		const _Table_& table;
	};
};

class _sqlite3_table : public _Table_
{
public:
	_sqlite3_table() : _Table_(){}
	_sqlite3_table(const _sqlite3_table& table);
	~_sqlite3_table() override = default;
	//返回创建的sql语句
	Buffer Create()override;
	//删除表
	Buffer Drop() override;
	//增删改查
	Buffer Insert(const _Table_& values) override;
	Buffer Delete(const _Table_& values) override;
	Buffer Modify(const _Table_& values) override;
	Buffer Query() override;
	//创建一个基于表的对象
	PTable Copy() const override;
	void ClearFieldUsed() override;
public:
	//获取表的全名
	operator const Buffer() const override;

};

class _sqlite3_field_ : public _Field_
{
public:
	_sqlite3_field_();
	_sqlite3_field_(int ntype, const Buffer& name, unsigned attr, const Buffer& type, const Buffer& size, const Buffer& default_, const Buffer& check);
	_sqlite3_field_(const _sqlite3_field_& field);
	~_sqlite3_field_() override;
	Buffer Create() override;
	void LoadFromStr(const Buffer& str) override;
	//where 语句使用的
	Buffer toEqualExp()const override;
	Buffer toSqlStr()const override;
	//列的全名
	operator const Buffer()const override;

private:
	Buffer Str2Hex(const Buffer& data)const;
	union
	{
		bool Bool;
		int Integer;
		double Double;
		Buffer* String;

	}Value;
	int nType;
};