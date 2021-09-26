#ifndef IO_BUF_H_
#define IO_BUF_H_
#pragma once

class io_buf {
public:
  // 构造，创建一个io_buf对象
  io_buf(int size);

  // 清空数据
  void clear();

  // 将已经处理过的数据清空;将为处理的数据提前到数据首地址
  void adjust();

  // 将其他io_buf对象数据拷贝到自己手中
  void copy(const io_buf *other);

  // 处理长度为len的数据 和移动head和修改length
  void pop(int len);

  // 如果存在多个buffer采用链表的形式链接起来
  io_buf *next;

  // 当前buffer的缓存容量大小
  int capacity;
  // 当前buffer有效数据的长度
  int length;
  // 未处理数据的头部索引
  int head;
  // 当前io_buf所保存的数据地址
  char* data;

};

#endif // IO_BUF_H_
