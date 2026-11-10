# 第 4.2 章：服务注册和发现

在支持内核模块的 Linux 环境中：

```sh
make
sudo insmod kernel/mini_binder.ko
./build/manager_test
sudo rmmod mini_binder
```

实验启动管理者、服务端和客户端三个进程。完整说明见教程第 4.2 章。
