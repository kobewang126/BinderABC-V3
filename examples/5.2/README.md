# 第 5.2 章：线程待办队列

在支持内核模块的 Linux 环境中：

```sh
make
sudo insmod kernel/mini_binder.ko
./build/thread_reply_test
sudo rmmod mini_binder
```

实验验证同一打开文件的两个线程接收各自的回复。完整说明见教程第 5.2 章。
