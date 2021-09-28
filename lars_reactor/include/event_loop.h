#ifndef EVENT_LOOP_H_
#define EVENT_LOOP_H_

#include <sys/epoll.h>
#include <ext/hash_map>
#include <ext/hash_set>
#include <sys/socket.h>
#include "event_base.h"

#define MAXEVENTS 10

// map: fd=>io_event
typedef __gnu_cxx::hash_map<int, io_event> io_event_map;
// 定义指向上面map类型的迭代器
typedef __gnu_cxx::hash_map<int, io_event>::iterator io_event_map_it;
// 全部正在监听的fd集合
typedef __gnu_cxx::hash_set<int> listen_fd_set;

class event_loop {
public:
  // 构造 初始化epoll堆
  event_loop();
  // 阻塞循环处理事件
  void event_process();
  // 添加一个io事件组到loop中
  void add_io_event(int fd, io_callback *proc, int mask, void *args=nullptr);
  // 从loop中删除一个io事件
  void del_io_event(int fd);
  // 删除一个io事件的EPOLLIN/EPOLLOUT
  void del_io_event(int fd, int mask);
private:
  int _epfd; // epoll fd
  // 当前event_loop监控的fd和对应事件的关系
  io_event_map _io_evs;
  // 当前event_loop 一共有那些fd在监听
  listen_fd_set listen_fds;
  // 一次性最大处理事件 已经通过epoll_wait返回的被激活需要上层处理的fd集合
  struct epoll_event _fired_evs[MAXEVENTS];
};

#endif // EVENT_LOOP_H_
