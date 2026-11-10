# 第 3.5 章：多个服务端与事务路由

在支持内核模块的 Linux 环境中：

```sh
make
sudo insmod kernel/mini_binder.ko
./build/multi_server_test
sudo rmmod mini_binder
```

实验启动两个服务端，验证路由和服务端退出后的隔离。完整说明见教程第 3.5 章。
