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
	//是否连接
	bool IsConnected() override;
private:
	static int ExecCallback(void* arg, int count, char** names, char** values);
	int ExecCallback(Result& result,const _Table_& table, int count, char** names, char** values);
private:
	sqlite3_stmt* m_stmt;
	sqlite3* m_db;

	class ExecParam
	{
	public:
		ExecParam(CSqlite3Client* obj,Result& result,const _Table_& table)
			:obj(obj),result(result),table(table)
		{
			
		}
		CSqlite3Client* obj;
		Result& result;
		const _Table_& table;
	};
};
