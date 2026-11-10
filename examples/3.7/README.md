# 第 3.7 章：转交已有引用

在支持内核模块的 Linux 环境中：

```sh
make
sudo insmod kernel/mini_binder.ko
./build/transfer_test
sudo rmmod mini_binder
```

实验启动三个进程，验证跨文件引用转换与调用。完整说明见教程第 3.7 章。
