import { build } from 'esbuild';
import { cp, mkdir, readFile, readdir, rm, writeFile } from 'node:fs/promises';
import { execFileSync } from 'node:child_process';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { marked } from 'marked';
import { flows } from './flows-data.js';
import { v3Specs } from './flows-v3-spec.js';

const site = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(site, '..');
const sourceRoot = path.join(root, 'examples');
const out = path.join(site, 'dist', 'BinderABC-V3');
await rm(out, { recursive: true, force: true });
await mkdir(out, { recursive: true });
const exampleEntries = await readdir(sourceRoot, { withFileTypes: true });
for (const chapter of exampleEntries.filter(entry => entry.isDirectory() && /^\d+\.\d+$/.test(entry.name)).map(entry => entry.name)) {
  await cp(path.join(sourceRoot, chapter), path.join(out, 'examples', chapter), {
    recursive: true,
    filter: source => !source.split(path.sep).some(part => part === 'build' || part === '.git') && !/\.(?:o|ko|mod|cmd)$/.test(source) && !/\/(?:Module\.symvers|modules\.order)$/.test(source)
  });
}
for (const directory of ['00-preparation', '01-rpc', '02-transaction', '03-objects', '04-concurrency', '05-threading', '06-file-descriptors', '07-protocol', '08-framework', 'appendices']) {
  await cp(path.join(root, 'docs', directory), path.join(out, 'docs', directory), { recursive: true });
}
await cp(path.join(root, 'README.md'), path.join(out, 'README.md'));
await cp(path.join(root, 'validation'), path.join(out, 'validation'), { recursive: true });
await cp(path.join(site, 'index.html'), path.join(out, 'index.html'));
await cp(path.join(site, 'style.css'), path.join(out, 'style.css'));
await cp(path.join(site, 'terminal.css'), path.join(out, 'terminal.css'));
await cp(path.join(site, 'flow.css'), path.join(out, 'flow.css'));

const readme = await readFile(path.join(root, 'README.md'), 'utf8');
const chapters = [];
for (const match of readme.matchAll(/\[([^\]]+)\]\((docs\/[^)]+\.md)\)/g)) {
  if (!/^docs\/(?:00-preparation|01-rpc|02-transaction|03-objects|04-concurrency|05-threading|06-file-descriptors|07-protocol|08-framework)\//.test(match[2]) && !/^docs\/appendices\/(?:[0-8]-review|reading-guide|protocol-evolution)\.md$/.test(match[2])) continue;
  if (chapters.some(chapter => chapter.path === match[2])) continue;
  chapters.push({ title: match[1], path: match[2], markdown: await readFile(path.join(root, match[2]), 'utf8') });
}
await writeFile(path.join(out, 'README.md'), `# BinderABC 离线阅读包\n\n打开 index.html 阅读。模拟终端不会执行真实命令。源码按章节保存在 examples/，请在 Linux 环境中根据教程进行真实实验。\n\n## 阅读目录\n\n${chapters.map(chapter => `- [${chapter.title}](${chapter.path})`).join('\n')}\n`);
const files = {};
async function walk(dir, prefix) {
  for (const entry of await readdir(dir, { withFileTypes: true })) {
    const name = path.join(dir, entry.name);
    const key = `${prefix}/${entry.name}`;
    if (entry.isDirectory()) await walk(name, key);
    else if (entry.isFile()) files[key] = await readFile(name, 'utf8');
  }
}
await walk(path.join(out, 'examples'), 'examples');
const extraFlows = v3Specs.map(spec => {
  const chapter = chapters.find(item => item.path === spec.path);
  const diagramIndex = spec.diagram || 0;
  const diagram = [...(chapter?.markdown.matchAll(/```mermaid\s*\n([\s\S]*?)```/g) || [])][diagramIndex]?.[1];
  if (!diagram) throw new Error(`Missing diagram: ${spec.path}`);
  let events;
  if (spec.kind === 'sequence') {
    events = [...diagram.matchAll(/^\s*\w+\s*(?:-->>|->>)\s*\w+\s*:\s*(.+)$/gm)].map(match => match[1].trim());
  } else {
    const nodes = new Map();
    for (const match of diagram.matchAll(/\b(\w+)(?:\[([^\]]+)\]|\{([^}]+)\})/g)) if (!nodes.has(match[1])) nodes.set(match[1], match[2] ?? match[3]);
    events = [...nodes.values()];
  }
  if (events.length !== spec.refs.length) throw new Error(`Flow step count mismatch: ${spec.path}: ${events.length} vs ${spec.refs.length}`);
  const part = spec.sourceChapter || spec.path.match(/\/(\d+\.\d+)-/)?.[1];
  const steps = spec.refs.map(([source, needle, context, occurrence = 0], index) => {
    const file = `examples/${part}/${source}`, lines = files[file]?.split('\n');
    const positions = lines?.flatMap((line, lineIndex) => line.includes(needle) ? [lineIndex] : []) ?? [];
    const position = positions[occurrence] ?? -1;
    if (position < 0) throw new Error(`Missing flow anchor: ${file}: ${needle}`);
    const start = position + 1, end = Math.min(lines.length, start + (/^\s*static (?:long|int)/.test(lines[position]) ? 18 : 6));
    const explanations = [
      ['list_add_tail(&slot->reply_work', '驱动把回复工作加入发起调用的线程的 todo；事务槽记录了这个调用线程。'],
      ['list_add_tail(&slot->work', '驱动将请求槽加入目标 proc 的待办队列，并在释放锁后唤醒等待线程。'],
      ['wake_up_interruptible(&slot->caller->wait)', '驱动唤醒事务记录的调用线程；线程随后获取状态锁并消费回复。'],
      ['list_first_entry(&ctx->todo', '服务线程在状态锁下取出队首工作，避免另一个线程领取同一事务。'],
      ['fd_file = fget(', '发送线程从自己的 fd 表取得文件引用，驱动在交付前持有它。'],
      ['fd_install(reserved[i]', '接收线程在自己的 fd 表中安装已预留的编号，随后由接收方关闭。'],
      ['install_fds(&thread->reply_fds', '回复元数据成功复制后，驱动将文件引用安装到接收线程所在进程的 fd 表。'],
      ['nodes[j].owner == receiver', '驱动发现接收方就是 Node 的拥有者，恢复本地对象描述和原 cookie。'],
      ['ret = translate_objects(', '驱动按对象位置表转换本地对象或 handle，为目标上下文建立或复用 Ref。'],
      ['transaction.reply_offset =', '驱动用当前调用线程预留的位置定位回复，线程从共享映射中读取自己的数据。']
    ];
    const specific = explanations.find(([key]) => needle.includes(key))?.[1];
    const detail = spec.kind === 'concept'
      ? `${events[index]}是结构关系的示意；右侧为相关定义或实验代码，并非逐条执行轨迹。`
      : specific || `${events[index]}。右侧展示该步骤相关的代码，执行上下文标注在上方。`;
    return { message: index, title: events[index], context, detail, file, ranges: [[start, end]], anchor: [start, needle] };
  });
  return { chapter: spec.path, diagram: diagramIndex, kind: spec.kind, title: chapter.title, events, steps };
});
flows.push(...extraFlows);
for (const chapter of chapters) {
  const diagrams = [...chapter.markdown.matchAll(/(?:```|~~~)mermaid\s*\n([\s\S]*?)(?:```|~~~)/g)];
  for (let index = 0; index < diagrams.length; index++) {
    if (/^docs\/(?:00-preparation|01-rpc|02-transaction)\//.test(chapter.path) && !flows.some(flow => flow.chapter === chapter.path && flow.diagram === index)) throw new Error(`Missing flow configuration: ${chapter.path}:${index}`);
  }
}
for (const flow of flows) {
  const chapter = chapters.find(item => item.path === flow.chapter);
  const diagrams = chapter && [...chapter.markdown.matchAll(/(?:```|~~~)mermaid\s*\n([\s\S]*?)(?:```|~~~)/g)];
  const diagram = diagrams?.[flow.diagram]?.[1];
  if (!diagram || flow.events.some(event => !diagram.replace(/<br\s*\/?>/g, '').includes(event))) throw new Error(`Flow diagram mismatch: ${flow.chapter}:${flow.diagram}`);
  for (const step of flow.steps) {
    const lines = files[step.file]?.split('\n');
    if (!lines || !lines[step.anchor[0] - 1]?.includes(step.anchor[1]) || step.ranges.some(([a, b]) => a < 1 || b < a || b > lines.length)) {
      throw new Error(`Flow source mismatch: ${step.file}:${step.anchor[0]} (${step.anchor[1]})`);
    }
  }
}
const annotations = [];
for (const chapter of chapters) {
  const tokens = marked.lexer(chapter.markdown).filter(token => token.type !== 'space');
  for (let i = 0; i < tokens.length; i++) {
    const reference = tokens[i];
    if (reference.type !== 'paragraph' || !reference.text.includes('源码：`examples/')) continue;
    const match = reference.text.match(/`(examples\/[^`]+)`.*?第\s*([\d\s–—,，、-]+)\s*行/);
    if (!match) throw new Error(`Cannot associate source explanation: ${chapter.path}`);
    const ranges = [...match[2].matchAll(/(\d+)(?:\s*[–—-]\s*(\d+))?/g)].map(m => [Number(m[1]), Number(m[2] || m[1])]);
    const lines = files[match[1]]?.split('\n');
    if (!lines || ranges.some(([a, b]) => a < 1 || b < a || b > lines.length)) throw new Error(`Invalid annotation source: ${match[1]}`);
    if (tokens[i + 1]?.type !== 'code') continue;
    const included = ranges.flatMap(([a, b]) => lines.slice(a - 1, b));
    let cursor = 0;
    for (const sourceLine of tokens[i + 1].text.split('\n')) {
      const trimmed = sourceLine.trim();
      if (!trimmed || /^\/\*\s*\.\.\./.test(trimmed) || trimmed === '...') continue;
      const found = included.findIndex((candidate, index) => index >= cursor && candidate.trim() === trimmed);
      if (found < 0) throw new Error(`Annotation excerpt mismatch: ${match[1]}:${match[2]}: ${trimmed}`);
      cursor = found + 1;
    }
    const explanation = tokens[i + 2]?.type === 'paragraph' ? tokens[i + 2].text : reference.text.split('源码：')[0];
    const text = explanation.replace(/`/g, '').replace(/\*\*/g, '').replace(/\[([^\]]+)\]\([^)]+\)/g, '$1');
    annotations.push({ chapter: chapter.path, file: match[1], ranges, text });
  }
}
await writeFile(path.join(out, 'data.js'), `window.BINDERABC_DATA = ${JSON.stringify({ chapters, files, annotations, extraFlows })};\n`);
await build({ entryPoints: [path.join(site, 'app.js')], bundle: true, minify: true, outfile: path.join(out, 'app.bundle.js'), platform: 'browser', target: ['chrome100', 'safari16'] });
const sourceReadme = `# BinderABC V3 配套源码

本包包含预备篇及第一至第七篇的完整独立示例、驱动和测试，源码与下载页面的教程一致。

## 使用方法

- 解压后在 Linux 中打开 examples/，根据教程进入对应章节目录。
- 每章独立编译和运行，无需切换 Git 标签；压缩包不包含 Git 历史。
- 进入 examples/ 对应章节目录执行 make，再直接运行 build/ 中的程序；具体命令按各章正文操作。
- 驱动实验需要 C 编译器、Make、kmod、匹配运行内核的构建目录和模块加载权限。
- 同名 mini_binder 模块不能同时加载多个章节；切换前先停止服务端并卸载旧模块。
- 本包不包含编译产物；先用 make 编译，再直接执行生成的二进制。

实验步骤、预期输出和限制请对照教程中的对应章节。网页模拟终端不会执行真实程序。
`;
execFileSync('python3', ['-c', `
import json, os, sys, zipfile
from pathlib import Path
payload = json.load(sys.stdin)
out = Path(sys.argv[1])
source_zip = out / 'BinderABC-V3-source.zip'
with zipfile.ZipFile(source_zip.with_suffix('.zip.tmp'), 'w', zipfile.ZIP_DEFLATED) as archive:
    archive.writestr('BinderABC-V3-source/README.md', payload['readme'])
    for name, content in sorted(payload['files'].items()):
        archive.writestr('BinderABC-V3-source/' + name, content)
os.replace(source_zip.with_suffix('.zip.tmp'), source_zip)
reader_zip = out.parent / 'BinderABC-V3.zip'
with zipfile.ZipFile(reader_zip.with_suffix('.zip.tmp'), 'w', zipfile.ZIP_DEFLATED) as archive:
    for file in sorted(out.rglob('*')):
        if file.is_file():
            archive.write(file, file.relative_to(out.parent))
os.replace(reader_zip.with_suffix('.zip.tmp'), reader_zip)
deploy_zip = out.parent / 'BinderABC-V3-deploy.zip'
with zipfile.ZipFile(deploy_zip.with_suffix('.zip.tmp'), 'w', zipfile.ZIP_DEFLATED) as archive:
    for file in sorted(out.rglob('*')):
        if file.is_file():
            archive.write(file, file.relative_to(out))
os.replace(deploy_zip.with_suffix('.zip.tmp'), deploy_zip)
`, out], { input: JSON.stringify({ files, readme: sourceReadme }) });
console.log(`Built ${chapters.length} chapters, ${flows.length} diagrams, ${annotations.length} explanations and ${Object.keys(files).length} source files in ${out}`);
console.log('Created source download ZIP and complete offline reader ZIP.');
