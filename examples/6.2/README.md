# 第 6.2 章：在请求中传递文件描述符

在支持内核模块的 Linux 环境中：

```sh
make
sudo insmod kernel/mini_binder.ko
./build/mixed_object_test
sudo rmmod mini_binder
```

实验在同一事务中传递 Binder 对象和文件描述符。完整说明见教程第 6.2 章。
