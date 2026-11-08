# 第 2.6 章：BC 和 BR 命令

在有匹配内核构建目录的 Linux 环境中运行：

```sh
make
sudo insmod kernel/mini_binder.ko
./build/version
./build/calc_server
```

在另一终端依次运行 `./build/calc_client add 7 3`、`./build/calc_client sub 7 3`、`./build/calc_test` 和 `./build/command_test`。计算器使用 BC/BR 命令路径，请求仍然串行处理。停止服务端后，可以运行 `./build/stale_reply`、`./build/server_death` 和 `./build/mmap_access`。结束时执行 `sudo rmmod mini_binder` 和 `make clean`。完整说明见教程第 2.6 章。
