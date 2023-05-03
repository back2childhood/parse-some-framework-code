/* ************************************************************************
> File Name:     SafeQueue.h
> Author:        Qian JiLi
> mail:          193937157@qq.com
> Created Time:  Wed May  3 22:25:32 2023
> Description:
 ************************************************************************/

#ifndef _SAFEQUEUE_H_
#define _SAFEQUEUE_H_

#pragma once

#include <mutex>
#include <queue>

// Thread safe implementation of a Queue using an std::queue
template <typename T> class SafeQueue {
private:
  std::queue<T> m_queue;
  std::mutex m_mutex; // 任何对队列的操作都是保证互斥的
public:
  SafeQueue() {}

  SafeQueue(SafeQueue &other) {
    // TODO:
  }

  ~SafeQueue() {}

  bool empty() {
    std::unique_lock<std::mutex> lock(m_mutex);
    return m_queue.empty();
  }

  int size() {
    std::unique_lock<std::mutex> lock(m_mutex);
    return m_queue.size();
  }

  void enqueue(T &t) {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_queue.push(t);
  }

  bool dequeue(T &t) {
    std::unique_lock<std::mutex> lock(m_mutex);

    if (m_queue.empty()) {
      return false;
    }
    // 使用move将队列头的任务的所有权交给t，然后将队列头的元素弹出
    t = std::move(m_queue.front());

    m_queue.pop();
    return true;
  }
};

#endif // _SAFEQUEUE_H_
