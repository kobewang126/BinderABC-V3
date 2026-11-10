# 第 5.3 章：一条同步调用链

在支持内核模块的 Linux 环境中：

```sh
make
sudo insmod kernel/mini_binder.ko
./build/nested_call_test
sudo rmmod mini_binder
```

实验验证服务线程在处理请求时同步调用另一个服务。完整说明见教程第 5.3 章。
