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
	//�������ݿ�
	virtual int Connect(const KeyValue& args) = 0;
	//ִ��
	virtual int Exec(const Buffer& sql) = 0;
	//ִ�д����
	virtual int Exec(const Buffer& sql, Result& result, const _Table_& table) = 0;
	//��������
	virtual int StartTransaction() = 0;
	//�ύ����
	virtual int CommitTransaction() = 0;
	//�ع�����
	virtual int RollbackTransaction() = 0;
	//�ر����ݿ�
	virtual int Close() = 0;
	//�Ƿ�����
	virtual bool IsConnected() = 0;
};

//����еĻ����ʵ��
class _Field_;
using PFiled = std::shared_ptr<_Field_>;
using FieldArray = std::vector<PFiled>;
using FieldMap = std::map<Buffer, PFiled>;


class _Table_
{
public:
	_Table_() = default;
	virtual ~_Table_() = default;
	//���ش�����sql���
	virtual Buffer Create() = 0;
	//ɾ����
	virtual Buffer Drop() = 0;
	//��ɾ�Ĳ�
	virtual Buffer Insert(const _Table_& values) = 0;
	virtual Buffer Delete() = 0;
	virtual Buffer Modify() = 0;// TODO:���������Ż�
	virtual Buffer Query() = 0;
	//����һ�����ڱ�Ķ���
	virtual PTable Copy() const = 0;
public:
	//��ȡ���ȫ��
	virtual operator const Buffer() const = 0;

public:
	//��������db����
	Buffer Database;
	Buffer Name;
	FieldArray FieldDefine;//�еĶ��� (�洢��ѯ���)
	FieldMap Fields;//�еĶ��� (ӳ���)
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
	//where ���ʹ�õ�
	virtual Buffer toEqualExp()const = 0;
	virtual Buffer toString()const = 0;
	//�е�ȫ��
	virtual operator const Buffer()const = 0;

public:
	Buffer Name;
	Buffer Type;
	Buffer Size;
	unsigned Attr;
	Buffer Default;
	Buffer Check;
};