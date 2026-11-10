# 第 5.4 章：同一个上下文既能调用也能提供对象

在支持内核模块的 Linux 环境中：

```sh
make
sudo insmod kernel/mini_binder.ko
./build/role_test
sudo rmmod mini_binder
```

实验验证无需注册服务端即可创建和传递本地对象，并处理对该对象的调用。完整说明见教程第 5.4 章。
