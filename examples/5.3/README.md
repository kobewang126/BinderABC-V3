# 第 5.3 章：两个服务线程怎样共同处理请求？

在支持内核模块的 Linux 环境中：

```sh
make
sudo insmod kernel/mini_binder.ko
./build/version
./build/service_threads_demo
sudo rmmod mini_binder
```

服务进程打开一次设备并建立一份映射，两个服务线程共用 fd，等待、领取和回复两轮共四笔请求。客户端的两个调用线程核对自己的结果。实验读取自身线程的 proc 状态和 syscall，确认服务线程阻塞后才发送请求。

主线只讲正常工作流程。退出、取消和复制失败等保护仍保留在驱动和附加回归中，不作为本章阅读前提。

## 编码与验收入口

本章的 `manager_test` 沿用 Parcel 的字符串编解码与 object_parcel 的对象位置表。`include/parcel_binding.h` 只绑定事务长度、对象位置和借用的数据视图；映射输入只读，必须在提交回复或释放回复前读完。它不管理对象或 fd 生命周期。

`./build/parcel_binding_test` 是纯用户态兼容与边界检查：确认原有线格式不变，并拒绝截断、无效对象位置和越界值；它不验证真实 fd 转移。正常业务对象和 fd 演示复用上述编码器，失败验收可以直接构造协议包。线程队列实验的一字节负载只作请求标记，额外协议测试仍可手工构造错误字节包。

先运行 `./build/service_threads_demo`。`thread_reply_test`、`thread_cancel_test`、`service_exit_test` 和 `thread_exit_test` 等用于附加回归。

## 回复的线程归属

沿用 5.2 规则：公开事务结构不含 id。服务线程领取后绑定 current_transaction，BC_REPLY 通过其 caller 找到原调用线程；回复前不能再领取，其他线程不能代回。

附加回归保护（不在本章主线展开）：显式 THREAD_EXIT 先结束待回复事务，再回收线程记录。`./build/service_exit_test` 验证等待者收到 EPIPE、已取消事务回收和后续调用复用；不会自动检测未发退出通知的线程死亡。

## 与 4.6 的衔接

继续沿用无需注册服务端的双向上下文。READ 由用户态选择 MINI_READ_REQUEST / MINI_READ_REPLY；测试中的 test_io.h 只设置读取意图后调用 ioctl。前两个 256 字节位置预留请求，回复位于偏移 512 和 768，使用本章的回复元数据定位。
