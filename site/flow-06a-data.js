const client = 'examples/ch06a/userspace/mapped_client.c';
const server = 'examples/ch06a/userspace/transaction_server.c';
const kernel = 'examples/ch06a/kernel/mini_binder.c';

// Line anchors make stale tutorial snapshots fail at build time.
export const steps06a = [
  { message: 0, title: '服务端注册并发起映射', context: '服务端线程 · 用户态', detail: '服务端先通过 ioctl 注册，再调用 mmap 请求映射；这两次系统调用会分别进入驱动。', file: server, ranges: [[22, 27]], anchor: [22, 'MINI_BINDER_REGISTER_SERVER'] },
  { message: 0, title: '内核准备服务端缓冲区', context: '服务端线程 · 内核态', detail: '驱动注册服务端并分配请求页；随后 mmap 处理函数把该页映射到服务端。两次系统调用都在发起它们的服务端线程上下文中执行。', file: kernel, ranges: [[47, 60], [68, 85]], anchor: [47, 'register_server'] },
  { message: 1, title: '客户端打开设备并映射', context: '客户端线程 · 用户态', detail: '客户端打开设备，调用 mmap，为接收回复准备一个只读映射。', file: client, ranges: [[24, 30]], anchor: [29, 'mmap('] },
  { message: 1, title: '内核映射客户端缓冲区', context: '客户端线程 · 内核态', detail: '客户端的 mmap 进入驱动：驱动为这个打开文件准备页，并建立只读映射。', file: kernel, ranges: [[68, 85]], anchor: [84, 'remap_vmalloc_range'] },
  { message: 2, title: '编码并提交请求', context: '客户端线程 · 用户态', detail: '客户端把 42 和 Binder 编码成 Parcel，然后调用 TRANSACT_MAPPED 进入内核。', file: client, ranges: [[34, 38]], anchor: [38, 'MINI_BINDER_TRANSACT_MAPPED'] },
  { message: 3, title: '复制请求并通知服务端', context: '客户端线程 · 内核态', detail: '驱动检查请求长度，将字节复制到服务端页，发布 PENDING 状态并唤醒服务端等待队列。', file: kernel, ranges: [[99, 103], [137, 152]], anchor: [140, 'memcpy(server_ctx->buffer'] },
  { message: 3, title: '客户端等待回复', context: '客户端线程 · 内核态', detail: '客户端在线程等待队列上等待 DONE 或 FAILED；等待时线程可能休眠。', file: kernel, ranges: [[154, 155]], anchor: [154, 'wait_event_interruptible(client_wait'] },
  { message: 4, title: '服务端领取请求', context: '服务端线程 · 内核态', detail: 'RECEIVE 可能在请求到来前已经等待；此处展示唤醒后领取 PENDING 请求，并将元数据复制给服务端。', file: kernel, ranges: [[222, 241]], anchor: [229, 'RPC_PROCESSING'] },
  { message: 5, title: '服务端解码并计算', context: '服务端线程 · 用户态', detail: '服务端从自己的映射页读取 Parcel，把 42 加倍为 84，并把结果和字符串编码为回复。', file: server, ranges: [[49, 60]], anchor: [54, 'parcel_read_u32'] },
  { message: 6, title: '服务端提交回复', context: '服务端线程 · 用户态', detail: '服务端调用 TRANSACTION_REPLY，将回复结构交给驱动。', file: server, ranges: [[62, 67]], anchor: [62, 'MINI_BINDER_TRANSACTION_REPLY'] },
  { message: 6, title: '驱动保存回复并唤醒客户端', context: '服务端线程 · 内核态', detail: '驱动复制并检查回复，匹配编号和模式，保存回复并唤醒客户端等待队列。此时还不是客户端线程在执行。', file: kernel, ranges: [[256, 262], [269, 290]], anchor: [283, 'transaction_reply = incoming_transaction'] },
  { message: 7, title: '客户端线程写入回复页', context: '客户端线程 · 内核态', detail: '被唤醒并获得调度后，客户端线程继续执行 call_server，将已保存的回复复制到自己的映射页，标记回复槽已占用。', file: kernel, ranges: [[154, 163]], anchor: [162, 'memcpy(client_ctx->buffer'] },
  { message: 8, title: '返回回复元数据', context: '客户端线程 · 内核态', detail: '驱动释放事务槽，并将事务结构复制回客户端；映射回复的字节在客户端页内，结构中的 data 仍是请求。', file: kernel, ranges: [[175, 186]], anchor: [183, 'copy_to_user(arg, &transaction'] },
  { message: 9, title: '客户端读取并打印回复', context: '客户端线程 · 用户态', detail: 'ioctl 返回后，客户端检查状态和长度，从映射页读取 Parcel，打印 84 和 Binder。', file: client, ranges: [[42, 53]], anchor: [46, 'parcel_init(&p, (void *)mapping'] },
  { message: 10, title: '客户端请求释放回复槽', context: '客户端线程 · 用户态', detail: '客户端用完映射内容后调用 RELEASE_REPLY；之后不能再依赖该回复保持不变。', file: client, ranges: [[55, 59]], anchor: [56, 'MINI_BINDER_RELEASE_REPLY'] },
  { message: 10, title: '驱动允许复用', context: '客户端线程 · 内核态', detail: '驱动在锁内清除 reply_ready；这允许下次回复覆盖该页，本操作不解除映射。', file: kernel, ranges: [[204, 215]], anchor: [213, 'reply_ready = false'] }
];
