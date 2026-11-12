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
