# 7.4 统一 WRITE_READ，完成服务注册、获取与调用

本目录是第七篇的完整收尾版本，协议 `0704`。保留前章独立源码；运行本章不需要切换 Git 标签。Linux 中先停止其他章节程序并卸载旧模块：

```sh
make
sudo insmod kernel/mini_binder.ko
./build/version
```

然后在三个终端依次运行：

```sh
./build/servicemanager
```

```sh
./build/calculator_server
```

```sh
./build/calculator_client
```

主实验为三个真实独立进程：管理者保存服务名和自己的 handle；服务端注册 LOCAL 对象；客户端查询获得自己的 handle，直接调用加法和减法。服务端和管理者持续响应，用 Ctrl+C 结束；停止二者后才卸载模块。它们在收到一条请求后使用“提交上一条回复 + 等待下一条事件”的同一次 ioctl。

读者无需先阅读补充测试。要自动运行并清理三个进程：

```sh
python3 tests/service_flow.py
```

测试还检查未注册名称、多次查询与调用，以及重复注册拒绝。脚本通过子进程日志确认就绪，不使用固定启动延迟；退出时清理自己的子进程。它需要 Python 3，请勿与手动启动的管理者同时运行。

## 补充回归

```sh
./build/write_read_test
./build/thread_reply_test
./build/object_demo
./build/protocol_test
./build/manager_test
./build/mapping_test
./build/stale_reply
./build/server_death
./build/looper_demo
./build/looper_test
sudo rmmod mini_binder
```

- write_read_test：只写在服务端领取前返回；读失败重试保留提交进度；无效参数与只读控制块；删除的旧 ioctl 被拒绝；客户端关闭时回收待领取请求、拒绝已领取请求的迟到回复；显式线程退出取消；成功回复在服务端关闭后仍可领取。
- thread_reply_test：同一打开文件的两个调用线程、倒序回复、各自映射位置与错误后重试、槽位耗尽与复用。
- object_demo、protocol_test：自动 Node 创建/复用、失败回滚、容量、对象返回 owner，以及双向 fd、EFAULT/EMFILE 读取重试。
- manager_test：额外三进程发现、无效名称、管理者和提供者退出后的 handle 行为。
- mapping_test、stale_reply、server_death：映射限制、取消旧回复和服务退出唤醒。
- looper_demo、looper_test：统一事件读取、普通线程隔离、回复优先、ENTER/EXIT、信号和解除映射唤醒。looper_test 需能读取自身与子进程的 /proc syscall。

## 协议边界

MINI_BINDER_WRITE_READ 是唯一命令交换 ioctl，每个方向为 0 或一个完整命令包，consumed 为 0 或完整包长；仍不是原生变长命令流。写阶段只提交，读阶段等待事件。保留固定全局两槽、单上下文八 Ref/八线程记录、全局八 Node 与 256 字节负载。每个线程最多一个在途调用，不支持嵌套或在等待回复时领取普通请求。单服务名管理者不做服务替换、死亡清理或权限管理。

控制块和映射必须在调用期间保持有效；不要在另一个线程并发修改或解除映射。command_io 是最小传输助手，不自动重试错误；需要恢复的调用应自行保留 consumed，不得在 ioctl 失败后无条件重发事务。信号打断在读阶段取消当前在途调用，这是本教材规则，与原生 Binder 不完全相同。

## 实际验证记录

2026-11-12，在 binder-lab 的 Ubuntu 24.04 ARM64、Linux 6.8.0-147-generic 中构建、加载并运行 version、service_flow.py、write_read_test、thread_reply_test 和以上其余全部回归，最终卸载成功。service_flow（包含两个并发客户端）、共享文件线程回复各额外连续五轮通过；write_read、protocol、looper、取消旧回复回归也各额外五轮通过。编译器名称与内核构建记录不同、缺少 vmlinux 的 BTF 提示不影响已验证的编译与加载。

未进行性能测试，未穷尽控制块与映射并发变化、请求领取失败、多个取消和 fd 耗尽等所有交错。固定表与槽位仍可能耗尽；成功执行的方法不能因用户态没收到确认就假定未执行。
