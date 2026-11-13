# 第 5.4 章：服务线程怎样找到该回复的调用线程？

基于5.3以current_transaction和caller路由回复，删除公开id。回复前不能领取下一笔，其他线程不能代回。异常退出保护保留在附加回归，不作为主线；service_exit_test验证显式清理，不能自动检测线程死亡。

```sh
make
sudo insmod kernel/mini_binder.ko
./build/version
./build/thread_reply_test --demo
sudo rmmod mini_binder
```

每章源码独立，必须配对使用本章驱动和用户态程序。协议版本显示为 `0504`，不能与同章旧二进制混用。更多回归从本章 build/ 直接执行；对应验证记录见教程 validation/。
