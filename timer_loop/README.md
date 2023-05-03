## 运行原理

![在这里插入图片描述](https://img-blog.csdnimg.cn/82a334ecb24c40b697ef5574728b14eb.png)
指针指向轮子上的一个槽，轮子以恒定的速度顺时针转动，每转动一步就指向下一个槽（虚线指针指向的槽），每次转动称为一个 tick，一个 tick 的时间称为时间轮的槽间隔 slot interval，即心搏时间。该时间轮转一周的时间是槽数*槽间隔（N*si）。每个槽指向一个定时器链表，每条链表上的定时器具有相同的特征：他们的定时时间相差 N\*si 的整数倍。时间轮正是利用这个关系将定时器散列到不同的链表中，假如现在指针指向槽 cs，我们要添加一个定时时间为 ti 的定时器，则该定时器将被插入槽 timer slot 对应的链表中：ts=(cs + (ti/si))%N。
时间轮使用了哈希表的思想，将定时器散列到不同的链表上。这样每条链表上的定时器数目都将明显少于原来的排序链表中的定时器数目，插入操作的效率基本不受定时器数目的影响。
对于时间轮而言，要提高定时精度，就要 si 值足够小；要提高执行效率，则要求 N 值足够大。

## 多级时间轮

![在这里插入图片描述](https://img-blog.csdnimg.cn/006532a1138e42d5809a4934b1218c1e.png)

上图是 5 个时间轮级联的效果图。中间的大轮是工作轮，只有在它上的任务才会被执行；其他轮上的任务时间到后迁移到下一级轮上，他们最终都会迁移到工作轮上而被调度执行。
多级时间轮的原理也容易理解：就拿时钟做说明，秒针转动一圈分针转动一格；分针转动一圈时针转动一格；同理时间轮也是如此：当低级轮转动一圈时，高一级轮转动一格，同时会将高一级轮上的任务重新分配到低级轮上。从而实现了多级轮级联的效果。

多级时间轮应该至少包括以下内容：

- 每一级时间轮对象
- 轮子上指针的位置
  因为：
- 定义多级时间轮，首先需要明确的便是级联的层数，也就是说需要确定有几个时间轮。
- 轮子上指针位置，就是当前时间轮运行到的位置，它与真实时间的差便是后续时间轮需要调度执行，它们的差值是时间轮运作起来的驱动力。

采用了一个 32 位的无符号整型作为 5 级时间轮计时器的计时辅助，此 32 位无符号整型自程序运行开始一直递增，每一个 tick 自增 1，采用这样的方式计数，可以通过这一个 32 位无符号整型来同时表示 5 个轮子的时间刻度位置。
![请添加图片描述](https://img-blog.csdnimg.cn/f5ea129dfca74af68cbb7169832c4a58.png)
事件轮对象定义为：

````cpp
// 第1个轮占的位数
#define TVR_BITS 8
// 第1个轮的长度
#define TVR_SIZE (1 << TVR_BITS)
// 第n个轮占的位数
#define TVN_BITS 6
// 第n个轮的长度
#define TVN_SIZE (1 << TVN_BITS)
// 掩码：取模或整除用
#define TVR_MASK (TVR_SIZE - 1)
#define TVN_MASK (TVN_SIZE - 1)

struct tvec {
    struct list_head vec[TVN_SIZE];/*64个格子*/
};

struct tvec_root{
    struct list_head vec[TVR_SIZE];/*256个格子*/
};```
这样，当我们想取某个轮上的指针位置为：
```cpp
// 第1个圈的当前指针位置
#define FIRST_INDEX(v) ((v) & TVR_MASK)
// 后面第n个圈的当前指针位置
#define NTH_INDEX(v, n) (((v) >> (TVR_BITS + (n - 1) * TVN_BITS)) & TVN_MASK)
````

## 多级时间轮代码

```cpp
// list.h 模拟双向链表
#if !defined(_BLKID_LIST_H) && !defined(LIST_HEAD)
#define _BLKID_LIST_H
#ifdef __cplusplus
extern "C" {
#endif
struct list_head {
 struct list_head *next, *prev;
};
#define LIST_HEAD_INIT(name) { &(name), &(name) }
#define LIST_HEAD(name) \
 struct list_head name = LIST_HEAD_INIT(name)
#define INIT_LIST_HEAD(ptr) do { \
 (ptr)->next = (ptr); (ptr)->prev = (ptr); \
} while (0)
static inline void
__list_add(struct list_head *entry,
                struct list_head *prev, struct list_head *next)
{
    next->prev = entry;
    entry->next = next;
    entry->prev = prev;
    prev->next = entry;
}
static inline void
list_add(struct list_head *entry, struct list_head *head)
{
    __list_add(entry, head, head->next);
}
static inline void
list_add_tail(struct list_head *entry, struct list_head *head)
{
    __list_add(entry, head->prev, head);
}
static inline void
__list_del(struct list_head *prev, struct list_head *next)
{
    next->prev = prev;
    prev->next = next;
}
static inline void
list_del(struct list_head *entry)
{
    __list_del(entry->prev, entry->next);
}
static inline void
list_del_init(struct list_head *entry)
{
    __list_del(entry->prev, entry->next);
    INIT_LIST_HEAD(entry);
}
static inline void list_move_tail(struct list_head *list,
      struct list_head *head)
{
 __list_del(list->prev, list->next);
 list_add_tail(list, head);
}
static inline int
list_empty(struct list_head *head)
{
    return head->next == head;
}
static inline void list_replace(struct list_head *old,
    struct list_head *new)
{
 new->next = old->next;
 new->next->prev = new;
 new->prev = old->prev;
 new->prev->next = new;
}
#define list_first_entry(ptr, type, member) \
    list_entry((ptr)->next, type, member)
static inline void list_replace_init(struct list_head *old,
     struct list_head *new)
{
 list_replace(old, new);
 INIT_LIST_HEAD(old);
}
#define list_entry(ptr, type, member) \
 ((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member)))
#define list_for_each(pos, head) \
 for (pos = (head)->next; pos != (head); pos = pos->next)
#define list_for_each_safe(pos, pnext, head) \
 for (pos = (head)->next, pnext = pos->next; pos != (head); \
      pos = pnext, pnext = pos->next)
#ifdef __cplusplus
}
#endif
#endif /* _BLKID_LIST_H */

// log.h debug
#ifndef _LOG_h_
#define _LOG_h_
#include <stdio.h>
#define COL(x)  "\033[;" #x "m"
#define RED     COL(31)
#define GREEN   COL(32)
#define YELLOW  COL(33)
#define BLUE    COL(34)
#define MAGENTA COL(35)
#define CYAN    COL(36)
#define WHITE   COL(0)
#define GRAY    "\033[0m"
#define errlog(fmt, arg...) do{     \
    printf(RED"[#ERROR: Toeny Sun:"GRAY YELLOW" %s:%d]:"GRAY WHITE fmt GRAY, __func__, __LINE__, ##arg);\
}while(0)
#define log(fmt, arg...) do{     \
    printf(WHITE"[#DEBUG: Toeny Sun: "GRAY YELLOW"%s:%d]:"GRAY WHITE fmt GRAY, __func__, __LINE__, ##arg);\
}while(0)
#endif
```

```cpp
// timewheel.c
/*
 *毫秒定时器  采用多级时间轮方式  借鉴linux内核中的实现
 *支持的范围为1 ~  2^32 毫秒(大约有49天)
 *若设置的定时器超过最大值 则按最大值设置定时器
 **/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include "list.h"
#include "log.h"
#define TVN_BITS   6
#define TVR_BITS   8
#define TVN_SIZE   (1<<TVN_BITS)
#define TVR_SIZE   (1<<TVR_BITS)

#define TVN_MASK   (TVN_SIZE - 1)
#define TVR_MASK   (TVR_SIZE - 1)

#define SEC_VALUE   0
#define USEC_VALUE   2000

struct tvec_base;
#define INDEX(N) ((ba->current_index >> (TVR_BITS + (N) * TVN_BITS)) & TVN_MASK)

typedef void (*timeouthandle)(unsigned long );


struct timer_list{
    struct list_head entry;          //将时间连接成链表
    unsigned long expires;           //超时时间
    void (*function)(unsigned long); //超时后的处理函数
    unsigned long data;              //处理函数的参数
    struct tvec_base *base;          //指向时间轮
};

struct tvec {
    struct list_head vec[TVN_SIZE];
};

struct tvec_root{
    struct list_head vec[TVR_SIZE];
};

//实现5级时间轮 范围为0~ (2^8 * 2^6 * 2^6 * 2^6 *2^6)=2^32
struct tvec_base
{
    unsigned long   current_index;
    pthread_t     thincrejiffies;
    pthread_t     threadID;
    struct tvec_root  tv1; /*第一个轮*/
    struct tvec       tv2; /*第二个轮*/
    struct tvec       tv3; /*第三个轮*/
    struct tvec       tv4; /*第四个轮*/
    struct tvec       tv5; /*第五个轮*/
};

static void internal_add_timer(struct tvec_base *base, struct timer_list *timer)
{
    struct list_head *vec;
    unsigned long expires = timer->expires;
    unsigned long idx = expires - base->current_index;
#if 1
    if( (signed long)idx < 0 ) /*这里是没有办法区分出是过时还是超长定时的吧?*/
    {
        vec = base->tv1.vec + (base->current_index & TVR_MASK);/*放到第一个轮的当前槽*/
    }
 else if ( idx < TVR_SIZE ) /*第一个轮*/
    {
        int i = expires & TVR_MASK;
        vec = base->tv1.vec + i;
    }
    else if( idx < 1 << (TVR_BITS + TVN_BITS) )/*第二个轮*/
    {
        int i = (expires >> TVR_BITS) & TVN_MASK;
        vec = base->tv2.vec + i;
    }
    else if( idx < 1 << (TVR_BITS + 2 * TVN_BITS) )/*第三个轮*/
    {
        int i = (expires >> (TVR_BITS + TVN_BITS)) & TVN_MASK;
        vec = base->tv3.vec + i;
    }
    else if( idx < 1 << (TVR_BITS + 3 * TVN_BITS) )/*第四个轮*/
    {
        int i = (expires >> (TVR_BITS + 2 * TVN_BITS)) & TVN_MASK;
        vec = base->tv4.vec + i;
    }
    else            /*第五个轮*/
    {
        int i;
        if (idx > 0xffffffffUL)
        {
            idx = 0xffffffffUL;
            expires = idx + base->current_index;
        }
        i = (expires >> (TVR_BITS + 3 * TVN_BITS)) & TVN_MASK;
        vec = base->tv5.vec + i;
    }
#else
 /*上面可以优化吧*/;
#endif
    list_add_tail(&timer->entry, vec);
}

static inline void detach_timer(struct timer_list *timer)
{
    struct list_head *entry = &timer->entry;
    __list_del(entry->prev, entry->next);
    entry->next = NULL;
    entry->prev = NULL;
}

static int __mod_timer(struct timer_list *timer, unsigned long expires)
{
    if(NULL != timer->entry.next)
        detach_timer(timer);

    internal_add_timer(timer->base, timer);

    return 0;
}

//修改定时器的超时时间外部接口
int mod_timer(void *ptimer, unsigned long expires)
{
    struct timer_list *timer  = (struct timer_list *)ptimer;
    struct tvec_base *base;

 base = timer->base;
    if(NULL == base)
        return -1;

    expires = expires + base->current_index;
    if(timer->entry.next != NULL  && timer->expires == expires)
        return 0;

    if( NULL == timer->function )
    {
        errlog("timer's timeout function is null\n");
        return -1;
    }

 timer->expires = expires;
    return __mod_timer(timer,expires);
}

//添加一个定时器
static void __ti_add_timer(struct timer_list *timer)
{
    if( NULL != timer->entry.next )
    {
        errlog("timer is already exist\n");
        return;
    }

    mod_timer(timer, timer->expires);
}

/*添加一个定时器  外部接口
 *返回定时器
 */
void* ti_add_timer(void *ptimewheel, unsigned long expires,timeouthandle phandle, unsigned long arg)
{
    struct timer_list  *ptimer;

    ptimer = (struct timer_list *)malloc( sizeof(struct timer_list) );
    if(NULL == ptimer)
        return NULL;

    bzero( ptimer,sizeof(struct timer_list) );
    ptimer->entry.next = NULL;
    ptimer->base = (struct tvec_base *)ptimewheel;
    ptimer->expires = expires;
    ptimer->function  = phandle;
    ptimer->data = arg;

    __ti_add_timer(ptimer);

    return ptimer;
}

/*
 *删除一个定时器  外部接口
 *
 * */
void ti_del_timer(void *p)
{
    struct timer_list *ptimer =(struct timer_list*)p;

    if(NULL == ptimer)
        return;

    if(NULL != ptimer->entry.next)
        detach_timer(ptimer);

    free(ptimer);
}
/*时间轮级联*/
static int cascade(struct tvec_base *base, struct tvec *tv, int index)
{
    struct list_head *pos,*tmp;
    struct timer_list *timer;
    struct list_head tv_list;

 /*将tv[index]槽位上的所有任务转移给tv_list,然后清空tv[index]*/
    list_replace_init(tv->vec + index, &tv_list);/*用tv_list替换tv->vec + index*/

    list_for_each_safe(pos, tmp, &tv_list)/*遍历tv_list双向链表，将任务重新添加到时间轮*/
    {
        timer = list_entry(pos,struct timer_list,entry);/*struct timer_list中成员entry的地址是pos, 获取struct timer_list的首地址*/
        internal_add_timer(base, timer);
    }

    return index;
}

static void *deal_function_timeout(void *base)
{
    struct timer_list *timer;
    int ret;
    struct timeval tv;
    struct tvec_base *ba = (struct tvec_base *)base;

    for(;;)
    {
        gettimeofday(&tv, NULL);
        while( ba->current_index <= (tv.tv_sec*1000 + tv.tv_usec/1000) )/*单位：ms*/
        {
           struct list_head work_list;
           int index = ba->current_index & TVR_MASK;/*获取第一个轮上的指针位置*/
           struct list_head *head = &work_list;
     /*指针指向0槽时，级联轮需要更新任务列表*/
           if(!index && (!cascade(ba, &ba->tv2, INDEX(0))) &&( !cascade(ba, &ba->tv3, INDEX(1))) && (!cascade(ba, &ba->tv4, INDEX(2))) )
               cascade(ba, &ba->tv5, INDEX(3));

            ba->current_index ++;
            list_replace_init(ba->tv1.vec + index, &work_list);
            while(!list_empty(head))
            {
                void (*fn)(unsigned long);
                unsigned long data;
                timer = list_first_entry(head, struct timer_list, entry);
                fn = timer->function;
                data = timer->data;
                detach_timer(timer);
                (*fn)(data);
            }
        }
    }
}

static void init_tvr_list(struct tvec_root * tvr)
{
    int i;

    for( i = 0; i<TVR_SIZE; i++ )
        INIT_LIST_HEAD(&tvr->vec[i]);
}


static void init_tvn_list(struct tvec * tvn)
{
    int i;

    for( i = 0; i<TVN_SIZE; i++ )
        INIT_LIST_HEAD(&tvn->vec[i]);
}

//创建时间轮  外部接口
void *ti_timewheel_create(void )
{
    struct tvec_base *base;
    int ret = 0;
    struct timeval tv;

    base = (struct tvec_base *) malloc( sizeof(struct tvec_base) );
    if( NULL==base )
        return NULL;

    bzero( base,sizeof(struct tvec_base) );

    init_tvr_list(&base->tv1);
    init_tvn_list(&base->tv2);
    init_tvn_list(&base->tv3);
    init_tvn_list(&base->tv4);
    init_tvn_list(&base->tv5);

    gettimeofday(&tv, NULL);
    base->current_index = tv.tv_sec*1000 + tv.tv_usec/1000;/*当前时间毫秒数*/

    if( 0 != pthread_create(&base->threadID,NULL,deal_function_timeout,base) )
    {
        free(base);
        return NULL;
    }
    return base;
}

static void ti_release_tvr(struct tvec_root *pvr)
{
    int i;
    struct list_head *pos,*tmp;
    struct timer_list *pen;

    for(i = 0; i < TVR_SIZE; i++)
    {
        list_for_each_safe(pos,tmp,&pvr->vec[i])
        {
            pen = list_entry(pos,struct timer_list, entry);
            list_del(pos);
            free(pen);
        }
    }
}

static void ti_release_tvn(struct tvec *pvn)
{
    int i;
    struct list_head *pos,*tmp;
    struct timer_list *pen;

    for(i = 0; i < TVN_SIZE; i++)
    {
        list_for_each_safe(pos,tmp,&pvn->vec[i])
        {
            pen = list_entry(pos,struct timer_list, entry);
            list_del(pos);
            free(pen);
        }
    }
}


/*
 *释放时间轮 外部接口
 * */
void ti_timewheel_release(void * pwheel)
{
    struct tvec_base *base = (struct tvec_base *)pwheel;

    if(NULL == base)
        return;

    ti_release_tvr(&base->tv1);
    ti_release_tvn(&base->tv2);
    ti_release_tvn(&base->tv3);
    ti_release_tvn(&base->tv4);
    ti_release_tvn(&base->tv5);

    free(pwheel);
}

/************demo****************/
struct request_para{
    void *timer;
    int val;
};

void mytimer(unsigned long arg)
{
    struct request_para *para = (struct request_para *)arg;

    log("%d\n",para->val);
    mod_timer(para->timer,3000);  //进行再次启动定时器

 sleep(10);/*定时器依然被阻塞*/

    //定时器资源的释放是在这里完成的
    //ti_del_timer(para->timer);
}

int main(int argc,char *argv[])
{
    void *pwheel = NULL;
    void *timer  = NULL;
    struct request_para *para;


    para = (struct request_para *)malloc( sizeof(struct request_para) );
    if(NULL == para)
        return 0;
    bzero(para,sizeof(struct request_para));

    //创建一个时间轮
    pwheel = ti_timewheel_create();
    if(NULL == pwheel)
        return -1;

    //添加一个定时器
    para->val = 100;
    para->timer = ti_add_timer(pwheel, 3000, &mytimer, (unsigned long)para);

    while(1)
    {
        sleep(2);
    }

    //释放时间轮
    ti_timewheel_release(pwheel);

    return 0;
}
```
