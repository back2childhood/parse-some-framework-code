/* ************************************************************************
> File Name:     ThreadPool.h
> Author:        Qian JiLi
> mail:          193937157@qq.com
> Created Time:  Wed May  3 22:25:14 2023
> Description:
 ************************************************************************/

#ifndef _THREADPOOL_H_
#define _THREADPOOL_H_

#pragma once

#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>
#include <vector>

#include "SafeQueue.h"

class ThreadPool {
private:
  class ThreadWorker {
  private:
    int m_id;
    ThreadPool *m_pool;

  public:
    ThreadWorker(ThreadPool *pool, const int id) //	初始化
        : m_pool(pool), m_id(id) {               // 线程的参数
    }

    void operator()() {
      std::function<void()> func;
      bool dequeued;
      while (!m_pool->m_shutdown) {
        {
          std::unique_lock<std::mutex> lock(m_pool->m_conditional_mutex);
          if (m_pool->m_queue.empty()) {
            m_pool->m_conditional_lock.wait(lock);
          }
          dequeued = m_pool->m_queue.dequeue(func);
        }
        if (dequeued) {
          func();
        }
      }
    }
  };

  bool m_shutdown;                            // 是否关闭
  SafeQueue<std::function<void()>> m_queue;   // 安全的任务队列
  std::vector<std::thread> m_threads;         // 存放线程的容器
  std::mutex m_conditional_mutex;             // 互斥锁
  std::condition_variable m_conditional_lock; // 搭配互斥锁的条件变量
public:
  ThreadPool(const int n_threads)
      : m_threads(std::vector<std::thread>(n_threads)), m_shutdown(false) {}

  // 将默认的拷贝构造函数和赋值构造函数均禁用

  ThreadPool(const ThreadPool &) = delete;
  ThreadPool(ThreadPool &&) = delete;

  ThreadPool &operator=(const ThreadPool &) = delete;
  ThreadPool &operator=(ThreadPool &&) = delete;

  // 初始化线程池，创建若干线程放入容器中
  void init() {
    for (int i = 0; i < m_threads.size(); ++i) {
      m_threads[i] =
          std::thread(ThreadWorker(this, i)); // 将本线程池和线程编号传入线程
    }
  }

  // 等待所有线程执行完后，关闭线程池
  void shutdown() {
    m_shutdown = true; // 将标志置为true，防止有线程继续循环获取任务
    m_conditional_lock.notify_all(); // 解锁所有线程

    for (int i = 0; i < m_threads.size(); ++i) {
      if (m_threads[i].joinable()) { // 等待仍在执行任务的线程执行结束
        m_threads[i].join();
      }
    }
  }

  // 向线程池提交一个任务，由线程异步执行
  // 使用&& 通用引用，既可以接收右值也可以接收左值
  // 因为不知道传入参数的类型及个数，所以使用template编写，使用...表示参数个数>=0
  // 返回值类型也不确定，使用自动推导
  template <typename F, typename... Args>
  auto submit(F &&f, Args &&...args) -> std::future<decltype(f(args...))> {

    // 因不知道函数返回值具体类型，所以使用decltype(f(args...))
    // std::forward完美转发，保留参数的属性，右值仍为右值
    // std::bind将函数和参数绑定在一起，下面再使用只需要写func()
    // 创建一个带有可执行参数的函数
    std::function<decltype(f(args...))()> func =
        std::bind(std::forward<F>(f), std::forward<Args>(args)...);

    // 使用packaged_task将其包装起来，它会将结果自动传递给std::future对象
    // 使用make_shared将其封装到智能指针中，以便能够复制构造/赋值
    auto task_ptr =
        std::make_shared<std::packaged_task<decltype(f(args...))()>>(func);

    // 将打包的任务包装成void函数
    std::function<void()> wrapper_func = [task_ptr]() { (*task_ptr)(); };
    // 为了将std::packaged_task转为std::function需要先包一层std::shared_ptr再包一层lambda表达式才可以。
    // std::function
    // 要求被类型擦除（也就是从不同类型转为同一类型的过程）的类型支持拷贝构造，
    // 但是std::packaged_task不支持拷贝构造，所以不能完成转换。
    // (from http://www.zhihu.com/question/27908489/answer/355105668)

    // 将包装好的函数放入队列
    m_queue.enqueue(wrapper_func);

    // 通知一个正在等待的线程来取任务
    m_conditional_lock.notify_one();

    // 执行完后返回future中的值
    return task_ptr->get_future();
  }
};

#endif // _THREADPOOL_H_
