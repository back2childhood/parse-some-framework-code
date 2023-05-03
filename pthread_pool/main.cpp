/* ************************************************************************
> File Name:     main.cpp
> Author:        Qian JiLi
> mail:          193937157@qq.com
> Created Time:  Wed May  3 22:26:02 2023
> Description:
 ************************************************************************/
#include "ThreadPool.h"

int func(int x, int y) { return x * y; }

int main() {
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
