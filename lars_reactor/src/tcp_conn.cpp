#include "tcp_conn.h"
#include "event_loop.h"
#include "message.h"
#include <cstdint>
#include <cstdio>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

// 回显业务
void callback_echo(const char *data, uint32_t len, int msgid, void *args,
                   tcp_conn *conn) {
  conn->send_message(data, len, msgid);
}

// 连续的读事件回调
static void conn_rd_callback(event_loop *loop, int fd, void *args) {
  tcp_conn *conn = (tcp_conn*)args;
  conn->do_read();
}

// 连续的写事件回调
static void conn_wt_callback(event_loop *loop, int fd, void *args) {
  tcp_conn *conn = (tcp_conn*)args;
  conn->do_write();
}

// 初始化tcp
tcp_conn::tcp_conn(int connfd, event_loop *loop) {
  _connfd = connfd;
  _loop = loop;

  // 将connfd设置为非阻塞状态
  int flag = fcntl(_connfd,F_GETFL, 0);
  fcntl(_connfd, F_SETFL, flag|O_NONBLOCK);

  // 设置TCP_NODELAY禁止做读写缓存 降低小包延迟
  int op = 1;
  setsockopt(_connfd, IPPROTO_TCP, TCP_NODELAY, &op, sizeof(op));

  // 将该链接的读事件让event_loop监控
  _loop->add_io_event(_connfd, conn_rd_callback, EPOLLIN, this);

  // 将该链接集成到对应的tcp_server中
  // TODO
}

// 处理读业务
void tcp_conn::do_read() {
  // 从套接字读取数据
  int ret = ibuf.read_data(_connfd);
  if (ret == -1) {
    fprintf(stderr, "read data from socket\n");
    this->clean_conn();
    return;
  } else if (ret == 0) {
    // 对端正常关闭
    printf("connection closed by peer\n");
    clean_conn();
    return;
  }

  // 解析msg_head数据
  msg_head head;

  // 这里使用while 可以一次性读取多个完整包过来
  while(ibuf.length() >= MESSAGE_HEAD_LEN) {
    // 读取msg_head头部 固定长度MESSAGE_HEAD_LEN
    memcpy(&head, ibuf.data(), MESSAGE_HEAD_LEN);
    if (head.msglen > MESSAGE_LENGTH_LIMIT || head.msglen < 0) {
      fprintf(stderr, "data format error, need close, msglen=%d\n", head.msglen);
      this->clean_conn();
      break;
    }
    if (ibuf.length() < MESSAGE_HEAD_LEN+head.msglen) {
      // 缓存buf中剩余的数据小于实际上应该接受的数据
      // 说明是一个不完整的包 应该抛弃
      break;
    }
    // 再根据头长度数据读取数据体 然后针对数据体处理业务
    // TODO 添加包路由模式

    // 头部处理完了 往后偏移MESSAGE_HEAD_LEN长度
    ibuf.pop(MESSAGE_HEAD_LEN);

    // 处理ibuf.data() 业务数据
    printf("read data: %s\n", ibuf.data());

    // 回显业务
    callback_echo(ibuf.data(),head.msglen,head.msgid, nullptr, this);

    // 消息体处理完了 往后偏移msglen长度
    ibuf.pop(head.msglen);
  }
  ibuf.adjust();
  return;
}

// 处理写业务
void tcp_conn::do_write() {
  // do_write是触发完event事件要处理的事情，
  // 应该是直接将out_buf里的数据io写会对方客户端
  // 而不是在这里组装一个message再发
  // 组装message的过程应该是主动调用

  // 只要obuf中有数据就写
  while(obuf.length()) {
    int ret = obuf.write2fd(_connfd);
    if (ret == -1) {
      fprintf(stderr, "write2fd error, close conn!\n");
      this->clean_conn();
      return;
    }
    if (ret == 0) {
      // 不是错误 表示不可继续写
      break;
    }
  }
  printf("server do_write() over\n");

  if (obuf.length() == 0) {
    // 数据已经全部写完 将_connfd的写事件取消掉
    _loop->del_io_event(_connfd, EPOLLOUT);
  }
  return;
}

// 发送消息的方法
int tcp_conn::send_message(const char *data, int msglen, int msgid) {
  printf("server send_message:%s.%d, msgid=%d\n", data, msglen, msgid);
  bool active_epollout = false;
  if (obuf.length() == 0) {
    // 如果现在数据都已经发送完了 那么是一定要激活写事件 的
    // 如果有数据 说明数据还没有完全写到对端 那么没必要再激活 等写完再激活
    active_epollout = true;
  }

  // 先封装message消息头
  msg_head head;
  head.msglen = msglen;
  head.msgid = msgid;

  // 写消息头
  int ret = obuf.send_data((const char*)&head, MESSAGE_HEAD_LEN);
  if (ret != 0) {
    fprintf(stderr, "send head error\n");
    return 1;
  }

  // 写消息体
  ret = obuf.send_data(data, msglen);
  if (ret != 0) {
    // 如果写消息失败 那就回滚 将消息头的发送也取消
    obuf.pop(MESSAGE_HEAD_LEN);
    return -1;
  }

  if (active_epollout == true) {
    // 激活EPOLLOUT事件
    _loop->add_io_event(_connfd, conn_wt_callback, EPOLLOUT, this);
  }
  return 0;

}

// 销毁tcp_conn
void tcp_conn::clean_conn() {
  // 链接清理工作
  // 将该链接从tcp_server中摘除
  // TODO

  // 将该链接从event_loop中摘除
  _loop->del_io_event(_connfd);

  // buf清空
  ibuf.clear();
  obuf.clear();
  // 关闭原始套接字
  int fd = _connfd;
  _connfd = -1;
  close(fd);
}
