#pragma once
#include "Logger.h"
#include "Server.h"
#include "CHttpParser.h"
#include "Crypto.h"
#include "MysqlClient.h"
#include "jsoncpp/json.h"
#include <map>


DECLARE_TABLE_CLASS(edoyunLogin_user_mysql, _mysql_table_)
DECLARE_MYSQL_FIELD(TYPE_INT, user_id, NOT_NULL | PRIMARY_KEY | AUTOINCREMENT, "INTEGER", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_VARCHAR, user_qq, NOT_NULL, "VARCHAR", "(15)", "", "")  //QQ号
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_name, NOT_NULL, "TEXT", "", "", "")  //姓名
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_nick, NOT_NULL, "TEXT", "", "", "")  //昵称
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_password, NOT_NULL, "TEXT", "", "", "")
DECLARE_TABLE_CLASS_EDN()


#define ERR_RETURN(ret,err) if(ret != 0){TRACEE("ret=%d errno=%d msg=<%s>", ret, errno, strerror(errno));return err;}

#define WARN_CONTINUE(ret) if(ret != 0){TRACEW("ret=%d errno=%d msg=<%s>", ret, errno, strerror(errno));continue;}

class CEPlayerServer : public CBusiness
{
public:
	CEPlayerServer(const unsigned count) :CBusiness() {
		m_count = count;
	}
	~CEPlayerServer() {
		if(m_db != nullptr){
			CDatabaseClient* db = m_db;
			m_db = nullptr;
			db->Close();
			delete db;
		}
		m_epoll.Close();
		m_pool.Close();
		for(const auto it : m_mapClients){
			delete it.second;
		}
		m_mapClients.clear();
	}

	int BusinessProcess(CProcess* proc) override {
		using  namespace std::placeholders;
		int ret{ 0 };
		m_db = new CMysqlClient;
		if(m_db == nullptr){
			TRACEE("no more memory!");
			return -1;
		}

		KeyValue args;
		args["host"] = "192.168.133.133";
		args["user"] = "miku";
		args["password"] = "123";
		args["port"] = 3306;
		args["db"] = "edoyun";

		ret = m_db->Connect(args);
		ERR_RETURN(ret, -2)
		edoyunLogin_user_mysql user;
		ret = m_db->Exec(user.Create());
		ERR_RETURN(ret, -3)
		ret = SetConnectCallback(&CEPlayerServer::Connected, this, _1);
		TRACEI("*** SetConnectCallback ***");

		ERR_RETURN(ret, -4)
		ret = SetRecvCallback(&CEPlayerServer::Received, this, _1, _2);
		ERR_RETURN(ret, -5)
		ret = m_epoll.Create(m_count);
		ERR_RETURN(ret, -6)
		ret = m_pool.Start(m_count);
		ERR_RETURN(ret, -7)

		for(unsigned i = 0; i < m_count; i++){
			ret = m_pool.AddTask(&CEPlayerServer::ThreadFunc, this);
			ERR_RETURN(ret, -8)
		}
		TRACED("m_pool.AddTask(&CEPlayerServer::ThreadFunc)");

		int sock = 0;
		sockaddr_in addrin;
		while(m_epoll != -1){
			ret = proc->RecvSocket(sock, &addrin);
			TRACEI("RecvSocket ret:%d",ret);
			if(ret < 0 || (sock == 0)) break;
			CSocketBase* pClient = new CSocket(sock);
			if(pClient == nullptr)continue;
			ret = pClient->Init(CSockParam(&addrin, SOCK_IS_IP));
			WARN_CONTINUE(ret)
			ret = m_epoll.Add(sock, EpollData((void*)pClient));
			if(m_connectedcallback){
				(*m_connectedcallback)(pClient);
			}
			WARN_CONTINUE(ret)
		}
		return -0;
	}

private:
	int Connected(CSocketBase* pClient) {
		//客户端连接处理
		sockaddr_in* paddr = *pClient;
		TRACEI("client connected addr:%s port:%d", inet_ntoa(paddr->sin_addr), paddr->sin_port);
		return 0;
	}

	int Received(CSocketBase* pClient, const Buffer& data) {
		TRACEI("recv DATA:<%s>",(char*)data);
		//主要业务处理
		//Http 解析 数据库查询 登陆请求验证
		int ret{ 0 };
		Buffer response = "";
		ret = HttpParser(data);
		TRACED("HttpParser:%d",ret);

		// 验证结果反馈
		if(ret != 0){//验证失败
			TRACEE("http parser failed!%d", ret);
			//失败应答

		}
		response = MakeResponse(ret);
		ret = pClient->Send(response);
		if(ret != 0){
			TRACEE("http response failed!%d <%s>", ret, (char*)response);

		} else{
			TRACEI("http response success!%d", ret);

		}
		return 0;
	}

	/**
 * 错误返回值说明：
 * -1: HTTP 请求解析失败。
 *     原因可能是解析器返回的 `size` 为 0，或者解析器的 `Errno()` 返回了非 0 的错误码。
 * -2: URL 解析失败。
 *     原因可能是 `UrlParser::Parser()` 方法返回了非 0 的错误码。
 * -3: 数据库查询执行失败。
 *     原因可能是执行 SQL 查询时返回了错误码（`m_db->Exec()` 返回值非 0）。
 * -4: 数据库中没有找到对应的用户记录。
 *     原因可能是查询结果为空（`result.empty()`）。
 * -5: 数据库查询返回了多个用户记录。
 *     原因可能是查询条件不够唯一，导致返回的记录数超过 1。
 * -6: 签名验证失败。
 *     原因可能是客户端提供的 `sign` 值与计算出的 MD5 值不匹配。
 * -7: 处理 HTTP 请求时出现未知的请求类型或异常。
 *     原因可能是既不是 GET 请求，也不是 POST 请求，或没有为 POST 请求提供处理逻辑。
 */

	int HttpParser(const Buffer& data) {
		CHttpParser parser;
		size_t size = parser.Parser(data);
		TRACED("HttpParser:%s",(char*)data);
		if(size == 0 || (parser.Errno() != 0)){
			TRACEE("size:%llu errno:%u", size, parser.Errno());
			return -1; //解析失败
		}
		if(parser.Method() == HTTP_GET){
			UrlParser url("https://192.168.133.133" + parser.Url());
			int ret = url.Parser();
			if(ret != 0){
				TRACEE("ret=%d url[%s]", ret, "https://192.168.133.133" + parser.Url());
				return -2;
			}
			Buffer uri = url.Uri();
			TRACED("uri:%s",(char*)uri);
			if(uri == "login"){
				//处理登陆
				Buffer time = url["time"];
				Buffer salt = url["salt"];
				Buffer user = url["user"];
				Buffer sign = url["sign"];
				TRACEI("time:%s salt:%s user:%s:sign %s", (char*)time, (char*)salt, (char*)user, (char*)sign);
				//数据库查询，登陆请求验证
				TRACED("check sign info from DB");
				edoyunLogin_user_mysql dbuser;
				Result result;
				Buffer sql = dbuser.Query("user_name=\"" + user + "\"");
				ret = m_db->Exec(sql, result, dbuser);
				if(ret != 0){
					TRACEE("sql=%s ret = %d", (char*)sql, ret);
					return -3;
				}
				if(result.empty()){
					TRACEE("no result sql=%s ret=%d", (char*)sql, ret);
					return -4;
				}
				if(result.size() != 1){
					TRACEE("more than one sql=%s ret=%d", (char*)sql, ret);
					return -5;
				}

				auto user1 = result.front();
				Buffer pwd = *user1->Fields["user_password"]->Value.String;
				TRACEI("password = %s", (char*)pwd);
				auto MD5_KEY = "*&^%$#@b.v+h-b*g/h@n!h#n$d^ssx,.kl<kl";
				Buffer md5str = time + MD5_KEY + pwd + salt;
				Buffer md5 = Crypto::MD5(md5str);
				TRACEI("md5=%s", (char*)md5);
				if(md5 == sign){
					return  0;
				}
				return -6;

			}
		} else if(parser.Method() == HTTP_POST){
			//post处理
		}
		return -7;
	}

	/**
	 * http格式：
	 * 状态行
	 * 响应头1<key(头名),value(响应值)>
	 * @param ret 
	 * @return 
	 */
	static Buffer MakeResponse(int ret) {
		Json::Value root; //创建Json对象：
		root["status"] = ret;

		if(ret != 0){
			root["message"] = "登陆失败,可能是用户名或者密码错误!";
		} else{
			root["message"] = "success";
		}
		Buffer json = root.toStyledString();
		Buffer result = "HTTP/1.1 200 OK\r\n"; //状态行
		time_t t;
		time(&t);
		tm* ptm = localtime(&t);
		char temp[64] = "";
		strftime(temp, sizeof(temp), "%a,%d %b %G %T GMT\r\n", ptm);
		Buffer Date = Buffer("Date: ") + temp;
		Buffer Server = "Server: Edoyun/1.0\r\nContent-Type: text/html;charset=utf-8\r\nX-Frame-Options: DENY\r\n";
		snprintf(temp, sizeof(temp), "%d", json.size());
		Buffer Length = Buffer("Content-Length: ") + temp + "\r\n";
		Buffer Stub = "X-Content-Type-Options: nosniff\r\nReferrer-Policy: same-origin\r\n\r\n";
		result += Date + Server + Length + Stub + json;
		TRACEI("\n<<----------------- respomse start ----------------->>:\n"
										" %s\n"
                 "<<----------------- respomse end ----------------->>", (char*)result);
		return result;
	}

private:
	int ThreadFunc() {
		int ret{ 0 };
		EP_EVENTS events;
		TRACEI("ThreadFunc(EP)");

		while(m_epoll != -1){
			const ssize_t size = m_epoll.WaitEvents(events);
			if(size < 0){
				break;
			}
			if(size > 0){
				for(size_t i = 0; i < static_cast<size_t>(size); i++){
					if(events[i].events & EPOLLERR) break;
					if(events[i].events & EPOLLIN){
						const auto pClient{ static_cast<CSocketBase*>(events[i].data.ptr) };
						if(pClient){
							Buffer data;
							ret = pClient->Recv(data);
							TRACEI("recv data size:%d", ret);
							if(ret <= 0){
								TRACEW("ret=%d errno=%d msg=<%s>", ret, errno, strerror(errno));
								m_epoll.Del(*pClient);
								continue;
							}
							if(m_recvedcallback){
								(*m_recvedcallback)(pClient, data);
							}
						}
					}
				}
			}
		}

		return 0;
	}

private:
	CEpoll m_epoll;
	CThreadPool m_pool;
	std::map<int, CSocketBase*>m_mapClients;
	unsigned m_count;
	CDatabaseClient* m_db;
};