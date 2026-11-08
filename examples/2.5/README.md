# 第 2.5 章：事务命令

在有匹配内核构建目录的 Linux 环境中运行：

```sh
make
sudo insmod kernel/mini_binder.ko
./build/version
./build/calc_server
```

在另一终端运行 `./build/calc_client add 7 3` 和 `./build/calc_test`。停止服务端后，可以运行 `./build/stale_reply`、`./build/server_death` 和 `./build/mmap_access`。结束时执行 `sudo rmmod mini_binder` 和 `make clean`。计算器服务和事务服务使用不同请求格式，需要配对客户端。完整说明见教程第 2.5 章。
