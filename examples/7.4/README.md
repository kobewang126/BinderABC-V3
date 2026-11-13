# 7.4：按 Node 排队的 oneway

本章完整独立源码，协议0704；保留独立 WRITE/READ。相对7.3增加请求 flags、Node异步队列和 FREE_BUFFER。

```sh
make
sudo insmod kernel/mini_binder.ko
./build/version
./build/oneway_test --demo
./build/oneway_test
sudo rmmod mini_binder
```

主实验只走正常通知流程；完整验收覆盖同对象串行、不同对象并发、容量、回收、fd、线程/进程退出及请求交付失败。线程等待观测依赖可读的 proc syscall/状态。其余原有同步协议、映射、对象、取消、looper和退出回归保留。已在Ubuntu24.04 ARM64/Linux6.8.0-147构建运行，未穷尽并发交错；详见教程及validation记录。
