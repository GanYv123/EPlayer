#include "CSqlite3Client.h"
#include "Logger.h"

int CSqlite3Client::Connect(const KeyValue& args) {
	auto it = args.find("host");
	if(it == args.end()) return -1;
	if(m_db != nullptr) return -2;
	int ret = sqlite3_open(it->second, &m_db);
	if(ret != 0){
		TRACEE("connect failed%d<%s>", ret, sqlite3_errmsg(m_db));
		return -3;
	}
	return 0;
}

int CSqlite3Client::Exec(const Buffer& sql) {
	if(m_db == nullptr) return -1;
	int ret = sqlite3_exec(m_db, sql, nullptr, this, nullptr);
	if(ret != SQLITE_OK){
		TRACEE("sql{%s}", sql);
		TRACEE("Exec failed%d<%s>", ret, sqlite3_errmsg(m_db));
		return -2;
	}
	return 0;
}

int CSqlite3Client::Exec(const Buffer& sql, Result& result, const _Table_& table) {
	if(m_db == nullptr) return -1;
	char* errmsg{ nullptr };
	ExecParam param(this,result,table);
	int ret = sqlite3_exec(m_db, sql, &CSqlite3Client::ExecCallback, (void*)&param, &errmsg);
	if(ret != SQLITE_OK){
		TRACEE("sql{%s}", sql);
		TRACEE("Exec failed%d<%s>", ret, sqlite3_errmsg(m_db));
		return -2;
	}
	return 0;
}

int CSqlite3Client::ExecCallback(void* arg, int count, char** names, char** values) {
	const auto param = static_cast<ExecParam*>(arg);
	return param->obj->ExecCallback(param->result, param->table, count, names, values);
}

// ReSharper disable once CppMemberFunctionMayBeStatic
int CSqlite3Client::ExecCallback(Result& result, const _Table_& table, int count, char** names, char** values)
{
	PTable pTable = table.Copy();
	if(pTable == nullptr){
		TRACEE("table %s error!",(const char*)(Buffer)table);
		return -1;
	}
	for (int i = 0;i < count; i++){
		Buffer name = names[i];
		auto it = pTable->Fields.find(name);
		if(it == pTable->Fields.end()){
			TRACEE("table %s error!", (const char*)(Buffer)table);
			return -2;
		}
		if(values[i] != nullptr)
			it->second->LoadFromStr(values[i]);
	}
	result.push_back(pTable);
	return 0;
}
