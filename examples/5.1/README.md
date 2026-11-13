# 第 5.1 章：多个线程怎样共用一个 Binder 上下文？

基于4.4只增加线程身份；一次open对应共享proc，dup共享资源。还没有独立线程调用状态。

```sh
make
sudo insmod kernel/mini_binder.ko
./build/version
./build/thread_test --demo
sudo rmmod mini_binder
```

每章源码独立，必须配对使用本章驱动和用户态程序。协议版本显示为 `0501`，不能与同章旧二进制混用。更多回归从本章 build/ 直接执行；对应验证记录见教程 validation/。
