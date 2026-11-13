# 第 5.3 章：服务线程退出时结束待回复事务

在支持内核模块的 Linux 环境中：

```sh
make
sudo insmod kernel/mini_binder.ko
./build/service_exit_test --demo
sudo rmmod mini_binder
```

服务端 fd 和其他线程仍存活时，处理请求的线程显式退出，调用者得到 EPIPE；新线程继续完成下一笔调用。完整 service_exit_test 还验证客户端取消后的回收与 12 次线程复用。thread_exit_test 保留为调用线程遗留回复位置清理的附加验收。

## 编码与验收入口

本章的 `manager_test` 沿用 Parcel 的字符串编解码与 object_parcel 的对象位置表。`include/parcel_binding.h` 只绑定事务长度、对象位置和借用的数据视图；映射输入只读，必须在提交回复或释放回复前读完。它不管理对象或 fd 生命周期。

`./build/parcel_binding_test` 是纯用户态兼容与边界检查：确认原有线格式不变，并拒绝截断、无效对象位置和越界值；它不验证真实 fd 转移。正常业务对象和 fd 演示复用上述编码器，失败验收可以直接构造协议包。线程队列实验的一字节负载只作请求标记，额外协议测试仍可手工构造错误字节包。

先运行 `./build/service_exit_test --demo` 观察正常机制；不带 `--demo` 的同名程序保留完整验收。

## 回复的线程归属

沿用 5.2 规则：公开事务结构不含 id。服务线程领取后绑定 current_transaction，BC_REPLY 通过其 caller 找到原调用线程；回复前不能再领取，其他线程不能代回。

沿用 5.3 规则：显式 THREAD_EXIT 先结束待回复事务，再回收线程记录。`./build/service_exit_test` 验证等待者收到 EPIPE、已取消事务回收和后续调用复用；不会自动检测未发退出通知的线程死亡。
