export function initTerminal({ files, icon, paintIcons, onBuildChange }) {
  const $ = id => document.getElementById(id);
  const terminal = $('terminal');
  const output = $('terminal-output');
  const input = $('terminal-input');
  const sessions = [0, 1, 2].map((_, index) => ({ cwd: '/', visited: index === 0, lines: [], history: [], historyIndex: 0, server: null, lastStatus: 0 }));
  const built = new Map();
  const logs = [];
  let active = 0;
  let loaded = null;
  let chapterDir = '/examples';
  const directorySet = new Set(['/']);
  for (const path of Object.keys(files)) {
    const parts = path.split('/'); parts.pop();
    while (parts.length) { directorySet.add(`/${parts.join('/')}`); parts.pop(); }
  }

  $('terminal-icon').innerHTML = icon('terminal-square');
  $('terminal-chapter').innerHTML = icon('folder-input');
  $('terminal-clear').innerHTML = icon('eraser');
  $('terminal-collapse').innerHTML = icon('chevron-down');
  paintIcons();

  function normalize(path, base = sessions[active].cwd) {
    if (path === '~') return '/';
    const inputPath = path.startsWith('~/') ? `/${path.slice(2)}` : path;
    const parts = (inputPath.startsWith('/') ? inputPath : `${base}/${inputPath}`).split('/');
    const result = [];
    for (const part of parts) {
      if (!part || part === '.') continue;
      if (part === '..') result.pop(); else result.push(part);
    }
    return `/${result.join('/')}`;
  }
  function line(text, kind = '') {
    sessions[active].lines.push({ text, kind });
    if (sessions[active].lines.length > 400) sessions[active].lines.shift();
  }
  function prompt() { return `${sessions[active].cwd} $`; }
  function render() {
    const session = sessions[active];
    output.replaceChildren();
    for (const item of session.lines) {
      const row = document.createElement('div'); row.className = `terminal-line ${item.kind}`; row.textContent = item.text; output.append(row);
    }
    $('terminal-prompt').textContent = session.server ? '运行中' : prompt();
    input.placeholder = session.server ? '服务端运行中，按 Ctrl+C 停止' : '输入 help 查看命令';
    input.setAttribute('aria-label', session.server ? '服务端运行中，按 Ctrl+C 停止' : '模拟终端命令');
    document.querySelectorAll('.terminal-tab').forEach((tab, index) => {
      tab.classList.toggle('active', index === active);
      tab.setAttribute('aria-selected', String(index === active));
      tab.querySelector('span').textContent = sessions[index].server ? '运行中' : '';
    });
    output.scrollTop = output.scrollHeight;
  }
  function error(text) { sessions[active].lastStatus = 1; line(text, 'error'); }
  function currentBuild() { return built.get(sessions[active].cwd); }
  function getArtifacts() {
    const artifacts = [];
    for (const [cwd, targets] of built) {
      const base = cwd.slice(1);
      if (targets.has('*')) {
        if (base === 'examples/0.4') artifacts.push(`${base}/binderabc_hello.ko`);
        else {
          const makefile = files[`${base}/Makefile`] || '';
          const targetLine = makefile.match(/^userspace:\s*(.*)$/m)?.[1] ?? makefile.match(/^all:\s*(.*)$/m)?.[1] ?? '';
          for (const match of targetLine.matchAll(/\bbuild\/[\w.-]+/g)) artifacts.push(`${base}/${match[0]}`);
          if (files[`${base}/kernel/mini_binder.c`]) artifacts.push(`${base}/kernel/mini_binder.ko`);
        }
      } else {
        for (const target of targets) artifacts.push(`${base}/${target}`);
      }
    }
    return artifacts.sort();
  }
  function reportBuild(changedDir) { onBuildChange(getArtifacts(), changedDir.slice(1)); }
  function hasBinary(name, cwd = sessions[active].cwd) {
    const build = built.get(cwd);
    if (!build) return false;
    const base = cwd.slice(1);
    const makefile = files[`${base}/Makefile`] || '';
    const exists = makefile.includes(`build/${name}`);
    return exists && (build.has('*') || build.has(`build/${name}`));
  }
  function list(path) {
    const directory = normalize(path || '.');
    if (!directorySet.has(directory) && !(directory.endsWith('/build') && built.has(directory.slice(0, -6)))) return error(`ls: ${path}: 没有这个目录`);
    const prefix = directory === '/' ? '/' : `${directory}/`;
    const names = new Set();
    for (const dir of directorySet) if (dir.startsWith(prefix) && dir !== directory) names.add(dir.slice(prefix.length).split('/')[0] + '/');
    for (const file of Object.keys(files)) {
      const full = `/${file}`;
      if (full.startsWith(prefix)) { const rest = full.slice(prefix.length); if (!rest.includes('/')) names.add(rest); }
    }
    for (const file of getArtifacts()) {
      const full = `/${file}`;
      if (full.startsWith(prefix)) {
        const rest = full.slice(prefix.length);
        names.add(rest.includes('/') ? `${rest.split('/')[0]}/` : rest);
      }
    }
    const build = built.get(directory);
    if (build && directory !== '/examples/0.4') names.add('build/');
    if (build && directory === '/examples/0.4') names.add('binderabc_hello.ko');
    if (directory.endsWith('/build')) {
      const base = directory.slice(0, -6);
      const makefile = files[`${base.slice(1)}/Makefile`] || '';
      for (const match of makefile.matchAll(/\bbuild\/([\w.-]+)/g)) {
        const name = match[1];
        if (built.get(base).has('*') || built.get(base).has(`build/${name}`)) names.add(name);
      }
    }
    line([...names].sort().join('  ') || '(空目录)');
  }
  function make(args) {
    const cwd = sessions[active].cwd;
    const makefile = files[`${cwd.slice(1)}/Makefile`];
    if (!makefile) return error('make: 当前目录没有 Makefile');
    if (!args.length || args[0] === 'all') { built.set(cwd, new Set(['*'])); reportBuild(cwd); return line('模拟：构建完成。没有编译真实文件。', 'info'); }
    if (args.length === 1 && args[0] === 'clean') { built.delete(cwd); reportBuild(cwd); return line('模拟：已清理构建状态。', 'info'); }
    if (args.every(arg => /^build\/[\w.-]+$/.test(arg) && (makefile.includes(arg) || files[`${cwd.slice(1)}/userspace/${arg.slice(6)}.c`] || files[`${cwd.slice(1)}/tests/${arg.slice(6)}.c`]))) {
      const entries = built.get(cwd) || new Set(); args.forEach(arg => entries.add(arg)); built.set(cwd, entries);
      reportBuild(cwd);
      return line(`模拟：已构建 ${args.join(' ')}。`, 'info');
    }
    error(`make: 不支持的模拟目标 ${args.join(' ')}`);
  }
  function moduleCommand(args) {
    const cwd = sessions[active].cwd;
    if (args[0] === 'insmod' && args.length === 2) {
      const isHello = cwd === '/examples/0.4' && /^\.?\/?binderabc_hello\.ko$/.test(args[1]);
      const isBinder = !!files[`${cwd.slice(1)}/kernel/mini_binder.c`] && args[1] === 'kernel/mini_binder.ko';
      if (!isHello && !isBinder) return error('insmod: 模拟器不支持该模块路径');
      if (!currentBuild()?.has('*')) return error('insmod: 未模拟构建模块，请先运行 make');
      if (loaded) return error('insmod: 已有模拟模块加载，请先卸载');
      loaded = { cwd, name: isHello ? 'binderabc_hello' : 'mini_binder' };
      logs.push(isHello ? 'binderabc_hello: loaded' : 'mini_binder: registered');
      return line(`模拟：已加载 ${loaded.name}。`, 'info');
    }
    if (args[0] === 'rmmod' && args.length === 2) {
      if (!loaded || loaded.name !== args[1]) return error('rmmod: 模块未加载');
      if (sessions.some(session => session.server)) return error('rmmod: 服务端仍在运行，请先在对应终端按 Ctrl+C');
      logs.push(loaded.name === 'binderabc_hello' ? 'binderabc_hello: unloaded' : 'mini_binder: unregistered');
      loaded = null; return line('模拟：已卸载模块。', 'info');
    }
    error('sudo: 仅支持模拟 insmod、rmmod 和 dmesg');
  }
  const versions = { '1.2': '1', '1.3': '3', '1.4': '4', '2.1': '0201', '2.3': '0203', '2.4': '0204', '2.5': '0205', '2.6': '0206', '2.7': '0207' };
  for (const part of [3, 4, 5, 6, 7]) for (let chapter = 1; chapter <= 7; chapter++) versions[`${part}.${chapter}`] = `0${part}0${chapter}`;
  for (const chapter of ['8.2', '8.3', '8.4']) versions[chapter] = '0704';
  const chapterTests = {
    '8.1': { local_demo: 'local BBinder add(7, 3) = 10\nlocal BBinder sub(7, 3) = 4', local_test: 'local dispatch, malformed arguments and reused C Parcel boundaries passed' },
    '8.2': { process_demo: 'two threads: one ProcessState, shared device fd; protocol=0704\ncookie 123 -> local BBinder: add(7, 3) = 10' },
    '3.1': { object_test: 'two objects, invalid target and code, recovery passed' },
    "3.3": { object_parcel_test: "Parcel: 32 bytes, 2 objects\noffsets[0]=4 -> LOCAL, value=0x1122334455667788\noffsets[1]=20 -> HANDLE, value=0x9\nordinary integers at offsets 0 and 16 are not object entries\nregression: reject overlap, out-of-range offsets, invalid type and insufficient space\nobject offsets, decoding, malformed data and rollback passed" },
    "3.4": { ref_test: "file A: local add object -> handle 1, add(7, 3) = 10\nfile B: local multiply object -> handle 1, mul(7, 3) = 21\nsame handle number, separate open files, different Nodes\nregression: invalid handle, malformed object table and a fresh process\nper-file handles, node conversion and calls passed" },
    '3.5': { multi_server_test: 'handle 1 -> server 1: 1\nhandle 2 -> server 2: 2\ntwo servers: routing and exit isolation passed' },
    '3.6': { local_transfer_test: 'local Node -> receiver Ref 1 -> original object: 77', transfer_test: 'handle 2 translated to recipient handle 1 and invoked successfully' },
    '4.1': { ref_test: 'file A: handle 1, add(7, 3) = 10\nfile B: handle 1, mul(7, 3) = 21\ndup A: handle 1, add(8, 4) = 12\nindependent proc refs and dup shared refs passed' },
    "4.4": { manager_test: "manager: handle 0 ready\nprovider: calculator registered with manager\nclient: get calculator through handle 0 -> local handle 1\nclient: call handle 1 -> result 42\nregression: duplicate manager, malformed query, missing name and process exits\nservice calculator: registered, found and called (result 42)\nmanager handle 0: add, get, call and manager exit passed" },
    '5.1': { thread_test: 'proc A: main thread 1, workers 2 and 3\ndup A: thread 1; proc B: thread 1\none proc, distinct thread records; dup shares proc passed' },
    '5.2': { workers_test: 'two client requests queued before workers started\nthread 1 handled request 1: 10 -> 11\nthread 2 handled request 2: 11 -> 12\ntwo server threads receive separate queued requests' },
    '4.2': { return_to_owner_test: 'reply Ref -> owner local object: 42\nrequest Ref -> owner local object: 42' },
    '5.3': { thread_reply_test: 'thread input 10: reply 11 at offset 512\nthread input 20: reply 21 at offset 768\nreverse replies, thread isolation, full slots and reuse passed' },
    '5.4': { thread_reply_test: 'thread 1: 10 -> 11, offset 512\nthread 2: 20 -> 21, offset 768\nreverse replies, thread isolation, full slots and reuse passed', target_wakeup_test: 'reply -> binder thread=2: target wake events 1; other wake events 0\nreply -> binder thread=1\nboth threads received their own replies' },
    '4.3': { role_test: 'ordinary client passed a local object and handled its call: twice(7) = 14' },
    '6.1': { fd_demo: 'fd 3 and dup 5 share an open file\nchild via dup: ab; parent via fd: cd\nindependent open: ab' },
    '6.2': { fd_request_demo: 'receiver fd 4 read: data\nrequest fd delivered', mixed_object_test: 'sender fd 3 -> receiver fd 4; binder handle 1\nreceiver read after sender closed: data' },
    "6.3": { fd_failure_test: "receiver: one free fd=63, request contains two fd objects\nreceiver: READ returned EMFILE; second reservation failed\nreceiver: open reused fd=63; first reservation rolled back\nsender: WRITE returned EPIPE; delivery failed\nsender: both pipes reached EOF; driver holds no write-end references\ninvalid fd, queued cancellation and receiver death released file references\nfd-table exhaustion and copy failure rolled back reserved descriptors" },
    '6.4': { fd_reply_demo: 'reply fd 4 read: data', fd_reply_test: 'reply fds 62 and 63 after EFAULT and EMFILE retries: data\nunread fd replies released by thread exit, free and file close\nsubsequent transaction returned 55' }
  };
  const demoOutputs = {
    '4.4': { manager_test: chapterTests['4.4'].manager_test.replace(/regression:.*\n/, '').replace(/\nmanager handle 0:.*$/, '') },
    '5.1': { thread_test: 'proc A: main thread 1, workers 3 and 2\ndup A: thread 1; proc B: thread 1\none proc, distinct thread records; dup shares proc passed' },
    '5.3': { thread_reply_test: chapterTests['5.3'].thread_reply_test.replace('reverse replies, thread isolation, full slots and reuse passed', 'reverse replies and thread isolation passed') },
    '5.4': { thread_reply_test: chapterTests['5.4'].thread_reply_test.replace('reverse replies, thread isolation, full slots and reuse passed', 'reverse replies and thread isolation passed') },
  };
  for (const ch of ['7.1', '7.2', '7.3']) chapterTests[ch] = {
    object_demo: 'GET_OBJECT 1: LOCAL(cookie=77) -> client handle 1\nGET_OBJECT 2: LOCAL(cookie=77) -> client handle 1\nhandle 1: sub(7, 3) = 4\nsame local object reused; handle call passed',
    protocol_test: 'failed translations rolled back; eight Nodes fit; repeated objects reuse handles\nhandle returned to owner as LOCAL; reply reused original handle\nrequest/reply fds, EFAULT and EMFILE retry passed',
    manager_test: 'service calculator: registered, found and called (result 42)\nmanager handle 0: add, get, call and manager exit passed',
    mapping_test: 'one read-only mapping, address-space check and remapping passed',
    stale_reply: 'cancelled reply discarded; next client received correct result',
    server_death: 'server death wakes both clients with EPIPE',
    parcel_binding_test: 'Parcel wire compatibility, sequential objects and boundary checks passed'
  };
  Object.assign(chapterTests['7.3'], {
    looper_demo: 'ordinary READ: BR_REPLY, result=11\nBC_ENTER_LOOPER: current thread can receive process requests\nsame READ: BR_TRANSACTION, cookie=77\nBC_REPLY: reverse caller received 22; BC_EXIT_LOOPER completed',
    looper_test: "ordinary caller: unified READ returned BR_REPLY without entering looper\nordinary reader: did not steal queued request or another thread's reply\nlooper READ 1: BR_REPLY, result=11 (own reply has priority)\nlooper READ 2: BR_TRANSACTION, cookie=77\nENTER/EXIT checks, interrupted READ and unmap wakeup passed"
  });
  chapterTests['7.4'] = {
    write_read_test: 'write-only returned before server received request\nread retry preserved write_consumed: each request submitted exactly once\nclosing callers before receive reclaimed pending slots\nclosing callers after receive detached pointers; late replies rejected\nthread-exit cancellation and completed reply after server close passed',
    parcel_binding_test: 'name + LOCAL Parcel matches existing wire format; sequential decoding passed\nHANDLE/FD codec, malformed offsets, truncated objects and failed-write rollback passed'
  };
  Object.assign(chapterTests['5.4'] ||= {}, {
    thread_reply_test: "thread 1: 10 -> 11, offset 512\nthread 2: 20 -> 21, offset 768\nreverse replies, thread isolation, full slots and reuse passed",
  });
  Object.assign(chapterTests['5.4'] ||= {}, {
    thread_reply_test: "thread 2: 10 -> 11, offset 768\nthread 1: 20 -> 21, offset 512\nreverse replies, thread isolation, full slots and reuse passed",
    service_exit_test: "service thread exit: waiting caller received EPIPE\ncancelled transaction and repeated thread records reclaimed\nnext service thread: reply = 42; slots reused",
  });
  chapterTests['5.2'].service_threads_demo = "one service fd and mapping; two service threads sleeping in READ\nround 1, worker 1: 10 -> 11\nround 1, worker 2: 20 -> 21\nround 2, worker 1: 11 -> 12\nround 2, worker 2: 21 -> 22\ntwo service threads completed four calls; callers received their own replies";
  chapterTests['5.4'].service_threads_demo = chapterTests['5.2'].service_threads_demo;
  chapterTests['5.4'].thread_exit_test = '12 exited threads reused record; reply offset 512 reused\nexplicit thread exit and reply cleanup passed';
  demoOutputs['5.4'].service_exit_test = "service thread exit: waiting caller received EPIPE\nnext service thread: reply = 42; slots reused";
  Object.assign(chapterTests['6.2'] ||= {}, {
    thread_reply_test: "thread 1: 10 -> 11, offset 512\nthread 2: 20 -> 21, offset 768\nreverse replies, thread isolation, full slots and reuse passed",
    service_exit_test: "service thread exit: waiting caller received EPIPE\ncancelled transaction and repeated thread records reclaimed\nnext service thread: reply = 42; slots reused",
  });
  Object.assign(chapterTests['6.3'] ||= {}, {
    thread_reply_test: "thread 1: 10 -> 11, offset 512\nthread 2: 20 -> 21, offset 768\nreverse replies, thread isolation, full slots and reuse passed",
    service_exit_test: "service thread exit: waiting caller received EPIPE\ncancelled transaction and repeated thread records reclaimed\nnext service thread: reply = 42; slots reused",
  });
  Object.assign(chapterTests['6.4'] ||= {}, {
    thread_reply_test: "thread 2: 10 -> 11, offset 768\nthread 1: 20 -> 21, offset 512\nreverse replies, thread isolation, full slots and reuse passed",
    service_exit_test: "service thread exit: waiting caller received EPIPE\ncancelled transaction and repeated thread records reclaimed\nnext service thread: reply = 42; slots reused",
  });
  Object.assign(chapterTests['7.1'] ||= {}, {
    service_exit_test: "service thread exit: waiting caller received EPIPE\ncancelled transaction and repeated thread records reclaimed\nnext service thread: reply = 42; slots reused",
  });
  Object.assign(chapterTests['7.2'] ||= {}, {
    service_exit_test: "service thread exit: waiting caller received EPIPE\ncancelled transaction and repeated thread records reclaimed\nnext service thread: reply = 42; slots reused",
  });
  Object.assign(chapterTests['7.3'] ||= {}, {
    service_exit_test: "service thread exit: waiting caller received EPIPE\ncancelled transaction and repeated thread records reclaimed\nnext service thread: reply = 42; slots reused",
  });
  Object.assign(chapterTests['7.4'] ||= {}, {
    thread_reply_test: "thread 1: 10 -> 11, offset 512\nthread 2: 20 -> 21, offset 768\nreverse replies, thread isolation, full slots and reuse passed",
    service_exit_test: "service thread exit: waiting caller received EPIPE\ncancelled transaction and repeated thread records reclaimed\nnext service thread: reply = 42; slots reused",
  });
  function serviceLog(role, text) {
    const session = sessions.find(item => item.server === role);
    if (session) session.lines.push({ text, kind: '' });
  }
  function program(name, args, cwd = sessions[active].cwd) {
    const ch = cwd.split('/').at(-1);
    if (!hasBinary(name, cwd)) return error(`${name}: 未模拟构建该程序`);
    const introductoryPrograms = {
      'p01-call': 'add(1, 2) = 3',
      'p01-fork': 'child:  pid=1235, value=20, address=0xfffff1234560\nparent: pid=1234, value=10, address=0xfffff1234560',
      'p02-syscalls': 'Hello from userspace\nopen failed: errno=2 (No such file or directory)',
      '1.1-rpc-pipe': 'remote add(1, 2) = 3'
    };
    if (['0.1', '0.2', '1.1'].includes(ch) && name in introductoryPrograms && !args.length) return line(introductoryPrograms[name]);
    if (name === 'parcel_demo' && args.length === 0) return line('encoded size: 16 bytes\ndecoded: number=42, text=Binder');
    if (name === 'parcel_test' && args.length === 0) { line('模拟回放：未实际运行测试。', 'info'); return line('parcel tests passed'); }
    if (['3.3', '6.1', '8.1'].includes(ch) && name in (chapterTests[ch] || {}) && !args.length) {
      line('模拟回放：未实际运行程序。', 'info'); return line(chapterTests[ch][name]);
    }
    if (!loaded || loaded.name !== 'mini_binder') return error('open /dev/mini_binder: 模拟设备未加载');
    if (loaded.cwd !== cwd) return error('模拟驱动版本与当前章节不匹配，请卸载后加载对应章节模块');
    if (args.length === 1 && args[0] === '--demo' && demoOutputs[ch]?.[name]) {
      if (sessions.some(session => session.server)) return error('请先停止普通服务端');
      line('模拟回放：未实际运行程序。', 'info');
      return line(demoOutputs[ch][name]);
    }
    if (['7.4', '8.3', '8.4'].includes(ch) && ['servicemanager', 'calculator_server', 'calculator_client', 'service_parcel_test', 'framework_server', 'framework_client', 'framework_test'].includes(name)) {
      const manager = sessions.find(item => item.server === 'manager');
      const provider = sessions.find(item => item.server === 'provider');
      if (name === 'servicemanager') {
        if (args.length || manager) return error('ServiceManager: 参数错误或管理者已存在');
        sessions[active].server = 'manager';
        return line('ServiceManager ready: handle 0');
      }
      if (!manager) return error('ServiceManager 尚未启动');
      if (name === 'calculator_server' || name === 'framework_server') {
        if (args.length) return error('calculator_server: 不支持的参数');
        if (provider) return error('addService failed: -17');
        sessions[active].server = 'provider';
        sessions[active].framework = name === 'framework_server';
        serviceLog('manager', 'addService(calculator): manager handle=1');
        return line(name === 'framework_server' ? 'BBinder calculator registered: LOCAL(cookie=123)' : 'calculator registered: LOCAL(cookie=123)');
      }
      if (name === 'framework_test') {
        if (args.length || !provider) return error('请先启动 framework_server');
        line('模拟回放：未实际运行测试。', 'info');
        return line('local BBinder::transact -> onTransact: 10\nunknown code and truncated Parcel rejected; owned reply survived buffer reuse\ntwo calling threads: same ProcessState/fd, distinct IPCThreadState and correct replies\n12 successive calling threads: TLS destruction reclaimed kernel thread records\nC++ Parcel reused the C object codec, including 64-bit cookies and capacity checks');
      }
      if (name === 'service_parcel_test') {
        if (args.length || !provider) return error('请先启动 calculator_server');
        line('模拟回放：未实际运行测试。', 'info');
        return line('Parcel RPC: truncated name/arguments and trailing arguments rejected; valid call recovered');
      }
      if (args.length > 1) return error('calculator_client: 不支持的参数');
      if ((args[0] && args[0] !== 'calculator') || !provider) return error(`getService(${args[0] || 'calculator'}): -2`);
      serviceLog('manager', 'getService(calculator): return HANDLE object');
      if (!provider.framework) serviceLog('provider', 'add(7, 3) = 10\nsub(7, 3) = 4');
      if (name === 'framework_client') {
        const kind = ch === '8.3' ? 'IPCThreadState' : 'BpBinder';
        line('模拟回放：未实际运行程序。', 'info');
        return line(`${kind} calculator: handle=1\n${kind} add(7, 3) = 10\n${kind} sub(7, 3) = 4`);
      }
      return line('getService(calculator): client handle=1\nremote add(7, 3) = 10\nremote sub(7, 3) = 4');
    }
    if (name === 'version') {
      if (!args.length) return line(`mini-binder protocol version: ${versions[ch]}`);
      if (args.length === 1 && args[0] === '--invalid') return line('unknown ioctl rejected: ENOTTY');
      if (args.length === 1 && args[0] === '--bad-address') return line('invalid user address rejected: EFAULT');
      return error('version: 不支持的参数');
    }
    if (name === 'server' || name === 'transaction_server' || name === 'calc_server' || name === 'object_server') {
      if (args.length) return error('服务端不接受参数');
      if (sessions.some(session => session.server)) return error('register server: 模拟服务端已注册');
      sessions[active].server = name === 'server' ? 'add' : name === 'calc_server' ? 'calculator' : name === 'object_server' ? 'object' : 'transaction';
      if (name === 'object_server') return line(ch === '3.1' ? 'object server ready' : 'object 1: node=1\nobject 2: node=2\nobject server ready');
      return line(name === 'server' ? 'server ready' : name === 'calc_server' ? 'calculator server ready' : 'transaction server ready');
    }
    const service = sessions.find(session => session.server)?.server;
    if (name === 'object_client') {
      if (service !== 'object') return error('object_client: 对象服务端尚未启动');
      const params = args.slice(1);
      if (args.length !== 4 || !['1', '2'].includes(args[0])) return error('object_client: 对象编号无效');
      if (params.length !== 3 || !(ch === '3.1' ? ['add', 'sub', 'mul'] : ['add', 'mul']).includes(params[0]) || !params.slice(1).every(x => /^-?\d+$/.test(x))) return error('object_client: 参数无效');
      if (ch !== '3.1' && (args.length !== 4 || !['1', '2'].includes(args[0]) || (args[0] === '1') !== (params[0] === 'add'))) return error('object_client: 对象编号或操作无效');
      const [a, b] = params.slice(1).map(BigInt);
      const result = params[0] === 'add' ? a + b : params[0] === 'sub' ? (args[0] === '2' ? b - a : a - b) : a * b;
      if ([a, b, result].some(v => v < -2147483648n || v > 2147483647n)) return error('object_client: 参数或结果超出范围');
      return line(`${ch === '3.1' ? `object ${args[0]}: ` : ''}${params[0]}(${a}, ${b}) = ${result}`);
    }
    if (ch === '6.3' && name === 'fd_failure_test' && args.length === 1 && args[0] === '--fd-full') {
      if (service) return error('fd_failure_test: 请先停止普通服务端');
      line('模拟回放：未实际运行测试。', 'info');
      return line("receiver: one free fd=63, request contains two fd objects\nsender: WRITE returned EPIPE; delivery failed\nsender: both pipes reached EOF; driver holds no write-end references\nreceiver: READ returned EMFILE; second reservation failed\nreceiver: open reused fd=63; first reservation rolled back\ntwo-fd reservation rollback passed");
    }
    if (name in (chapterTests[ch] || {}) && !args.length) {
      if (['3.1', '3.4', '4.1'].includes(ch) && service !== 'object') return error(`${name}: 对象服务端尚未启动`);
      if (service && !['3.1', '3.4', '4.1'].includes(ch)) return error(`${name}: 请先停止普通服务端`);
      line('模拟回放：未实际运行测试。', 'info');
      if (ch === '5.4' && name === 'target_wakeup_test') logs.push('mini_binder: service thread=1 replies to caller thread=2', 'mini_binder: service thread=2 replies to caller thread=1');
      if (ch === '5.4' && name === 'thread_reply_test') logs.push('mini_binder: service thread=2 replies to caller thread=2', 'mini_binder: service thread=1 replies to caller thread=1');
      return line(chapterTests[ch][name]);
    }
    if (name === 'client') {
      if (!service) return error('call: 模拟服务端尚未启动');
      if (service !== 'add') return error('client: 服务端类型不匹配');
      if (args.length !== 2 || !args.every(arg => /^-?\d+$/.test(arg))) return error('usage: client <int32> <int32>');
      const [a, b] = args.map(BigInt);
      if ([a, b].some(v => v < -2147483648n || v > 2147483647n)) return error('usage: client <int32> <int32>');
      const sum = a + b;
      return sum < -2147483648n || sum > 2147483647n ? error('server error: -34') : line(`remote add(${a}, ${b}) = ${sum}`);
    }
    if (name === 'calc_client') {
      if (service !== 'calculator') return error('calc_client: 计算器服务端尚未启动');
      if (args.length !== 3 || !['add', 'sub', 'mul', 'div'].includes(args[0]) || !args.slice(1).every(arg => /^-?\d+$/.test(arg))) return error('usage: calc_client <add|sub|mul|div> <int32> <int32>');
      const [a, b] = args.slice(1).map(BigInt);
      if ([a, b].some(v => v < -2147483648n || v > 2147483647n)) return error('calc_client: 参数超出范围');
      if (args[0] === 'div' && b === 0n) return error('server error: -33');
      const result = args[0] === 'add' ? a + b : args[0] === 'sub' ? a - b : args[0] === 'mul' ? a * b : a / b;
      if (result < -2147483648n || result > 2147483647n) return error('server error: -34');
      return line(`${args[0]}(${a}, ${b}) = ${result}`);
    }
    if (name === 'calc_test' && !args.length) {
      if (service !== 'calculator') return error('calc_test: 计算器服务端尚未启动');
      line('模拟回放：未实际运行测试。', 'info');
      return line('arithmetic commands and error recovery passed');
    }
    if (['transaction_client', 'mapped_client', 'transaction_test', 'mapped_test'].includes(name)) {
      if (args.length) return error(`${name}: 不支持的参数`);
      if (!service) return error(`${name}: 模拟服务端尚未启动`);
      if (service !== 'transaction' && !(name === 'mapped_test' && service === 'calculator' && ['2.6', '2.7'].includes(ch))) return error(`${name}: 服务端类型不匹配`);
      const text = { transaction_client: 'reply: number=52, text=Binder', mapped_client: 'mapped reply: number=52, text=Binder', transaction_test: 'oversized, malformed and overflowing requests rejected', mapped_test: 'mapped reply permissions and reuse passed' };
      if (name.endsWith('_test')) line('模拟回放：未实际运行测试。', 'info');
      return line(text[name]);
    }
    const tests = { cancel_test: 'cancelled reply discarded; next call succeeded', stale_reply: ch === '1.4' ? 'stale reply rejected; next client received correct result' : 'cancelled reply discarded; next client received correct result', server_death: 'server death wakes client with EPIPE', mmap_access: 'mapping permissions and lifetime passed' };
    if (ch === '2.7') Object.assign(tests, { concurrent_test: "receive id=1: add(10, 1), mapping + 0\nreceive id=2: sub(11, 1), mapping + 256\ntwo requests held before either reply\nthird request: EBUSY (both slots occupied)\nreply id=2: result=10 (reverse receive order)\nclient 11: result=10, reply released\nreply id=1: result=11 (reverse receive order)\nclient 10: result=11, reply released\nreuse id=3: mapping + 0\nclient 20: result=21, reply released\ntwo slots, distinct user addresses, reverse replies, full capacity and reuse passed", server_mapping_test: 'server mapping ownership, unmap wakeup and remap passed', stale_reply: 'cancelled reply discarded; next client received correct result', server_death: 'server death wakes both clients with EPIPE' });
    if (['2.6', '2.7'].includes(ch)) tests.command_test = 'unknown command, invalid address and empty read rejected';
    if (name in tests && !args.length) {
      if (service && name !== 'command_test') return error(`${name}: 请先停止普通服务端`);
      line('模拟回放：未实际运行测试。', 'info');
      return line(tests[name]);
    }
    error(`${name}: 不支持的模拟程序或参数`);
  }
  function execute(raw) {
    const session = sessions[active];
    if (session.server) { error('服务端正在运行，请切换到另一个终端或按 Ctrl+C'); return; }
    const text = raw.trim(); if (!text) return;
    line(`${prompt()} ${text}`, 'command');
    session.history.push(text); session.historyIndex = session.history.length;
    if (text === 'echo $?') { line(String(session.lastStatus)); session.lastStatus = 0; return; }
    session.lastStatus = 0;
    if (text === "lsmod | grep '^binderabc_hello '") {
      if (loaded?.name === 'binderabc_hello') line('binderabc_hello       16384  0');
      else session.lastStatus = 1;
      return;
    }
    if (text === 'test -d "/lib/modules/$(uname -r)/build" || echo "缺少当前内核构建目录"') return;
    if (/[|;`$<>]/.test(text) && text !== 'sudo dmesg | tail -n 20') return error('模拟器不支持管道、重定向或 Shell 表达式');
    const background = /\s+&$/.test(text);
    const words = (background ? text.replace(/\s+&$/, '') : text).split(/\s+/);
    if (background && words[0] !== './build/client') return error('模拟器仅支持客户端后台命令');
    const command = words.shift();
    if (command === 'help') return line('支持: pwd, ls, cd, cat, make, uname, sudo insmod/rmmod/dmesg\n      ./build/程序, timeout 10 ./build/测试, wait, clear\n三个终端共享模拟的模块和服务端状态；Ctrl+C 可停止服务端。', 'info');
    if (command === 'clear' && !words.length) { session.lines = []; return; }
    if (command === 'pwd' && !words.length) return line(session.cwd);
    if (command === 'ls' && words.join(' ') === '-l /dev/mini_binder') {
      if (loaded?.name !== 'mini_binder') return error('ls: cannot access /dev/mini_binder: No such file or directory');
      return line('crw-rw-rw- 1 root root 10, 123 Nov  6 12:00 /dev/mini_binder');
    }
    if (command === 'ls' && words.length <= 1) return list(words[0]);
    if (command === 'cd' && words.length <= 1) {
      const target = normalize(words[0] || '/');
      if (!directorySet.has(target) && !(target.endsWith('/build') && built.has(target.slice(0, -6)))) return error('cd: 没有这个目录');
      session.cwd = target; return;
    }
    if (command === 'cat' && words.length === 1) {
      const key = normalize(words[0]).slice(1);
      return key in files ? line(files[key].trimEnd()) : error('cat: 没有这个文件');
    }
    if (command === 'make') return make(words);
    if (command === 'uname' && words.length === 1 && ['-r', '-m'].includes(words[0])) return line(words[0] === '-r' ? '6.8.0-147-generic' : 'aarch64');
    if (command === 'modinfo' && words.length === 1 && words[0] === './binderabc_hello.ko' && session.cwd === '/examples/0.4') {
      if (!currentBuild()?.has('*')) return error('modinfo: 模拟模块尚未构建');
      return line('name:           binderabc_hello\nvermagic:       6.8.0-147-generic SMP preempt mod_unload modversions aarch64');
    }
    if (text === 'sudo dmesg | tail -n 20' || (command === 'sudo' && words.join(' ') === 'dmesg')) return line(logs.join('\n') || '(模拟日志为空)');
    if (command === 'sudo' && words[0]?.startsWith('./build/')) return program(words[0].slice(8), words.slice(1));
    if (command === 'sudo') return moduleCommand(words);
    if (command === 'wait' && !words.length) return line('模拟：后台客户端已完成。', 'info');
    if (command === 'timeout' && /^[1-9][0-9]*$/.test(words[0]) && words[1]?.startsWith('./build/')) return program(words[1].slice(8), words.slice(2));
    if (command.startsWith('./build/')) return program(command.slice(8), words);
    if (command.startsWith('./') && session.cwd.endsWith('/build') && !command.slice(2).includes('/')) return program(command.slice(2), words, session.cwd.slice(0, -6));
    error(`${command}: 不支持的模拟命令；输入 help 查看支持范围`);
  }
  document.querySelectorAll('.terminal-tab').forEach((tab, index) => { tab.onclick = () => { if (!sessions[index].visited) { sessions[index].cwd = sessions[active].cwd; sessions[index].visited = true; } active = index; render(); input.focus(); }; });
  $('terminal-form').onsubmit = event => { event.preventDefault(); execute(input.value); input.value = ''; render(); };
  input.onkeydown = event => {
    const session = sessions[active];
    if (event.ctrlKey && event.key.toLowerCase() === 'c') {
      event.preventDefault();
      if (session.server) { session.server = null; line('^C', 'command'); line('模拟：服务端已停止。', 'info'); }
      else { input.value = ''; line(`${prompt()} ^C`, 'command'); }
      render();
    } else if (event.key === 'ArrowUp' || event.key === 'ArrowDown') {
      event.preventDefault();
      session.historyIndex = Math.max(0, Math.min(session.history.length, session.historyIndex + (event.key === 'ArrowUp' ? -1 : 1)));
      input.value = session.history[session.historyIndex] || '';
    }
  };
  $('terminal-clear').onclick = () => { sessions[active].lines = []; render(); input.focus(); };
  $('terminal-chapter').onclick = () => {
    if (sessions[active].server) { error('服务端正在运行，请先按 Ctrl+C 或切换终端'); render(); return; }
    sessions[active].cwd = chapterDir;
    line(`模拟：进入 ${chapterDir}`, 'info'); render(); input.focus();
  };
  $('terminal-collapse').onclick = () => {
    const collapsed = terminal.classList.toggle('collapsed');
    $('terminal-collapse').innerHTML = icon(collapsed ? 'chevron-up' : 'chevron-down');
    $('terminal-collapse').title = collapsed ? '展开终端' : '折叠终端';
    $('terminal-collapse').setAttribute('aria-label', $('terminal-collapse').title); paintIcons();
  };
  const resize = $('terminal-resize');
  function setHeight(height) {
    const panelHeight = $('source-panel').getBoundingClientRect().height;
    terminal.style.height = `${Math.max(145, Math.min(panelHeight * 0.7, height))}px`;
  }
  resize.onpointerdown = event => {
    if (terminal.classList.contains('collapsed')) return;
    event.preventDefault(); resize.setPointerCapture(event.pointerId);
    const initial = terminal.getBoundingClientRect().height, start = event.clientY;
    resize.onpointermove = move => setHeight(initial + start - move.clientY);
    resize.onpointerup = () => { resize.onpointermove = null; resize.onpointerup = null; };
  };
  resize.onkeydown = event => { if (event.key === 'ArrowUp' || event.key === 'ArrowDown') { event.preventDefault(); setHeight(terminal.getBoundingClientRect().height + (event.key === 'ArrowUp' ? 20 : -20)); } };
  sessions.forEach(session => session.lines.push({ text: 'BinderABC 模拟终端 · 输出为示例回放，未执行真实命令', kind: 'info' }));
  render();
  return { setChapter(path) {
    let chapter = path.split('/').at(-1).match(/^(\d+\.\d+)-/)?.[1];
    if (chapter === '7.5') chapter = '7.4';
    chapterDir = !chapter || chapter === '0.3' ? '/examples' : `/examples/${chapter}`;
    $('terminal-chapter').title = `进入当前章节目录 ${chapterDir}`;
  } };
}
