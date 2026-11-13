# 第八篇自测：用户态 Binder 库

## 题目

1. IBinder、BBinder、BpBinder 分别负责什么？调用它们是否都要进入驱动？
2. 为什么先在 8.1 做本地调用，而不直接写一个跨进程的类？
3. ProcessState 与 mini_binder_proc 是同一份对象吗？两个线程怎样共享连接？
4. addObject、向管理者注册 LOCAL、内核创建 Node 分别发生在哪一层？
5. IPCThreadState 为什么是 thread_local？线程结束时为什么不关闭共享 fd？
6. 原生类名相同，是否意味着教学库可以使用 /dev/binder 或原生 Android ABI？
7. 为什么教学 Parcel 要复制映射中的回复，再发送 FREE_REPLY？代价是什么？
8. joinThreadPool 会自动创建多少线程？是否支持 A→B→A 的原线程嵌套调用？
9. BpBinder(1) 与 BBinder(cookie=1) 是否表示同一个对象？可以把 handle 直接复制给别的进程吗？
10. 这四章是否修改了驱动？用户态 shared_ptr 是否补齐了内核 Node/Ref 引用计数？

## 参考答案

1. IBinder 定义统一调用入口；BBinder 表示本地实现，直接进入本地 onTransact；BpBinder 保存远端 handle，由当前线程通信类经驱动发送请求。
2. 先理解公共调用与虚函数分发，再引入共享连接、线程收发和远端代理，便于分清每层新增的职责。
3. 不是。前者是用户态 C++ 对象，后者归内核打开文件；本库用一次 open 和共享 ProcessState 对应一个上下文。
4. addObject 登记用户态 cookie 表；注册服务是向 handle 0 提交名称和 LOCAL 的事务；Node 在 LOCAL 发送时由驱动创建或复用。
5. 每线程有自己的通信入口对象，共同引用 ProcessState；TLS 析构清理当前内核线程记录，连接仍供其他线程使用。
6. 不意味着。类名表示职责借鉴，协议、布局、命令与内核生命周期均是教学实现。
7. 回复位置在 FREE_REPLY 后可以复用；独立字节副本使返回 Parcel 继续有效，代价是额外的有界用户态复制。原生外部缓冲区回收机制未实现。
8. 不创建线程，只让当前线程登记 looper 并循环读取。没有事务栈，也不在等待自己的回复时分发同步回调。
9. 不一定，handle 归当前 Ref 表，cookie 归对象拥有者的本地表。跨进程传 HANDLE 对象需经驱动转换。
10. 8.1 无驱动，8.2～8.4 与 7.4 的驱动和共享头逐字相同，协议仍为 0704；shared_ptr 只管理用户态生命周期，没有实现内核强弱引用。
