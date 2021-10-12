#pragma once

/*
 * 网络通信的抽象类，任何需要进行收发消息的模块，都可以实现该类
 * */
class net_connection
{
public:

    //发送消息的接口
    virtual int send_message(const char *data, int datalen, int cmdid) = 0;

    //获取通信文件描述符的接口 
    //virtual int get_fd() = 0;
};

// 创建链接/销毁链接 要触发的 回调函数类型
typedef void (* conn_callback)(net_connection *conn, void *args);
