# 第 5.1 章：同一上下文中的两个调用线程

在支持内核模块的 Linux 环境中：

```sh
make
sudo insmod kernel/mini_binder.ko
./build/thread_reply_test --demo
sudo rmmod mini_binder
```

实验验证同一打开文件的两个线程分别取得回复。完整说明见教程第 5.1 章。

## 编码与验收入口

本章的 `manager_test` 沿用 Parcel 的字符串编解码与 object_parcel 的对象位置表。`include/parcel_binding.h` 只绑定事务长度、对象位置和借用的数据视图；映射输入只读，必须在提交回复或释放回复前读完。它不管理对象或 fd 生命周期。

`./build/parcel_binding_test` 是纯用户态兼容与边界检查：确认原有线格式不变，并拒绝截断、无效对象位置和越界值；它不验证真实 fd 转移。正常业务对象和 fd 演示复用上述编码器，失败验收可以直接构造协议包。线程队列实验的一字节负载只作请求标记，额外协议测试仍可手工构造错误字节包。

先运行 `./build/thread_reply_test --demo` 观察正常机制；不带 `--demo` 的同名程序保留完整验收。

## 与 4.6 的衔接

继续沿用无需注册服务端的双向上下文。READ 由用户态选择 MINI_READ_REQUEST / MINI_READ_REPLY；测试中的 test_io.h 只设置读取意图后调用 ioctl。前两个 256 字节位置预留请求，回复位于偏移 512 和 768，使用本章的回复元数据定位。
