# 第三、四篇的可选回归测试

各章正文的主实验用于观察本章新机制。本页列出额外的自动化检查，适合调整驱动后运行；测试保留在各章独立源码目录中，不需要切换 Git 版本。

先停止其他章节的服务进程并卸载旧模块。进入要检查的章节目录，执行 `make`、`sudo insmod kernel/mini_binder.ko`，再运行下表中该章的程序。测试完成后执行 `sudo rmmod mini_binder`。不要同时加载或混用不同章节的驱动。

| 章节 | 额外程序 | 检查内容与条件 |
|---|---|---|
| 3.1 | `./build/object_test` | 保持本章 `object_server` 运行，检查两个对象、错误操作码及恢复。 |
| 3.1 | `./build/concurrent_test`、`./build/stale_reply`、`./build/server_death` | 先停止 `object_server`；各测试自行启动服务端。 |
| 3.2 | `./build/object_test 1 2` | 保持 `object_server` 运行，参数替换为服务端实际输出的两个 Node 编号。 |
| 3.2 | `./build/node_test` | 先停止 `object_server`；检查 Node 创建权限、重复、容量和重启。 |
| 3.3 | `./build/object_parcel_test` | 正文主实验已包含边界、错误数据和写入回滚检查，无需加载驱动。 |
| 3.4 | `./build/ref_test` | 正文主实验已包含两个独立文件、无效对象位置和新进程检查；需要 `object_server` 运行。 |
| 3.5 | `./build/multi_server_test` | 正文主实验同时检查一个服务端退出后另一个仍可调用。 |
| 3.6 | `./build/multi_server_test` | 测试自行启动两个服务端，检查路由和退出隔离。 |
| 4.1 | `./build/ref_test` | 正文主实验已检查独立打开文件和 `dup()` 共享；需要 `object_server` 运行。 |
| 4.2 | `./build/local_transfer_test`、`./build/multi_server_test` | 检查本地对象传递和多服务端路由；各程序自行启动参与进程。 |
| 4.3 | `./build/manager_test`、`./build/local_transfer_test`、`./build/multi_server_test` | 分别检查服务发现、本地对象传递和多服务端路由。 |
| 4.4、4.5 | `./build/pending_cancel_test` | 检查尚未领取的请求被取消后，队列和槽能继续使用；需要能够读取自身子进程的 `/proc/<pid>/syscall`。 |
| 4.4、4.5 | `./build/concurrent_test`、`./build/server_death`、`./build/stale_reply` | 检查两个事务槽、服务端退出和已领取请求取消后的迟到回复。 |
| 4.4、4.5 | `./build/manager_test`、`./build/local_transfer_test`、`./build/multi_server_test`、`./build/thread_test` | 检查前面章节的服务发现、对象传递、多服务端和线程记录。 |
| 4.5 | `./build/workers_test` | 检查两个服务线程领取各自事务。 |

这些测试覆盖列出的情况，不代表已经穷尽取消、映射关闭、进程退出和对象转换的所有并发组合。
