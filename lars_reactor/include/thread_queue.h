#ifndef THREAD_QUEUE_H_
#define THREAD_QUEUE_H_

#include "event_loop.h"
#include <pthread.h>
#include <queue>
#include <stdio.h>
#include <sys/eventfd.h>
#include <unistd.h>

// 每个thread对应的 消息任务队列
template <typename T> class thread_queue {
public:
  thread_queue() {
    _loop = nullptr;
    pthread_mutex_init(&_queue_mutex, nullptr);
    _evfd = eventfd(0, EFD_NONBLOCK);
    if (_evfd == -1) {
      perror("eventfd(0, EFD_NONBLOCK)");
      exit(1);
    }
  }

  ~thread_queue() {
    pthread_mutex_destroy(&_queue_mutex);
    close(_evfd);
  }

  // 向队列添加一个任务
  // 通过send将任务发送给消息队列。
  void send(const T &task) {
    // 触发消息事件的占位传输 TODO 测试为什么要使用这个数据类型
    unsigned long long idle_num = 1;

    pthread_mutex_lock(&_queue_mutex);
    // 将任务添加到队列
    _queue.push(task);

    // 向_evfd写 触发对应的EPOLLIN事件 来处理该任务
    int ret = write(_evfd, &idle_num, sizeof(unsigned long long));
    if (ret == -1) {
      perror("_evfd write");
    }

    pthread_mutex_unlock(&_queue_mutex);
  }

  // 获取队列，(当前队列中已经有任务)
  // 在io_callback中调用recv取得task任务，根据任务的不同类型，处理自定义不同业务流程
  void recv(std::queue<T> &new_queue) {
    unsigned int long long idle_num = 1;
    pthread_mutex_lock(&_queue_mutex);
    // 把占位的数据读出来，确保底层缓冲没有数据存留
    int ret = read(_evfd, &idle_num, sizeof(unsigned long long));
    if (ret == -1) {
      perror("_evfd read");
    }

    // 将当前的队列拷贝出去 将一个空队列换回当前队列 同时清空自身队列
    // 请确保new_queue是空队列
    std::swap(new_queue, _queue);

    pthread_mutex_unlock(&_queue_mutex);
  }

  // 设置当前thread_queue是被哪个事件触发event_loop监控
  void set_loop(event_loop *loop) { _loop = loop; }

  // 设置当前消息任务队列的每个业务的回调业务
  // 通过event_loop触发注册的io_callback得到消息队列里的任务
  void set_callback(io_callback *cb, void *args = nullptr) {
    if (_loop != nullptr) {
      _loop->add_io_event(_evfd, cb, EPOLLIN, args);
    }
  }

  // 得到当前loop
  event_loop *get_loop() { return _loop; }

private:
  int _evfd; // 触发消息任务队列读取的每个消息业务的fd
  event_loop *_loop; // 当前消息队列所绑定在那个event_loop事件触发机制中
  std::queue<T> _queue;         // 队列
  pthread_mutex_t _queue_mutex; // 进行添加任务 读取任务的保护锁
};

#endif // THREAD_QUEUE_H_
