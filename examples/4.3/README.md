# 第 4.3 章：Binder 线程

在支持内核模块的 Linux 环境中：

```sh
make
sudo insmod kernel/mini_binder.ko
./build/thread_test
sudo rmmod mini_binder
```

实验验证同一 proc 中的多个线程。完整说明见教程第 4.3 章。
