# lars
Lars学习
## 内存管理与buffer封装
​ 在完成网络框架之前，我们先把必须的内存管理和buffer的封装完成。

这里我们先创建一个io_buf类，主要用来封装基本的buffer结构。然后用一个buf_pool来管理全部的buffer集合。

### io_buf 内存块

``` c++
lars_reactor/include/io_buf.h

#pragma once

/*
    定义一个 buffer存放数据的结构
 * */
class io_buf {
public:
    //构造，创建一个io_buf对象
    io_buf(int size);

    //清空数据
    void clear();

    //将已经处理过的数据，清空,将未处理的数据提前至数据首地址
    void adjust();

    //将其他io_buf对象数据考本到自己中
    void copy(const io_buf *other);

    //处理长度为len的数据，移动head和修正length
    void pop(int len);

    //如果存在多个buffer，是采用链表的形式链接起来
    io_buf *next;

    //当前buffer的缓存容量大小
    int capacity;
    //当前buffer有效数据长度
    int length;
    //未处理数据的头部位置索引
    int head;
    //当前io_buf所保存的数据地址
    char *data;
};

```

对应的io_buf实现的文件,如下

``` c++
lars_reactor/src/io_buf.cpp

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "io_buf.h"

//构造，创建一个io_buf对象
io_buf::io_buf(int size):
capacity(size), 
length(0),
head(0),
next(NULL) 
{
   data = new char[size];
   assert(data);
}

//清空数据
void io_buf::clear() {
    length = head = 0;
}

//将已经处理过的数据，清空,将未处理的数据提前至数据首地址
void io_buf::adjust() {
    if (head != 0) {
        if (length != 0) {
            memmove(data, data+head, length);
        }
        head = 0;
    }
}

//将其他io_buf对象数据考本到自己中
void io_buf::copy(const io_buf *other) {
    memcpy(data, other->data + other->head, other->length);
    head = 0;
    length = other->length;
}

//处理长度为len的数据，移动head和修正length
void io_buf::pop(int len) {
    length -= len;
    head += len;
}

```


​ 这里主要要注意io_buf的两个索引值length和head，一个是当前buffer的有效内存长度，haed则为可用的有效长度首数据位置。 capacity是io_buf的总容量空间大小。

​ 所以每次pop()则是弹出已经处理了多少，那么buffer剩下的内存就接下来需要处理的。

​ 然而adjust()则是从新重置io_buf,将所有数据都重新变成未处理状态。

​ clear()则是将length和head清0，这里没有提供delete真是删除物理内存的方法，因为这里的buffer设计是不需要清理的，接下来是用一个buf_pool来管理全部未被使用的io_buf集合。而且buf_pool的管理的内存是程序开始预开辟的，不会做清理工作.

### buf_pool 内存池
​ 接下来我们看看内存池的设计.

``` c++
lars_reactor/include/buf_pool.h

#pragma once

#include <ext/hash_map>
#include "io_buf.h"

typedef __gnu_cxx::hash_map<int, io_buf*> pool_t;

enum MEM_CAP {
    m4K     = 4096,
    m16K    = 16384,
    m64K    = 65536,
    m256K   = 262144,
    m1M     = 1048576,
    m4M     = 4194304,
    m8M     = 8388608
};


//总内存池最大限制 单位是Kb 所以目前限制是 5GB
#define EXTRA_MEM_LIMIT (5U *1024 *1024) 

/*
 *  定义buf内存池
 *  设计为单例
 * */
class buf_pool 
{
public:
    //初始化单例对象
    static void init() {
        //创建单例
        _instance = new buf_pool();
    }

    //获取单例方法
    static buf_pool *instance() {
        //保证init方法在这个进程执行中 只被执行一次
        pthread_once(&_once, init);
        return _instance;
    }

    //开辟一个io_buf
    io_buf *alloc_buf(int N);
    io_buf *alloc_buf() { return alloc_buf(m4K); }


    //重置一个io_buf
    void revert(io_buf *buffer);

    
private:
    buf_pool();

    //拷贝构造私有化
    buf_pool(const buf_pool&);
    const buf_pool& operator=(const buf_pool&);

    //所有buffer的一个map集合句柄
    pool_t _pool;

    //总buffer池的内存大小 单位为KB
    uint64_t _total_mem;

    //单例对象
    static buf_pool *_instance;

    //用于保证创建单例的init方法只执行一次的锁
    static pthread_once_t _once;

    //用户保护内存池链表修改的互斥锁
    static pthread_mutex_t _mutex;
};

```

​ 首先buf_pool采用单例的方式进行设计。因为系统希望仅有一个内存池管理模块。这里内存池用一个__gnu_cxx::hash_map<int, io_buf*>的map类型进行管理，其中key是每个组内存的空间容量，参考

``` c++
enum MEM_CAP {
    m4K     = 4096,
    m16K    = 16384,
    m64K    = 65536,
    m256K   = 262144,
    m1M     = 1048576,
    m4M     = 4194304,
    m8M     = 8388608
};

```

​ 其中每个key下面挂在一个io_buf链表。而且buf_pool预先会给map下的每个key的内存组开辟好一定数量的内存块。然后上层用户在使用的时候每次取出一个内存块，就会将该内存块从该内存组摘掉。当然使用完就放回来。如果不够使用会额外开辟，也有最大的内存限制，在宏EXTRA_MEM_LIMIT中。

具体的buf_pool实现如下:

``` c++
lars_reactor/src/buf_pool.cpp

#include "buf_pool.h"
#include <assert.h>


//单例对象
buf_pool * buf_pool::_instance = NULL;

//用于保证创建单例的init方法只执行一次的锁
pthread_once_t buf_pool::_once = PTHREAD_ONCE_INIT;


//用户保护内存池链表修改的互斥锁
pthread_mutex_t buf_pool::_mutex = PTHREAD_MUTEX_INITIALIZER;


//构造函数 主要是预先开辟一定量的空间
//这里buf_pool是一个hash，每个key都是不同空间容量
//对应的value是一个io_buf集合的链表
//buf_pool -->  [m4K] -- io_buf-io_buf-io_buf-io_buf...
//              [m16K] -- io_buf-io_buf-io_buf-io_buf...
//              [m64K] -- io_buf-io_buf-io_buf-io_buf...
//              [m256K] -- io_buf-io_buf-io_buf-io_buf...
//              [m1M] -- io_buf-io_buf-io_buf-io_buf...
//              [m4M] -- io_buf-io_buf-io_buf-io_buf...
//              [m8M] -- io_buf-io_buf-io_buf-io_buf...
buf_pool::buf_pool():_total_mem(0)
{
    io_buf *prev; 
    
    //----> 开辟4K buf 内存池
    _pool[m4K] = new io_buf(m4K);
    if (_pool[m4K] == NULL) {
        fprintf(stderr, "new io_buf m4K error");
        exit(1);
    }

    prev = _pool[m4K];
    //4K的io_buf 预先开辟5000个，约20MB供开发者使用
    for (int i = 1; i < 5000; i ++) {
        prev->next = new io_buf(m4K);
        if (prev->next == NULL) {
            fprintf(stderr, "new io_buf m4K error");
            exit(1);
        }
        prev = prev->next;
    }
    _total_mem += 4 * 5000;



    //----> 开辟16K buf 内存池
    _pool[m16K] = new io_buf(m16K);
    if (_pool[m16K] == NULL) {
        fprintf(stderr, "new io_buf m16K error");
        exit(1);
    }

    prev = _pool[m16K];
    //16K的io_buf 预先开辟1000个，约16MB供开发者使用
    for (int i = 1; i < 1000; i ++) {
        prev->next = new io_buf(m16K);
        if (prev->next == NULL) {
            fprintf(stderr, "new io_buf m16K error");
            exit(1);
        }
        prev = prev->next;
    }
    _total_mem += 16 * 1000;



    //----> 开辟64K buf 内存池
    _pool[m64K] = new io_buf(m64K);
    if (_pool[m64K] == NULL) {
        fprintf(stderr, "new io_buf m64K error");
        exit(1);
    }

    prev = _pool[m64K];
    //64K的io_buf 预先开辟500个，约32MB供开发者使用
    for (int i = 1; i < 500; i ++) {
        prev->next = new io_buf(m64K);
        if (prev->next == NULL) {
            fprintf(stderr, "new io_buf m64K error");
            exit(1);
        }
        prev = prev->next;
    }
    _total_mem += 64 * 500;


    //----> 开辟256K buf 内存池
    _pool[m256K] = new io_buf(m256K);
    if (_pool[m256K] == NULL) {
        fprintf(stderr, "new io_buf m256K error");
        exit(1);
    }

    prev = _pool[m256K];
    //256K的io_buf 预先开辟200个，约50MB供开发者使用
    for (int i = 1; i < 200; i ++) {
        prev->next = new io_buf(m256K);
        if (prev->next == NULL) {
            fprintf(stderr, "new io_buf m256K error");
            exit(1);
        }
        prev = prev->next;
    }
    _total_mem += 256 * 200;


    //----> 开辟1M buf 内存池
    _pool[m1M] = new io_buf(m1M);
    if (_pool[m1M] == NULL) {
        fprintf(stderr, "new io_buf m1M error");
        exit(1);
    }

    prev = _pool[m1M];
    //1M的io_buf 预先开辟50个，约50MB供开发者使用
    for (int i = 1; i < 50; i ++) {
        prev->next = new io_buf(m1M);
        if (prev->next == NULL) {
            fprintf(stderr, "new io_buf m1M error");
            exit(1);
        }
        prev = prev->next;
    }
    _total_mem += 1024 * 50;


    //----> 开辟4M buf 内存池
    _pool[m4M] = new io_buf(m4M);
    if (_pool[m4M] == NULL) {
        fprintf(stderr, "new io_buf m4M error");
        exit(1);
    }

    prev = _pool[m4M];
    //4M的io_buf 预先开辟20个，约80MB供开发者使用
    for (int i = 1; i < 20; i ++) {
        prev->next = new io_buf(m4M);
        if (prev->next == NULL) {
            fprintf(stderr, "new io_buf m4M error");
            exit(1);
        }
        prev = prev->next;
    }
    _total_mem += 4096 * 20;



    //----> 开辟8M buf 内存池
    _pool[m8M] = new io_buf(m8M);
    if (_pool[m8M] == NULL) {
        fprintf(stderr, "new io_buf m8M error");
        exit(1);
    }

    prev = _pool[m8M];
    //8M的io_buf 预先开辟10个，约80MB供开发者使用
    for (int i = 1; i < 10; i ++) {
        prev->next = new io_buf(m8M);
        if (prev->next == NULL) {
            fprintf(stderr, "new io_buf m8M error");
            exit(1);
        }
        prev = prev->next;
    }
    _total_mem += 8192 * 10;
}


//开辟一个io_buf
//1 如果上层需要N个字节的大小的空间，找到与N最接近的buf hash组，取出，
//2 如果该组已经没有节点使用，可以额外申请
//3 总申请长度不能够超过最大的限制大小 EXTRA_MEM_LIMIT
//4 如果有该节点需要的内存块，直接取出，并且将该内存块从pool摘除
io_buf *buf_pool::alloc_buf(int N) 
{
    //1 找到N最接近哪hash 组
    int index;
    if (N <= m4K) {
        index = m4K;
    }
    else if (N <= m16K) {
        index = m16K;
    }
    else if (N <= m64K) {
        index = m64K;
    }
    else if (N <= m256K) {
        index = m256K;
    }
    else if (N <= m1M) {
        index = m1M;
    }
    else if (N <= m4M) {
        index = m4M;
    }
    else if (N <= m8M) {
        index = m8M;
    }
    else {
        return NULL;
    }


    //2 如果该组已经没有，需要额外申请，那么需要加锁保护
    pthread_mutex_lock(&_mutex);
    if (_pool[index] == NULL) {
        if (_total_mem + index/1024 >= EXTRA_MEM_LIMIT) {
            //当前的开辟的空间已经超过最大限制
            fprintf(stderr, "already use too many memory!\n");
            exit(1);
        }

        io_buf *new_buf = new io_buf(index);
        if (new_buf == NULL) {
            fprintf(stderr, "new io_buf error\n");
            exit(1);
        }
        _total_mem += index/1024;
        pthread_mutex_unlock(&_mutex);
        return new_buf;
    }

    //3 从pool中摘除该内存块
    io_buf *target = _pool[index];
    _pool[index] = target->next;
    pthread_mutex_unlock(&_mutex);
    
    target->next = NULL;
    
    return target;
}


//重置一个io_buf,将一个buf 上层不再使用，或者使用完成之后，需要将该buf放回pool中
void buf_pool::revert(io_buf *buffer)
{
    //每个buf的容量都是固定的 在hash的key中取值
    int index = buffer->capacity;
    //重置io_buf中的内置位置指针
    buffer->length = 0;
    buffer->head = 0;

    pthread_mutex_lock(&_mutex);
    //找到对应的hash组 buf首届点地址
    assert(_pool.find(index) != _pool.end());

    //将buffer插回链表头部
    buffer->next = _pool[index];
    _pool[index] = buffer;

    pthread_mutex_unlock(&_mutex);
}

```

​ 其中，buf_pool构造函数中实现了内存池的hash预开辟内存工作，具体的数据结构如下

//buf_pool -->  [m4K] --> io_buf-io_buf-io_buf-io_buf...
//              [m16K] --> io_buf-io_buf-io_buf-io_buf...
//              [m64K] --> io_buf-io_buf-io_buf-io_buf...
//              [m256K] --> io_buf-io_buf-io_buf-io_buf...
//              [m1M] --> io_buf-io_buf-io_buf-io_buf...
//              [m4M] --> io_buf-io_buf-io_buf-io_buf...
//              [m8M] --> io_buf-io_buf-io_buf-io_buf...
​ alloc_buf()方法，是调用者从内存池中取出一块内存，如果最匹配的内存块存在，则返回，并将该块内存从buf_pool中摘除掉，如果没有则开辟一个内存出来。 revert()方法则是将已经使用完的io_buf重新放回buf_pool中。

3.3 读写buffer机制
​ 那么接下来我们就需要实现一个专门用来读(输入)数据的input_buf和专门用来写(输出)数据的output_buf类了。由于这两个人都应该拥有一些io_buf的特性，所以我们先定义一个基础的父类reactor_buf。

A. reactor_buf类
lars_reactor/include/reactor_buf.h

#pragma once
#include "io_buf.h"
#include "buf_pool.h"
#include <assert.h>
#include <unistd.h>


/*
 * 给业务层提供的最后tcp_buffer结构
 * */
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


​ 这个的作用就是将io_buf作为自己的一个成员，然后做了一些包装。具体方法实现如下。

lars_reactor/src/reactor.cpp

#include "reactor_buf.h"
#include <sys/ioctl.h>
#include <string.h>

reactor_buf::reactor_buf() 
{
    _buf = NULL;
}

reactor_buf::~reactor_buf()
{
    clear();
}

const int reactor_buf::length() const 
{
    return _buf != NULL? _buf->length : 0;
}

void reactor_buf::pop(int len) 
{
    assert(_buf != NULL && len <= _buf->length);

    _buf->pop(len);

    //当此时_buf的可用长度已经为0
    if(_buf->length == 0) {
        //将_buf重新放回buf_pool中
        buf_pool::instance()->revert(_buf);
        _buf = NULL;
    }
}

void reactor_buf::clear()
{
    if (_buf != NULL)  {
        //将_buf重新放回buf_pool中
        buf_pool::instance()->revert(_buf);
        _buf = NULL;
    }
}
B. input_buf类
​ 接下来就可以集成reactor_buf类实现input_buf类的设计了。

lars_reactor/include/reactor_buf.h

//读(输入) 缓存buffer
class input_buf : public reactor_buf 
{
public:
    //从一个fd中读取数据到reactor_buf中
    int read_data(int fd);

    //取出读到的数据
    const char *data() const;

    //重置缓冲区
    void adjust();
};
​ 其中data()方法即取出已经读取的数据，adjust()含义和io_buf含义一致。主要是read_data()方法。具体实现如下。

lars_reactor/src/reactor.cpp

//从一个fd中读取数据到reactor_buf中
int input_buf::read_data(int fd)
{
    int need_read;//硬件有多少数据可以读

    //一次性读出所有的数据
    //需要给fd设置FIONREAD,
    //得到read缓冲中有多少数据是可以读取的
    if (ioctl(fd, FIONREAD, &need_read) == -1) {
        fprintf(stderr, "ioctl FIONREAD\n");
        return -1;
    }

    
    if (_buf == NULL) {
        //如果io_buf为空,从内存池申请
        _buf = buf_pool::instance()->alloc_buf(need_read);
        if (_buf == NULL) {
            fprintf(stderr, "no idle buf for alloc\n");
            return -1;
        }
    }
    else {
        //如果io_buf可用，判断是否够存
        assert(_buf->head == 0);
        if (_buf->capacity - _buf->length < (int)need_read) {
            //不够存，冲内存池申请
            io_buf *new_buf = buf_pool::instance()->alloc_buf(need_read+_buf->length);
            if (new_buf == NULL) {
                fprintf(stderr, "no ilde buf for alloc\n");
                return -1;
            }
            //将之前的_buf的数据考到新申请的buf中
            new_buf->copy(_buf);
            //将之前的_buf放回内存池中
            buf_pool::instance()->revert(_buf);
            //新申请的buf成为当前io_buf
            _buf = new_buf;
        }
    }

    //读取数据
    int already_read = 0;
    do { 
        //读取的数据拼接到之前的数据之后
        if(need_read == 0) {
            //可能是read阻塞读数据的模式，对方未写数据
            already_read = read(fd, _buf->data + _buf->length, m4K);
        } else {
            already_read = read(fd, _buf->data + _buf->length, need_read);
        }
    } while (already_read == -1 && errno == EINTR); //systemCall引起的中断 继续读取
    if (already_read > 0)  {
        if (need_read != 0) {
            assert(already_read == need_read);
        }
        _buf->length += already_read;
    }

    return already_read;
}

//取出读到的数据
const char *input_buf::data() const 
{
    return _buf != NULL ? _buf->data + _buf->head : NULL;
}

//重置缓冲区
void input_buf::adjust()
{
    if (_buf != NULL) {
        _buf->adjust();
    }
}

C. output_buf类
​ 接下来就可以集成reactor_buf类实现output_buf类的设计了。

lars_reactor/include/reactor_buf.h

//写(输出)  缓存buffer
class output_buf : public reactor_buf 
{
public:
    //将一段数据 写到一个reactor_buf中
    int send_data(const char *data, int datalen);

    //将reactor_buf中的数据写到一个fd中
    int write2fd(int fd);
};

​ send_data()方法主要是将数据写到io_buf中，实际上并没有做真正的写操作。而是当调用write2fd方法时，才会将io_buf的数据写到对应的fd中。send_data是做一些buf内存块的申请等工作。具体实现如下

lars_reactor/src/reactor.cpp

//将一段数据 写到一个reactor_buf中
int output_buf::send_data(const char *data, int datalen)
{
    if (_buf == NULL) {
        //如果io_buf为空,从内存池申请
        _buf = buf_pool::instance()->alloc_buf(datalen);
        if (_buf == NULL) {
            fprintf(stderr, "no idle buf for alloc\n");
            return -1;
        }
    }
    else {
        //如果io_buf可用，判断是否够存
        assert(_buf->head == 0);
        if (_buf->capacity - _buf->length < datalen) {
            //不够存，冲内存池申请
            io_buf *new_buf = buf_pool::instance()->alloc_buf(datalen+_buf->length);
            if (new_buf == NULL) {
                fprintf(stderr, "no ilde buf for alloc\n");
                return -1;
            }
            //将之前的_buf的数据考到新申请的buf中
            new_buf->copy(_buf);
            //将之前的_buf放回内存池中
            buf_pool::instance()->revert(_buf);
            //新申请的buf成为当前io_buf
            _buf = new_buf;
        }
    }

    //将data数据拷贝到io_buf中,拼接到后面
    memcpy(_buf->data + _buf->length, data, datalen);
    _buf->length += datalen;

    return 0;
}

//将reactor_buf中的数据写到一个fd中
int output_buf::write2fd(int fd)
{
    assert(_buf != NULL && _buf->head == 0);

    int already_write = 0;

    do { 
        already_write = write(fd, _buf->data, _buf->length);
    } while (already_write == -1 && errno == EINTR); //systemCall引起的中断，继续写


    if (already_write > 0) {
        //已经处理的数据清空
        _buf->pop(already_write);
        //未处理数据前置，覆盖老数据
        _buf->adjust();
    }

    //如果fd非阻塞，可能会得到EAGAIN错误
    if (already_write == -1 && errno == EAGAIN) {
        already_write = 0;//不是错误，仅仅返回0，表示目前是不可以继续写的
    }

    return already_write;
}

​ 现在我们已经完成了内存管理及读写buf机制的实现，接下来就要简单的测试一下，用我们之前的V0.1版本的reactor server来测试。

3.4 完成Lars Reactor V0.2开发
A. 修改tcp_server
​ 主要修正do_accept()方法，加上reactor_buf机制.

lars_reactor/src/tcp_server.cpp

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>

#include "tcp_server.h"
#include "reactor_buf.h"



//server的构造函数
tcp_server::tcp_server(const char *ip, uint16_t port)
{
    //...
}

//开始提供创建链接服务
void tcp_server::do_accept()
{
    int connfd;    
    while(true) {
        //accept与客户端创建链接
        printf("begin accept\n");
        connfd = accept(_sockfd, (struct sockaddr*)&_connaddr, &_addrlen);
        if (connfd == -1) {
            if (errno == EINTR) {
                fprintf(stderr, "accept errno=EINTR\n");
                continue;
            }
            else if (errno == EMFILE) {
                //建立链接过多，资源不够
                fprintf(stderr, "accept errno=EMFILE\n");
            }
            else if (errno == EAGAIN) {
                fprintf(stderr, "accept errno=EAGAIN\n");
                break;
            }
            else {
                fprintf(stderr, "accept error");
                exit(1);
            }
        }
        else {
            //accept succ!
            
            int ret = 0;
            input_buf ibuf;
            output_buf obuf;

            char *msg = NULL;
            int msg_len = 0;
            do { 
                ret = ibuf.read_data(connfd);
                if (ret == -1) {
                    fprintf(stderr, "ibuf read_data error\n");
                    break;
                }
                printf("ibuf.length() = %d\n", ibuf.length());

                
                //将读到的数据放在msg中
                msg_len = ibuf.length();
                msg = (char*)malloc(msg_len);
                bzero(msg, msg_len);
                memcpy(msg, ibuf.data(), msg_len);
                ibuf.pop(msg_len);
                ibuf.adjust();

                printf("recv data = %s\n", msg);

                //回显数据
                obuf.send_data(msg, msg_len);
                while(obuf.length()) {
                    int write_ret = obuf.write2fd(connfd);
                    if (write_ret == -1) {
                        fprintf(stderr, "write connfd error\n");
                        return;
                    }
                    else if(write_ret == 0) {
                        //不是错误，表示此时不可写
                        break;
                    }
                }
                 

                free(msg);
                    
            } while (ret != 0);     


            //Peer is closed
            close(connfd);
        }
    }
}
编译生成新的liblreactor.a

$cd lars_reactor/
$make
g++ -g -O2 -Wall -fPIC -Wno-deprecated -c -o src/tcp_server.o src/tcp_server.cpp -I./include
g++ -g -O2 -Wall -fPIC -Wno-deprecated -c -o src/io_buf.o src/io_buf.cpp -I./include
g++ -g -O2 -Wall -fPIC -Wno-deprecated -c -o src/reactor_buf.o src/reactor_buf.cpp -I./include
g++ -g -O2 -Wall -fPIC -Wno-deprecated -c -o src/buf_pool.o src/buf_pool.cpp -I./include
mkdir -p lib
ar cqs lib/liblreactor.a src/tcp_server.o src/io_buf.o src/reactor_buf.o src/buf_pool.o

B. 编译V0.2 server APP
​ 我们将lars_reactor/example/lars_reactor_0.1 的代码复制一份到 lars_reactor/example/lars_reactor_0.2中。

由于我们这里使用了pthread库，所以在lars_reactor_0.2的Makefile文件要加上pthread库的关联

lars_reactor/example/lars_reactor_0.2/Makefile

CXX=g++
CFLAGS=-g -O2 -Wall -fPIC -Wno-deprecated 

INC=-I../../include
LIB=-L../../lib -llreactor -lpthread
OBJS = $(addsuffix .o, $(basename $(wildcard *.cc)))

all:
	$(CXX) -o lars_reactor $(CFLAGS)  lars_reactor.cpp $(INC) $(LIB)

clean:
	-rm -f *.o lars_reactor
编译在lars_reactor/example/lars_reactor_0.2/

$ cd lars_reactor/example/lars_reactor_0.2/
$ make
g++ -o lars_reactor -g -O2 -Wall -fPIC -Wno-deprecated   lars_reactor.cpp -I../../include -L../../lib -llreactor  -lpthread
C. 测试
启动server

$ ./lars_reactor 
begin accept
启动client

$ nc 127.0.0.1 7777
客户端输入 文字，效果如下：

服务端：

ibuf.length() = 21
recv data = hello lars, By Aceld
客户端：

$ nc 127.0.0.1 7777
hello lars, By Aceld
hello lars, By Aceld
​ ok!现在我们的读写buffer机制已经成功的集成到我们的lars网络框架中了。
