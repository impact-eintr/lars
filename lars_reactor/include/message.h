#ifndef MESSAGE_H_
#define MESSAGE_H_

// 接下来我们每次在server和 client之间传递数据的时候，都发送这种数据格式的头再加上后面的数据内容即可。
struct msg_head
{
  int msgid;
  int msglen;
};

// 消息头的二进制长度 固定数
#define MESSAGE_HEAD_LEN 8

// 消息头+消息体的最大长度限制
#define MESSAGE_LENGTH_LIMIT (65535 - MESSAGE_HEAD_LEN)


#endif // MESSAGE_H_