# mini-binder

《从零设计 Binder》的简化 IPC 主线，代码随章节演进。基础知识示例由独立的 examples 仓库维护。

## 第 1.2 章

在具备匹配内核构建目录的 Linux 实验系统中：

```sh
make
sudo insmod kernel/mini_binder.ko
./build/version
./build/version --invalid
./build/version --bad-address
sudo rmmod mini_binder
make clean
```

可用 `make KDIR=/path/to/kernel-build` 选择目标内核的构建目录。模块必须与实际运行内核匹配，加载还受权限和系统策略约束。当前版本仅注册字符设备并提供协议版本查询，不实现 RPC；详细说明和验收步骤见教程第 1.2 章。
