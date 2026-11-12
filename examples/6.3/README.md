# 第 6.3 章：fd 传递的失败与回收

在支持内核模块的 Linux 环境中：

```sh
make
sudo insmod kernel/mini_binder.ko
timeout 20 ./build/fd_failure_test --fd-full
timeout 30 ./build/fd_failure_test
sudo rmmod mini_binder
```

先用 `--fd-full` 观察两个 fd、一个空位时的回滚：接收方重新打开相同编号，发送方观察两个管道 EOF。然后不带参数运行完整异常回归。主实验要求 fd 硬限制至少为 64；完整回归另需可读取线程的 proc 状态。完整说明见教程第 6.3 章。

## 编码与验收入口

本章的 `manager_test` 沿用 Parcel 的字符串编解码与 object_parcel 的对象位置表。`include/parcel_binding.h` 只绑定事务长度、对象位置和借用的数据视图；映射输入只读，必须在提交回复或释放回复前读完。它不管理对象或 fd 生命周期。

`./build/parcel_binding_test` 是纯用户态兼容与边界检查：确认原有线格式不变，并拒绝截断、无效对象位置和越界值；它不验证真实 fd 转移。正常业务对象和 fd 演示复用上述编码器，失败验收可以直接构造协议包。线程队列实验的一字节负载只作请求标记，额外协议测试仍可手工构造错误字节包。
