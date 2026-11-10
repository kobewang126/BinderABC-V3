# 第 6.2 章：在请求中传递文件描述符

在支持内核模块的 Linux 环境中：

```sh
make
sudo insmod kernel/mini_binder.ko
./build/fd_request_demo
sudo rmmod mini_binder
```

主实验传递文件描述符；`./build/mixed_object_test` 检查同一事务中的 Binder 对象和文件描述符。完整说明见教程第 6.2 章。
