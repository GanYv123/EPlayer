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
	//����
	int Connect(const KeyValue& args) override;
	//ִ��
	int Exec(const Buffer& sql) override;
	//�������ִ��
	int Exec(const Buffer& sql, Result& result, const _Table_& table) override;
	//��������
	int StartTransaction() override;
	//�ύ����
	int CommitTransaction() override;
	//�ع�����
	int RollbackTransaction() override;
	//�ر�����
	int Close() override;
	//�Ƿ����� true��ʾ������ false��ʾδ����
	bool IsConnected() override;
private:
	MYSQL m_db;
	bool m_bInit;//Ĭ����false ��ʾû�г�ʼ�� ��ʼ��֮����Ϊtrue����ʾ�Ѿ�����
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
	//���ش�����SQL���
	Buffer Create()override;
	//ɾ����
	Buffer Drop()override;
	//��ɾ�Ĳ�
	//TODO:���������Ż�
	Buffer Insert(const _Table_& values)override;
	Buffer Delete(const _Table_& values)override;
	//TODO:���������Ż�
	Buffer Modify(const _Table_& values)override;
	Buffer Query() override;
	//����һ�����ڱ�Ķ���
	PTable Copy()const override;
	void ClearFieldUsed()override;
public:
	//��ȡ���ȫ��
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
	//where ���ʹ�õ�
	Buffer toEqualExp() const override;
	Buffer toSqlStr() const override;
	//�е�ȫ��
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