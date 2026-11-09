# 第 3.2 章：服务对象与 Node

在有匹配内核构建目录、允许加载模块的 Linux 环境中运行：

```sh
make
sudo insmod kernel/mini_binder.ko
./build/version
./build/object_server
```

在另一终端使用服务端打印的 Node 编号运行 `./build/object_client 1 add 7 3`、`./build/object_client 2 mul 7 3` 和 `./build/object_test 1 2`。停止服务端后可运行 `./build/node_test`；最后执行 `sudo rmmod mini_binder` 和 `make clean`。完整说明见教程第 3.2 章。
