# 第 3.4 章：Node 到 Ref

在支持内核模块的 Linux 环境中：

```sh
make
sudo insmod kernel/mini_binder.ko
./build/object_server
```

在另一终端运行 `./build/ref_test`。停止服务端后执行 `sudo rmmod mini_binder` 和 `make clean`。完整说明见教程第 3.4 章。
