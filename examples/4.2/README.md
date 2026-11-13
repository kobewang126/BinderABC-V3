# 第 4.2 章：把引用传回对象拥有者

基于4.1补全LOCAL恢复分支，保留固定角色与扫描事务槽；没有管理者、线程记录或todo队列。

```sh
make
sudo insmod kernel/mini_binder.ko
./build/version
./build/return_to_owner_test
sudo rmmod mini_binder
```

每章源码独立，必须配对使用本章驱动和用户态程序。协议版本显示为 `0402`，不能与同章旧二进制混用。更多回归从本章 build/ 直接执行；对应验证记录见教程 validation/。
