## Namespace 简介

namespace是一个抽象概念，用来隔离Linux的全局系统资源。处于相同namespace的进程可以看到相同的资源，该资源对于其他namespace的进程是不可见的。目前(5.13)提供了8种不同的namespace。

- Cgroup, CLONE_NEWCGROUP: cgroup资源
- IPC, CLONE_NEWIPC: System V IPC和POSIX消息队列
- Network, CLONE_NEWNET: 网络设备、协议栈、端口等等
- Mount, CLONE_NEWNS: 挂载点
- PID, CLONE_NEWPID: 进程ID
- Time, CLONE_NEWTIME: 启动时间、计时器
- User, CLONE_NEWUSER: 用户和用户组
- UTS, CLONE_NEWUTS: 主机名和NIS域名

### Namespce API

除了的`/proc`目录的下各种文件提供Namespace信息外，Linux提供了以下四种系统调用API：
 - clone: 创建一个子进程. 如果，`flags`参数指定了上面的一种或者几种`CLONE_NEW*`标记, 则新建flag对应的namespace，并且把子进程添加的新建的namespace中.
 - setns: 当前进程加入到已经存在的namespace. 通过`/proc/[pid]/ns`文件指定需要加入的namespace. 从5.8开始, 内核支持把当前进程加入到某个PID相同的namespace中. 
 - unshare: 字面理解不和其他进程共享namespace, 就是把当前进程加入到新建的namespace中. 
 - ioctl: 获取namespace信息

### ioctl_ns 

主要以下三种用法：
1. 获取namespace的之间的关系, 调用方式是 `new_fd = ioctl(fd, request);`, fd指向`/proc/[pid]/ns/*`文件. 如果成功，将返回新fd. `NS_GET_USERNS`获取fd指向的namespace的所有者user namespace. `NS_GET_PARENT`获取fd指向的namespace的父namespace，首先fd必须是有层级关系的namespace(PID, User). 
2. 获取namespace的类型，调用方式是`nstype = ioctl(fd, NS_GET_NSTYPE);`,返回`CLONE_NEW*`
3. 获取user namespace的所有者id, 调用方式`uid_t uid;errno=ioctl(fd, NS_GET_OWNER_UID, &uid);`. fd必须指向`/proc/[pid]/ns/user`文件.

实例代码`ns_show.c`,大部分来自`Linux manual page`,增加了`o`参数打印`OWNER_UID`.

### clone

函数原型如下：
```C
    int clone(int (*fn)(void *), void *stack, int flags, void *arg, ...
    /* pid_t *parent_tid, void *tls, pid_t *child_tid */ );
```

参考：
1. https://man7.org/linux/man-pages/man7/namespaces.7.html
2. https://man7.org/linux/man-pages/man2/ioctl_ns.2.html