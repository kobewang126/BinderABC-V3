# 第 4.4 章：proc 待办队列

在支持内核模块的 Linux 环境中：

```sh
make
sudo insmod kernel/mini_binder.ko
./build/workers_test
sudo rmmod mini_binder
```

实验验证两个服务线程领取工作。取消和退出等回归测试保留在 `tests/`，完整说明见教程第 4.4 章。

## 编码与验收入口

本章的 `manager_test` 沿用 Parcel 的字符串编解码与 object_parcel 的对象位置表。`include/parcel_binding.h` 只绑定事务长度、对象位置和借用的数据视图；映射输入只读，必须在提交回复或释放回复前读完。它不管理对象或 fd 生命周期。

`./build/parcel_binding_test` 是纯用户态兼容与边界检查：确认原有线格式不变，并拒绝截断、无效对象位置和越界值；它不验证真实 fd 转移。正常业务对象和 fd 演示复用上述编码器，失败验收可以直接构造协议包。线程队列实验的一字节负载只作请求标记，额外协议测试仍可手工构造错误字节包。
