# mini-binder

《从零设计 Binder》的简化 IPC 主线，代码随章节演进。基础知识示例由独立的 examples 仓库维护。

## 第 1.3 章

在具备匹配内核构建目录的 Linux 实验系统中：

```sh
make
sudo insmod kernel/mini_binder.ko
./build/server
```

服务端输出 `server ready` 后，在另一个终端运行：

```sh
./build/client 1 2
```

在服务端终端按 Ctrl+C 退出，再执行 `sudo rmmod mini_binder` 和 `make clean`。当前只支持一个服务端和一笔在途调用，其他调用返回 `EBUSY`。无超时、访问控制或 Android Binder 兼容性。具体步骤和限制见教程第 1.3 章。
