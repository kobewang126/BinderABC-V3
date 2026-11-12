# BinderABC 第三版约定

- 以本目录中的 Markdown 和 `examples/` 为独立维护入口。每章源码独立，实验应从对应目录运行，不切换其他章节的 Git 版本。
- 使用问题、方案、关键代码、实验、限制与自测组织内容；代码引用标明本版路径和行号。公开 Android 参考源码应注明具体版本。
- 按需引入状态和接口；保留客户端取消、服务端退出和迟到回复所需的保护。不要以简化为由删除必要的正确性保证。
- 2.3 至 2.5 使用同一 `mini_binder_transaction_data` 布局及共同 ioctl 编号；`code` 在 2.3、2.4 预留，2.5 起用于操作分发。
- 修改代码后核对片段、行号和实际运行结果。2.3、2.4 在 Ubuntu 24.04 ARM64、Linux 6.8.0-147 上完成模块、普通事务、异常、取消、服务端退出和映射测试；2.4 另验证映射回复。其他章节沿用旧版代码及既有验证记录，不代表第三版重新运行过所有章节。
- 网页不是源码或实验的替代品。模拟输出须明确标识；生成网页时须重新核对图和源码锚点。
- 网页顶部提供“下载源码 ZIP”，下载文件为 `site/dist/BinderABC-V3/BinderABC-V3-source.zip`。`npm run build` 使用 Python 3 标准库自动生成源码包及完整离线阅读包 `site/dist/BinderABC-V3.zip`；部署须保留源码包。源码 ZIP 来自网页同一次构建的章节快照，含独立源码、测试和说明，不含 Git 历史及编译产物。
- 教学实验优先遵循“先预测、运行最小场景、观察真实状态、解释结果”；正常演示先于异常回归。输出应显示本章机制，不仅打印 passed。结构整理章须说明外部行为是否改变。
- 2026-11-12 改善 2.7、3.1、3.3、3.4、4.2、6.3：增加槽位与对象布局过程日志；3.1 用正序/逆序计算器分开选择对象和 code；3.4、4.2 正常路径前置；6.3 用 `fd_failure_test --fd-full` 独立观察预留回滚。六章在 binder-lab 的 Ubuntu 24.04 ARM64、Linux 6.8.0-147 上重新构建；对应新实验及 3.1 对象边界、2.7 计算器/映射回复、6.3 全部失败与混合对象回归通过。3.3 是纯用户态实验，配套驱动仅编译。本次未修改驱动协议，也未宣称重跑其他章节。正文失效的回归附录链接已移除，原附录的已有删除保持不变。
- 2026-11-12 完成第七篇至 7.3：`docs/07-protocol/` 与 `examples/7.1/`、`7.2/`、`7.3/` 为独立教材入口，版本 0701/0702/0703。7.1 删除 CREATE_NODE ioctl/结构，LOCAL 传输时按 owner+非零 cookie 自动创建或复用 Node；转换失败回滚本次新 Node、临时 Ref 与文件引用，成功转换之后的完整引用生命周期仍未实现。7.2 删除公开 target_node，普通调用只按发送方 handle 查 Ref，0 为管理入口；内部 Ref 仍用 node_id 关联 Node。7.3 删除 READ_REQUEST/READ_REPLY 输入值，READ 返回 BR_REPLY 或 BR_TRANSACTION，当前线程回复优先；主动 BC_ENTER_LOOPER/EXIT_LOOPER 控制普通进程请求资格，不创建线程。WRITE 仍同步等待，READ 交付 pending_reply，非原生事务栈/命令流；保留独立 ioctl、固定槽和显式线程清理。
- 7.1～7.3 全部在 binder-lab 的 Ubuntu 24.04 ARM64、Linux 6.8.0-147-generic 构建并运行 version、object_demo、protocol_test、manager_test、mapping_test、stale_reply、server_death；对象与协议实验各额外五轮通过。7.3 的精简 looper_demo 和额外 looper_test 也实际运行并各额外五轮通过，后者覆盖非 looper 不偷工作、回复优先、ENTER/EXIT 错误、信号与 unmap 唤醒；观测实际阻塞需读取 /proc syscall。源码仅剔除 receive_request 的冗余外层循环后，7.3 全套重新通过。14 处正文源码片段逐字核对行号，新增链接检查通过，最终模块已卸载。未穷尽并发取消与映射/复制失败组合。网页未重新生成或发布。
- 2026-11-12 完成 7.4 `docs/07-protocol/7.4-write-read.md` 与完整独立 `examples/7.4/`，协议 0704。删除独立 WRITE/READ，统一 MINI_BINDER_WRITE_READ，按方向交换 0 或一个命令包，size/consumed/buffer 描述进度。写阶段只提交事务，读阶段等待线程完成并转移回复至映射；一个线程仍最多一笔在途，不在等待回复时领取普通请求，不支持嵌套调用。用户地址/控制块/映射须保持有效，最终进度写回失败不具有任意竞态下 exactly-once 保证；最小 command_io 不自动重试，恢复须保留 consumed 并区分错误。
- 7.4 主实验拆成 userspace/servicemanager.c、calculator_server.c、calculator_client.c：真实三进程注册 calculator、查询 HANDLE、直接调用加减；管理者与服务端用一次 WRITE_READ 提交上一条回复并读下一事件。名称表仅一项，重复注册 EEXIST，不实现服务替换、死亡清理或权限策略。service.h 仅共享命令/名称编码与最小传输；后续统一函数签名应在用户态对象分发层定义，内核 Node 不执行用户函数。
- 异步提交对应生命周期已调整：最终关闭或显式 THREAD_EXIT 取消当前在途；待领取请求移除回收，已领取请求切断 caller 指针并保留 CANCELLED 至迟到回复或服务端退出；未回复服务端退出导致 EPIPE，已完成回复则可在服务端退出后领取。信号打断读等待会取消当前调用，这是教学规则；没有完整自动线程死亡清理和引用计数。state_lock 保护事务/工作队列，mapping_lock 保护映射，统一读共享等待队列并筛选工作；移除不再使用的旧线程 wait/todo_count，线程 todo 仍保留。
- 7.4 在 binder-lab Ubuntu 24.04 ARM64、Linux 6.8.0-147-generic 实际构建并运行全部二进制与 service_flow.py。三独立程序注册/查询/调用、两个并发客户端、同文件两个线程倒序回复/位置隔离/容量复用各额外五轮通过；write_read 进度与关闭回收、对象/fd 协议、looper、旧回复取消各额外五轮通过。全部最终回归通过、模块卸载。新增测试覆盖写阶段在服务端领取前返回、读 EFAULT 后不重发、只读控制块拒绝、旧 ioctl 拒绝、调用方关闭与线程退出、完成回复在服务端关闭后仍可领取。未穷尽请求交付失败、取消/映射/控制块变化与 fd 耗尽等交错，无性能评估。7.1～7.4 共 23 处源码片段逐字核对行号，新链接通过；第七篇自测扩至 10 题，网页未生成/部署。旧回归附录的用户删除保持未提交。
- 2026-11-12 补齐 7.4 对前章 Parcel 的复用：从 3.3 复制已有 parcel.h/c 与 object_parcel.h/c，基础 Parcel 实现不变，对象封装补 FD 类型、高位检查和 read_next 顺序读取。三个独立主程序用 Parcel 读写服务名、参数、结果及 LOCAL/HANDLE，对映射数据使用只读约定的借用视图，service.h 只绑定事务长度和对象位置。驱动及协议 0704、线格式保持不变。尚无 on_transact 或代理框架，输入视图尚无编译期 const 隔离，FD 编解码不管理 fd 生命周期。
- Parcel 接入后在 binder-lab 同环境干净构建并重新运行三进程 service_flow（含两并发客户端）、全部既有回归、基础 parcel_test 与新增 parcel_binding_test。新增 service_parcel_test 在真实管理者和计算器运行时检查截断名称、缺失/尾部多余参数与正常请求恢复；对象绑定测试校验旧布局兼容、HANDLE/FD、对象位置/高位/截断与写入失败回滚。全部通过并卸载模块；此次不宣称额外重跑此前五轮或穷尽新并发组合。第七篇25处源码片段核对通过，正文/README已更新，网页未生成/发布，用户旧附录删除不提交。
- 7.4 移除仅转发并断言的 call_service，主程序和 service_parcel_test 直接 command_io 并在调用处检查 BR_REPLY；正文同步。binder-lab 重新编译用户态，service_flow 的注册/查找/计算、两并发客户端、异常 Parcel 与重复注册检查通过，模块卸载；25处正文源码片段重新核对。驱动和0704协议不变。
