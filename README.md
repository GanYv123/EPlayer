# 加密播放器服务器(Linux端)

---

## 日志模块的实现

### 日志等级的分级

采用枚举进行等级分级，该分级会配合宏进行格式化日志输出。

```cpp
enum LogLevel
{
//logger.h 11 Line
  LOG_INFO,
  LOG_DEBUG,
  LOG_WARING,
  LOG_ERROR,
  LOG_FATAL
};
```

---

### 日期模块的启动

- 流程：启动日志服务器，初始化日志文件、Epoll、套接字以及线程，准备接受日志写入和处理请求
- epoll 能动态监控文件描述符（如 UNIX 套接字）的读写状态，适合多客户端接入的场景
- 日志吞吐量需求: 当日志写入频率很高、连接数较多时，epoll 的高性能 能显著提升系统吞吐量

```cpp
int Start(){
    if(m_server != nullptr) return -1;
    if(access("log", W_OK | R_OK) != 0){
        mkdir("log", S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
    }
    //1.打开文件
    m_file = fopen(m_path, "w+");
    if(m_file == nullptr) return -2;
    //2.创建epoll 该epoll为二次封装的epoll
    int ret = m_epoll.Create(1);
    if(ret != 0) return -3;
    //3.创建本地套接字
    m_server = new CSocket();
    if(m_server == nullptr){
        Close();
        return -4;
    }
    //4.初始化本地套接字
    ret = m_server->Init(CSockParam("./log/server.sock", static_cast<int>(SOCK_IS_SERVER| SOCK_IS_REUSE)));
    if(ret != 0){
        Close();
        return -5;
    }
    ret = m_epoll.Add(*m_server, EpollData((void*)m_server),EPOLLIN|EPOLLERR);
    if(ret != 0){
        Close();
        return -6;
    }
    ret = m_thread.Start();
    if(ret != 0){
        Close();
        return -7;
    }
    return 0;
}
```

---

### 日志的写入模块

*WriteLog* 为内部使用的方法 不是公开的结构

```cpp
void WriteLog(const Buffer& data) const
{
    if(m_file != nullptr)
    {
        FILE* pFile = m_file;
        fwrite(static_cast<char*>(data), 1, data.size(), pFile);
        flush(pFile);//确保写入的日志立即写入磁盘，而不是仅停留在缓冲区中
    }
}
```

---

### 日志模块对外的接口以及宏定义

对外接口：
外部模块需要使用 ```Trace``` 来进行日志记录 
```thread_local```关键字来确保每个线程都维护一个独立的客户端连接 (```client```) 来与日志服务器通信

```cpp
static void Trace(const LogInfo& info){
    int ret{ 0 };
    static thread_local CSocket client;//每个线程都独立的客户端连接
    if(client == -1){//如果没有初始化，那么就在后续的代码中进行初始化
        ret = client.Init(CSockParam("./log/server.sock", 0));//本地套接字
        if(ret != 0){//初始化失败就退出
            return;
        }
        ret = client.Link();//连接到本地套接字服务器（日志服务器）
    }
    ret = client.Send(info);
    }
```

宏定义：

```cpp
#define TRACEI(...) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_INFO,__VA_ARGS__))
#define LOGI LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_INFO)
#define DUMPI(data, size) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(),  LOG_INFO, data, size))
```

---

### epoll日志服务器的核心逻辑模块

该模块基于 epoll 事件模型处理多个客户端的日志请求。它负责接受客户端连接、接收日志数据，并将日志写入文件

```cpp
int CLoggerServer::ThreadFunc() const {
    // 定义变量
    EP_EVENTS events; // 用于存储epoll事件列表
    std::map<int, CSocketBase*> mapClient; // 管理客户端连接的映射表

    // 主循环，确保线程有效且服务器和epoll都处于有效状态
    while (m_thread.IsValid() && (m_epoll != -1) && (m_server != nullptr)) {
        // 等待epoll事件，超时时间为1000ms
        const ssize_t ret = m_epoll.WaitEvents(events, 1000);
        if (ret < 0) break; // 如果epoll等待失败，退出循环

        // 如果有事件需要处理
        if (ret > 0) {
            for (ssize_t i = 0; i < ret; i++) {
                // 检查是否有错误事件
                if (events[i].events & EPOLLERR) {
                    break; // 有错误，退出处理
                }
                // 检查是否有可读事件
                if (events[i].events & EPOLLIN) {
                    // 处理服务器的可读事件（新的客户端连接）
                    if (events[i].data.ptr == m_server) {
                        // 接受新的客户端连接
                        CSocketBase* pClient{ nullptr };
                        int r = m_server->Link(&pClient); // 接收连接
                        if (r < 0) continue; // 如果连接失败，跳过
                        // 将新客户端添加到epoll监听中
                        r = m_epoll.Add(*pClient, EpollData((void*)pClient), EPOLLIN | EPOLLERR);
                        if (r < 0) { // 添加失败，释放资源
                            delete pClient;
                            continue;
                        }
                        // 如果客户端已存在于map中，释放旧资源
                        auto it = mapClient.find(*pClient);
                        if (it != mapClient.end() && it->second) {
                            delete it->second;
                        }
                        mapClient[*pClient] = pClient; // 保存新客户端
                    } 
                    // 处理客户端的可读事件（接收数据）
                    else {
                        // 获取触发事件的客户端
                        auto pClient = static_cast<CSocketBase*>(events[i].data.ptr);
                        if (pClient != nullptr) {
                            Buffer data(1024 * 1024); // 分配缓冲区接收数据
                            const int r = pClient->Recv(data); // 从客户端接收数据
                            if (r <= 0) { // 如果接收失败，释放客户端资源
                                mapClient[*pClient] = nullptr;
                                delete pClient;
                            } else { // 成功接收数据，写入日志
                                WriteLog(data);
                            }
                        }
                    }
                }
            }
        }
    }

    // 清理所有已连接的客户端资源
    for (auto it = mapClient.begin(); it != mapClient.end(); ++it) {
        if (it->second) {
            delete it->second;
        }
    }
    mapClient.clear(); // 清空客户端映射表

    return 0; // 线程退出
}

```

#### **运行流程简述**

1. **初始化**：
   - 启动线程和epoll，设置服务器套接字。

2. **等待事件**：
   - 通过 `epoll.WaitEvents` 等待事件（例如客户端连接或客户端数据）。

3. **处理新连接**：
   - 当有客户端连接时，接受连接并将其添加到 `epoll` 监听中。

4. **处理客户端数据**：
   - 如果客户端有数据可读，接收数据并将其记录到日志中。
  
5. **资源管理**：
   - 清理不再需要的客户端连接和资源。

6. **退出**：
   - 当线程停止或发生错误时，退出并清理所有资源。

---

## 服务器及其业务模块的实现

### 服务器的初始化

```cpp
CEPlayerServer business(2); //传入的是业务模块中线程池的线程数量
CServer server;//服务器的初始化
erver.Init(&business);
```

服务器初始化的代码：
业务处理为单独的进程，在服务器的初始化过程中设置

```cpp
int CServer::Init(CBusiness* business, const Buffer& ip, short port) {
    // 检查业务逻辑对象是否为空
    if (business == nullptr) return -1;
    m_business = business;

    // 配置子进程并启动（与业务逻辑绑定）
    int ret = m_process.SetEntryFunction(&CBusiness::BusinessProcess, m_business, &m_process);//设置业务进程的入口函数
    if (ret != 0) return -2;
    ret = m_process.CreateSubProcess();//开启业务子进程
    if (ret != 0) return -3;

    // 服务器模块中 启动线程池（初始化8个线程）
    ret = m_pool.Start(8);
    if (ret != 0) return -4;

    // 服务器模块中 创建 epoll 实例，容量为 8
    ret = m_epoll.Create(8);
    if (ret != 0) return -5;

    // 初始化服务器 socket 对象，用于监听客户端请求
    m_server = new CSocket();
    if (m_server == nullptr) return -6;
    ret = m_server->Init(CSockParam(ip, port, SOCK_IS_SERVER | SOCK_IS_IP | SOCK_IS_REUSE));
    if (ret != 0) return -7;

    // 将服务器 socket 添加到 epoll 监听中
    ret = m_epoll.Add(*m_server, EpollData((void*)m_server));
    if (ret != 0) return -8;

    // 为线程池添加工作任务，执行 `CServer::ThreadFunc`
    for (size_t i = 0; i < m_pool.Size(); i++) {
        ret = m_pool.AddTask(&CServer::ThreadFunc, this);
        if (ret != 0) return -9;
    }

    return 0; // 初始化成功
}
```

---

### 业务模块的实现

```cpp
int BusinessProcess(CProcess* proc) override {
    int ret{ 0 };
    // 1. 创建并初始化数据库连接
    m_db = new CMysqlClient;
    if (m_db == nullptr) return -1; // 内存不足
    KeyValue args{
        {"host", "192.168.133.133"},
        {"user", "miku"},
        {"password", "123"},
        {"port", "3306"},
        {"db", "edoyun"}
    };
    ret = m_db->Connect(args); // 数据库连接
    if (ret != 0) return -2;

    // 2. 创建表（初始化）
    edoyunLogin_user_mysql user;
    ret = m_db->Exec(user.Create());
    if (ret != 0) return -3;

    // 3. 设置回调函数 连接成功的回调 & 接收到数据的回调
    ret = SetConnectCallback(&CEPlayerServer::Connected, this, std::placeholders::_1); // 连接回调
    if (ret != 0) return -4;
    ret = SetRecvCallback(&CEPlayerServer::Received, this, std::placeholders::_1, std::placeholders::_2); // 接收数据回调
    if (ret != 0) return -5;

    // 4. 初始化 epoll 和线程池 m_count参考服务器初始化时传入的参数
    ret = m_epoll.Create(m_count); // 创建 epoll 实例
    if (ret != 0) return -6;
    ret = m_pool.Start(m_count); // 启动线程池
    if (ret != 0) return -7;

    // 5. 为每个线程池线程添加任务
    //这里添加的任务很重要，是epoll监听客户端socket的函数
    for (unsigned i = 0; i < m_count; i++) {
        ret = m_pool.AddTask(&CEPlayerServer::ThreadFunc, this); // 添加线程任务
        if (ret != 0) return -8;
    }

    // 6. 主循环：监听 socket 连接
    //当父进程接收到请求时 会将拿到的客户端套接字发送给业务模块处理
    int sock = 0;
    sockaddr_in addrin;
    while (m_epoll != -1) {
        // 从父进程接收 socket 文件描述符
        ret = proc->RecvSocket(sock, &addrin);
        if (ret < 0 || (sock == 0)) break;

        // 为每个连接创建 socket 对象并加入 epoll 监听
        CSocketBase* pClient = new CSocket(sock);
        if (pClient == nullptr) continue;
        ret = pClient->Init(CSockParam(&addrin, SOCK_IS_IP));
        if (ret != 0) continue;

        ret = m_epoll.Add(sock, EpollData((void*)pClient)); // 加入 epoll
        if (ret != 0) continue;

        // 执行连接回调 该回调在上面设置
        if (m_connectedcallback) {
            (*m_connectedcallback)(pClient);
        }
    }

    return 0;
}

```

---

#### HTTP请求的处理

##### 处理请求的函数

```cpp
int HttpParser(const Buffer& data) {
    // 创建一个 HTTP 解析器实例
    CHttpParser parser;
    // 解析输入的数据
    size_t size = parser.Parser(data);
    // 检查解析结果，返回错误代码
    if (size == 0 || (parser.Errno() != 0)) {
        TRACEE("size:%llu errno:%u", size, parser.Errno()); // 记录解析错误信息
        return -1; // 解析失败
    }
    // 处理 HTTP GET 请求
    if (parser.Method() == HTTP_GET) {
        // 解析 URL，前缀为固定的主机地址
        UrlParser url("https://192.168.133.133" + parser.Url());
        int ret = url.Parser(); // 解析 URL
        // 检查 URL 解析结果
        if (ret != 0) {
            TRACEE("ret=%d url[%s]", ret, "https://192.168.133.133" + parser.Url());
            return -2; // URL 解析失败
        }
        Buffer uri = url.Uri(); // 获取 URI 部分
        if (uri == "login") { // 判断是否为登录请求
            // 获取请求中的参数
            Buffer time = url["time"];
            Buffer salt = url["salt"];
            Buffer user = url["user"];
            Buffer sign = url["sign"];
            TRACEI("time:%s salt:%s user:%s:sign %s", (char*)time, (char*)salt, (char*)user, (char*)sign);
            // 数据库查询，验证用户登录请求
            edoyunLogin_user_mysql dbuser; // 创建数据库用户查询对象
            Result result; // 存储查询结果
            Buffer sql = dbuser.Query("user_name=\"" + user + "\""); // 构建 SQL 查询
            // 执行 SQL 查询
            ret = m_db->Exec(sql, result, dbuser);
            if (ret != 0) {
                TRACEE("sql=%s ret = %d", (char*)sql, ret);
                return -3; // SQL 执行失败
            }
            // 检查查询结果是否为空
            if (result.empty()) {
                TRACEE("no result sql=%s ret=%d", (char*)sql, ret);
                return -4; // 未找到用户
            }
            // 确保查询结果唯一
            if (result.size() != 1) {
                TRACEE("more than one sql=%s ret=%d", (char*)sql, ret);
                return -5; // 找到多个用户
            }
            // 取出查询到的用户信息
            auto user1 = result.front();
            Buffer pwd = *user1->Fields["user_password"]->Value.String; // 获取用户密码
            TRACEI("password = %s", (char*)pwd);
            // 使用 MD5 生成签名
            auto MD5_KEY = "*&^%$#@b.v+h-b*g/h@n!h#n$d^ssx,.kl<kl"; // 密钥
            Buffer md5str = time + MD5_KEY + pwd + salt; // 生成 MD5 字符串
            Buffer md5 = Crypto::MD5(md5str); // 计算 MD5
            TRACEI("md5=%s", (char*)md5);

            // 验证生成的 MD5 签名与请求中的签名是否匹配
            if (md5 == sign) {
                return 0; // 登录成功
            }
            return -6; // 签名验证失败
        }
    } else if (parser.Method() == HTTP_POST) {
        // 处理 HTTP POST 请求（未实现）
    }
    return -7; // 未处理的情况
}

```

##### 应答数据处理函数

```cpp
static Buffer MakeResponse(int ret) {
    // 创建一个 JSON 根对象
    Json::Value root; 
    // 设置 status 字段为传入的 ret 参数
    root["status"] = ret; 
    // 根据 ret 值设置 message 字段
    if (ret != 0) {
        root["message"] = "登陆失败,可能是用户名或者密码错误!"; // 登录失败的消息
    } else {
        root["message"] = "success"; // 登录成功的消息
    }
    // 将 JSON 对象转换为字符串，准备作为 HTTP 响应的一部分
    Buffer json = root.toStyledString(); 
    // 开始构建 HTTP 响应
    Buffer result = "HTTP/1.1 200 OK\r\n"; // 状态行，表示请求成功
    // 获取当前时间并格式化为 GMT 格式
    time_t t;
    time(&t); // 获取当前时间
    tm* ptm = localtime(&t); // 转换为本地时间
    char temp[64] = "";
    strftime(temp, sizeof(temp), "%a, %d %b %G %T GMT\r\n", ptm); // 格式化时间为字符串
    Buffer Date = Buffer("Date: ") + temp; // 构建日期头部
    // 构建其他 HTTP 头部信息
    Buffer Server = "Server: Edoyun/1.0\r\nContent-Type: application/json;charset=utf-8\r\n"; // 服务器信息和内容类型
    snprintf(temp, sizeof(temp), "%d", json.size()); // 获取 JSON 字符串的长度
    Buffer Length = Buffer("Content-Length: ") + temp + "\r\n"; // 内容长度头部
    // 其他安全相关头部
    Buffer Stub = "X-Content-Type-Options: nosniff\r\nReferrer-Policy: same-origin\r\n\r\n"; // 安全头部
    // 拼接所有部分，形成完整的 HTTP 响应
    result += Date + Server + Length + Stub + json;
    return result; // 返回完整的 HTTP 响应
}

```

##### HTTP 响应格式笔记

###### **HTTP 响应结构**

```cpp
<状态行>
<响应头>
<空行>
<消息体>
```

###### **1. 状态行**

格式：

```cpp
<HTTP版本> <状态码> <原因短语>
```

- **HTTP版本**：如 `HTTP/1.1`
- **状态码**：如 `200` (成功), `404` (未找到), `500` (服务器错误)
- **原因短语**：如 `OK`, `Not Found`, `Internal Server Error`

**示例：**

```cpp
HTTP/1.1 200 OK
```

###### **2. 响应头**

格式：

```cpp
<头字段名>: <值>
```

常见字段：

- `Date`: 响应时间（如 `Date: Wed, 28 Nov 2024 12:34:56 GMT`）
- `Content-Type`: 内容类型（如 `Content-Type: application/json`）
- `Content-Length`: 消息体长度（如 `Content-Length: 73`）
- `Server`: 服务器信息（如 `Server: Edoyun/1.0`）

**示例：**

```cpp
Date: Wed, 28 Nov 2024 12:34:56 GMT
Server: Edoyun/1.0
Content-Type: application/json
Content-Length: 31
```

###### **3. 空行**

- 响应头和消息体之间的分隔符。
- 必须存在，即使消息体为空。

###### **4. 消息体**

- 包含返回的数据，格式由 `Content-Type` 指定：
  - JSON (`application/json`)：
  
    ```json
    {"status":0,"message":"success"}
    ```

  - HTML (`text/html`)：
  
    ```html
    <html><body>Success</body></html>
    ```

###### **完整示例**

```cpp
HTTP/1.1 200 OK
Date: Wed, 27 Nov 2024 12:34:56 GMT
Server: Edoyun/1.0
Content-Type: application/json
Content-Length: 31

{"status":0,"message":"success"}
```

---

**用途**：常用于 Web API 响应和浏览器与服务器的通信。

---