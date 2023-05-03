## 简介

在《Linux 高性能服务器编程》中，介绍了三种定时方法：

- socket 选项 SO_RCVTIMEO 和 SO_SNDTIMEO
- SIGALRM 信号
- I/O 复用系统调用的超时参数

## 基础知识

- 非活跃，是指客户端（这里是浏览器）与服务器端建立连接后，长时间不交换数据，一直占用服务器端的文件描述符，导致连接资源的浪费。
- 定时事件，是指固定一段时间之后触发某段代码，由该段代码处理一个事件，如从内核事件表删除事件，并关闭文件描述符，释放连接资源。
- 定时器，是指利用结构体或其他形式，将多种定时事件进行封装起来。具体的，这里只涉及一种定时事件，即定期检测非活跃连接，这里将该定时事件与连接资源封装为一个结构体定时器。
- 定时器容器，是指使用某种容器类数据结构，将上述多个定时器组合起来，便于对定时事件统一管理。具体的，项目中使用升序链表将所有定时器串联组织起来。

在 web 服务中，如果某一用户 connect()到服务器之后，长时间不交换数据，一直占用服务器端的文件描述符，导致连接资源的浪费。这时候就应该利用定时器把这些超时的非活动连接释放掉，关闭其占用的文件描述符。这种情况也很常见，当你登录一个网站后长时间没有操作该网站的网页，再次访问的时候你会发现需要重新登录。

## 时间堆原理

将所有定时器中超时时间最小的一个定时器的超时值作为心搏间隔，这样，一旦心搏函数 tick 被调用，到期的一定是超时时间最小的时间器。然后，再从剩下的定时器中找出超时时间最小的那个座位心搏间隔，如此反复直至定时器执行完。
从上面的运行过程我们可以看出，最小堆很适合这种定时方案。
但是使用最小堆虽然可以很方便的取得当前时间最小的时间器，但是却不支持随机访问，非常不便于维护连接的过期时间。由于最小堆是一种完全二叉树，所以我们可以用数组来组织其中的元素。
对于数组中的任意一个位置 i 上的元素，其作儿子节点在位置 2i+1 上，右儿子节点在位置 2i+2 上，父节点在位置「(i-1)/2」上。用数组来表示堆不仅节省空间，而且更容易实现堆的插入、删除等操作。
那么如何维护这个数组使它具有最小堆堆性质呢？假设我们已经有一个包含 N 个元素的数组，现在要把它初始化为一个最小堆。我们可以将元素挨个插入数组中，但是这样效率偏低。正确的做法是：对数组的第「(N-1)/2」～ 0 个元素执行下虑操作，即可保证该数组构成一个最小堆。这是因为包含 N 个元素的完全二叉树有「(N-1)/2」个非叶节点，这些非叶节点正是该完全二叉树的第 0 ～「(N-1)/2」个节点。我们只需要确保这些非叶子节点构成的子树都具有堆序性质即可。
**用数组模拟堆的算法代码（向上调整、向下调整等）见此：《敬请期待。。》**

## 代码实现

```cpp
// heaptimer.h
#ifndef HEAP_TIMER_H
#define HEAP_TIMER_H

#include <queue>
#include <unordered_map>
#include <time.h>
#include <algorithm>
#include <arpa/inet.h>
#include <functional>
#include <assert.h>
#include <chrono>
#include "../log/log.h"

typedef std::function<void()> TimeoutCallBack;
typedef std::chrono::high_resolution_clock Clock;
typedef std::chrono::milliseconds MS;
typedef Clock::time_point TimeStamp;

struct TimerNode {		// 使用结构体作为时间器
    int id;				// 每个定时器所唯一的
    TimeStamp expires;	// 设置该定时器的过期时间
    TimeoutCallBack cb;	// 当超时时执行回调函数，用来关闭过期连接
    // 由于需要维护最小堆，所以需要重载比较运算符，方便定时器之间的比较
    bool operator<(const TimerNode& t) {
        return expires < t.expires;
    }
};

class HeapTimer {		// 管理所有的定时器的堆
public:
    HeapTimer() { heap_.reserve(64); }

    ~HeapTimer() { clear(); }

    // 调整指定id的结点
    void adjust(int id, int newExpires);

	// 添加一个定时器
    void add(int id, int timeOut, const TimeoutCallBack& cb);

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

#endif //HEAP_TIMER_H
```

```cpp
#include "heaptimer.h"

void HeapTimer::siftup_(size_t i) {
    assert(i >= 0 && i < heap_.size());
    size_t j = (i - 1) / 2;
    while(j >= 0) {
        if(heap_[j] < heap_[i]) { break; }
        SwapNode_(i, j);
        i = j;
        j = (i - 1) / 2;
    }
}

void HeapTimer::SwapNode_(size_t i, size_t j) {
    assert(i >= 0 && i < heap_.size());
    assert(j >= 0 && j < heap_.size());
    std::swap(heap_[i], heap_[j]);
    ref_[heap_[i].id] = i;
    ref_[heap_[j].id] = j;
}

bool HeapTimer::siftdown_(size_t index, size_t n) {
    assert(index >= 0 && index < heap_.size());
    assert(n >= 0 && n <= heap_.size());
    size_t i = index;
    size_t j = i * 2 + 1;
    while(j < n) {
        if(j + 1 < n && heap_[j + 1] < heap_[j]) j++;
        if(heap_[i] < heap_[j]) break;
        SwapNode_(i, j);
        i = j;
        j = i * 2 + 1;
    }
    return i > index;
}

void HeapTimer::add(int id, int timeout, const TimeoutCallBack& cb) {
    assert(id >= 0);
    size_t i;
    if(ref_.count(id) == 0) {
        /* 新节点：堆尾插入，调整堆 */
        i = heap_.size();
        ref_[id] = i;	// map更新
        heap_.push_back({id, Clock::now() + MS(timeout), cb});	// 将新节点插入堆中
        siftup_(i);		// 此时新节点为叶节点，向上维护
    }
    else {
        /* 已有结点：调整堆 */
        i = ref_[id];
        heap_[i].expires = Clock::now() + MS(timeout);	// 更新其超时时间
        heap_[i].cb = cb;
        if(!siftdown_(i, heap_.size())) {
            siftup_(i);
        }
    }
}

void HeapTimer::doWork(int id) {
    /* 删除指定id结点，并触发回调函数 */
    if(heap_.empty() || ref_.count(id) == 0) {
        return;
    }
    size_t i = ref_[id];
    TimerNode node = heap_[i];
    node.cb();		// 执行其该定时器的任务
    del_(i);		// 执行结束将定时器删除
}

void HeapTimer::del_(size_t index) {
    /* 删除指定位置的结点 */
    assert(!heap_.empty() && index >= 0 && index < heap_.size());
    /* 将要删除的结点换到队尾，然后调整堆 */
    size_t i = index;
    size_t n = heap_.size() - 1;
    assert(i <= n);
    if(i < n) {
        SwapNode_(i, n);
        if(!siftdown_(i, n)) {
            siftup_(i);
        }
    }
    /* 队尾元素删除 */
    ref_.erase(heap_.back().id);
    heap_.pop_back();
}

void HeapTimer::adjust(int id, int timeout) {
    /* 调整指定id的结点 */
    assert(!heap_.empty() && ref_.count(id) > 0);
    heap_[ref_[id]].expires = Clock::now() + MS(timeout);;
    siftdown_(ref_[id], heap_.size());
}

void HeapTimer::tick() {
    /* 清除超时结点 */
    if(heap_.empty()) {
        return;
    }
    while(!heap_.empty()) {
        TimerNode node = heap_.front();
        if(std::chrono::duration_cast<MS>(node.expires - Clock::now()).count() > 0) {
            break;
        }
        node.cb();
        pop();
    }
}

void HeapTimer::pop() {
    assert(!heap_.empty());
    del_(0);
}

void HeapTimer::clear() {
    ref_.clear();
    heap_.clear();
}

int HeapTimer::GetNextTick() {
    tick();
    size_t res = -1;
    if(!heap_.empty()) {
        res = std::chrono::duration_cast<MS>(heap_.front().expires - Clock::now()).count();
        if(res < 0) { res = 0; }
    }
    return res;
}
```
