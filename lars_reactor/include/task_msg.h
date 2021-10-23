#ifndef TASK_MSG_H_
#define TASK_MSG_H_
#include "event_loop.h"

// 定义异步任务回调函数类型
// 第一个参数当然就是让哪个loop机制去执行这个task任务。
// 很明显，一个loop是对应一个thread线程的。也就是让哪个thread去执行这个task任务
typedef void (*task_func)(event_loop *loop, void *args);

struct task_msg {
  enum TASK_TYPE {
    NEW_CONN, // 新建链接的任务
    NEW_TASK, // 一般的任务
  };

  TASK_TYPE type;

  // 任务的一些参数
  union {
    // 针对 NEW_CONN新建链接任务 需要床底connfd
    int connfd;

    // 针对 NEW_TASK 新建任务 可以给一个任务提供一个回调函数
    struct {
      void (*task_cb)(event_loop *, void *args);
      void *args;
    };
  };
};

#endif // TASK_MSG_H_
