# 第 6.3 章：fd 传递的失败与回收

在支持内核模块的 Linux 环境中：

```sh
make
sudo insmod kernel/mini_binder.ko
./build/fd_failure_test
sudo rmmod mini_binder
```

实验验证发送失败、取消、接收方退出及 fd 安装失败时的回收。完整说明见教程第 6.3 章。
