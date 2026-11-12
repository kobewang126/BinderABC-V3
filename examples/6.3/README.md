# 第 6.3 章：fd 传递的失败与回收

在支持内核模块的 Linux 环境中：

```sh
make
sudo insmod kernel/mini_binder.ko
timeout 20 ./build/fd_failure_test --fd-full
timeout 30 ./build/fd_failure_test
sudo rmmod mini_binder
```

先用 `--fd-full` 观察两个 fd、一个空位时的回滚：接收方重新打开相同编号，发送方观察两个管道 EOF。然后不带参数运行完整异常回归。主实验要求 fd 硬限制至少为 64；完整回归另需可读取线程的 proc 状态。完整说明见教程第 6.3 章。
