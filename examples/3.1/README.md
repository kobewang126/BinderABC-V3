# 第 3.1 章：两个用户态对象

在有匹配内核构建目录、允许加载模块的 Linux 环境中运行：

```sh
make
sudo insmod kernel/mini_binder.ko
./build/version
./build/object_server
```

在另一终端运行：

```sh
./build/object_client 1 sub 7 3
./build/object_client 2 sub 7 3
./build/object_client 1 add 7 3
./build/object_client 2 mul 7 3
./build/object_test
```

前两次调用只切换对象，结果分别为 4 和 -4；第一、三次只切换操作。对象 1 按原顺序计算，对象 2 交换参数再计算，两者均支持加减乘。最后的测试覆盖错误与恢复。结束时停止服务端，执行 `sudo rmmod mini_binder` 和 `make clean`。完整说明见教程第 3.1 章。
