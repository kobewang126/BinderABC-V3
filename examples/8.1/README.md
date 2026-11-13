# 第 8.1 章：BBinder 与本地调用

纯用户态程序；只实现 Parcel 的 C++ 包装、IBinder 和本地 BBinder，不打开设备。

```sh
make
./build/local_demo
./build/local_test
```

本章完整源码独立维护。实际验证环境、覆盖与限制见教程 validation/README.md；不实现 Android ABI、嵌套事务栈或自动线程池。
