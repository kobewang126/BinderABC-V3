# 第 5.3 章：两个调用线程怎样分别收到回复？

基于5.2拆分调用线程状态与两个回复位置（512/768），公开id仍保留。两线程共享客户端fd并核对倒序回复。

```sh
make
sudo insmod kernel/mini_binder.ko
./build/version
./build/thread_reply_test --demo
sudo rmmod mini_binder
```

每章源码独立，必须配对使用本章驱动和用户态程序。协议版本显示为 `0503`，不能与同章旧二进制混用。更多回归从本章 build/ 直接执行；对应验证记录见教程 validation/。
