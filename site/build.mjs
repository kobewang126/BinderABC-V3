import { build } from 'esbuild';
import { cp, mkdir, readFile, readdir, writeFile } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { marked } from 'marked';
import { flows } from './flows-data.js';

const site = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(site, '..');
const sourceRoot = path.join(root, 'examples');
const out = path.join(site, 'dist', 'BinderABC-V3');
await mkdir(out, { recursive: true });
for (const chapter of ['0.1', '0.2', '0.3', '1.1', '1.2', '1.3', '1.4', '2.1', '2.2', '2.3', '2.4', '2.5', '2.6', '2.7']) {
  await cp(path.join(sourceRoot, chapter), path.join(out, 'examples', chapter), {
    recursive: true,
    filter: source => !source.split(path.sep).some(part => part === 'build' || part === '.git') && !/\.(?:o|ko|mod|cmd)$/.test(source) && !/\/(?:Module\.symvers|modules\.order)$/.test(source)
  });
}
await cp(path.join(sourceRoot, 'Makefile'), path.join(out, 'examples', 'Makefile'));
for (const directory of ['00-preparation', '01-rpc', '02-transaction']) {
  await cp(path.join(root, 'docs', directory), path.join(out, 'docs', directory), { recursive: true });
}
await cp(path.join(root, 'README.md'), path.join(out, 'README.md'));
await cp(path.join(site, 'index.html'), path.join(out, 'index.html'));
await cp(path.join(site, 'style.css'), path.join(out, 'style.css'));
await cp(path.join(site, 'terminal.css'), path.join(out, 'terminal.css'));
await cp(path.join(site, 'flow.css'), path.join(out, 'flow.css'));

const readme = await readFile(path.join(root, 'README.md'), 'utf8');
const chapters = [];
for (const match of readme.matchAll(/\[([^\]]+)\]\((docs\/[^)]+\.md)\)/g)) {
  if (!/^docs\/(?:00-preparation|01-rpc|02-transaction)\//.test(match[2])) continue;
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
for (const chapter of chapters) {
  const diagrams = [...chapter.markdown.matchAll(/(?:```|~~~)mermaid\s*\n([\s\S]*?)(?:```|~~~)/g)];
  for (let index = 0; index < diagrams.length; index++) {
    if (!flows.some(flow => flow.chapter === chapter.path && flow.diagram === index)) throw new Error(`Missing flow configuration: ${chapter.path}:${index}`);
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
await writeFile(path.join(out, 'data.js'), `window.BINDERABC_DATA = ${JSON.stringify({ chapters, files, annotations })};\n`);
await build({ entryPoints: [path.join(site, 'app.js')], bundle: true, minify: true, outfile: path.join(out, 'app.bundle.js'), platform: 'browser', target: ['chrome100', 'safari16'] });
console.log(`Built ${chapters.length} chapters, ${flows.length} diagrams, ${annotations.length} explanations and ${Object.keys(files).length} source files in ${out}`);
