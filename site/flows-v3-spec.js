// Anchors are resolved against the packaged chapter sources at build time.
const k = (needle, role = '调用线程', occurrence = 0) => ['kernel/mini_binder.c', needle, `${role} · 内核态`, occurrence];
const u = (file, needle, role, occurrence = 0) => [file, needle, `${role} · 用户态`, occurrence];
const c = needle => ['kernel/mini_binder.c', needle, '概念示意 · 内核结构'];
const cu = (file, needle) => [file, needle, '概念示意 · 用户态示例'];
const s = (path, kind, refs) => ({path, kind, refs});

export const v3Specs = [
  s('docs/03-objects/3.1-user-objects.md', 'sequence', [
    u('userspace/object_ipc.h','parcel_init(&p, tx.data','客户端线程'), k('static long receive_request','服务端线程'),
    u('userspace/object_server.c','parcel_init(&input','服务端线程'), u('userspace/object_server.c','objects[i].invoke','服务端线程'),
    u('userspace/object_server.c','exchange_command(fd, &outgoing','服务端线程'), k('static long read_command','客户端线程'),
    u('userspace/object_ipc.h','parcel_read_u32(&p, &bits_result','客户端线程'), u('userspace/object_ipc.h','exchange_command(fd, &release','客户端线程')]),
  s('docs/03-objects/3.4-node-to-ref.md', 'sequence', [
    u('tests/ref_test.c','MINI_CALC_GET_OBJECT, 0, 0','客户端线程'), k('static long receive_request','服务端线程'),
    u('userspace/object_server.c','parcel_write_u32(&output, MINI_BINDER_OBJECT_LOCAL','服务端线程'), k('static long translate_reply','客户端线程'),
    k('ret = translate_reply(client_ctx','客户端线程'), u('tests/ref_test.c','MINI_CALC_ADD, ha, 7, 3','客户端线程'),
    k('transaction.object_cookie = nodes[i].cookie','客户端线程'), u('userspace/object_server.c','objects[i].invoke','服务端线程'), k('static long read_command','客户端线程')]),
  s('docs/03-objects/3.5-multiple-servers.md', 'concept', [c('if (transaction.target_handle)'), c('__u64 refs['), c('struct mini_binder_node {'), c('struct file *owner;'), cu('userspace/object_server.c','objects[i].invoke')]),
  s('docs/03-objects/3.6-transfer-ref.md', 'sequence', [
    u('tests/transfer_test.c','req.transaction.code = 5','发送方 A'), k('static long receive_request','对象提供者线程'),
    u('tests/transfer_test.c','put32(reply.transaction.data, MINI_BINDER_OBJECT_LOCAL)','对象提供者'), k('static long translate_reply','发送方 A 线程'),
    u('tests/transfer_test.c','put32(req.transaction.data + 4, 2)','发送方 A'), k('static long translate_request','发送方 A 线程'),
    k('delivered.object_count = slot->request.object_count','接收方 B 线程'), u('tests/transfer_test.c','call.transaction.target_handle = get32(data + 4)','接收方 B'),
    u('tests/transfer_test.c','assert(reply.transaction.status == 0 && reply.transaction.size == 0)','发送方 A'),
    u('tests/transfer_test.c','transact(fd, &call, &result)','接收方 B'), k('transaction.object_cookie = nodes[i].cookie','接收方 B 线程'),
    u('tests/transfer_test.c','put32(reply.transaction.data, 99)','对象提供者'), u('tests/transfer_test.c','get32(mapping) == 99','接收方 B')]),
  s('docs/04-concurrency/4.1-binder-proc.md', 'concept', [c('static int mini_binder_open'),c('struct mini_binder_proc {'),c('struct mini_binder_ref {'),c('struct mini_binder_node {'),c('struct mini_binder_proc *owner;'),cu('userspace/object_server.c','objects[i].invoke')]),
  s('docs/04-concurrency/4.2-service-manager.md', 'sequence', [
    u('tests/manager_test.c','MINI_BINDER_REGISTER_MANAGER) == 0','管理者'), u('tests/manager_test.c','req.transaction.code = 1;','服务端'),
    k('ret = translate_objects(file, server_ctx','服务端线程'), u('tests/manager_test.c','put_name(req.transaction.data, "calculator")','客户端', 1),
    k('static long receive_request','管理者线程'), u('tests/manager_test.c','put32(reply.transaction.data + 4, saved)','管理者'),
    k('ret = translate_objects(slot->receiver, client_ctx','客户端线程'), u('tests/manager_test.c','req.transaction.target_handle = 1; req.transaction.code = 3','客户端'),
    k('transaction.object_cookie = nodes[i].cookie','客户端线程')]),
  s('docs/04-concurrency/4.3-binder-thread.md', 'concept', [c('struct mini_binder_proc {'),c('struct mini_binder_thread {'),c('struct mini_binder_thread threads[8]'),c('struct mini_binder_ref refs[')]),
  s('docs/04-concurrency/4.4-proc-todo.md', 'sequence', [
    u('tests/workers_test.c','ioctl(fd, MINI_BINDER_WRITE, &req)','客户端'), k('list_add_tail(&slot->work','客户端线程'), k('wake_up_interruptible_all(&server_wait)','客户端线程', 1),
    u('tests/workers_test.c','ioctl(worker->fd, MINI_BINDER_READ, &req)','服务线程'), k('list_first_entry(&ctx->todo','服务线程'), k('delivered.id = slot->request.id','服务线程'),
    u('tests/workers_test.c','ioctl(worker->fd, MINI_BINDER_WRITE, &reply)','服务线程'), k('wake_up_interruptible(&client_wait)','服务线程', 1)]),
  s('docs/04-concurrency/4.5-ref-back-to-owner.md', 'sequence', [
    u('tests/return_to_owner_test.c','put_object(call.transaction.data, MINI_BINDER_OBJECT_LOCAL','拥有者 A'), k('static long translate_objects','拥有者 A 线程'),
    u('tests/return_to_owner_test.c','put_object(reply.transaction.data, MINI_BINDER_OBJECT_HANDLE','进程 B'), k('if (nodes[j].owner == receiver)','拥有者 A 线程'),
    u('tests/return_to_owner_test.c','get64(mapping + 4) == cookie','拥有者 A'), u('tests/return_to_owner_test.c','put_object(call.transaction.data, MINI_BINDER_OBJECT_HANDLE','进程 B'),
    k('if (nodes[j].owner == receiver)','进程 B 线程'), u('tests/return_to_owner_test.c','returned->invoke(21)','拥有者 A'),
    u('tests/return_to_owner_test.c','put32(reply.transaction.data, (uint32_t)returned->invoke(21))','拥有者 A'), u('tests/return_to_owner_test.c','get32(mapping) == 42','进程 B')]),
  s('docs/05-threading/5.1-thread-replies.md', 'sequence', [
    u('tests/thread_reply_test.c','ioctl(worker->fd, MINI_BINDER_WRITE, &call)','客户端线程 A'), u('tests/thread_reply_test.c','ioctl(worker->fd, MINI_BINDER_WRITE, &call)','客户端线程 B'),
    u('tests/thread_reply_test.c','ioctl(fd, MINI_BINDER_READ, &received[i])','服务端线程'), u('tests/thread_reply_test.c','for (int i = 1; i >= 0; i--)','服务端线程'),
    u('tests/thread_reply_test.c','ioctl(fd, MINI_BINDER_WRITE, &reply)','服务端线程'), k('transaction.reply_offset =','客户端线程 B'), k('transaction.reply_offset =','客户端线程 A'),
    u('tests/thread_reply_test.c','ioctl(worker->fd, MINI_BINDER_WRITE, &release)','客户端线程 A'), u('tests/thread_reply_test.c','ioctl(worker->fd, MINI_BINDER_WRITE, &release)','客户端线程 B')]),
  s('docs/05-threading/5.2-thread-todo.md', 'sequence', [
    u('tests/thread_reply_test.c','ioctl(worker->fd, MINI_BINDER_WRITE, &call)','客户端线程 A'), u('tests/thread_reply_test.c','ioctl(worker->fd, MINI_BINDER_WRITE, &call)','客户端线程 B'),
    k('list_add_tail(&slot->work','客户端线程'), u('tests/thread_reply_test.c','ioctl(fd, MINI_BINDER_READ, &received[i])','服务端线程'),
    u('tests/thread_reply_test.c','for (int i = 1; i >= 0; i--)','服务端线程'), k('list_add_tail(&slot->reply_work','服务端线程'), k('wake_up_interruptible(&slot->caller->wait)','服务端线程', 1),
    u('tests/thread_reply_test.c','ioctl(fd, MINI_BINDER_WRITE, &reply)','服务端线程'), k('list_add_tail(&slot->reply_work','服务端线程'), k('wake_up_interruptible(&slot->caller->wait)','服务端线程', 1)]),
  s('docs/05-threading/5.3-thread-exit.md', 'concept', [c('struct mini_binder_proc {'),c('struct mini_binder_thread {'),c('struct pid *pid;'),c('struct list_head todo;'),c('unsigned int reply_index;'),c('struct mini_binder_ref refs[')]),
  s('docs/05-threading/5.4-client-objects.md', 'sequence', [
    u('tests/role_test.c','ioctl(fd, MINI_BINDER_CREATE_NODE, &node)','进程 A', 1), u('tests/role_test.c','ioctl(fd, MINI_BINDER_CREATE_NODE, &node)','进程 B'),
    u('tests/role_test.c','put32(call.transaction.data, MINI_BINDER_OBJECT_LOCAL)','进程 A'), k('ret = translate_objects(file, server_ctx','进程 A 线程'),
    u('tests/role_test.c','reply.transaction.id = req.transaction.id','进程 B'), k('thread->pending_reply = transaction','进程 A 线程'),
    u('tests/role_test.c','call.transaction.target_handle = get32(payload + 4)','进程 B'), k('transaction.object_cookie = nodes[i].cookie','进程 B 线程'),
    u('tests/role_test.c','local.invoke((int)get32(payload))','进程 A'), u('tests/role_test.c','ioctl(fd, MINI_BINDER_WRITE, &reply)','进程 A', 1),
    u('tests/role_test.c','get32(mapping + result.transaction.reply_offset) == 14','进程 B')]),
  s('docs/06-file-descriptors/6.1-file-descriptors.md', 'concept', [cu('fd_demo.c','fd = mkstemp(path)'),cu('fd_demo.c','fd = mkstemp(path)'),cu('fd_demo.c','child = fork()'),cu('fd_demo.c','duplicate = dup(fd)'),cu('fd_demo.c','independent = open(path'),cu('fd_demo.c','independent = open(path'),cu('fd_demo.c','unlink(path)')]),
  s('docs/06-file-descriptors/6.2-fd-request.md', 'sequence', [
    u('tests/mixed_object_test.c','sender.call.transaction.object_count = 2','发送进程 A'), k('type == MINI_BINDER_OBJECT_LOCAL','发送线程'),
    k('fd_file = fget(','发送线程'), u('tests/mixed_object_test.c','assert(close(data_fd) == 0)','发送进程 A'),
    u('tests/mixed_object_test.c','ioctl(binder, MINI_BINDER_READ, &req)','接收进程 B'), k('fd_install(reserved[i]','接收线程'),
    k('delivered.object_count = slot->request.object_count','接收线程'), u('tests/mixed_object_test.c','read(received_fd, result.data','接收进程 B')]),
  s('docs/06-file-descriptors/6.3-fd-lifetime.md', 'concept', [c('fd_file = fget('),c('request_slot->fds[request_slot->fd_count].file'),c('fd_install(reserved[i]'),c('drop_request_fds('),cu('tests/mixed_object_test.c','close(received_fd)')]),
  s('docs/06-file-descriptors/6.4-fd-reply.md', 'sequence', [
    u('tests/fd_reply_test.c','ioctl(binder, MINI_BINDER_WRITE, &call)','客户端线程'), k('static long receive_request','服务端线程'),
    u('tests/fd_reply_test.c','ioctl(binder, MINI_BINDER_WRITE, &invalid) == 0','服务端线程'), k('thread->pending_reply = transaction','客户端线程'),
    u('tests/fd_reply_test.c','close(data_fd) == 0','服务端线程'), u('tests/fd_reply_test.c','reply.cmd = MINI_READ_REPLY','客户端线程'),
    k('install_fds(&thread->reply_fds','客户端线程'), u('tests/fd_reply_test.c','read(received, data, 2)','客户端线程')])
];
