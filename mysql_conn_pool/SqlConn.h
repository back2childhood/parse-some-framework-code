/* ************************************************************************
> File Name:     SqlConn.h
> Author:        Qian JiLi
> mail:          193937157@qq.com
> Created Time:  Wed May  3 22:30:55 2023
> Description:
 ************************************************************************/

#ifndef _SQLCONN_H_
#define _SQLCONN_H_

#include "../log/log.h"
#include <mutex>
#include <mysql/mysql.h>
#include <queue>
#include <semaphore.h>
#include <string>
#include <thread>

class SqlConnPool {
public:
  static SqlConnPool *Instance(); // 使用static保证所有线程共享同一个实例

  MYSQL *GetConn();           // 获取连接
  void FreeConn(MYSQL *conn); // 释放连接
  int GetFreeConnCount();     // 获取当前可使用的连接的数目

  void Init(const char *host, int port, const char *user, const char *pwd,
            const char *dbName, int connSize); // 初始化连接池
  void ClosePool();                            // 关闭该连接池

private:
  SqlConnPool();
  ~SqlConnPool();

  int MAX_CONN_;  // 连接池的最大连接数
  int useCount_;  // 已使用的连接
  int freeCount_; // 空闲连接

  std::queue<MYSQL *> connQue_; // 连接队列
  std::mutex mtx_;
  sem_t semId_; // 信号量
};

#endif // _SQLCONN_H_
