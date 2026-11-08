# 第 2.7 章：多个在途事务

在有匹配内核构建目录、允许加载模块的 Linux 环境中运行：

```sh
make
sudo insmod kernel/mini_binder.ko
./build/version
timeout 15 ./build/concurrent_test
timeout 10 ./build/server_mapping_test
timeout 10 ./build/stale_reply
timeout 10 ./build/server_death
timeout 10 ./build/mmap_access
./build/command_test
./build/calc_server
```

在另一终端运行 `./build/calc_test` 和 `./build/mapped_test`。结束时停止服务端，执行 `sudo rmmod mini_binder` 和 `make clean`。完整说明见教程第 2.7 章。
