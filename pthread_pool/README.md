### 简介

多线程任务中可能会频繁创建线程，这将会导致大量的资源消耗，线程池是一种帮助开发者更简单更高效的控制并发的技术。程序一开始变初始化若干线程并让他们保持休眠，直到有任务发来，线程接收到条件变量便被唤醒去执行任务。

### 原理

![请添加图片描述](https://img-blog.csdnimg.cn/c8ba5a897cfa4c9d9142c6383b1be8fe.png)
线程池由三部分组成：

- _任务队列_：存放待执行的任务
- _线程池_：存放持续从任务队列去任务执行的线程
- _已完成的任务_：线程在执行完任务之后，返回返回值给任务本身，通知他任务已做完

### 代码

#### 任务队列

线程不停的向任务队列请求任务，为了避免两个线程同时请求同一个任务，使用 C++中的 mutex 互斥锁来约束它们的并发访问。
如：

```cpp
void enqueue(T& t){
	std::unique_lock<std::mutex> lock(m_mutex); // 将mutex上锁来保证没有其他人能获取到资源
	m_queue.push(t);	// 将任务加入队列中
}	// lock超过它的范围，自动解锁
```

完整代码如下：

```cpp
#pragma once

#include <mutex>
#include <queue>

// Thread safe implementation of a Queue using an std::queue
template <typename T>
class SafeQueue {
private:
	  std::queue<T> m_queue;
	  std::mutex m_mutex;		// 任何对队列的操作都是保证互斥的
public:
	  SafeQueue() {

	  }

	  SafeQueue(SafeQueue& other) {
	      //TODO:
	  }

	  ~SafeQueue() {
	  }

	  bool empty() {
	  	  std::unique_lock<std::mutex> lock(m_mutex);
   		  return m_queue.empty();
	  }

	  int size() {
	      std::unique_lock<std::mutex> lock(m_mutex);
	      return m_queue.size();
	  }

	  void enqueue(T& t) {
    	  std::unique_lock<std::mutex> lock(m_mutex);
    	  m_queue.push(t);
	  }

	  bool dequeue(T& t) {
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
```

#### 线程池

- 线程池中的线程活动如下：

```cpp
loop
	if work queue is not empty
		dequeue work
		do work
```

明显，当任务队列中没有任务时，所有的线程一直在死循环，不停的询问队列是否为空，这对资源也是一种消耗，我们可以加入信号量来提升效率，当有任务来了时，使用信号量 notify_one()，如下：

```cpp
loop
	if work  queue is empty
		wait signal
	dequeue work
	do it
```

c++中使用条件变量(conditional variables)来控制，条件变量总是和互斥锁一同使用，执行任务的最终代码如下：

```cpp
void operator()() {
    std::function<void()> func;				// 返回值和参数均为空
    bool dequeued;							// 判断是否成功拿到任务
    while (!m_pool->m_shutdown) {		// 若线程池未关闭，则一直循环取任务
        {
			std::unique_lock<std::mutex> lock(m_pool->m_conditional_mutex);	// 上锁，保证只有一个线程在取任务
         	if (m_pool->m_queue.empty()) {		// 若任务队列为空，条件变量m_conditional_lock等待触发
           		m_pool->m_conditional_lock.wait(lock);
            }
            dequeued = m_pool->m_queue.dequeue(func); 	// 从任务队列中pop一个任务是否成功，
            											// dequeue的返回值为一个函数，放入func中
	     }			// 跳出锁的scope，自动解锁unique_lock
         if (dequeued) {			// 若成功拿到一个任务
         	 func();		// 执行任务
         }
    }
}
```

线程池完整代码如下：

```cpp
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
			  ThreadPool * m_pool;
  		  public:
    		  ThreadWorker(ThreadPool * pool, const int id)		//	初始化
      		    : m_pool(pool), m_id(id) {	// 线程的参数
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

    bool m_shutdown;								// 是否关闭
    SafeQueue<std::function<void()>> m_queue;		// 安全的任务队列
    std::vector<std::thread> m_threads;		// 存放线程的容器
    std::mutex m_conditional_mutex;			// 互斥锁
    std::condition_variable m_conditional_lock;	// 搭配互斥锁的条件变量
public:
    ThreadPool(const int n_threads)
      : m_threads(std::vector<std::thread>(n_threads)), m_shutdown(false) {
    }

	// 将默认的拷贝构造函数和赋值构造函数均禁用

    ThreadPool(const ThreadPool &) = delete;
    ThreadPool(ThreadPool &&) = delete;

    ThreadPool & operator=(const ThreadPool &) = delete;
    ThreadPool & operator=(ThreadPool &&) = delete;

  // 初始化线程池，创建若干线程放入容器中
    void init() {
  	    for (int i = 0; i < m_threads.size(); ++i) {
   	        m_threads[i] = std::thread(ThreadWorker(this, i));		// 将本线程池和线程编号传入线程
 	    }
    }

  // 等待所有线程执行完后，关闭线程池
    void shutdown() {
    	m_shutdown = true;			// 将标志置为true，防止有线程继续循环获取任务
    	m_conditional_lock.notify_all();		// 解锁所有线程

   	    for (int i = 0; i < m_threads.size(); ++i) {
      		if(m_threads[i].joinable()) {		// 等待仍在执行任务的线程执行结束
        		m_threads[i].join();
      		}
    	}
  	}

  // 向线程池提交一个任务，由线程异步执行
  // 使用&& 通用引用，既可以接收右值也可以接收左值
  // 因为不知道传入参数的类型及个数，所以使用template编写，使用...表示参数个数>=0
  // 返回值类型也不确定，使用自动推导
    template<typename F, typename...Args>
    auto submit(F&& f, Args&&... args) -> std::future<decltype(f(args...))> {

    // 因不知道函数返回值具体类型，所以使用decltype(f(args...))
    // std::forward完美转发，保留参数的属性，右值仍为右值
    // std::bind将函数和参数绑定在一起，下面再使用只需要写func()
    // 创建一个带有可执行参数的函数
        std::function<decltype(f(args...))()> func = std::bind(std::forward<F>(f), std::forward<Args>(args)...);

    // 使用packaged_task将其包装起来，它会将结果自动传递给std::future对象
    // 使用make_shared将其封装到智能指针中，以便能够复制构造/赋值
    	auto task_ptr = std::make_shared<std::packaged_task<decltype(f(args...))()>>(func);

    // 将打包的任务包装成void函数
    	std::function<void()> wrapper_func = [task_ptr]() {
      		(*task_ptr)();
    	};
// 为了将std::packaged_task转为std::function需要先包一层std::shared_ptr再包一层lambda表达式才可以。
// std::function 要求被类型擦除（也就是从不同类型转为同一类型的过程）的类型支持拷贝构造，
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
```

### example

```cpp
int func(int x, int y){
	return x * y;
}

int main(){
	ThreadPool pool(3);

    // Initialize pool
    pool.init();

    // Submit (partial) multiplication table
    for (int i = 1; i < 3; ++i) {
  	    for (int j = 1; j < 10; ++j) {
        	pool.submit(func, i, j);
    	}
  	}
}
```
