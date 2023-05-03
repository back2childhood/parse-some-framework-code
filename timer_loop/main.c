/* ************************************************************************
> File Name:     main.c
> Author:        Qian JiLi
> mail:          193937157@qq.com
> Created Time:  Wed May  3 22:55:45 2023
> Description:
 ************************************************************************/
#include "TimeWheel.c"
struct request_para {
  void *timer;
  int val;
};

void mytimer(unsigned long arg) {
  struct request_para *para = (struct request_para *)arg;

  log("%d\n", para->val);
  mod_timer(para->timer, 3000); // 进行再次启动定时器

  sleep(10); /*定时器依然被阻塞*/

  // 定时器资源的释放是在这里完成的
  // ti_del_timer(para->timer);
}

int main(int argc, char *argv[]) {
  void *pwheel = NULL;
  void *timer = NULL;
  struct request_para *para;

  para = (struct request_para *)malloc(sizeof(struct request_para));
  if (NULL == para)
    return 0;
  bzero(para, sizeof(struct request_para));

  // 创建一个时间轮
  pwheel = ti_timewheel_create();
  if (NULL == pwheel)
    return -1;

  // 添加一个定时器
  para->val = 100;
  para->timer = ti_add_timer(pwheel, 3000, &mytimer, (unsigned long)para);

  while (1) {
    sleep(2);
  }

  // 释放时间轮
  ti_timewheel_release(pwheel);

  return 0;
}
