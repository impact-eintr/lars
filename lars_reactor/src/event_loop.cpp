#include "event_loop.h"
#include "event_base.h"
#include <assert.h>
#include <sys/epoll.h>

// 构造 初始化epoll堆
event_loop::event_loop(){
  // flag = 0 等价于epoll_create
  _epfd = epoll_create(0);
  if (_epfd == -1) {
    fprintf(stderr, "epoll_create error\n");
    exit(1);
  }
}

// 阻塞循环处理事件
void event_loop::event_process(){
  while(true) {
    io_event_map_it ev_it;

    int nfds = epoll_wait(_epfd, _fired_evs, MAXEVENTS, 10);
    for (int i = 0;i < nfds;i++) {
      // 通过触发的fd找到对应的绑定事件
      ev_it = _io_evs.find(_fired_evs[i].data.fd);
      assert(ev_it != _io_evs.end());

      io_event *ev = &(ev_it->second);

      if (_fired_evs[i].events & EPOLLIN) {
        // 读事件 调回调函数
        void *args = ev->rcb_args;
        ev->read_callback(this, _fired_evs[i].data.fd, args);
      } else if (_fired_evs[i].events & EPOLLOUT) {
        // 写事件 调写回调函数
        void *args = ev->wcb_args;
        ev->write_callback(this, _fired_evs[i].data.fd, args);
      } else if (_fired_evs[i].events & (EPOLLHUP|EPOLLERR)) {
        // 水平触发未处理 可能出现HUP事件 正常处理读写 没有则清空
        if (ev->read_callback != nullptr) {
          void *args = ev->rcb_args;
          ev->read_callback(this, _fired_evs[i].data.fd, args);
        } else if (ev->write_callback != nullptr) {
          void *args = ev->wcb_args;
          ev->write_callback(this, _fired_evs[i].data.fd, args);
        } else {
          // 删除
          fprintf(stderr, "fd %d get error, delete it from epoll\n", _fired_evs[i].data.fd);
          this->del_io_event(_fired_evs[i].data.fd);
        }
      }
    }
  }
}

/*
 * 这里我们处理的事件机制是
 * 如果EPOLLIN 在mask中， EPOLLOUT就不允许在mask中
 * 如果EPOLLOUT 在mask中， EPOLLIN就不允许在mask中
 * 如果想注册EPOLLIN|EPOLLOUT的事件， 那么就调用add_io_event() 方法两次来注册。
 * */
