# 从零设计 Binder

从 C 函数调用出发，逐步设计可运行的 mini-binder，并对照 Android Binder 源码。

这是第三版教程，保留逐步设计和独立章节实验；第 2.3 至 2.5 章沿用同一事务结构，并在需要时启用操作码。

## 阅读目录

### 0 预备篇

- [0.1 从 main.c 到 Linux 进程](docs/00-preparation/0.1-process.md)
- [0.2 从函数调用到系统调用](docs/00-preparation/0.2-syscalls.md)
- [0.3 第一个 Linux 内核模块](docs/00-preparation/0.3-kernel-module.md)
- [0.4 搭建 Linux 内核实验环境](docs/00-preparation/0.4-kernel-environment.md)

### 1 第一篇：从函数调用到同步 RPC

- [第 1.1 章 为什么需要 Binder？](docs/01-rpc/1.1-why-binder.md)
- [第 1.2 章 设计第一个字符设备驱动](docs/01-rpc/1.2-character-device.md)
- [第 1.3 章 实现第一个同步 RPC](docs/01-rpc/1.3-sync-rpc.md)
- [第 1.4 章 让多个客户端等待调用](docs/01-rpc/1.4-multiple-clients.md)

### 2 第二篇：映射、Parcel 与事务

- [第 2.1 章 从数据复制到映射缓冲区](docs/02-transaction/2.1-mmap.md)
- [第 2.2 章 从普通数据到 Parcel](docs/02-transaction/2.2-parcel.md)
- [第 2.3 章 让 Parcel 通过驱动传输](docs/02-transaction/2.3-transaction.md)
- [第 2.4 章 让客户端从映射区读取回复](docs/02-transaction/2.4-client-mmap.md)
- [第 2.5 章 用事务命令选择服务函数](docs/02-transaction/2.5-command-dispatch.md)
- [第 2.6 章 使用 BC 和 BR 命令](docs/02-transaction/2.6-bc-br.md)
- [第 2.7 章 多笔事务与用户地址](docs/02-transaction/2.7-concurrent-transactions.md)

实验时使用各章自带的驱动和程序，不能跨章节混用。

从 2.1、2.3 至 2.7 的当前示例开始，版本查询按四位章号显示，例如 `0201` 表示 2.1；驱动协议中使用整数 `201`，显示时补零。2.2 是纯用户态示例，没有驱动版本。版本号用于识别章节协议，修改历史仍由 Git 提交区分。

### 3 第三篇：服务对象与引用

- [第 3.1 章 一个服务端，两个用户态对象](docs/03-objects/3.1-user-objects.md)
- [第 3.2 章 在驱动中表示服务对象](docs/03-objects/3.2-node.md)
- [第 3.3 章 在 Parcel 中描述对象](docs/03-objects/3.3-parcel-objects.md)

Node、Ref 和 ServiceManager 在后续章节逐步引入。

配套代码由 `examples` 按章节保存，是运行实验和调整示例的入口。获取源码后，在 Linux 环境中进入对应章节目录，按照说明编译和运行，无需切换 Git 版本；源码压缩包同样可以使用。

本版调整过的 2.3、2.4 已在 Ubuntu 24.04 ARM64、Linux 6.8.0-147 上构建并运行正常与异常实验；其他章节沿用旧版源码和既有验证记录。
