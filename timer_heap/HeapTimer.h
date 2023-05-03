/* ************************************************************************
> File Name:     HeapTimer.h
> Author:        Qian JiLi
> mail:          193937157@qq.com
> Created Time:  Wed May  3 22:38:49 2023
> Description:
 ************************************************************************/

#ifndef _HEAPTIMER_H_
#define _HEAPTIMER_H_

// #include "../log/log.h"
#include <algorithm>
#include <arpa/inet.h>
#include <assert.h>
#include <chrono>
#include <functional>
#include <queue>
#include <time.h>
#include <unordered_map>

typedef std::function<void()> TimeoutCallBack;
typedef std::chrono::high_resolution_clock Clock;
typedef std::chrono::milliseconds MS;
typedef Clock::time_point TimeStamp;

struct TimerNode {   // 使用结构体作为时间器
  int id;            // 每个定时器所唯一的
  TimeStamp expires; // 设置该定时器的过期时间
  TimeoutCallBack cb; // 当超时时执行回调函数，用来关闭过期连接
  // 由于需要维护最小堆，所以需要重载比较运算符，方便定时器之间的比较
  bool operator<(const TimerNode &t) { return expires < t.expires; }
};

class HeapTimer { // 管理所有的定时器的堆
public:
  HeapTimer() { heap_.reserve(64); }

  ~HeapTimer() { clear(); }

  // 调整指定id的结点
  void adjust(int id, int newExpires);

  // 添加一个定时器
  void add(int id, int timeOut, const TimeoutCallBack &cb);

  // 删除指定id的节点，并执行其中的回调函数
  void doWork(int id);

  void clear();

  // 处理过期的定时器
  void tick();

  // 清除idx = 0的定时器
  void pop();

  // 下一次处理过期定时器的时间
  int GetNextTick();

private:
  // 直接操作时间堆的函数不需要暴露给用户
  // 删除数组下标为i处的定时器
  void del_(size_t i);

  // 向上调整
  void siftup_(size_t i);

  // 向下调整
  bool siftdown_(size_t index, size_t n);

  // 交换节点
  void SwapNode_(size_t i, size_t j);

  // 使用vector来模拟最小堆，里面存的类型是前面定义的时间器
  std::vector<TimerNode> heap_;

  // map中存储的信息为<时间器的id，该时间器在vector中的数组下标>
  std::unordered_map<int, size_t> ref_;
};

#endif // HEAP_TIMER_H
