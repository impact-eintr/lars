# epoll学习

# 理解IO复用 阻塞与非阻塞

### 流
- 可以进行IO操作的内核对象
- 文件、管道、套接字
- 流的入口 文件描述符

### 阻塞
- 阻塞等待
- 非阻塞 忙轮询

### IO多路复用
既阻塞还能同时监听所个IO请求的状态

# IO多路复用解决的问题

### 阻塞+多进程/多线程
资源消耗大

### 非阻塞+忙轮询

``` c++
while true {
    for i in 流[] {
        if i.data != NULL {
            read();
        } else {
            // 其他处理
        }
    }
}
```

### select

``` c++
while true {
    select(流[]); // 阻塞 cpu可以参与其他运算
    
    // 阻塞结束 开始处理io事件
    for i in 流[] { // 还是需要遍历全部流 有点浪费资源
        if i.data != NULL {
            read();
        } else {
            // 其他处理
        }
    }
}
```

### epoll

``` c++
while true {
    可处理的流[] = epoll_wait(epoll_fd); // 阻塞 cpu可以参与其他运算
    
    // 阻塞结束 开始处理io事件
    for i in 可处理的流[] {
        if i.data != NULL {
            read();
        } else {
            // 其他处理
        }
    }
}
```

- 与select poll一样，对I/O多路复用的技术
- 只关心`活跃`链接，无需遍历全部描述符集合
- 能够处理大量的链接请求

# API
- 创建epoll

``` c++
int epfd = epoll_create(1000);
```

这里的`epfd`是在内核创建一颗红黑树的根节点

- 控制epoll

``` c++
int epoll_ctl(int epfd, /*epoll句柄*/
    int op, /*EPOLL_CTL_ADD添加新的 EPOLL_CTL_MOD修改已有的 EPOLL_CTL_DEL epfd删除一个fd*/
    int fd, /*需要监听的文件描述符*/
    struct epoll_event *event /*告诉内核需要监听的事件*/)
```

``` c++
struct epoll_event {
    __uint32_t events; // epoll事件
    epoll_data_t data; // 用户传递的数据
}

// events: {EPOLLIN EPOLLOUT EPOLLPRI EPOLLHUP EPOLLET EPOLLONESHOT}
typedef union epoll_data {
    void *ptr;
    int fd;
    uint32_t u32;
    uint64_t u64;
} epoll_data_t;

```

``` c++
struct epoll_event new_event;
new_event.events = EPOLLIN | EPOLLOUT;
new_event.data.fd = 5;

epoll_ctl(epfd, EPOLL_CTL_ADD, 5, &new_event);
```

- 等待epoll

``` c++
int epoll_wait(int epfd, /*epoll句柄*/
    struct epoll_event *event, /*从内核得到的事件集合*/
    int maxevents, /*告知内核这个events有多大 注意这个值不能大于创建时的大小*/
    int timeout /*超时时间 -1 永久阻塞 0 立刻返回非阻塞 >0 指定微妙后返回*/
)
```

``` c++
struct epoll_event my_event[1000];
int event_cnt = epoll_wait(epfd, my_event, 1000, -1);
```

## epoll编程框架

``` c++
// 创建epoll
int epfd = epoll_create(1000);

// 将listen_fd添加到epoll中
epoll_ctl(epfd, RPOLL_CTL_ADD, listen_fd, &listen_event);

while(1) {
    // 阻塞等待 epoll中的fd触发
    int active_cnt = epoll_wait(epfd, events, 1000, -1);
    
    for (int i = 0;i < active_cnt;i++) {
        if (events[i].data.fd == listen_fd) {
            // accept 并且将新accept的fd添加到epoll中
        } else if (events[i].events & EPOLLIN) {
            read(fd);
        } else if (events[i].events & EPOLLOUT) {
            write(fd);
        }
    }
}

```


# 触发模式
## 水平触发(Level Triggered) LT
安全 不丢包 在epoll很大的时候影响性能

## 边缘触发(Edge Triggered) ET
只对外暴露一次 不安全 会丢包 但性能就好一点

# 常见io多路复用并发模型提纲

## 单线程Accept (无io复用)
串行模型，简单易学


## 单线程Accept+多线程读写业务(无io复用)
- 支持并发，使用比较灵活，一个clinet对应一个thread单独处理，server处理业务的内聚性高
- 资源消耗高，并发量受限于硬件


## 单线程IO多路复用
- 单流程体可以同时监听多个客户端读写状态的模型，不需要1:1的 c:sthread 数量关系 阻塞模型 节省cpu资源
- 虽然可以监听多个客户端读写状态，但同一时间，只能处理一个客户端的读写请求，实际上业务并发量为1,
当多个客户端访问server时，业务是串行执行，大量请求会有排队延迟的现象

## 单线程IO多路复用+多线程读写业务(业务工作池)
- 优点
  - 对于上一个模型 将业务处理的部分，通过工作池的方式分离出来，能够减少客户端访问Server导致业务串行执行会有大量请求排队的延迟时间
  - 实际上的业务并发为1 但是业务流程的并发量为work_pool_size，加快了业务处理的并行效率
- 缺点
  - 读写依然是main thread单独处理，对高的读写并发量仍然为1
  - 虽然多个worker线程处理业务，但是最后返回给客户端仍旧需要排队


## 单线程IO复用+多线程IO复用(链接线程池)
N(核心数)*单线程IO多路复用

## 单进程多路IO复用+多进程多路IO复用(进程池)


## 单线程多路IO复用+多线程IO复用+多线程

