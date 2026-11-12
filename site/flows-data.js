import { steps06a } from './flow-06a-data.js';

const S = (message, title, context, detail, file, start, end, anchor) => ({ message, title, context, detail, file: `examples/${file}`, ranges: [[start, end]], anchor: [start, anchor] });
const flow = (chapter, diagram, kind, title, events, steps) => ({ chapter, diagram, kind, title, events, steps });

const legacyFlows = [
  flow('docs/00-preparation/p01-process.md', 0, 'concept', '进程地址空间', ['进程 A 的虚拟地址', '进程 A 的地址映射', '进程 B 的虚拟地址', '进程 B 的地址映射', '内存页 A', '内存页 B'], [
    S(0, '进程 A 的虚拟地址', '概念示意', '每个进程中的指针都是虚拟地址；相关实验在 fork 后分别打印父子进程中的局部变量地址。', 'p01/fork.c', 9, 10, 'int value'),
    S(1, '进程 A 的地址映射', '概念示意', '映射由操作系统维护，不是示例程序直接实现的函数；图用于说明地址属于进程的地址空间。', 'p01/fork.c', 9, 10, 'int value'),
    S(2, '进程 B 的虚拟地址', '概念示意', '子进程打印自己的局部变量地址，数值相同并不证明父子进程共享普通私有内存。', 'p01/fork.c', 16, 19, 'if (child == 0)'),
    S(3, '进程 B 的地址映射', '概念示意', '不同进程的虚拟地址分别由自己的地址空间解释；代码只观察结果，不展示内核页表。', 'p01/fork.c', 16, 19, 'if (child == 0)'),
    S(4, '内存页 A', '概念示意', '父进程的值不因子进程修改而改变；实际系统可能先采用写时复制，不能假设 fork 后立即分配独立物理页。', 'p01/fork.c', 23, 33, 'int status'),
    S(5, '内存页 B', '概念示意', '子进程修改自己的 value 后打印 20；图中的独立页是解释隔离结果的简化示意。', 'p01/fork.c', 16, 19, 'if (child == 0)')]),
  flow('docs/00-preparation/p02-syscalls.md', 0, 'sequence', 'write 系统调用路径', ['write(fd, buffer, count)', '进入系统调用', '返回结果', '返回字节数或 -1'], [
    S(0, '用户程序调用 write', '用户程序线程 · 用户态', '示例从尚未写入的位置调用 C 库的 write。', 'p02/syscalls.c', 12, 14, 'while (offset'),
    S(1, '进入系统调用', '用户程序线程 · 内核态', '库封装进入 Linux 内核；右侧仅展示用户态调用位置，库和内核实现不在本例源码中。', 'p02/syscalls.c', 13, 14, 'ssize_t count'),
    S(2, '内核返回结果', '用户程序线程 · 内核态', '内核完成或拒绝写入后返回；具体内核实现不在本例中。', 'p02/syscalls.c', 13, 14, 'ssize_t count'),
    S(3, '检查返回值', '用户程序线程 · 用户态', '调用返回后检查失败、信号中断和零进展；示例按实际写入长度继续。', 'p02/syscalls.c', 15, 25, 'if (count < 0)')]),
  flow('docs/01-rpc/01-why-binder.md', 0, 'sequence', '管道请求与回复', ['请求 add(1, 2)', '读取请求', '计算 1 + 2', '回复 3', '读取回复'], [
    S(0, '客户端发送请求', '父进程 · 用户态', '父进程把请求写入请求管道。write_all 最终通过 write 进入内核。', 'ch01/rpc_pipe.c', 90, 99, 'struct request req'),
    S(1, '服务端读取请求', '子进程 · 用户态', '子进程从请求管道读取完整请求。read_all 内部会调用 read。', 'ch01/rpc_pipe.c', 74, 77, 'close(requests[1])'),
    S(2, '服务端计算', '子进程 · 用户态', '用更宽整数计算并检查结果范围。', 'ch01/rpc_pipe.c', 79, 82, 'int64_t sum'),
    S(3, '服务端发送回复', '子进程 · 用户态', '将计算结果写入回复管道。', 'ch01/rpc_pipe.c', 82, 84, 'result ='),
    S(4, '客户端接收回复', '父进程 · 用户态', '父进程读取回复，稍后等待子进程并打印结果。', 'ch01/rpc_pipe.c', 99, 102, 'close(requests[1])')]),
  flow('docs/01-rpc/02-character-device.md', 0, 'sequence', '字符设备版本查询', ['open /dev/mini_binder', '文件描述符', 'ioctl GET_VERSION', 'unlocked_ioctl', '拷贝协议版本', 'close'], [
    S(0, '打开设备', '用户程序线程 · 用户态', '程序通过 open 请求访问设备。', 'ch02/userspace/version.c', 21, 25, 'int fd = open'),
    S(1, '获得文件描述符', '用户程序线程 · 用户态', 'open 返回进程内文件描述符；VFS 内部实现不在本示例中。', 'ch02/userspace/version.c', 21, 25, 'int fd = open'),
    S(2, '请求版本', '用户程序线程 · 用户态', '程序调用 GET_VERSION ioctl。', 'ch02/userspace/version.c', 46, 50, 'if (ioctl'),
    S(3, '分派到设备回调', '用户程序线程 · 内核态', 'VFS 使用文件关联的 unlocked_ioctl 回调；图中 VFS 的内部实现不在本仓库。', 'ch02/kernel/mini_binder.c', 25, 29, 'static const struct file_operations'),
    S(4, '复制版本', '用户程序线程 · 内核态', '驱动把版本复制到用户地址并处理 EFAULT。', 'ch02/kernel/mini_binder.c', 16, 19, 'case MINI_BINDER_GET_VERSION'),
    S(5, '关闭设备', '用户程序线程 · 用户态', '程序输出版本并关闭文件描述符。', 'ch02/userspace/version.c', 51, 55, 'printf(')]),
  flow('docs/01-rpc/03-sync-rpc.md', 0, 'sequence', '单客户端同步 RPC', ['REGISTER_SERVER', '注册成功', 'RECEIVE，等待请求', 'CALL(a=1, b=2)', '唤醒并交付请求', '计算 1 + 2 = 3', 'REPLY(result=3, status=0)', 'CALL 返回结果 3'], [
    S(0, '注册服务端', '服务端线程 · 用户态', '服务端提交注册请求。', 'ch03/userspace/server.c', 19, 23, 'if (ioctl'),
    S(1, '注册完成', '服务端线程 · 内核态', '驱动在锁内记录服务端文件。', 'ch03/kernel/mini_binder.c', 22, 33, 'static long register_server'),
    S(2, '等待请求', '服务端线程 · 内核态', '服务端通过 RECEIVE 等待 PENDING 请求，不持锁睡眠。', 'ch03/kernel/mini_binder.c', 86, 103, 'mutex_lock'),
    S(3, '提交请求', '客户端线程 · 用户态', '客户端调用 CALL，参数进入驱动。', 'ch03/userspace/client.c', 40, 44, 'if (ioctl'),
    S(4, '领取请求', '服务端线程 · 内核态', '驱动把状态改为 PROCESSING 并复制请求。', 'ch03/kernel/mini_binder.c', 91, 99, 'if (state == PENDING)'),
    S(5, '计算结果', '服务端线程 · 用户态', '服务端计算并检查溢出。', 'ch03/userspace/server.c', 37, 41, 'sum ='),
    S(6, '提交回复', '服务端线程 · 内核态', '驱动检查状态后保存回复并唤醒客户端。', 'ch03/kernel/mini_binder.c', 116, 133, 'mutex_lock'),
    S(7, '返回结果', '客户端线程 · 内核态', '客户端醒来后取出回复，清理状态并复制结果。', 'ch03/kernel/mini_binder.c', 63, 76, 'mutex_lock')]),
  flow('docs/01-rpc/03a-multiple-clients.md', 0, 'sequence', '两个客户端等待', ['REGISTER_SERVER，然后等待 RECEIVE', 'CALL(1, 2)', '交付请求 A', 'CALL(10, 20)', '计算请求 A', 'REPLY A', 'CALL 返回结果 3', '交付请求 B', '计算请求 B', 'REPLY B', 'CALL 返回结果 30'], [
    S(0, '服务端等待', '服务端线程 · 用户态', '服务端注册后调用 RECEIVE；后续执行可能阻塞。', 'ch03a/userspace/server.c', 19, 31, 'if (ioctl'),
    S(1, '客户端 A 调用', '客户端 A 线程 · 内核态', 'A 获取 call_lock 后发布请求；此处展示一种调度顺序。', 'ch03a/kernel/mini_binder.c', 42, 65, 'if (copy_from_user'),
    S(2, '服务端领取 A', '服务端线程 · 内核态', 'RECEIVE 领取当前请求并复制元数据。', 'ch03a/kernel/mini_binder.c', 99, 107, 'if (state == RPC_PENDING)'),
    S(3, '客户端 B 等待', '客户端 B 线程 · 内核态', '若 A 尚持有 call_lock，B 在获取这把锁时等待。', 'ch03a/kernel/mini_binder.c', 44, 47, 'ret = mutex_lock_interruptible'),
    S(4, '服务端计算 A', '服务端线程 · 用户态', '服务端按请求计算加法。', 'ch03a/userspace/server.c', 37, 42, 'sum ='),
    S(5, '服务端回复 A', '服务端线程 · 内核态', '驱动匹配编号并保存回复。', 'ch03a/kernel/mini_binder.c', 125, 138, 'mutex_lock'),
    S(6, 'A 接收回复', '客户端 A 线程 · 内核态', 'A 清理事务并释放 call_lock；B 可能在 A 返回用户态前继续。', 'ch03a/kernel/mini_binder.c', 69, 80, 'mutex_lock'),
    S(7, '服务端领取 B', '服务端线程 · 内核态', 'B 获取 call_lock 后提交，服务端领取下一笔请求。', 'ch03a/kernel/mini_binder.c', 99, 107, 'if (state == RPC_PENDING)'),
    S(8, '服务端计算 B', '服务端线程 · 用户态', '同一处理逻辑计算第二笔请求。', 'ch03a/userspace/server.c', 37, 42, 'sum ='),
    S(9, '服务端回复 B', '服务端线程 · 内核态', '驱动检查请求编号并交付 B 的回复。', 'ch03a/kernel/mini_binder.c', 125, 138, 'mutex_lock'),
    S(10, 'B 接收回复', '客户端 B 线程 · 内核态', 'B 清理事务、返回结果并释放 call_lock。', 'ch03a/kernel/mini_binder.c', 69, 80, 'mutex_lock')]),
  flow('docs/02-transaction/04-mmap.md', 0, 'concept', '映射与物理页', ['驱动的内核虚拟地址', '请求缓冲区的物理页', '服务端的用户虚拟地址'], [
    S(0, '内核虚拟地址', '概念示意', '服务端注册时驱动分配缓冲区；内核保留自己的虚拟地址。', 'ch04/kernel/mini_binder.c', 44, 50, 'static long register_server'),
    S(1, '底层页', '概念示意', '图示两侧访问同一底层页；vmalloc 的内核虚拟地址连续不意味着一般情况下物理连续。', 'ch04/kernel/mini_binder.c', 44, 50, 'static long register_server'),
    S(2, '服务端虚拟地址', '服务端线程 · 用户态', '服务端通过 mmap 获得自己的用户态地址。', 'ch04/userspace/server.c', 33, 38, 'mapping = mmap')]),
  flow('docs/02-transaction/04-mmap.md', 1, 'sequence', '映射请求缓冲区', ['REGISTER_SERVER，分配一页', 'mmap，一次建立只读映射', 'CALL，复制客户端结构到内核', 'memcpy 参数到缓冲区', 'RECEIVE 返回 id、offset、size', '从映射读取参数', 'REPLY 返回计算结果', '唤醒并复制回复'], [
    S(0, '注册与分配', '服务端线程 · 内核态', '注册服务端并分配一页。', 'ch04/kernel/mini_binder.c', 44, 59, 'static long register_server'),
    S(1, '建立映射', '服务端线程 · 用户态', '服务端发起只读共享映射。', 'ch04/userspace/server.c', 33, 38, 'mapping = mmap'),
    S(2, '提交 CALL', '客户端线程 · 内核态', '驱动复制客户端请求结构。', 'ch04/kernel/mini_binder.c', 92, 98, 'if (copy_from_user'),
    S(3, '写入服务端页', '客户端线程 · 内核态', '驱动将请求参数写入服务端页并发布状态。', 'ch04/kernel/mini_binder.c', 116, 125, 'server_ctx ='),
    S(4, '领取元数据', '服务端线程 · 内核态', '服务端 RECEIVE 领取请求并复制元数据。', 'ch04/kernel/mini_binder.c', 163, 171, 'if (state == RPC_PENDING)'),
    S(5, '从映射读参数', '服务端线程 · 用户态', '服务端检查元数据，读取映射并计算。', 'ch04/userspace/server.c', 53, 63, 'if (req.size'),
    S(6, '提交回复', '服务端线程 · 内核态', '驱动检查编号和状态，保存回复并唤醒客户端。', 'ch04/kernel/mini_binder.c', 190, 208, 'mutex_lock'),
    S(7, '客户端恢复', '客户端线程 · 内核态', '客户端被唤醒后取得回复并复制到用户地址。', 'ch04/kernel/mini_binder.c', 127, 144, 'ret = wait_event_interruptible')]),
  flow('docs/02-transaction/06-transaction.md', 0, 'sequence', 'Parcel 同步事务', ['注册并 mmap', 'Parcel 编码 42 和 Binder', 'TRANSACT（长度和字节）', '写入请求载荷', 'RECEIVE 返回编号、偏移、长度', '解码请求', '计算 84，编码回复', 'TRANSACTION_REPLY（编号和字节）', '唤醒，复制回复到调用结构', '解码并打印结果'], [
    S(0, '服务端准备映射', '服务端线程 · 用户态', '服务端注册并发起映射。', 'ch06/userspace/transaction_server.c', 22, 27, 'if (ioctl'),
    S(1, '客户端编码', '客户端线程 · 用户态', '编码整数与字符串。', 'ch06/userspace/transaction_client.c', 20, 23, 'parcel_init'),
    S(2, '提交事务', '客户端线程 · 用户态', '客户端调用 TRANSACT。', 'ch06/userspace/transaction_client.c', 24, 30, 'fd = open'),
    S(3, '驱动写入请求', '客户端线程 · 内核态', '驱动将载荷复制到服务端页并发布请求。', 'ch06/kernel/mini_binder.c', 125, 140, 'server_ctx ='),
    S(4, '服务端领取元数据', '服务端线程 · 内核态', 'RECEIVE 领取请求并复制元数据。', 'ch06/kernel/mini_binder.c', 191, 199, 'if (state == RPC_PENDING)'),
    S(5, '服务端解码', '服务端线程 · 用户态', '从映射页读取 Parcel 并验证。', 'ch06/userspace/transaction_server.c', 49, 55, 'reply.id'),
    S(6, '计算并编码', '服务端线程 · 用户态', '计算结果并编码回复。', 'ch06/userspace/transaction_server.c', 56, 60, 'number > UINT32_MAX'),
    S(7, '服务端提交回复', '服务端线程 · 内核态', '驱动验证并保存回复，唤醒客户端。', 'ch06/kernel/mini_binder.c', 232, 253, 'mutex_lock'),
    S(8, '返回客户端', '客户端线程 · 内核态', '客户端唤醒后把回复复制到事务结构，并通过 copy_to_user 返回用户态。', 'ch06/kernel/mini_binder.c', 142, 166, 'ret = wait_event_interruptible'),
    S(9, '解码回复', '客户端线程 · 用户态', '客户端验证、解码并输出结果。', 'ch06/userspace/transaction_client.c', 32, 43, 'if (transaction.status')]),
  flow('docs/02-transaction/06a-client-mmap.md', 0, 'sequence', '映射回复', ['注册服务端并 mmap', 'open 并 mmap', 'TRANSACT_MAPPED，提交请求', '写入请求', 'RECEIVE 返回请求元数据', '解码请求', 'TRANSACTION_REPLY，提交回复', '复制回复到客户端页', '返回状态和回复长度', '从偏移 0 解码回复', 'RELEASE_REPLY，允许复用'], steps06a)
];

const chapterPaths = {
  'docs/00-preparation/p01-process.md': 'docs/00-preparation/0.1-process.md',
  'docs/00-preparation/p02-syscalls.md': 'docs/00-preparation/0.2-syscalls.md',
  'docs/01-rpc/01-why-binder.md': 'docs/01-rpc/1.1-why-binder.md',
  'docs/01-rpc/02-character-device.md': 'docs/01-rpc/1.2-character-device.md',
  'docs/01-rpc/03-sync-rpc.md': 'docs/01-rpc/1.3-sync-rpc.md',
  'docs/01-rpc/03a-multiple-clients.md': 'docs/01-rpc/1.4-multiple-clients.md'
};
const sourcePaths = { p01: '0.1', p02: '0.2', ch01: '1.1', ch02: '1.2', ch03: '1.3', ch03a: '1.4' };
export const flows = legacyFlows.filter(item => chapterPaths[item.chapter]).map(item => ({
  ...item,
  chapter: chapterPaths[item.chapter],
  steps: item.steps.map(step => ({ ...step, file: step.file.replace(/^examples\/([^/]+)/, (_, dir) => `examples/${sourcePaths[dir]}`) }))
}));

flows.push(flow('docs/02-transaction/2.6-bc-br.md', 0, 'sequence', 'BC 与 BR 事务', [
  'READ，等待 BR_TRANSACTION', 'WRITE：BC_TRANSACTION 和请求', 'READ 返回 BR_TRANSACTION',
  '读取映射中的 Parcel，根据 code 计算', 'WRITE：BC_REPLY 和回复', 'WRITE 返回',
  'READ，取得 BR_REPLY', 'READ 返回 BR_REPLY', '从客户端映射读取回复', 'WRITE：BC_FREE_REPLY'
], [
  S(0, '服务端等待请求', '服务端线程 · 用户态', '服务端调用 exchange_command 读取命令，驱动在无请求时等待。', '2.6/userspace/calc_server.c', 76, 81, 'int ret = exchange_command'),
  S(1, '客户端提交事务', '客户端线程 · 用户态', '客户端将事务装入 BC_TRANSACTION 并调用命令接口。', '2.6/userspace/calc_ipc.h', 25, 30, 'parcel_init'),
  S(2, '交付请求', '服务端线程 · 内核态', '驱动领取事务并将请求元数据复制给服务端。', '2.6/kernel/mini_binder.c', 182, 210, 'static long receive_request'),
  S(3, '解码并计算', '服务端线程 · 用户态', '服务端读取映射中的参数，并根据事务 code 选择计算函数。', '2.6/userspace/calc_server.c', 87, 98, 'if (parcel_read_u32'),
  S(4, '提交回复', '服务端线程 · 用户态', '服务端将回复放入 BC_REPLY 并提交。', '2.6/userspace/calc_server.c', 105, 107, 'ret = exchange_command'),
  S(5, '客户端调用返回', '客户端线程 · 内核态', '客户端等待结束后将回复写入客户端映射页并返回。', '2.6/kernel/mini_binder.c', 146, 158, 'memcpy(client_ctx->buffer'),
  S(6, '请求读取回复', '客户端线程 · 用户态', 'exchange_command 在写入完成后调用 READ 获取回复命令。', '2.6/userspace/command.h', 16, 25, 'if (read_cmd)'),
  S(7, '交付回复元数据', '客户端线程 · 内核态', '驱动将保存的回复元数据通过 READ 返回。', '2.6/kernel/mini_binder.c', 273, 296, 'static long read_command'),
  S(8, '解码映射回复', '客户端线程 · 用户态', '客户端从映射页解码计算结果。', '2.6/userspace/calc_ipc.h', 39, 40, 'if (parcel_read_u32'),
  S(9, '释放回复槽', '客户端线程 · 用户态', '读取完成后发送 BC_FREE_REPLY，允许复用回复页。', '2.6/userspace/calc_ipc.h', 43, 43, 'exchange_command(fd, &release')
]));

const concurrentEvents = [
  'BC_TRANSACTION：加法', 'BC_TRANSACTION：减法', 'READ', 'BR_TRANSACTION：请求 A 的 id 和用户地址',
  'READ', 'BR_TRANSACTION：请求 B 的 id 和用户地址', '按各自用户地址读取两份数据',
  'BC_REPLY：先回复 B 的 id', '唤醒等待中的 B', 'BC_REPLY：再回复 A 的 id', '唤醒等待中的 A',
  'READ', 'BR_REPLY', '读取自己的回复页', 'BC_FREE_REPLY', 'READ', 'BR_REPLY', '读取自己的回复页', 'BC_FREE_REPLY'
];
const concurrentSteps = [
  [0, '客户端 A 提交', '客户端 A 线程 · 用户态', '测试客户端按输入构造加法请求并提交。', '2.7/tests/concurrent_test.c', 26, 30, 'request.transaction.code'],
  [1, '客户端 B 提交', '客户端 B 线程 · 用户态', '另一客户端构造减法请求并提交。', '2.7/tests/concurrent_test.c', 26, 30, 'request.transaction.code'],
  [2, '服务端读取 A', '服务端线程 · 用户态', '服务端循环读取两笔事务。', '2.7/tests/concurrent_test.c', 69, 76, 'for (i = 0; i < 2; i++)'],
  [3, '交付请求 A', '服务端线程 · 内核态', '驱动在领取时计算当前槽的用户地址并返回事务元数据。', '2.7/kernel/mini_binder.c', 266, 295, 'mutex_lock'],
  [4, '服务端读取 B', '服务端线程 · 用户态', '服务端继续读取下一笔事务。', '2.7/tests/concurrent_test.c', 69, 76, 'for (i = 0; i < 2; i++)'],
  [5, '交付请求 B', '服务端线程 · 内核态', '驱动返回另一槽位的编号和地址。', '2.7/kernel/mini_binder.c', 266, 295, 'mutex_lock'],
  [6, '读取两份请求', '服务端线程 · 用户态', '测试检查地址范围并通过返回的用户地址读取数据。', '2.7/tests/concurrent_test.c', 70, 86, 'if (exchange_command'],
  [7, '先回复 B', '服务端线程 · 用户态', '测试按领取顺序的逆序发送回复，此处展示 B 先回复的一种情况。', '2.7/tests/concurrent_test.c', 94, 105, 'for (i = 2; i-- > 0;)'],
  [8, '唤醒 B', '服务端线程 · 内核态', '驱动按编号找到槽位，保存回复并唤醒客户端等待队列。', '2.7/kernel/mini_binder.c', 327, 352, 'mutex_lock'],
  [9, '再回复 A', '服务端线程 · 用户态', '循环继续提交另一笔回复，使用对应请求编号。', '2.7/tests/concurrent_test.c', 94, 105, 'for (i = 2; i-- > 0;)'],
  [10, '唤醒 A', '服务端线程 · 内核态', '驱动检查另一笔事务编号并唤醒等待队列。', '2.7/kernel/mini_binder.c', 327, 352, 'mutex_lock'],
  [11, 'B 读取回复命令', '客户端 B 线程 · 用户态', '命令封装在写入事务返回后读取回复命令。', '2.7/userspace/command.h', 16, 25, 'if (read_cmd)'],
  [12, '返回 B 的回复元数据', '客户端 B 线程 · 内核态', '驱动返回当前打开文件上下文中的回复命令。', '2.7/kernel/mini_binder.c', 374, 398, 'static long read_command'],
  [13, 'B 读取回复页', '客户端 B 线程 · 用户态', '客户端验证自己的映射页中返回的结果。', '2.7/tests/concurrent_test.c', 30, 34, 'if (exchange_command'],
  [14, 'B 释放回复', '客户端 B 线程 · 用户态', '客户端完成读取后发送 BC_FREE_REPLY。', '2.7/tests/concurrent_test.c', 35, 35, 'exchange_command(fd, &release'],
  [15, 'A 读取回复命令', '客户端 A 线程 · 用户态', '另一客户端读取各自的回复命令。', '2.7/userspace/command.h', 16, 25, 'if (read_cmd)'],
  [16, '返回 A 的回复元数据', '客户端 A 线程 · 内核态', '驱动返回对应文件上下文中的回复命令。', '2.7/kernel/mini_binder.c', 374, 398, 'static long read_command'],
  [17, 'A 读取回复页', '客户端 A 线程 · 用户态', '另一客户端读取自己的回复页。', '2.7/tests/concurrent_test.c', 30, 34, 'if (exchange_command'],
  [18, 'A 释放回复', '客户端 A 线程 · 用户态', '读取完成后发送 BC_FREE_REPLY。', '2.7/tests/concurrent_test.c', 35, 35, 'exchange_command(fd, &release']
];
flows.push(flow('docs/02-transaction/2.7-concurrent-transactions.md', 1, 'sequence', '两笔事务与倒序回复', concurrentEvents, concurrentSteps.map(args => S(...args))));

flows.push(flow('docs/01-rpc/1.3-sync-rpc.md', 1, 'sequence', '同步调用的数据结构传递', [
  'CALL：copy_from_user 复制 call', 'RECEIVE：copy_to_user 复制 request',
  'REPLY：copy_from_user 复制 reply', 'CALL 返回：copy_to_user 复制 call'
], [
  S(0, '复制客户端调用', '客户端线程 · 内核态', '驱动复制 call，并从中提取请求参数。', '1.3/kernel/mini_binder.c', 40, 52, 'if (copy_from_user'),
  S(1, '复制请求', '服务端线程 · 内核态', '驱动将 request 复制给服务端。', '1.3/kernel/mini_binder.c', 91, 99, 'if (state == PENDING)'),
  S(2, '复制回复', '服务端线程 · 内核态', '驱动将服务端提交的 reply 复制到内核。', '1.3/kernel/mini_binder.c', 114, 128, 'if (copy_from_user'),
  S(3, '复制调用结果', '客户端线程 · 内核态', '客户端线程把回复填入 call 并复制回用户态。', '1.3/kernel/mini_binder.c', 63, 78, 'mutex_lock')
]));

flows.push(flow('docs/02-transaction/2.1-mmap.md', 0, 'concept', '映射与物理页', [
  '驱动的内核虚拟地址', '请求缓冲区的物理页', '服务端的用户虚拟地址'
], [
  S(0, '内核虚拟地址', '概念示意', '注册服务端时驱动分配页并保留内核地址。', '2.1/kernel/mini_binder.c', 44, 57, 'static long register_server'),
  S(1, '底层页', '概念示意', '映射两端访问同一底层页；示意图不表示内核虚拟地址与用户地址相同。', '2.1/kernel/mini_binder.c', 65, 80, 'static int mini_binder_mmap'),
  S(2, '服务端用户地址', '服务端线程 · 用户态', '服务端调用 mmap 建立只读映射。', '2.1/userspace/server.c', 33, 38, 'mapping = mmap')
]));

flows.push(flow('docs/02-transaction/2.3-transaction.md', 0, 'sequence', 'Parcel 同步事务', [
  '注册并 mmap', 'Parcel 编码 42、10 和 Binder', 'TRANSACT（长度和字节）', '写入请求载荷',
  'RECEIVE_TRANSACTION 返回编号和长度', '解码请求', '计算 42 + 10 = 52，编码回复',
  'TRANSACTION_REPLY（编号和字节）', '唤醒，复制回复到调用结构', '解码并打印结果'
], [
  S(0, '建立映射', '服务端线程 · 用户态', '服务端注册并建立只读映射。', '2.3/userspace/transaction_server.c', 22, 27, 'if (ioctl'),
  S(1, '编码请求', '客户端线程 · 用户态', '客户端将整数与字符串写入请求 Parcel。', '2.3/userspace/transaction_client.c', 20, 23, 'parcel_init'),
  S(2, '提交事务', '客户端线程 · 内核态', '驱动从用户态复制事务并检查请求长度。', '2.3/kernel/mini_binder.c', 91, 94, 'if (copy_from_user'),
  S(3, '写入请求', '客户端线程 · 内核态', '驱动把载荷写入服务端映射页并发布请求。', '2.3/kernel/mini_binder.c', 117, 123, 'server_ctx ='),
  S(4, '交付元数据', '服务端线程 · 内核态', '驱动将请求编号与长度复制到服务端。', '2.3/kernel/mini_binder.c', 163, 171, 'if (state == RPC_PENDING)'),
  S(5, '解码请求', '服务端线程 · 用户态', '服务端从映射页读取 Parcel 并检查格式。', '2.3/userspace/transaction_server.c', 43, 56, 'if (ioctl'),
  S(6, '计算回复', '服务端线程 · 用户态', '服务端计算加法并写入回复 Parcel。', '2.3/userspace/transaction_server.c', 53, 61, 'if (request.size'),
  S(7, '提交回复', '服务端线程 · 内核态', '驱动复制回复，检查编号并保存结果。', '2.3/kernel/mini_binder.c', 190, 210, 'if (copy_from_user'),
  S(8, '返回客户端', '客户端线程 · 内核态', '客户端被唤醒后取回复，并复制事务结构到用户态。', '2.3/kernel/mini_binder.c', 125, 143, 'ret = wait_event_interruptible'),
  S(9, '解码回复', '客户端线程 · 用户态', '客户端检查事务状态并解码打印回复。', '2.3/userspace/transaction_client.c', 32, 43, 'if (transaction.status')
]));

flows.push(flow('docs/02-transaction/2.3-transaction.md', 1, 'sequence', '事务数据传递', [
  'TRANSACT：copy_from_user 复制 transaction', 'memcpy 请求 data 到映射缓冲区',
  'RECEIVE_TRANSACTION：copy_to_user 复制请求元数据', '从映射区读取请求 Parcel',
  'TRANSACTION_REPLY：copy_from_user 复制 reply', 'TRANSACT 返回：copy_to_user 复制 transaction'
], [
  S(0, '复制事务', '客户端线程 · 内核态', '驱动复制客户端事务结构。', '2.3/kernel/mini_binder.c', 91, 94, 'if (copy_from_user'),
  S(1, '写入映射页', '客户端线程 · 内核态', '驱动将请求 data 拷贝到服务端页。', '2.3/kernel/mini_binder.c', 117, 123, 'server_ctx ='),
  S(2, '交付元数据', '服务端线程 · 内核态', '驱动复制事务结构中的请求元数据。', '2.3/kernel/mini_binder.c', 163, 171, 'if (state == RPC_PENDING)'),
  S(3, '读取映射数据', '服务端线程 · 用户态', '服务端直接从映射区解码请求。', '2.3/userspace/transaction_server.c', 49, 57, 'reply.id'),
  S(4, '复制回复', '服务端线程 · 内核态', '服务端回复结构由 copy_from_user 复制进内核。', '2.3/kernel/mini_binder.c', 190, 194, 'if (copy_from_user'),
  S(5, '交付回复', '客户端线程 · 内核态', '驱动将回复填入事务并复制回客户端。', '2.3/kernel/mini_binder.c', 125, 143, 'ret = wait_event_interruptible')
]));

flows.push(flow('docs/02-transaction/2.1-mmap.md', 1, 'sequence', '映射请求流程', [
  'REGISTER_SERVER，分配缓冲区', 'mmap，建立只读映射', 'RECEIVE，等待请求', 'CALL，提交参数',
  'RECEIVE 返回请求元数据', '从映射区读取参数并计算', 'REPLY，提交结果', 'CALL 返回结果'
], [
  S(0, '注册服务端', '服务端线程 · 内核态', '驱动分配并保存请求缓冲区。', '2.1/kernel/mini_binder.c', 44, 63, 'static long register_server'),
  S(1, '建立映射', '服务端线程 · 用户态', '服务端建立只读共享映射。', '2.1/userspace/server.c', 33, 38, 'mapping = mmap'),
  S(2, '等待请求', '服务端线程 · 用户态', '服务端调用 RECEIVE。', '2.1/userspace/server.c', 47, 52, 'if (ioctl'),
  S(3, '提交调用', '客户端线程 · 用户态', '客户端使用 CALL 提交参数。', '2.1/userspace/client.c', 40, 44, 'if (ioctl'),
  S(4, '交付元数据', '服务端线程 · 内核态', '驱动复制请求元数据到服务端。', '2.1/kernel/mini_binder.c', 162, 170, 'if (state == RPC_PENDING)'),
  S(5, '读取映射页', '服务端线程 · 用户态', '服务端验证大小，从映射页读取参数并计算。', '2.1/userspace/server.c', 53, 63, 'if (req.size'),
  S(6, '提交结果', '服务端线程 · 用户态', '服务端携带请求编号提交回复。', '2.1/userspace/server.c', 64, 69, 'if (ioctl'),
  S(7, '返回调用', '客户端线程 · 内核态', '客户端线程取回回复并复制 call 结构。', '2.1/kernel/mini_binder.c', 126, 143, 'ret = wait_event_interruptible')
]));

flows.push(flow('docs/02-transaction/2.1-mmap.md', 2, 'sequence', '请求映射的数据传递', [
  'CALL：copy_from_user 复制 call', 'memcpy 将 payload 写入映射缓冲区', 'RECEIVE：copy_to_user 复制 request 元数据',
  '从映射区直接读取 payload', 'REPLY：copy_from_user 复制 reply', 'CALL 返回：copy_to_user 复制 call'
], [
  S(0, '复制 call', '客户端线程 · 内核态', '驱动复制调用结构。', '2.1/kernel/mini_binder.c', 92, 94, 'if (copy_from_user'),
  S(1, '写入 payload', '客户端线程 · 内核态', '驱动将参数写入服务端映射页。', '2.1/kernel/mini_binder.c', 116, 124, 'server_ctx ='),
  S(2, '交付 request', '服务端线程 · 内核态', '驱动复制 request 元数据。', '2.1/kernel/mini_binder.c', 162, 170, 'if (state == RPC_PENDING)'),
  S(3, '读取 payload', '服务端线程 · 用户态', '服务端从映射页直接读取参数。', '2.1/userspace/server.c', 57, 63, 'payload ='),
  S(4, '复制 reply', '服务端线程 · 内核态', '驱动复制并检查服务端回复。', '2.1/kernel/mini_binder.c', 187, 203, 'if (copy_from_user'),
  S(5, '返回 call', '客户端线程 · 内核态', '驱动填入结果后复制 call 到客户端。', '2.1/kernel/mini_binder.c', 126, 143, 'ret = wait_event_interruptible')
]));

flows.push(flow('docs/02-transaction/2.4-client-mmap.md', 0, 'sequence', '客户端映射回复', [
  '注册并 mmap 服务端页', 'open 并 mmap 客户端页', 'TRANSACT_MAPPED：copy_from_user 事务结构和请求数据',
  'memcpy 请求数据到服务端页', 'copy_to_user 返回事务结构（元数据，data 清零）', '通过 mmap 直接读取请求数据',
  'TRANSACTION_REPLY：copy_from_user 回复结构和数据', 'memcpy 回复数据到客户端页',
  'copy_to_user 返回事务结构和回复长度', '通过 mmap 直接读取回复数据', 'RELEASE_REPLY，允许复用'
], [
  S(0, '映射服务端页', '服务端线程 · 用户态', '服务端注册并映射请求页。', '2.4/userspace/transaction_server.c', 22, 30, 'if (ioctl'),
  S(1, '映射客户端页', '客户端线程 · 用户态', '客户端建立用于读取回复的只读映射。', '2.4/userspace/mapped_client.c', 29, 32, 'mapping = mmap'),
  S(2, '复制请求', '客户端线程 · 内核态', '驱动复制事务结构和其中的请求数据。', '2.4/kernel/mini_binder.c', 95, 98, 'if (copy_from_user'),
  S(3, '写入服务端页', '客户端线程 · 内核态', '驱动将请求载荷写入服务端映射页。', '2.4/kernel/mini_binder.c', 129, 135, 'server_ctx ='),
  S(4, '返回请求元数据', '服务端线程 · 内核态', '驱动交付事务结构；请求数据由映射页提供。', '2.4/kernel/mini_binder.c', 200, 211, 'if (state == RPC_PENDING)'),
  S(5, '读取请求', '服务端线程 · 用户态', '服务端从映射页解码请求。', '2.4/userspace/transaction_server.c', 49, 57, 'reply.id'),
  S(6, '复制回复', '服务端线程 · 内核态', '驱动复制服务端提交的回复结构。', '2.4/kernel/mini_binder.c', 232, 235, 'if (copy_from_user'),
  S(7, '写入客户端页', '客户端线程 · 内核态', '客户端被唤醒后将回复载荷写入自己的映射页。', '2.4/kernel/mini_binder.c', 137, 149, 'ret = wait_event_interruptible'),
  S(8, '返回回复元数据', '客户端线程 · 内核态', '驱动将事务状态和长度复制回客户端。', '2.4/kernel/mini_binder.c', 159, 166, 'if (!ret && copy_to_user'),
  S(9, '读取回复', '客户端线程 · 用户态', '客户端从映射页解码回复。', '2.4/userspace/mapped_client.c', 48, 54, 'if (parcel_read_u32'),
  S(10, '释放回复槽', '客户端线程 · 用户态', '客户端发送 RELEASE_REPLY 允许回复页复用。', '2.4/userspace/mapped_client.c', 56, 59, 'if (ioctl')
]));

flows.push(flow('docs/02-transaction/2.4-client-mmap.md', 1, 'sequence', '映射事务数据传递', [
  'TRANSACT_MAPPED：copy_from_user 复制事务结构', 'memcpy 请求 data 到服务端映射页',
  'RECEIVE_TRANSACTION：copy_to_user 复制事务结构', '从服务端映射页读取请求 Parcel',
  'TRANSACTION_REPLY：copy_from_user 复制事务结构', 'memcpy 回复 data 到客户端映射页',
  'TRANSACT_MAPPED 返回：copy_to_user 复制事务结构', '从客户端映射页读取回复 Parcel', 'RELEASE_REPLY，允许回复槽复用'
], [
  S(0, '复制请求', '客户端线程 · 内核态', '驱动复制事务结构。', '2.4/kernel/mini_binder.c', 95, 98, 'if (copy_from_user'),
  S(1, '写入服务端页', '客户端线程 · 内核态', '请求 data 写入服务端映射页。', '2.4/kernel/mini_binder.c', 129, 135, 'server_ctx ='),
  S(2, '交付请求', '服务端线程 · 内核态', '驱动复制请求元数据。', '2.4/kernel/mini_binder.c', 200, 211, 'if (state == RPC_PENDING)'),
  S(3, '读取请求', '服务端线程 · 用户态', '服务端从映射页解码。', '2.4/userspace/transaction_server.c', 49, 57, 'reply.id'),
  S(4, '复制回复', '服务端线程 · 内核态', '驱动复制回复结构。', '2.4/kernel/mini_binder.c', 232, 235, 'if (copy_from_user'),
  S(5, '写入客户端页', '客户端线程 · 内核态', '驱动写入客户端映射页。', '2.4/kernel/mini_binder.c', 144, 149, 'memcpy(client_ctx->buffer'),
  S(6, '返回回复元数据', '客户端线程 · 内核态', '驱动复制事务结构回客户端。', '2.4/kernel/mini_binder.c', 159, 166, 'if (!ret && copy_to_user'),
  S(7, '读取回复', '客户端线程 · 用户态', '客户端从映射页读取回复。', '2.4/userspace/mapped_client.c', 48, 54, 'if (parcel_read_u32'),
  S(8, '释放回复', '客户端线程 · 用户态', '客户端完成读取后释放回复槽。', '2.4/userspace/mapped_client.c', 56, 59, 'if (ioctl')
]));

flows.push(flow('docs/02-transaction/2.5-command-dispatch.md', 0, 'sequence', '事务命令分派', [
  'RECEIVE_TRANSACTION，等待请求', 'TRANSACT_MAPPED：copy_from_user 复制事务结构',
  'memcpy 请求 Parcel 到服务端映射页，唤醒服务端', 'RECEIVE_TRANSACTION 返回：copy_to_user 复制事务结构',
  '读取映射页中的请求 Parcel', '根据 code 分派到计算函数', 'TRANSACTION_REPLY：copy_from_user 复制回复结构',
  '保存回复并唤醒客户端，memcpy 回复到客户端映射页', 'TRANSACT_MAPPED 返回：copy_to_user 复制事务结构',
  '读取映射页中的回复 Parcel', 'RELEASE_REPLY，允许回复槽复用'
], [
  S(0, '等待请求', '服务端线程 · 用户态', '服务端等待领取事务。', '2.5/userspace/calc_server.c', 73, 76, 'if (ioctl'),
  S(1, '提交请求', '客户端线程 · 内核态', '驱动复制包含 code 的事务结构。', '2.5/kernel/mini_binder.c', 95, 98, 'if (copy_from_user'),
  S(2, '写入映射页', '客户端线程 · 内核态', '驱动保存 code 并写入请求数据。', '2.5/kernel/mini_binder.c', 129, 136, 'server_ctx ='),
  S(3, '返回请求元数据', '服务端线程 · 内核态', '驱动向服务端交付事务元数据。', '2.5/kernel/mini_binder.c', 201, 213, 'if (state == RPC_PENDING)'),
  S(4, '读取 Parcel', '服务端线程 · 用户态', '服务端从映射页解码两个参数。', '2.5/userspace/calc_server.c', 77, 84, 'reply.id'),
  S(5, '按 code 分派', '服务端线程 · 用户态', '服务端按 code 选择加减乘除函数。', '2.5/userspace/calc_server.c', 38, 54, 'static int calculate'),
  S(6, '提交回复', '服务端线程 · 内核态', '驱动复制并保存服务端回复。', '2.5/kernel/mini_binder.c', 234, 237, 'if (copy_from_user'),
  S(7, '写入回复页', '客户端线程 · 内核态', '等待结束后客户端线程将回复载荷写入映射页。', '2.5/kernel/mini_binder.c', 138, 150, 'ret = wait_event_interruptible'),
  S(8, '返回回复元数据', '客户端线程 · 内核态', '驱动将事务状态和长度复制给客户端。', '2.5/kernel/mini_binder.c', 160, 167, 'if (!ret && copy_to_user'),
  S(9, '读取回复', '客户端线程 · 用户态', '客户端从回复映射页读取计算结果。', '2.5/userspace/calc_ipc.h', 31, 33, 'if (parcel_read_u32'),
  S(10, '释放回复', '客户端线程 · 用户态', '客户端发送 RELEASE_REPLY，允许复用回复页。', '2.5/userspace/calc_ipc.h', 35, 36, 'if (ioctl')
]));

flows.push(flow('docs/02-transaction/2.7-concurrent-transactions.md', 0, 'concept', '事务槽状态', [
  'IDLE', 'PENDING', 'PROCESSING', 'DONE'
], [
  S(0, 'IDLE：空闲', '概念示意', '驱动选择空闲槽提交新事务。', '2.7/kernel/mini_binder.c', 191, 208, 'for (i = 0; i < SLOT_COUNT; i++)'),
  S(1, 'PENDING：等待领取', '概念示意', '请求写入槽位后进入 PENDING，服务端可以领取。', '2.7/kernel/mini_binder.c', 202, 208, 'memcpy((char *)server_ctx->buffer'),
  S(2, 'PROCESSING：服务端处理', '概念示意', '服务端领取后状态变为 PROCESSING。', '2.7/kernel/mini_binder.c', 293, 297, 'WRITE_ONCE(slot->state, RPC_PROCESSING)'),
  S(3, 'DONE：回复完成', '概念示意', '匹配回复后状态变为 DONE，客户端取走回复后槽位可复用。', '2.7/kernel/mini_binder.c', 348, 352, 'WRITE_ONCE(slot->state, RPC_DONE)')
]));
