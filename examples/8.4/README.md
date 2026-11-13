# 第 8.4 章：BpBinder 与远端调用

在上一章之上加入 BpBinder，把远端 handle 包装为 IBinder 对象。

三个终端均进入本章目录。

```sh
make
sudo insmod kernel/mini_binder.ko
./build/servicemanager
```

第二个终端：

```sh
./build/framework_server
```

第三个终端：

```sh
./build/framework_client
```

停止手动启动的服务和管理者后，在模块仍加载时运行完整验收（自动清理子进程）：

```sh
python3 tests/framework_flow.py
sudo rmmod mini_binder
```

驱动与 include/mini_binder.h 和 7.5 逐字相同，版本仍为 0705。切换实验前停止服务并卸载旧模块。原 C 服务程序与驱动回归作为附加验收保留，不是本章主实验。

本章完整源码独立维护。实际验证环境、覆盖与限制见教程 validation/README.md；不实现 Android ABI、嵌套事务栈或自动线程池。

新增 oneway 验收：加载本章模块后直接运行 `./build/oneway_test --demo` 或 `./build/oneway_test`。同 Node 排队到前一笔 FREE_BUFFER 后交付；不同 Node 可并发。协议0705，flags 与 FREE_BUFFER 沿用7.4；不要混用修改前的二进制。
