# 第 4.3 章：同一个上下文既能调用，也能提供对象

基于4.2移除固定角色并增加读取意图；请求仍扫描事务槽。主实验用管道交换Node编号，尚无管理者。每个文件保留一份回复，固定在映射偏移512。

```sh
make
sudo insmod kernel/mini_binder.ko
./build/version
./build/role_test
sudo rmmod mini_binder
```

每章源码独立，必须配对使用本章驱动和用户态程序。协议版本显示为 `0403`，不能与同章旧二进制混用。更多回归从本章 build/ 直接执行；对应验证记录见教程 validation/。
