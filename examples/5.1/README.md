# 第 5.1 章：同一上下文中的两个调用线程

在支持内核模块的 Linux 环境中：

```sh
make
sudo insmod kernel/mini_binder.ko
./build/thread_reply_test
sudo rmmod mini_binder
```

实验验证同一打开文件的两个线程分别取得回复。完整说明见教程第 5.1 章。
