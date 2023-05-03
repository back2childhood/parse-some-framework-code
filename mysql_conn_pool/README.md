## 背景

在处理用户注册，登录请求的时候，我们需要将这些用户的用户名和密码保存下来用于新用户的注册及老用户的登录校验，相信每个人都体验过，当你在一个网站上注册一个用户时，应该经常会遇到“您的用户名已被使用”，或者在登录的时候输错密码了网页会提示你“您输入的用户名或密码有误”等等类似情况，这种功能是服务器端通过用户键入的用户名密码和数据库中已记录下来的用户名密码数据进行校验实现的。若每次用户请求我们都需要新建一个数据库连接，请求结束后我们释放该数据库连接，当用户请求连接过多时，这种做法过于低效，所以类似线程池的做法，我们构建一个数据库连接池，预先生成一些数据库连接放在那里供用户请求使用。
(找不到 mysql/mysql.h 头文件的时候，需要安装一个库文件：sudo apt install libmysqlclient-dev)

## 流程

官网：https://dev.mysql.com/doc/c-api/8.0/en/c-api-basic-interface-usage.html

我们首先看单个数据库连接是如何生成的：

- 使用 mysql_init()初始化连接使用
- mysql_real_connect()建立一个到 mysql 数据库的连接
- 使用 mysql_query()执行查询语句
- 使用 result = mysql_store_result(mysql)获取结果集
- 使用 mysql_num_fields(result)获取查询的列数，mysql_num_rows(result)获取结果集的行数
- 通过 mysql_fetch_row(result)不断获取下一行，然后循环输出
- 使用 mysql_free_result(result)释放结果集所占内存
- 使用 mysql_close(conn)关闭连接

对于一个数据库连接池来讲，就是预先生成多个这样的数据库连接，然后放在一个链表中，同时维护最大连接数 MAX_CONN，当前可用连接数 FREE_CONN 和当前已用连接数 CUR_CONN 这三个变量。同样注意在对连接池操作时（获取，释放），要用到锁机制，因为它被所有线程共享。

## 代码

```cpp
// sqlconnpool.h
#ifndef SQLCONNPOOL_H
#define SQLCONNPOOL_H

#include <mysql/mysql.h>
#include <string>
#include <queue>
#include <mutex>
#include <semaphore.h>
#include <thread>
#include "../log/log.h"

class SqlConnPool {
public:
    static SqlConnPool *Instance();		// 使用static保证所有线程共享同一个实例

    MYSQL *GetConn();				// 获取连接
    void FreeConn(MYSQL * conn);	// 释放连接
    int GetFreeConnCount();		// 获取当前可使用的连接的数目

    void Init(const char* host, int port,
              const char* user,const char* pwd,
              const char* dbName, int connSize);	// 初始化连接池
    void ClosePool();			// 关闭该连接池

private:
    SqlConnPool();
    ~SqlConnPool();

    int MAX_CONN_;				// 连接池的最大连接数
    int useCount_;				// 已使用的连接
    int freeCount_;				// 空闲连接

    std::queue<MYSQL *> connQue_;		// 连接队列
    std::mutex mtx_;
    sem_t semId_;				// 信号量
};

#endif // SQLCONNPOOL_H
```

```cpp
// sqlconnpool.cpp
#include "sqlconnpool.h"
using namespace std;

SqlConnPool::SqlConnPool() {
    useCount_ = 0;
    freeCount_ = 0;
}

SqlConnPool* SqlConnPool::Instance() {
    static SqlConnPool connPool;
    return &connPool;
}

void SqlConnPool::Init(const char* host, int port,
            const char* user,const char* pwd, const char* dbName,
            int connSize = 10) {
    assert(connSize > 0);
    for (int i = 0; i < connSize; i++) {		// 提前建立n个连接
        MYSQL *sql = nullptr;
        sql = mysql_init(sql);
        if (!sql) {					// 创建失败
            LOG_ERROR("MySql init error!");
            assert(sql);
        }
        sql = mysql_real_connect(sql, host,
                                 user, pwd,
                                 dbName, port, nullptr, 0);	// 建立连接
        if (!sql) {
            LOG_ERROR("MySql Connect error!");
        }
        connQue_.push(sql);					// 将创建好的连接放入连接队列中
    }
    MAX_CONN_ = connSize;				// 更新最大连接
    sem_init(&semId_, 0, MAX_CONN_);	// 更新信号量的值
}

MYSQL* SqlConnPool::GetConn() {
    MYSQL *sql = nullptr;
    if(connQue_.empty()){				// 没有空闲连接
        LOG_WARN("SqlConnPool busy!");
        return nullptr;
    }
    sem_wait(&semId_);					// 信号量-1
    {
        lock_guard<mutex> locker(mtx_);	// 上锁防止多个线程同时访问
        sql = connQue_.front();			// 弹出队列头的空闲连接
        connQue_.pop();
    }
    return sql;
}

void SqlConnPool::FreeConn(MYSQL* sql) {
    assert(sql);
    lock_guard<mutex> locker(mtx_);
    connQue_.push(sql);					// 当连接使用完了之后，将它再次放入队列
    sem_post(&semId_);					// 信号量+1
}

void SqlConnPool::ClosePool() {
    lock_guard<mutex> locker(mtx_);
    while(!connQue_.empty()) {			// 将队列中剩余的所有连接都释放掉
        auto item = connQue_.front();
        connQue_.pop();
        mysql_close(item);				// 关闭
    }
    mysql_library_end();
}

int SqlConnPool::GetFreeConnCount() {
    lock_guard<mutex> locker(mtx_);
    return connQue_.size();
}

SqlConnPool::~SqlConnPool() {
    ClosePool();
}
```
