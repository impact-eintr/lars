#ifndef REACTOR_BUF_H_
#define REACTOR_BUF_H_

#pragma once
#include "io_buf.h"
#include "buf_pool.h"
#include <assert.h>
#include <unistd.h>

// 给业务层提供最后的tcp_buffer结构
// 这个的作用就是将io_buf作为自己的一个成员，然后做了一些包装
class reactor_buf {
public:
  reactor_buf();
  ~reactor_buf();

  const int length() const;
  void pop(int len);
  void clear();

protected:
  io_buf *_buf;
};

//  其中data()方法即取出已经读取的数据，adjust()含义和io_buf含义一致。主要是read_data()方法
class input_buf : public reactor_buf {
public:
  // 从一个fd中读取数据到reactor_buf中
  int read_data(int fd);

  // 取出读到的数据
  const char *data() const;

  // 重置缓冲区
  void adjust();
};

// send_data()方法主要是将数据写到io_buf中，实际上并没有做真正的写操作。
// 而是当调用write2fd方法时，才会将io_buf的数据写到对应的fd中。
// send_data是做一些buf内存块的申请等工作
class output_buf : public reactor_buf {
public:
  // 将一段数据写到一个reactor_buf中
  int send_data(const char *data, int datalen);
  // 将reactor_buf的数据写到一个fd中
  int write2fd(int fd);
};

#endif // REACTOR_BUF_H_
