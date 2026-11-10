# 第 3.7 章：ServiceManager

在支持内核模块的 Linux 环境中：

```sh
make
sudo insmod kernel/mini_binder.ko
./build/manager_test
sudo rmmod mini_binder
```

实验在三个进程中运行管理者、服务端和客户端。完整说明见教程第 3.7 章。
