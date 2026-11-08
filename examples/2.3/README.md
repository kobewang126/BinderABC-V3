# 第 2.3 章：Parcel 事务

在有匹配内核构建目录的 Linux 环境中运行：

```sh
make
sudo insmod kernel/mini_binder.ko
./build/version
./build/transaction_server
```

在另一终端运行 `./build/transaction_client` 和 `./build/transaction_test`。停止服务端后，可以运行 `./build/stale_reply`、`./build/server_death`、`./build/mmap_access` 和 `./build/parcel_test`。结束时执行 `sudo rmmod mini_binder` 和 `make clean`。完整说明见教程第 2.3 章。
