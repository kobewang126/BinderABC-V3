# 7.3 进入 looper，用同一个 READ 接收事件

本目录保存完整独立源码；获取或解压后直接进入本目录，无需切换 Git 版本。不要跨章节混用驱动和程序。先停止其他章节的服务进程并卸载旧模块，再运行：

```sh
make
sudo insmod kernel/mini_binder.ko
./build/version
./build/looper_demo
```

协议版本为 `0703`。主实验输出和关键代码解读见第三版教程对应章节。7.1/7.2 的 object_demo 是两进程对象分发实验，以 handle 0 作为最小引导入口；服务名注册与查询由独立的 manager_test 演示。

```sh
./build/object_demo
./build/manager_test
./build/protocol_test
./build/mapping_test
./build/stale_reply
./build/server_death
./build/looper_test
sudo rmmod mini_binder
```

| 程序 | 验证范围 |
|---|---|
| object_demo | 重复传递 LOCAL 复用同一 handle；通过对象 handle 调用减法 |
| manager_test | 三进程注册、查询、调用；缺失服务、无效名称、管理者和提供者退出 |
| protocol_test | 无效 ioctl/地址/命令/handle；对象转换失败回滚；八 Node 容量；Ref 复用；回到 owner；双向 fd；回复读取 EFAULT/EMFILE 后重试 |
| mapping_test | 单次只读映射、跨地址空间拒绝、重新映射 |
| stale_reply | 取消后拒绝旧回复，后续调用正确 |
| server_death | 两笔调用等待时服务退出，都得到 EPIPE |
| looper_demo | 同一个 READ：普通调用线程先读回复，进入 looper 后处理反向请求 |
| looper_test | 普通线程不偷取工作；自身回复优先；同一 READ 返回两类事件；ENTER/EXIT 错误；信号和解除映射唤醒 |

`userspace/lab.h` 只复用实验的对象编码、映射和 ioctl 操作，不是 Framework。主实验先于异常测试；协议测试为集中回归，不要求初读时逐行阅读。程序包含断言，实验未使用 NDEBUG；断言失败应停止检查，不能忽略后继续运行。

## 实际验证

2026-11-12 在 Ubuntu 24.04 ARM64、Linux 6.8.0-147-generic 中构建并运行上述所有程序，模块最终卸载成功；对象主实验和协议测试各额外连续运行五次通过。looper_test 也额外连续运行五次通过；其中阻塞观测需要能读取本用户线程的 /proc syscall。编译器名称与内核构建记录不同、缺少 vmlinux 的 BTF 提示不影响本次已验证的构建与模块加载。未进行性能评估，未穷尽并发取消与解除映射等组合。

## 编码与验收入口

本章的 `manager_test` 沿用 Parcel 的字符串编解码与 object_parcel 的对象位置表。`include/parcel_binding.h` 只绑定事务长度、对象位置和借用的数据视图；映射输入只读，必须在提交回复或释放回复前读完。它不管理对象或 fd 生命周期。

`./build/parcel_binding_test` 是纯用户态兼容与边界检查：确认原有线格式不变，并拒绝截断、无效对象位置和越界值；它不验证真实 fd 转移。正常业务对象和 fd 演示复用上述编码器，失败验收可以直接构造协议包。线程队列实验的一字节负载只作请求标记，额外协议测试仍可手工构造错误字节包。
