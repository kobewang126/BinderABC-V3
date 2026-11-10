# 第 4.1 章：Binder 进程上下文

在支持内核模块的 Linux 环境中：

```sh
make
sudo insmod kernel/mini_binder.ko
./build/object_server
sudo rmmod mini_binder
```

在另一终端运行 `./build/ref_test`。停止服务端后卸载模块。完整说明见教程第 4.1 章。
