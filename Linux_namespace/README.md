# Namespace 简介

namespace是一个抽象概念，用来隔离Linux的全局系统资源。处于相同namespace的进程可以看到相同的资源，该资源对于其他namespace的进程是不可见的。目前(5.13)提供了8种不同的namespace。

- Cgroup, CLONE_NEWCGROUP: cgroup资源
- IPC, CLONE_NEWIPC: System V IPC和POSIX消息队列
- Network, CLONE_NEWNET: 网络设备、协议栈、端口等等
- Mount, CLONE_NEWNS: 挂载点
- PID, CLONE_NEWPID: 进程ID
- Time, CLONE_NEWTIME: 启动时间、计时器
- User, CLONE_NEWUSER: 用户和用户组
- UTS, CLONE_NEWUTS: 主机名和NIS域名

## Namespce API

除了的`/proc`目录的下各种文件提供Namespace信息外，Linux提供了以下四种系统调用API：
 - clone: 创建一个子进程. 如果，`flags`参数指定了上面的一种或者几种`CLONE_NEW*`标记, 则新建flag对应的namespace，并且把子进程添加的新建的namespace中.
 - setns: 当前进程加入到已经存在的namespace. 通过`/proc/[pid]/ns`文件指定需要加入的namespace. 从5.8开始, 内核支持把当前进程加入到某个进程所在的namespace中. 
 - unshare: 字面理解不和其他进程共享namespace, 就是把当前进程加入到新建的namespace中. 
 - ioctl: 获取namespace信息

### ioctl_ns 

函数原型如下：
```C
int ioctl(int fd, unsigned long request, ...);
```
这是个万能函数，具体到namespace方面，主要以下三种用法：
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
`clone`创建一个子进程，子进程从`fn`开始执行，`stack`指向子进程主线程的栈顶，`flags`控制子进程和父进程共享的资源，`arg`透传给`fn`. 和`fork`相比，主要就是`flags`参数的区别，使用`flags`可以控制子进程是否共享父进程的虚拟内存空间、文件描述符、信号处理句柄等等. 通过设置`CLONE_NEW*`标记，同时创建namespace，并把子进程添加到新建的namespace中. 
注意：通过`flags`组合，`clone`也是创建线程的系统调用. 

### unshare

函数原型如下：
```C
int unshare(int flags);
```
创建一个新的namespace，并把当前进程加入到namespace中. 
有几个注意点：
1. `CLONE_NEWPID`给所有子进程创建PID namespace，当前进程的PID namespace保持不变. **必须是单线程的进程才能使用这个标记.** 
2. `CLONE_NEWUSER` **必须是单线程的进程才能使用这个标记.** 

### setns

函数原型如下：
```C
int setns(int fd, int nstype);
```
当前进程加入到`fd`指向的namespace中. `fd`可以指向`/proc/[pid]/ns/`,从5.8开始，也可以是PID fd.

## PID namespace
PID namespace用来隔离进程id空间，也就是在不同PID namespace的进程可以有相同的进程id. 

### 初始进程
PID namespace中的第一个进程(clone和unshare有差异)是初始进程，pid是1。这个进程是所有孤儿进程的父进程。初始进程退出后，内核会杀死namespace中的所有其他进程,并且该namespace不允许在创建新进程(错误码: `ENOMEM`)。

内核对初始进程有保护机制，屏蔽namespace中其他进程发给初始进程，但是初始进程不会处理的信号，防止初始进程被无意杀死。同样屏蔽来自祖先namespace进程的，初始进程不处理的信号，但是`SIGKILL`和`SIGSTOP`除外，让祖先namespace中的进程可以关闭这个namespace。

### 嵌套PID namespace
PID namespace是嵌套关系，除了`root`PID namespace,都有父namespace，可以想象成是一个多叉树结构，`NS_GET_PARENT ioctl`可以获取父namespace。从3.7开始，树最大高32。进程可以看见同namespace和后代namespace的进程，看不见祖先namespace进程。同一个进程在不同namespace中的PID是不一样的。和父进程不在同一个namespace的进程获取的`ppid`是0。进程所在的namespace可以往下(后代)移，但是不能往上(祖先)方向移。

### setns(2) 和 unshare(2)
进程的PID始终不变。调用这两个函数后，子进程会进入新namespace。`/proc/[pid]/ns/pid_for_children`指向子进程的PID namespace。`unshare`只能调用一遍，调用后`/proc/[pid]/ns/pid_for_children`指向`null`，在创建子进程后，指向新的namespace。

## 其他
通过UNIX域套接字通信，内核会把发送方的pid转成在接收方namespace中的pid。有个问题：假如发送方在接收方namespace不可见，内核在接收方namespace中生成一个pid，还是其他方案？

## 演示
我写了个实例代码，输出pid和ppid，如下：
```
# ./pid
99049
59284
# unshare --pid --fork ./pid
1
0
```
第一次指向`./pid`，输出新建进程的id和父进程的id。第二次使用unshare新建namespace，在新namespace中执行`./pid`，输出1和0。



参考：
1. https://man7.org/linux/man-pages/man7/namespaces.7.html
2. https://man7.org/linux/man-pages/man2/ioctl_ns.2.html
3. https://man7.org/linux/man-pages/man2/clone.2.html
4. https://man7.org/linux/man-pages/man2/unshare.2.html
5. https://man7.org/linux/man-pages/man2/setns.2.html
6. https://man7.org/linux/man-pages/man7/pid_namespaces.7.html
