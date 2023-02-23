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

关于优雅关闭容器的建议：初始进程捕获`SIGTERM`,并把信号转发给子进程，等待子进程退出后，

### 嵌套PID namespace
PID namespace是嵌套关系，除了`root`PID namespace,都有父namespace，可以想象成是一个多叉树结构，`NS_GET_PARENT ioctl`可以获取父namespace。从3.7开始，树最大高32。进程可以看见同namespace和后代namespace的进程，看不见祖先namespace进程。同一个进程在不同namespace中的PID是不一样的。和父进程不在同一个namespace的进程获取的`ppid`是0。进程所在的namespace可以往下(后代)移，但是不能往上(祖先)方向移。

### setns(2) 和 unshare(2)
进程的PID始终不变。调用这两个函数后，子进程会进入新namespace。`/proc/[pid]/ns/pid_for_children`指向子进程的PID namespace。`unshare`只能调用一遍，调用后`/proc/[pid]/ns/pid_for_children`指向`null`，在创建子进程后，指向新的namespace。

### 其他
通过UNIX域套接字通信，内核会把发送方的pid转成在接收方namespace中的pid。有个问题：假如发送方在接收方namespace不可见，内核在接收方namespace中生成一个pid，还是其他方案？

### 演示
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

## User namespace
User namespace用来隔离安全相关的标识符和属性，特别是 用户id(user ids)、组id(group ids)、根目录(root director)、密钥(keyrings)、特权能力(capabilities)。在 user namespace内和外，一个进程可以是不同的用户id和组id。特别地，在User namespace内，进程可以是特性用户id 0，在namespace外，是其他的正常用户id；也就是，在namespace内，进程拥有完整的特权，可以操作namespace内的其他资源，在namespace外，该进程没有特权，仅仅是一个普通进程。

### 嵌套 namespace
User namespace可以嵌套；每个namespace，除了初始的(root)namespace，都有一个父namespace和0或多个子namespace。通过CLONE_NEWUSER标识创建User namespace的进程所在的User namesapce就是新建namespace的父namespace。这一点，User namespace和PID namespace是一样的。从Kernel 3.11开始，最大嵌套深度是32。

每个进程只能属于一个User namespace。子进程默认继承父进程的User namespace。单线程进程可以通过`setns`函数加入到其他User namespace，该进程在User namespace中必须有`CAP_SYS_ADMIN`权限。

### 能力(Capabilities)
通过`CLONE_NEWUSER`创建的进程在新的namespace中拥有root权限，但是它在父namespace中没有任何能力。假如进程需要访问namespace外的资源，也仅仅是一个普通用户。系统有一系列的手段限制namspace中的进程能力，防止出现能力逃逸。

当`CLONE_NEWUSER`和其他`CLONE_NEW*`同时指定时，User namespace首先被创建的，然后以新建的User namespace为oner在创建其他的namespace。

### 用户和组ID映射
Linux通过在父子nanmespace间建立uid和gid的映射来管理相关的资源。在User namespace创建时，uid和gid映射都是空的，这是获取到uid通常是65534。`/proc/[pid]/uid_map`和`/proc/[pid]/gid_map`暴露映射关系，这两个文件**只能写入一次**。这两个文件的每一行表示一对一的映射范围，每一行有三个数字，每个数字使用空格分隔。

```
ID_inside-ns   ID-outside-ns   length
```

每个字段的说明如下：

1. 在`pid`所在的namespace内部，映射id范围的开始值
2. 分两种情况

    2.1. 打开uid_map的进程和pid处于不同的User namespace，ID_inside-ns<->ID-outside-ns表示uid在两个namespace的映射关系。所以，不同的User namespace的进程查看相同的uid_map,可能是不一样的。
    2.2 打开uid_map的进程和pid处于同一个User namespace，ID_inside-ns<->ID-outside-ns表示uid在父子User namespace之间的映射关系。

3. 映射的长度

修改uid_map有很多权限要求：

1. 只能当前User namespace的进程和父User namespace的进程有权限修改uid_map。
2. 不能映射没有从父User namespace中映射过来的uid区间给子User namespace。
3. 非特权（没有CAP_SETUID能力）进程只能映射当前uid给子User namespace

除了上面的限制，为了安全还有其他的权限要求，这些安全限制为了防止进程把自己没有权限访问的资源传递给子User namespace中的进程，具体的要求可以参考手册。

当某个非特权进程要访问文件时，uid、gid和文件的Credentials会映射到初始User namespace中，然后在判断，进程是否有权限访问资源。其他不和User namespace绑定的系统资源，例如：修改系统时间、加载内核模块、创建设备等等，只有初始User namespace中进程才有权限。User namespace中的CAP_SYS_ADMIN是有非常多限制的。

进程加入User namespace后，特权被剥夺光了，似乎也没有判断授权，如果没有Network namespace，ping也没有权限。

## Mount namespace
`Mount namespace`用来隔离文件系统的挂载点。不同`Mount namespace`的进程有不同的文件系统目录结构。`/proc/[pid]/mounts`,`/proc/[pid]/mountinfo`和`/proc/[pid]/mountstats`,可以通过这三个文件看到进程的挂载信息,也就是进程所在的`Mount namespace`的挂载信息。新`Mount namespace`的初始挂载点拷贝自创建它的进程所在的`Mount namespace`。

### mount 的概念
比如说，Linux系统上安装了一些存储设备，例如：硬盘、U盘、光驱等等，在没有挂载之前，你可以在 `/dev/`目录下看到这个设备，或者通过`fdisk`查看设备。如果你想以文件目录的方式访问设备，就需要使用挂载。使用系统命令`mount`完成挂载。命令用法如下：

```
mount [-fnrsvw] [-t vfstype] [-o options] device dir
```

`device`表示设备号，`dir`表示要挂载到那个目录。

例如插入U盘后，`/dev/`多出来两个文件: `/dev/sdb`,`/dev/sdb1`,前一个表示U盘这个设备，后一个表示U盘上的第一个分区。如果U盘上有多个分区，还会出现`/dev/sdb2`等等。可以执行以下命令把U盘的第一个分区挂载到`/mnt/usb/`(iocharset=utf8表示使用utf8编码解析文件名)

```
mount -o iocharset=utf8 /dev/sdb1 /mnt/usb
``` 

同一个设备可以挂载到多个目录，一个目录也可以挂载多个设备。可以把目录的挂载信息当做栈，挂载就是入栈，取消挂载就是出栈，对目录的访问就是在访问栈顶的目录，当栈空时，访问真实的目录。

### 挂载传播
为了照顾其他的使用场景，比如说插入U盘后，需要在所有`Mount namespace`中都可以访问，Linux从2.6.15引入了挂载传播机制。这种传播机制使得各个`Mount namespace`之间的`mount`事件可以互相传播，可以在各个`Mount namespace`之间共享挂载点。

挂载点的传播类型：

 - MS_SHARED: 共享的挂载点，在任何一个`Mount namespace`中修改，其他的`Mount namespace`都会生效
 - MS_PRIVATE：默认类型，私有的挂载点，不和任何`Mount namespace`共享
 - MS_SLAVE：单向传播，它可以接收master的挂载事件，但是它的事件不会传播给master。该挂载点同时也是另一个共享组的master，可以把挂载事件传给它的slave。
 - MS_UNBINDABLE：首先是私有的，然后不能在被绑定挂载。该类型的主要目的是在递归绑定挂载时，防止挂载爆炸。

### 绑定挂载
引入绑定挂载后，device可以是一个普通的文件或者目录。用法如下：
```
mount --bind source_file target_file
```
绑定挂载有点类似于硬链接，在切换根目录后，仍然可以访问到正确的文件（软链接在大概率是不能访问了）。在使用上，它们有几个区别如下：

1. mount是非持久化的，系统重启后，需要重新挂载，ln是持久化的
2. mount可以处理文件和目录，ln只能处理文件
3. mount要求target_file必须存在，ln要求target_file不能存在
4. mount的target_file不能删除，只能使用 umount 取消挂载，ln可以上传target_file
5. source_file被删除后，当前session下，都可以访问target_file，系统重启后，mount的target_file不能访问了
6. mount会根据传播机制在各个`Mount namespace`间共享，ln在所有`Mount namespace`间共享
7. mount的source、target可以不在一个设备上，ln必须在一个设备上

### 切换根目录
根目录就是`/`代表的目录。子进程会继承父进程的根目录。介绍两种切换根目录的方式

#### chroot （change root directory）

 - 系统命令: `chroot [OPTION] NEWROOT [COMMAND [ARG]...]`
 - 系统API：`int chroot(const char *path);`

只改变当前进程的根目录，对其他进程没有影响。大概的实现原理为：内核在处理文件路径时，加上一层预处理，然后生产最终的文件路径，交给文件系统。举例说明：设置了 `/opt/busybox/`为根目录后，当前目录为`/etc`，需要访问`./passwd`文件，内核经过处理后的路径为`/opt/busybox/etc/passwd`。当然，内核已经堵住了想通过`../../`逃逸出`root`的做法。但是在手册上，对该函数的安全性持消极态度，所以，在沙箱场景上尽量避免使用该函数。

#### pivot_root

 - 系统命令： `pivot_root new_root put_old`
 - 系统API: `int syscall(SYS_pivot_root, const char *new_root, const char *put_old);`

改变当前`Mount namespace`中的根挂载。具体一点就是，先把`new_root`设置成根挂载，让把之前的根挂载移到`put_old`目录。和`chroot`相比，`pivot_root`之前在文件系统层面修改了根目录，更加的安全。如果在卸载`put_old`挂载点，就彻底无法访问原来的文件了。

在使用方面，有几个要注意的点：

1. 调用进程需要`CAP_SYS_ADMIN`权限，在`Mount namespace`的所有者`User namespace`中
2. `new_root`必须是挂载点，不能是当前的根
3. `pub_old`需要是`new_root`或者其子目录
4. `new_root`和当前根目录，这两个目录的父挂载不能是共享的，`put_old`也不能是共享的挂载点

#### 最佳实践
我们使用busybox镜像来操作一下：

1. 使用unshare创建namesapce: `unshare -m -p --fork`
2. 进入busybox目录：`cd /opt/busybox`
3. 为了保证busy是挂载点，执行一次挂载: `mount --bind /opt/busybox/ /opt/busybox/`
4. 执行切换：`pivot_root . .`
5. `/bin/umount .`
6. `/bin/mount -t proc proc /proc`
7. `cat /proc/self/mountinfo`

    `162 126 253:0 /opt/busybox / rw,relatime - xfs /dev/mapper/centos-root rw,attr2,inode64,logbufs=8,logbsize=32k,noquota`
    `163 162 0:5 / /proc rw,relatime - proc proc rw`

8. 之前的挂载都卸载了

## UTS namespaces
`UTS namespaces`用来隔离主机名和NIS域名，就是`hostname`、`domainname`两个命令获取和设置的字段。

为什么需要有这个namespace呢？目前已经有一系列的基于主机名和NIS域名的主机管理软件，要保证这些软件可以在容器里面正常运行。我司在虚拟机时期，编写了大量基于主机名的脚本，这些脚本有日志收集、故障分析、代码部署等等，这些脚本就可以顺利迁移到容器中使用。

## IPC namespaces
`IPC namespaces`用来隔离System V IPC和POSIX消息队列。

我比较其他的进程间通信通信方式是怎么隔离的?
shm_open, mmap创建的共享内存：由于这两个技术使用的文件系统映射来实现共享内存的，所以`Mount namespace`完成了共享内存资源隔离。Linux一切皆文件，除了网络，其他进程间通信的方式都是通过文件系统来实现的，所以，`Mount namespace`就搞定了这些资源的隔离。要相信Linux内核团队。

## Time namespaces
`Time namespaces`用来隔离单调时钟和启动时间，比较特殊的是这个namespace只能通过`unshare`来创建。
通过修改`/proc/PID/timens_offsets`来修改系统的单调时钟和启动时间。该文件只能在`Time namespace`中没有任何进程前修改，也就是说在进程调用`unshare`后，修改当前进程的`/proc/PID/timens_offsets`文件，然后在启动子进程。`timens_offsets`的格式如下：

```
<clock-id> <offset-secs> <offset-nanosecs>
```

`clock-id`有两种值： `monotonic`表示单调时钟，`boottime`表示启动时间。`offset-secs`可以是正负值，`offset-nanosecs`只能是正值。这两个偏移都是针对初始`Time namespace`的，不是当前创建`Time namespace`的进程，这点要特别注意。

模拟一个需求：在容器内的启动时间从0开始，模拟系统刚刚启动

1. `uptime --pretty`,获取当前`Time namespace`的启动时间，把启动时间转换成秒，比如：3600秒
2. 读取`/proc/self/timens_offsets`文件中，`boottime`内容，然后取负值
3. 调用`unshare`创建`Time namespace`
3. 把1和2获取的值，相加，再取负值后，写入到`/proc/self/timens_offsets`的`bootime`中

`clock_times`例子可以快速的帮忙计算启动时间。

参考`unshare(1)`的源代码：

```C
static void settime(time_t offset, clockid_t clk_id)
{
	char buf[sizeof(stringify_value(ULONG_MAX)) * 3];
	int fd, len;

	len = snprintf(buf, sizeof(buf), "%d %" PRId64 " 0", clk_id, (int64_t) offset);

	fd = open("/proc/self/timens_offsets", O_WRONLY);
	if (fd < 0)
		err(EXIT_FAILURE, _("failed to open /proc/self/timens_offsets"));

	if (write(fd, buf, len) != len)
		err(EXIT_FAILURE, _("failed to write to /proc/self/timens_offsets"));

	close(fd);
}
```

## Network namespace
`Network namespace`用来隔离网络相关的资源，比如说：网络设备、IPv4和IPv6协议栈、IP Tables、防火墙、`/proc/net`目录、`/sys/class/net`目录、`/proc/sys/net`目录下的各种文件、端口号等等。另外，`Network namespace`还可以隔离 unix socket。

通过创建进程创建的namespace，namespace中最后一个进程退出且没有进程引用namesapce后，namespace会自动销毁。

物理网络设备只能存在于一个`network namespace`。如果`network namespace`被销毁，那么它的物理网络设备将会移到初始（Root）`network namespace`中。

每一个namespace都是独立的，namespace中的进程要怎么和外界通信呢？我们这边介绍两种方法：

首先说下我的测试环境：VMWare虚机，Centos 9，内核5.14，双网卡(ens33,ens37)，网关192.168.192.2


### 方法一：就是把物理网卡移到namespace中

1. 创建network namespace，

    ``` shell
    [root@localhost ~]# PS1="netns# " unshare -m -n --fork
    netns# 
    ```

2. 测试网络功能,显而易见的，网络不可达

    ``` shell
    netns# ping 8.8.8.8
    ping: connect: Network is unreachable
    netns# ping 127.0.0.1
    ping: connect: Network is unreachable
    ```

3. 启用`lo`网络,可以使用127.0.0.1

    ``` shell
    netns# ip link set lo up
    netns# ping 127.0.0.1
    PING 127.0.0.1 (127.0.0.1) 56(84) bytes of data.
    64 bytes from 127.0.0.1: icmp_seq=1 ttl=64 time=0.096 ms
    ```

4. 查看`bash`的pid,在下面的指令中，使用 34634 指代`network namespace`

    ``` shell
    netns# echo $$
    34634
    ```

5. 在打开一个root权限的Terminal,可以看机器上有一个`lo`和两块网卡设备。注意：如果还在namespace的Terminal中，只能看到一个`lo`设备

    ``` shell
    [root@localhost ~]# ip link
    1: lo: <LOOPBACK,UP,LOWER_UP> mtu 65536 qdisc noqueue state UNKNOWN mode DEFAULT group default qlen 1000
        link/loopback 00:00:00:00:00:00 brd 00:00:00:00:00:00
    2: ens33: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc fq_codel state UP mode DEFAULT group default qlen 1000
        link/ether 00:0c:29:d8:90:3c brd ff:ff:ff:ff:ff:ff
        altname enp2s1
    3: ens37: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc fq_codel state UP mode DEFAULT group default qlen 1000
        link/ether 00:0c:29:d8:90:46 brd ff:ff:ff:ff:ff:ff
        altname enp2s5
    ```

6. 这一步我要把ens37移到34634 namespace中。可以看到ens37已经移走了

    ``` shell
    [root@localhost ~]# ip link set ens37 netns 34634
    [root@localhost ~]# ip link
    1: lo: <LOOPBACK,UP,LOWER_UP> mtu 65536 qdisc noqueue state UNKNOWN mode DEFAULT group default qlen 1000
        link/loopback 00:00:00:00:00:00 brd 00:00:00:00:00:00
    2: ens33: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc fq_codel state UP mode DEFAULT group default qlen 1000
        link/ether 00:0c:29:d8:90:3c brd ff:ff:ff:ff:ff:ff
        altname enp2s1
    ```

7. 回到namespace的终端，我们发现namespace中多了ens37设备，也可以看到ens37是关闭(down)状态

    ``` shell
    netns# ip link
    1: lo: <LOOPBACK,UP,LOWER_UP> mtu 65536 qdisc noqueue state UNKNOWN mode DEFAULT group default qlen 1000
        link/loopback 00:00:00:00:00:00 brd 00:00:00:00:00:00
    3: ens37: <BROADCAST,MULTICAST> mtu 1500 qdisc noop state DOWN mode DEFAULT group default qlen 1000
        link/ether 00:0c:29:d8:90:46 brd ff:ff:ff:ff:ff:ff
        altname enp2s5
    ```

8. 我们启用ens37后，发现还是无法联通网络。这是因为我们还没有给ens37配置IP

    ``` shell
    netns# ip link set ens37 up
    netns# ping 8.8.8.8
    ping: connect: Network is unreachable
    ```

9. 根据自己的IP情况配置好ens37的IP，发现可以ping通 192.168.192.131，但是还是无法ping通 8.8.8.8

    ``` shell
    netns# ip addr add 192.168.192.131/24 dev ens37 
    netns# ping 192.168.192.131
    PING 192.168.192.131 (192.168.192.131) 56(84) bytes of data.
    64 bytes from 192.168.192.131: icmp_seq=1 ttl=64 time=0.031 ms
    ```
    ``` shell
    netns# ping 8.8.8.8
    ping: connect: Network is unreachable
    netns# ip route
    192.168.192.0/24 dev ens37 proto kernel scope link src 192.168.192.131 
    ```

10. 根据上面最后一个命令`ip route`，我们看到只有一条录音信息，我们只能访问`192.168.192.0/24`网段。我们添加一条通过`192.168.192.2`的默认路由

    ``` shell
    netns# ip route add default via 192.168.192.2
    netns# ping 8.8.8.8
    PING 8.8.8.8 (8.8.8.8) 56(84) bytes of data.
    64 bytes from 8.8.8.8: icmp_seq=1 ttl=128 time=32.5 ms
    ```

11. 在第9步，如果网络里面有dhcp服务器，也可以通过dhcp配置ens37的IP。注意：dhcp client一般是后台运行的，记得在退出namespace时，手动销毁dhcp client的后台进程，如果创建了`PID namespace`,系统可以代劳杀死dhcp client。

### 方法二：使用虚拟以太网设备(veth)

veth都是成对使用的，就像一座桥，连接两个namespace，发送给一端的数据，会立即传输到另外一端。当namespace销毁时，它包含的veth也会被销毁。

1. 创建network namespace

    ``` shell
    [root@localhost ~]# PS1="netns# " unshare -m -n --fork
    netns# 
    ```

2. 测试网络功能

    ``` shell
    netns# ping 8.8.8.8
    ping: connect: Network is unreachable
    netns# ping 127.0.0.1
    ping: connect: Network is unreachable
    ```

3. 启用`lo`网络,可以使用127.0.0.1

    ``` shell
    netns# ip link set lo up
    netns# ping 127.0.0.1
    PING 127.0.0.1 (127.0.0.1) 56(84) bytes of data.
    64 bytes from 127.0.0.1: icmp_seq=1 ttl=64 time=0.096 ms
    ```

4. 查看`bash`的pid,在下面的指令中，使用 2911 指代`network namespace`

    ````
    netns# echo $$
    2911
    ````

5. 在打开一个root权限的Terminal,创建一对veth。我们看到索引4和5两个veth设备

    ```
    [root@localhost ~]# ip link add veth_a type veth peer name veth_b
    [root@localhost ~]# ip link list
        1: lo: <LOOPBACK,UP,LOWER_UP> mtu 65536 qdisc noqueue state UNKNOWN mode DEFAULT group default qlen 1000
            link/loopback 00:00:00:00:00:00 brd 00:00:00:00:00:00
        2: ens33: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc fq_codel state UP mode DEFAULT group default qlen 1000
            link/ether 00:0c:29:d8:90:3c brd ff:ff:ff:ff:ff:ff
            altname enp2s1
        3: ens37: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc fq_codel state UP mode DEFAULT group default qlen 1000
            link/ether 00:0c:29:d8:90:46 brd ff:ff:ff:ff:ff:ff
            altname enp2s5
        4: veth_b@veth_a: <BROADCAST,MULTICAST,M-DOWN> mtu 1500 qdisc noop state DOWN mode DEFAULT group default qlen 1000
            link/ether fa:53:53:42:42:49 brd ff:ff:ff:ff:ff:ff
        5: veth_a@veth_b: <BROADCAST,MULTICAST,M-DOWN> mtu 1500 qdisc noop state DOWN mode DEFAULT group default qlen 1000
            link/ether ce:ea:8a:1f:b1:1b brd ff:ff:ff:ff:ff:ff
    ```

6. 和操作物理设备一样，我们把 veth_a 移到 2911 namespace中。我们可以看到 veth_a 已经从root namespace中移走了

    ```
    [root@localhost ~]# ip link set veth_a netns 2911
    [root@localhost ~]# ip link list
    1: lo: <LOOPBACK,UP,LOWER_UP> mtu 65536 qdisc noqueue state UNKNOWN mode DEFAULT group default qlen 1000
        link/loopback 00:00:00:00:00:00 brd 00:00:00:00:00:00
    2: ens33: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc fq_codel state UP mode DEFAULT group default qlen 1000
        link/ether 00:0c:29:d8:90:3c brd ff:ff:ff:ff:ff:ff
        altname enp2s1
    3: ens37: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc fq_codel state UP mode DEFAULT group default qlen 1000
        link/ether 00:0c:29:d8:90:46 brd ff:ff:ff:ff:ff:ff
        altname enp2s5
    4: veth_b@if5: <BROADCAST,MULTICAST> mtu 1500 qdisc noop state DOWN mode DEFAULT group default qlen 1000
        link/ether fa:53:53:42:42:49 brd ff:ff:ff:ff:ff:ff link-netnsid 0
    ```

7. 分别配置veth_a和veth_b的IP为：10.60.0.2/24, 10.60.0.1/24,并且给Network namespace添加默认路由 10.60.0.1

    ```
    netns# ip addr add 10.60.0.2/24 dev veth_a
    netns# ip route add default via 10.60.0.1
    ```
    ```
    [root@localhost ~]# ip addr add 10.60.0.1/24 dev veth_b
    ```

8. 试试互相ping一下，发现10.60.0.0/24互相已经可以ping通了。但是在nemesapce中还是无法ping通外网
9. 按照我们的网络规划 10.60.0.0/24 子网在 192.168.192.0/24 下面，需要通过SNAT转换才能访问外网，我们给 root namespace增加一条NAT转换规则。下面简单介绍下这条 iptables 命令： `-t nat`表示NAT表， `-A POSTROUTING`在`POSTROUTING`链上增加规则，`-s` 表示源地址是 `10.60.0.0/16` 报文，`!-o veth_b`表示不通过 `veth_b` 网卡发出的报文，`-j SNAT`表示执行 SNAT 转换，`--to-source 192.168.192.129`表示把报文转成从 `192.168.192.129` 源地址发出。这样就可以访问外网IP了。

    ```
    [root@localhost ~]# iptables -t nat -A POSTROUTING -s 10.60.0.0/16 ! -o veth_b -j SNAT --to-source 192.168.192.129
    ```

10. 如果发现还是无法访问外网，可能的原因需要开启IP转发

    ```
    [root@localhost ~]# echo 1 > /proc/sys/net/ipv4/ip_forward
    ```

### 在配置的时候遇到几个问题

1. veth配置成物理网卡的网段，导致机器断网了
2. 上面使用了veth的两端直接通信，如果在同一台机器上创建多个`Network namespace`，它们之间需要互相通信，那么iptables的配置就会特别复杂。Linux提供了网桥(虚拟交互机)可以简化相关的配置。


## Cgroup namespaces

`Cgroup namespaces`隔离`cgroup`资源。内核通过虚拟目录结构来管理`cgroup`资源，在之前所有进程的`cgroup`资源都是相对根目录的，`Cgroup namespaces`通过给每个`namespace`单独设置根目录来隔离资源。

使用`Cgroup v2`来举例说明：

 1. 我们创建`cgroup`目录 `/sys/fs/cgroup/ns_test/tasks`
 2. 我们把容器进程放到 `/sys/fs/cgroup/ns_test/tasks/cgroup.procs` 中
 3. 容器初始化进程先 `umount -l /sys/fs/cgroup`,再 `mount -t cgroup2 cgroup2 /sys/fs/cgroup`
 4. 这样容器内的进程就只能看到 `/sys/fs/cgroup/ns_test/tasks/` 目录。容器外就可以`/sys/fs/cgroup/ns_test`控制容器的资源消耗

## 其他

### 几个方便的工具

 - lsns: 列出 namespace

    这条命令是通过遍历`/proc/`目录的所有进程获取 namespace 信息。因此，如果`/proc`挂载错误，会导致该命令执行结果不对。另外，该命令也无法列出进程已经退出、使用绑定挂载持续化到其他地方的 namespace

 - nsenter: 在指定的 namespace 中执行程序

    可以通过 `/proc/[pid]/ns/`目录的文件指定 namespace, 也可以指定`pid`指定该进程的 namespace

 - cpu_killer: 销毁CPU

    使用方法 `cpu_killer [number]`,使用 `[number]` 指定需要销毁多少个核。本人使用这个工具测试`cgroup`的功能。

## 参考：

1. https://man7.org/linux/man-pages/man7/namespaces.7.html
2. https://man7.org/linux/man-pages/man2/ioctl_ns.2.html
3. https://man7.org/linux/man-pages/man2/clone.2.html
4. https://man7.org/linux/man-pages/man2/unshare.2.html
5. https://man7.org/linux/man-pages/man2/setns.2.html
6. https://man7.org/linux/man-pages/man7/pid_namespaces.7.html
7. https://man7.org/linux/man-pages/man7/user_namespaces.7.html
8. https://man7.org/linux/man-pages/man7/mount_namespaces.7.html
9. https://man7.org/linux/man-pages/man7/network_namespaces.7.html