# 第 5.4 章：显式释放线程记录

在支持内核模块的 Linux 环境中：

```sh
make
sudo insmod kernel/mini_binder.ko
./build/thread_exit_test
sudo rmmod mini_binder
```

实验验证线程显式退出后，记录和未释放回复位置可以复用。完整说明见教程第 5.4 章。
