# 第 4.5 章：引用传回对象拥有者

在支持内核模块的 Linux 环境中：

```sh
make
sudo insmod kernel/mini_binder.ko
./build/return_to_owner_test
sudo rmmod mini_binder
```

实验验证引用作为请求参数和回复返回值传回对象拥有者。完整说明见教程第 4.5 章。
