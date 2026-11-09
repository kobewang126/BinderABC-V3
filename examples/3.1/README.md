# 第 3.1 章：两个用户态对象

在有匹配内核构建目录、允许加载模块的 Linux 环境中运行：

```sh
make
sudo insmod kernel/mini_binder.ko
./build/version
./build/object_server
```

在另一终端运行 `./build/object_client add 7 3`、`./build/object_client mul 7 3` 和 `./build/object_test`。结束时停止服务端，执行 `sudo rmmod mini_binder` 和 `make clean`。完整说明见教程第 3.1 章。
