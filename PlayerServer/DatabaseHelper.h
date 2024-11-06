#pragma once
#include "Public.h"
#include <map>
#include <list>
#include <memory>
#include <vector>

class _Table_;
using PTable = std::shared_ptr<_Table_>;

using KeyValue = std::map<Buffer, Buffer>;
using Result = std::list<PTable>;

class CDatabaseClient
{
public:
	CDatabaseClient() = default;
	virtual ~CDatabaseClient() = default;
public:
	CDatabaseClient(const CDatabaseClient&) = delete;
	CDatabaseClient& operator=(const CDatabaseClient&) = delete;
public:
	//连接数据库
	virtual int Connect(const KeyValue& args) = 0;
	//执行
	virtual int Exec(const Buffer& sql) = 0;
	//执行带结果
	virtual int Exec(const Buffer& sql, Result& result, const _Table_& table) = 0;
	//开启事物
	virtual int StartTransaction() = 0;
	//提交事物
	virtual int CommitTransaction() = 0;
	//回滚事物
	virtual int RollbackTransaction() = 0;
	//关闭数据库
	virtual int Close() = 0;
	//是否连接
	virtual bool IsConnected() = 0;
};

//表和列的基类的实现
class _Field_;
using PField = std::shared_ptr<_Field_>;
using FieldArray = std::vector<PField>;
using FieldMap = std::map<Buffer, PField>;


class _Table_
{
public:
	_Table_() = default;
	virtual ~_Table_() = default;
	//返回创建的sql语句
	virtual Buffer Create() = 0;
	//删除表
	virtual Buffer Drop() = 0;
	//增删改查
	virtual Buffer Insert(const _Table_& values) = 0;
	virtual Buffer Delete(const _Table_& values) = 0;
	virtual Buffer Modify(const _Table_& values) = 0;
	virtual Buffer Query(const Buffer& condition = "") = 0;
	//创建一个基于表的对象
	virtual PTable Copy() const = 0;
	virtual void ClearFieldUsed() = 0;
public:
	//获取表的全名
	virtual operator const Buffer() const = 0;

public:
	//表所属的db名称
	Buffer Database;
	Buffer Name;
	FieldArray FieldDefine;//列的定义 (存储查询结果)
	FieldMap Fields;//列的定义 (映射表)
};

enum my_enum
{
	SQL_INSERT = 1, //插入的列
	SQL_MODIFY = 2, //修改的列
	SQL_CONDITION = 4 //查询的列
};

enum
{
	NONE = 0,
	NOT_NULL = 1,
	DEFAULT = 2,
	UNIQUE = 4,
	PRIMARY_KEY = 8,
	CHECK = 16,
	AUTOINCREMENT = 32
};

using SqlType = enum
{
	TYPE_NULL = 0,
	TYPE_BOOL = 1,
	TYPE_INT = 2,
	TYPE_DATETIME = 4,
	TYPE_REAL = 8,
	TYPE_VARCHAR = 16,
	TYPE_TEXT = 32,
	TYPE_BLOB = 64
};


class _Field_
{
public:
	_Field_() = default;
	virtual ~_Field_() = default;
	_Field_(const _Field_& field) {
		Name = field.Name;
		Type = field.Type;
		Attr = field.Attr;
		Default = field.Default;
		Check = field.Check;
	}
	virtual _Field_& operator=(const _Field_& field) {
		if(this != &field){
			Name = field.Name;
			Type = field.Type;
			Attr = field.Attr;
			Default = field.Default;
			Check = field.Check;
		}
		return *this;
	}
public:
	virtual Buffer Create() = 0;
	virtual void LoadFromStr(const Buffer& str) = 0;
	//where 语句使用的
	virtual Buffer toEqualExp()const = 0;
	virtual Buffer toSqlStr()const = 0;
	//列的全名
	virtual operator const Buffer()const = 0;

public:
	Buffer Name;
	Buffer Type;
	Buffer Size;
	unsigned Attr;
	Buffer Default;
	Buffer Check;
public:
	//操作条件
	unsigned Condition;
	union
	{
		bool Bool;
		int Integer;
		double Double;
		Buffer* String;

	}Value;
	int nType;
};

#define DECLARE_TABLE_CLASS(name,base) class name:public base{ \
public: \
virtual PTable Copy() const{return PTable(new name(*this));} \
name():base(){Name=#name;

#define DECLARE_FIELD(ntype,name,attr,type,size,default_,check) \
{PField field(new _sqlite3_field_(ntype, #name, attr, type, size, default_, check)); \
FieldDefine.push_back(field); Fields[#name] = field;}

#define DECLARE_TABLE_CLASSEND() }};