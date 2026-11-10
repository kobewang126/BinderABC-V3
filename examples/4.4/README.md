# 第 4.4 章：proc 待办队列

在支持内核模块的 Linux 环境中：

```sh
make
sudo insmod kernel/mini_binder.ko
./build/workers_test
./build/pending_cancel_test
sudo rmmod mini_binder
```

实验验证服务线程领取工作和未领取请求取消。完整说明见教程第 4.4 章。
