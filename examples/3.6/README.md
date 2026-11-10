# 第 3.6 章：在请求中传递本地对象

在支持内核模块的 Linux 环境中：

```sh
make
sudo insmod kernel/mini_binder.ko
./build/local_transfer_test
sudo rmmod mini_binder
```

实验启动两个进程，验证本地 Node 转为接收方 Ref 并可调用。完整说明见教程第 3.6 章。
