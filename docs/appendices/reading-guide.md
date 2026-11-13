# 各章学习目标与最小实验

每章解决一个明确问题，不要求每章新增 ioctl。结构整理章观察数据归属，生命周期章观察资源是否回收；正常输出相同，也可能意味着内部交付机制已经改变。

先阅读章首问题，预测结果，再按正文从本章目录构建并运行。下表只列主要观察点；完整命令、源码文件与行号、实际输出及验证边界以各章正文为准。压缩包读者不需要 Git。切换驱动章节前应停止原程序并卸载同名模块。

按章号阅读，先准备环境，再进行内核模块实验。第四篇的 4.5 是对象转换的补全，第五篇的 5.4 是角色对称性的检查，不应当作新的线程调度机制。

## 第 0 篇

| 章 | 本章问题 | 最小实验入口 | 要观察的证据 |
| --- | --- | --- | --- |
| [0.1](../00-preparation/0.1-process.md) | 程序如何变成独立进程？ | 0.1/call.c 与 0.1/fork.c | 父子进程有不同 PID，数据变化彼此独立 |
| [0.2](../00-preparation/0.2-syscalls.md) | 普通函数怎样请求内核服务？ | 0.2/syscalls.c 与 strace | 观察 write 与失败的 openat，区分返回值和 errno |
| [0.3](../00-preparation/0.3-kernel-environment.md) | 怎样获得可构建、可加载的实验环境？ | 环境检查命令 | 运行内核、headers 和模块构建目标匹配 |
| [0.4](../00-preparation/0.4-kernel-module.md) | 内核模块怎样进入和退出？ | hello.c | 加载、卸载日志分别出现；先准备 0.3 环境 |

## 第 1 篇

| 章 | 本章问题 | 最小实验入口 | 要观察的证据 |
| --- | --- | --- | --- |
| [1.1](../01-rpc/1.1-why-binder.md) | 跨进程后为什么不能直接调用函数？ | rpc_pipe | 双管道把参数送到对端，再把结果送回来 |
| [1.2](../01-rpc/1.2-character-device.md) | 用户态怎样进入自己的驱动？ | version 与 ioctl 验收 | 设备可打开，版本可查询，错误 ioctl 被拒绝 |
| [1.3](../01-rpc/1.3-sync-rpc.md) | 怎样完成一次同步请求和回复？ | 服务端 + 客户端 | 客户端等到服务端计算完成后得到加法结果 |
| [1.4](../01-rpc/1.4-multiple-clients.md) | 第二个客户端怎样等待共享事务槽？ | 同一服务端 + 两个客户端 | 两次调用各自返回；等待排队不等于并行在途 |

## 第 2 篇

| 章 | 本章问题 | 最小实验入口 | 要观察的证据 |
| --- | --- | --- | --- |
| [2.1](../02-transaction/2.1-mmap.md) | 怎样避免把请求负载再复制回服务端？ | 映射请求服务端 + 客户端 | 服务端从只读映射读取，请求元数据仍有复制 |
| [2.2](../02-transaction/2.2-parcel.md) | 不同类型参数怎样按约定编码？ | parcel_demo | 按相同顺序写入和读取；不经过驱动 |
| [2.3](../02-transaction/2.3-transaction.md) | 怎样让 Parcel 成为 RPC 的负载？ | 事务服务端 + 客户端 | 整数和字符串一起到达对端，结果经回复返回 |
| [2.4](../02-transaction/2.4-client-mmap.md) | 客户端能否也从映射读取回复？ | 映射回复客户端 | 先读取结果，再释放回复位置，之后才可复用 |
| [2.5](../02-transaction/2.5-command-dispatch.md) | 同一个对象怎样提供多个方法？ | 计算器加减乘除 | 只改变 code，服务端选择不同用户态函数 |
| [2.6](../02-transaction/2.6-bc-br.md) | 驱动操作与服务方法怎样分开？ | BC/BR 计算器 | cmd 表示事务/回复等协议操作，code 表示计算方法 |
| [2.7](../02-transaction/2.7-concurrent-transactions.md) | 两笔在途请求怎样避免覆盖和串回？ | concurrent_test | 两笔请求占不同位置，按 id 倒序回复仍正确 |

## 第 3 篇

| 章 | 本章问题 | 最小实验入口 | 要观察的证据 |
| --- | --- | --- | --- |
| [3.1](../03-objects/3.1-user-objects.md) | 一个进程怎样包含两个可调用对象？ | 两个计算器对象 | 同一 code 在不同对象上得到不同结果 |
| [3.2](../03-objects/3.2-node.md) | 驱动怎样记录用户态对象的身份？ | object_server / node_test | Node 记录拥有者和 cookie；内核不执行用户函数 |
| [3.3](../03-objects/3.3-parcel-objects.md) | 对象作为参数时怎样在字节流中定位？ | object_parcel_test | 普通数据和对象描述共存，由对象位置表找到描述 |
| [3.4](../03-objects/3.4-node-to-ref.md) | 本地对象怎样成为对端可调用的引用？ | object_server + ref_test | LOCAL 转为接收方 HANDLE，再用该 handle 调用 |
| [3.5](../03-objects/3.5-multiple-servers.md) | 多个对象拥有者之间怎样正确路由？ | multi_server_test | 请求到达所选对象的拥有者，而非固定服务端 |
| [3.6](../03-objects/3.6-transfer-ref.md) | 已有 handle 能否作为参数转交？ | transfer_test | 接收方获得自己的 handle，仍指向同一 Node |

## 第 4 篇

| 章 | 本章问题 | 最小实验入口 | 要观察的证据 |
| --- | --- | --- | --- |
| [4.1](../04-concurrency/4.1-binder-proc.md) | 对象、引用和映射归谁管理？ | object_server + ref_test | 外部调用行为不变；内部状态集中到 proc |
| [4.2](../04-concurrency/4.2-service-manager.md) | 没有预先收到引用时怎样找到服务？ | manager_test --demo | 注册名称、查询 handle、调用服务，结果为 42 |
| [4.3](../04-concurrency/4.3-binder-thread.md) | 同一上下文怎样识别不同调用线程？ | thread_test --demo | dup 共享上下文；不同线程拥有不同记录 |
| [4.4](../04-concurrency/4.4-proc-todo.md) | 服务线程怎样领取进程待办？ | workers_test | 先看到两个请求排队，再由两个线程各领取一笔 |
| [4.5](../04-concurrency/4.5-ref-back-to-owner.md) | 引用传回拥有者时还需要 Ref 吗？ | return_to_owner_test | 请求和回复都恢复 LOCAL 与原 cookie，拥有者调用本地函数 |

## 第 5 篇

| 章 | 本章问题 | 最小实验入口 | 要观察的证据 |
| --- | --- | --- | --- |
| [5.1](../05-threading/5.1-thread-replies.md) | 共享文件的两个线程怎样分别接收回复？ | thread_reply_test --demo | 倒序回复仍匹配原线程，映射位置互不覆盖 |
| [5.2](../05-threading/5.2-thread-todo.md) | 服务线程不填 id，怎样找到该回复的调用线程？ | thread_reply_test --demo | 两个服务线程各自绑定请求，乱序回复正确；跨线程代回被拒绝 |
| [5.3](../05-threading/5.3-thread-exit.md) | 服务线程退出，尚未回复的事务怎么办？ | service_exit_test --demo | fd 仍存活时调用者收到 EPIPE，新线程和事务槽可以复用 |
| [5.4](../05-threading/5.4-client-objects.md) | 客户端能否也成为对象拥有者？ | role_test | 两个进程都不 REGISTER_SERVER，仍能按先后双向调用 |

## 第 6 篇

| 章 | 本章问题 | 最小实验入口 | 要观察的证据 |
| --- | --- | --- | --- |
| [6.1](../06-file-descriptors/6.1-file-descriptors.md) | fd 数字和打开的文件是什么关系？ | fd_demo | dup/fork 后的编号引用同一打开文件，单独关闭不立即销毁它 |
| [6.2](../06-file-descriptors/6.2-fd-request.md) | 请求参数中的 fd 怎样对接收方有效？ | fd_request_demo | 传递的是文件访问能力，接收方能读取管道数据 |
| [6.3](../06-file-descriptors/6.3-fd-lifetime.md) | 传递失败后，编号和文件引用谁回收？ | fd_failure_test --fd-full | 编号可重新打开且两个管道均出现 EOF |
| [6.4](../06-file-descriptors/6.4-fd-reply.md) | 回复中的 fd 是否也需要转换？ | fd_reply_demo | 客户端拿到自己进程的 fd 并读取数据 |

## 第 7 篇

| 章 | 本章问题 | 最小实验入口 | 要观察的证据 |
| --- | --- | --- | --- |
| [7.1](../07-protocol/7.1-auto-node.md) | 怎样撤掉显式 Node 创建？ | object_demo | 反复传同一 LOCAL 复用 handle，随后成功调用对象 |
| [7.2](../07-protocol/7.2-handle-routing.md) | 普通调用是否还需要公开 Node ID？ | object_demo（manager_test 为额外验收） | 只使用管理入口 0 和本地 handle，功能仍完整 |
| [7.3](../07-protocol/7.3-looper-events.md) | 同一 READ 怎样区分请求和回复？ | looper_demo | 普通线程得到 BR_REPLY；进入 looper 后得到 BR_TRANSACTION |
| [7.4](../07-protocol/7.4-write-read.md) | 怎样统一通信入口并完成完整服务流程？ | 三个独立程序 | 注册 calculator、查询 handle、调用加减并得到结果 |
| [7.5](../07-protocol/7.5-driver-api.md) | 怎样从整体理解最终驱动接口与结构？ | 复用 7.4 三个独立程序 | 区分 ioctl、cmd、code，追踪 handle 到 Node 与原调用线程 |

## 主实验与额外回归

带 `--demo` 的程序只执行正文正常路径；不带参数仍执行完整验收。管道、fork 和 barrier 用来稳定安排实验先后，不属于 Binder 协议。4.4 为证明“先排队再领取”需要观察线程阻塞，完整要求见正文。

线程和槽位实验中的一字节负载只是区分请求的标记，不是在重新设计 Parcel。服务名、对象和 fd 的主实验沿用已有 Parcel/object_parcel；边界测试可以手工构造无效字节包，不能把这些反例当作业务编码方式。

7.4 是综合实验。先运行三个程序并追踪成功调用，再阅读 consumed、复制失败和退出回收；不要求第一遍把所有异常分支都学完。
