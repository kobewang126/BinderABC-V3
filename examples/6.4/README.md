# 第 6.4 章：在回复中传递文件描述符

在支持内核模块的 Linux 环境中：

```sh
make
sudo insmod kernel/mini_binder.ko
./build/version
./build/fd_reply_demo
sudo rmmod mini_binder
```

主实验在回复中传递文件描述符；`./build/fd_reply_test` 检查混合对象、读取失败重试和未领取时的回收。完整说明见教程第 6.4 章。
