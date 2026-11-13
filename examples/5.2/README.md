# 第 5.2 章：两个服务线程怎样等待和领取任务？

基于5.1增加proc.todo。服务端一次open、一份映射、两个工作线程；调用端两个线程各自open，因为本章调用状态仍按文件共享。normal demo使用Parcel编解码、proc观测和实验屏障，回复仍填写id。

```sh
make
sudo insmod kernel/mini_binder.ko
./build/version
./build/service_threads_demo
sudo rmmod mini_binder
```

每章源码独立，必须配对使用本章驱动和用户态程序。协议版本显示为 `0502`，不能与同章旧二进制混用。更多回归从本章 build/ 直接执行；对应验证记录见教程 validation/。
