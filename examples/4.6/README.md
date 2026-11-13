# 第 4.6 章：同一个上下文既能调用也能提供对象

基于 4.5 移除固定服务端角色，增加明确的请求/回复读取选择。仍保留事务 id 和按打开文件保存的一份回复，不包含第五篇的线程回复隔离与事务绑定。

```sh
make
sudo insmod kernel/mini_binder.ko
./build/version
./build/role_test
./build/mapping_test
sudo rmmod mini_binder
```

两个进程都不注册服务端。A 先把本地对象交给 B，B 在第一笔调用完成后通过自己的 handle 调用 A；结果为 twice(7)=14。请求使用映射前两个 256 字节位置，回复固定从偏移 512 开始。映射必须在处理期间保持有效。

各测试从本章 build/ 直接运行，协议版本为 0406。管理者、对象往返、取消和退出测试保留；不是 Android 原生协议。
