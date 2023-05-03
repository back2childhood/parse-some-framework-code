/* ************************************************************************
> File Name:     SqlConn.cpp
> Author:        Qian JiLi
> mail:          193937157@qq.com
> Created Time:  Wed May  3 22:32:02 2023
> Description:
 ************************************************************************/
#include "SqlConn.h"
using namespace std;

SqlConnPool::SqlConnPool() {
  useCount_ = 0;
  freeCount_ = 0;
}

SqlConnPool *SqlConnPool::Instance() {
  static SqlConnPool connPool;
  return &connPool;
}

void SqlConnPool::Init(const char *host, int port, const char *user,
                       const char *pwd, const char *dbName, int connSize = 10) {
  assert(connSize > 0);
  for (int i = 0; i < connSize; i++) { // 提前建立n个连接
    MYSQL *sql = nullptr;
    sql = mysql_init(sql);
    if (!sql) { // 创建失败
      LOG_ERROR("MySql init error!");
      assert(sql);
    }
    sql = mysql_real_connect(sql, host, user, pwd, dbName, port, nullptr,
                             0); // 建立连接
    if (!sql) {
      LOG_ERROR("MySql Connect error!");
    }
    connQue_.push(sql); // 将创建好的连接放入连接队列中
  }
  MAX_CONN_ = connSize;            // 更新最大连接
  sem_init(&semId_, 0, MAX_CONN_); // 更新信号量的值
}

MYSQL *SqlConnPool::GetConn() {
  MYSQL *sql = nullptr;
  if (connQue_.empty()) { // 没有空闲连接
    LOG_WARN("SqlConnPool busy!");
    return nullptr;
  }
  sem_wait(&semId_); // 信号量-1
  {
    lock_guard<mutex> locker(mtx_); // 上锁防止多个线程同时访问
    sql = connQue_.front();         // 弹出队列头的空闲连接
    connQue_.pop();
  }
  return sql;
}

void SqlConnPool::FreeConn(MYSQL *sql) {
  assert(sql);
  lock_guard<mutex> locker(mtx_);
  connQue_.push(sql); // 当连接使用完了之后，将它再次放入队列
  sem_post(&semId_);  // 信号量+1
}

void SqlConnPool::ClosePool() {
  lock_guard<mutex> locker(mtx_);
  while (!connQue_.empty()) { // 将队列中剩余的所有连接都释放掉
    auto item = connQue_.front();
    connQue_.pop();
    mysql_close(item); // 关闭
  }
  mysql_library_end();
}

int SqlConnPool::GetFreeConnCount() {
  lock_guard<mutex> locker(mtx_);
  return connQue_.size();
}

SqlConnPool::~SqlConnPool() { ClosePool(); }
