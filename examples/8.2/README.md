# 第 8.2 章：ProcessState 与共享连接

基于本地 BBinder，增加进程共享的设备 fd、映射与 cookie 对象表；尚无 IPCThreadState 或 BpBinder。

```sh
make
sudo insmod kernel/mini_binder.ko
./build/process_demo
sudo rmmod mini_binder
```

驱动与 include/mini_binder.h 和 7.5 逐字相同，版本仍为 0705。切换实验前停止服务并卸载旧模块。原 C 服务程序与驱动回归作为附加验收保留，不是本章主实验。

本章完整源码独立维护。实际验证环境、覆盖与限制见教程 validation/README.md；不实现 Android ABI、嵌套事务栈或自动线程池。

新增 oneway 验收：加载本章模块后直接运行 `./build/oneway_test --demo` 或 `./build/oneway_test`。同 Node 排队到前一笔 FREE_BUFFER 后交付；不同 Node 可并发。协议0705，flags 与 FREE_BUFFER 沿用7.4；不要混用修改前的二进制。
