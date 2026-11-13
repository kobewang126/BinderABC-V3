# 第 4.4 章：客户端从哪里找到服务？

基于4.3引入唯一管理者入口；注册、查询、调用复用对象转换。仍无线程表和todo队列。

```sh
make
sudo insmod kernel/mini_binder.ko
./build/version
./build/manager_test --demo
sudo rmmod mini_binder
```

每章源码独立，必须配对使用本章驱动和用户态程序。协议版本显示为 `0404`，不能与同章旧二进制混用。更多回归从本章 build/ 直接执行；对应验证记录见教程 validation/。
