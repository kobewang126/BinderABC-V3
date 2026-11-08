# mini-binder

《从零设计 Binder》的简化 IPC 主线，代码随章节演进。基础知识示例由独立的 examples 仓库维护。

## 第 1.4 章

在具备匹配内核构建目录的 Linux 实验系统中：

```sh
make
sudo insmod kernel/mini_binder.ko
./build/server
```

服务端输出 `server ready` 后，在另一个终端运行 `./build/client 1 2`，也可以启动多个客户端。停止服务端后，可运行 `timeout 10 ./build/server_death` 和 `timeout 10 ./build/stale_reply`。最后执行 `sudo rmmod mini_binder`、`make clean`。

当前支持一个服务端、单笔在途事务及多个客户端串行等待，不支持事务超时、访问控制或 Android Binder 兼容性。具体步骤和限制见教程第 1.4 章。
