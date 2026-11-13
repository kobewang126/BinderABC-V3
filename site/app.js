import { marked } from 'marked';
import mermaid from 'mermaid';
import { createIcons, BookOpen, PanelRight, PanelLeft, Folder, FileCode2, ChevronRight, Code2, Copy, ArrowUpRight, TerminalSquare, FolderInput, Eraser, ChevronDown, ChevronUp, ArrowLeft, ArrowRight, RotateCcw, Maximize2, X } from 'lucide';
import { initTerminal } from './terminal.js';
import { attachFlow } from './flow-06a.js';
import { flows } from './flows-data.js';

const icons = { BookOpen, PanelRight, PanelLeft, Folder, FileCode2, ChevronRight, Code2, Copy, ArrowUpRight, TerminalSquare, FolderInput, Eraser, ChevronDown, ChevronUp, ArrowLeft, ArrowRight, RotateCcw, Maximize2, X };
const { chapters, files, annotations } = window.BINDERABC_DATA;
flows.push(...(window.BINDERABC_DATA.extraFlows || []));
const $ = (id) => document.getElementById(id);
let currentChapter = 0;
let currentFile = '';
let currentRanges = [];
let currentExplanation = null;
let currentArtifact = '';
let generatedArtifacts = [];
let diagramVersion = 0;
const makeIcon = (name, extra = '') => `<i data-lucide="${name}" class="${extra}"></i>`;
function paintIcons() { createIcons({ icons }); }
const terminal = initTerminal({ files, icon: makeIcon, paintIcons, onBuildChange: updateArtifacts });

$('mobile-chapters').innerHTML = makeIcon('panel-left');
$('mobile-source').innerHTML = makeIcon('panel-right');
$('toggle-tree').innerHTML = makeIcon('panel-left');
$('copy-code').innerHTML = makeIcon('copy');
$('file-icon').innerHTML = makeIcon('file-code-2');
$('empty-icon').innerHTML = makeIcon('code-2');
$('chapter-count').textContent = `${chapters.filter(chapter => !chapter.path.includes('/appendices/')).length} 章`;
$('source-count').textContent = `${Object.keys(files).length} 个文件`;

function closePanels() {
  $('navigation').classList.remove('open');
  $('source-panel').classList.remove('open');
  $('scrim').classList.remove('visible');
}
function showPanel(panel) {
  const open = !panel.classList.contains('open');
  closePanels();
  if (open) { panel.classList.add('open'); $('scrim').classList.add('visible'); }
}
$('mobile-chapters').onclick = () => showPanel($('navigation'));
$('mobile-source').onclick = () => showPanel($('source-panel'));
$('scrim').onclick = closePanels;
$('toggle-tree').onclick = () => $('tree').classList.toggle('hidden');
$('copy-code').onclick = async () => {
  if (!currentFile) return;
  try { await navigator.clipboard.writeText(files[currentFile]); $('copy-code').title = '已复制'; }
  catch { $('copy-code').title = '复制不可用，请手动选择'; }
};

function treeNode() { return { dirs: new Map(), files: [] }; }
function insertFile(sourceTree, filename, generated) {
  const parts = filename.split('/').slice(1);
  const name = parts.pop();
  let node = sourceTree;
  for (const part of parts) {
    if (!node.dirs.has(part)) node.dirs.set(part, treeNode());
    node = node.dirs.get(part);
  }
  node.files.push({ name, path: filename, generated });
}
function renderTreeNode(node, container, prefix = 'examples') {
  for (const [name, child] of [...node.dirs].sort(([a], [b]) => a.localeCompare(b))) {
    const details = document.createElement('details');
    const full = `${prefix}/${name}`;
    details.dataset.path = full;
    const summary = document.createElement('summary');
    summary.innerHTML = `${makeIcon('chevron-right', 'chevron')}${makeIcon('folder')}<span></span>`;
    summary.querySelector('span').textContent = name;
    const children = document.createElement('div');
    children.className = 'tree-children';
    renderTreeNode(child, children, full);
    details.append(summary, children);
    container.append(details);
  }
  for (const file of node.files.sort((a, b) => a.name.localeCompare(b.name))) {
    const button = document.createElement('button');
    button.className = 'file-item';
    button.dataset.file = file.path;
    button.title = file.generated ? `${file.path}（模拟产物）` : file.path;
    if (file.generated) button.classList.add('generated-item');
    button.innerHTML = `${makeIcon('file-code-2')}<span></span>`;
    button.querySelector('span').textContent = file.name;
    if (file.generated) {
      const label = document.createElement('span'); label.className = 'generated-label'; label.textContent = '模拟'; button.append(label);
      button.onclick = () => openArtifact(file.path);
    } else button.onclick = () => openSource(file.path);
    container.append(button);
  }
}
function rebuildTree(expand = '') {
  const opened = new Set([...document.querySelectorAll('#file-tree details[open]')].map(el => el.dataset.path));
  const sourceTree = treeNode();
  for (const filename of Object.keys(files).sort()) insertFile(sourceTree, filename, false);
  for (const filename of generatedArtifacts) insertFile(sourceTree, filename, true);
  $('file-tree').replaceChildren();
  renderTreeNode(sourceTree, $('file-tree'));
  document.querySelectorAll('#file-tree details').forEach(detail => {
    if (opened.has(detail.dataset.path) || (expand && (detail.dataset.path === expand || expand.startsWith(`${detail.dataset.path}/`)))) detail.open = true;
  });
  document.querySelectorAll('.file-item').forEach(item => item.classList.toggle('selected', item.dataset.file === (currentArtifact || currentFile)));
  paintIcons();
}
function updateArtifacts(artifacts, changedDir) {
  generatedArtifacts = artifacts;
  if (currentArtifact && !artifacts.includes(currentArtifact)) {
    currentArtifact = '';
    $('file-name').textContent = '模拟产物已清理';
    $('file-path').textContent = changedDir;
    $('code-footer').textContent = '模拟产物';
    $('line-info').textContent = '';
    const notice = document.createElement('div'); notice.className = 'artifact-notice'; notice.textContent = '该模拟产物已被 make clean 清理。';
    $('code-scroll').replaceChildren(notice);
  }
  const artifact = artifacts.find(path => path.startsWith(`${changedDir}/build/`)) || artifacts.find(path => path.startsWith(`${changedDir}/`));
  const expand = artifact ? artifact.slice(0, artifact.lastIndexOf('/')) : '';
  rebuildTree(expand);
  if (expand) {
    const node = [...document.querySelectorAll('#file-tree details')].find(detail => detail.dataset.path === expand);
    if (node) $('tree').scrollTop += node.getBoundingClientRect().top - $('tree').getBoundingClientRect().top - 35;
  }
}
function openArtifact(filename) {
  currentFile = '';
  currentArtifact = filename;
  currentExplanation = null;
  $('source-explanation').hidden = true;
  $('file-name').textContent = filename.split('/').pop();
  $('file-path').textContent = filename;
  $('code-footer').textContent = '模拟产物';
  $('line-info').textContent = '';
  const notice = document.createElement('div'); notice.className = 'artifact-notice';
  const title = document.createElement('strong'); title.textContent = '模拟构建产物';
  const detail = document.createElement('p'); detail.textContent = '这里只记录模拟构建状态，没有实际的二进制文件。请在 Linux 环境中编译源码以生成可执行文件或内核模块。';
  notice.append(title, detail); $('code-scroll').replaceChildren(notice);
  document.querySelectorAll('.file-item').forEach(item => item.classList.toggle('selected', item.dataset.file === filename));
}
rebuildTree();

function openSource(filename, ranges = [], { openPanel = true, explanation } = {}) {
  if (!(filename in files)) return;
  currentFile = filename;
  currentRanges = ranges;
  currentArtifact = '';
  const matched = annotations.find(item => item.file === filename && item.chapter === chapters[currentChapter].path && JSON.stringify(item.ranges) === JSON.stringify(ranges));
  currentExplanation = explanation === undefined ? (matched ? { label: `片段解读 · ${ranges.map(([a, b]) => a === b ? a : `${a}–${b}`).join('、')} 行`, text: matched.text } : null) : explanation;
  $('source-explanation').hidden = !currentExplanation;
  $('source-explanation-label').textContent = currentExplanation?.label || '';
  $('source-explanation-text').textContent = currentExplanation?.text || '';
  $('file-name').textContent = filename.split('/').pop();
  $('file-path').textContent = filename;
  $('code-footer').textContent = 'UTF-8 · 源码';
  const content = files[filename].split('\n');
  if (content.at(-1) === '') content.pop();
  $('line-info').textContent = `${content.length} 行`;
  const wrapper = document.createElement('div');
  wrapper.className = 'code-lines';
  let first = null;
  content.forEach((line, index) => {
    const n = index + 1;
    const row = document.createElement('div');
    row.className = 'code-line';
    if (ranges.some(([start, end]) => n >= start && n <= end)) {
      row.classList.add('highlight');
      first ||= row;
    }
    const number = document.createElement('span');
    number.className = 'line-number';
    number.textContent = n;
    const text = document.createElement('span');
    text.className = 'line-text';
    text.textContent = line || ' ';
    row.append(number, text);
    wrapper.append(row);
  });
  $('code-scroll').replaceChildren(wrapper);
  if (first) $('code-scroll').scrollTop = Math.max(0, first.offsetTop - wrapper.offsetTop - 75);
  document.querySelectorAll('.file-item').forEach(item => {
    item.classList.toggle('selected', item.dataset.file === filename);
    if (item.dataset.file === filename) {
      let parent = item.parentElement;
      while (parent && parent.id !== 'file-tree') { if (parent.tagName === 'DETAILS') parent.open = true; parent = parent.parentElement; }
    }
  });
  if (openPanel && window.matchMedia('(max-width: 900px)').matches && !$('source-panel').classList.contains('open')) showPanel($('source-panel'));
}

function group(path) {
  const groups = ['预备篇', '第一篇 · 同步 RPC', '第二篇 · 事务与映射', '第三篇 · 对象与引用', '第四篇 · 进程上下文、对象与服务发现', '第五篇 · 线程、任务队列与回复', '第六篇 · 文件描述符', '第七篇 · 协议收敛'];
  if (path.includes('appendices/') && !/\/[0-7]-review\.md$/.test(path)) return '阅读指南';
  const match = path.match(/docs\/(?:0([0-7])-[^/]+\/|appendices\/([0-7])-review\.md)/);
  return groups[Number(match?.[1] ?? match?.[2] ?? 0)];
}
function renderNavigation() {
  const list = $('chapter-list');
  list.replaceChildren();
  let previous = '';
  chapters.forEach((chapter, index) => {
    const label = group(chapter.path);
    if (label !== previous) {
      const heading = document.createElement('div'); heading.className = 'group-label'; heading.textContent = label; list.append(heading); previous = label;
    }
    const button = document.createElement('button');
    button.className = `chapter-link${index === currentChapter ? ' active' : ''}`;
    const match = chapter.title.match(/^(?:(第)\s*)?(\d+\.\d+)(?:\s*章)?\s*(.*)$/i);
    const indexText = match ? (match[2] || match[1]) : '';
    const title = match ? match[3] : chapter.title;
    const num = document.createElement('span'); num.className = 'chapter-index'; num.textContent = indexText;
    const name = document.createElement('span'); name.textContent = title;
    button.append(num, name); button.onclick = () => { closePanels(); showChapter(index); };
    list.append(button);
  });
}
function sourceReferences() {
  const refs = [];
  for (const p of $('article').querySelectorAll('p')) {
    if (!p.textContent.includes('源码：')) continue;
    const code = [...p.querySelectorAll('code')].find(item => item.textContent.trim().startsWith('examples/'));
    const filename = code?.textContent.trim();
    if (!filename || !(filename in files)) continue;
    const text = p.textContent;
    const lineText = text.match(/第\s*([\d\s–—,，、-]+)\s*行/)?.[1] || '';
    const ranges = [...lineText.matchAll(/(\d+)(?:\s*[–—-]\s*(\d+))?/g)].map(m => [Number(m[1]), Number(m[2] || m[1])]);
    const button = document.createElement('button');
    button.className = 'source-ref';
    button.innerHTML = `${makeIcon('code-2')}<span></span>${makeIcon('arrow-up-right')}`;
    button.querySelector('span').textContent = text.slice(text.indexOf('源码：'));
    button.onclick = () => openSource(filename, ranges);
    const prefix = text.slice(0, text.indexOf('源码：')).trim();
    if (prefix) { p.textContent = prefix; p.after(button); } else p.replaceWith(button);
    const next = button.nextElementSibling;
    if (next?.tagName === 'PRE') { next.classList.add('source-click'); next.tabIndex = 0; next.setAttribute('role', 'button'); next.setAttribute('aria-label', `查看 ${filename} 源码`); next.onclick = button.onclick; next.onkeydown = e => { if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); button.click(); } }; }
    refs.push({ filename, ranges });
  }
  return refs;
}
function outline() {
  const list = $('section-list'); list.replaceChildren();
  $('chapter-outline').hidden = !$('article').querySelector('h2,h3');
  $('article').querySelectorAll('h2,h3').forEach((heading, i) => {
    heading.id = `section-${i}`;
    const a = document.createElement('a'); a.className = `outline-link${heading.tagName === 'H3' ? ' sub' : ''}`;
    a.href = `#section-${i}`; a.textContent = heading.textContent;
    a.onclick = e => { e.preventDefault(); closePanels(); $('chapter-outline').open = false; heading.scrollIntoView({ behavior: 'smooth', block: 'start' }); };
    list.append(a);
  });
}
function renderPagination() {
  const parent = $('pagination'); parent.replaceChildren();
  for (const [direction, index] of [['← 上一章', currentChapter - 1], ['下一章 →', currentChapter + 1]]) {
    if (!chapters[index]) { parent.append(document.createElement('span')); continue; }
    const button = document.createElement('button'); button.className = 'pager';
    button.textContent = `${direction}  ${chapters[index].title}`;
    button.onclick = () => showChapter(index); parent.append(button);
  }
}
mermaid.initialize({ startOnLoad: false, securityLevel: 'strict', theme: 'neutral', fontFamily: 'sans-serif' });
function addStaticFlow(diagram, svg, flow) {
  const title = flow?.kind === 'sequence' ? '完整时序图' : '完整图示';
  const figure = document.createElement('figure');
  figure.className = 'static-flow';
  figure.innerHTML = `<figcaption><strong>${title}</strong><button type="button" class="flow-enlarge" title="放大${title.slice(2)}" aria-label="放大${title.slice(2)}">${makeIcon('maximize-2')} 放大</button></figcaption><div class="static-flow-scroll"></div>`;
  figure.querySelector('.static-flow-scroll').innerHTML = svg;
  const dialog = document.createElement('dialog');
  dialog.className = 'flow-modal';
  dialog.setAttribute('aria-label', title);
  dialog.innerHTML = `<div class="flow-modal-header"><strong>${title}</strong><button type="button" class="icon-button" aria-label="关闭${title.slice(2)}" title="关闭${title.slice(2)}">${makeIcon('x')}</button></div><div class="flow-modal-scroll"></div>`;
  figure.append(dialog);
  figure.querySelector('.flow-enlarge').onclick = () => {
    const clone = figure.querySelector('.static-flow-scroll svg').cloneNode(true);
    const idMap = new Map();
    [clone, ...clone.querySelectorAll('[id]')].filter(node => node.id).forEach(node => { const id = node.id; const replacement = `zoom-${id}`; idMap.set(id, replacement); node.id = replacement; });
    clone.querySelectorAll('*').forEach(node => {
      if (node.tagName.toLowerCase() === 'style') {
        for (const [oldId, newId] of idMap) node.textContent = node.textContent.replaceAll(`#${oldId}`, `#${newId}`);
      }
      for (const attribute of [...node.attributes]) {
        let value = attribute.value;
        for (const [oldId, newId] of idMap) value = value.replaceAll(`url(#${oldId})`, `url(#${newId})`);
        if (idMap.has(value.slice(1)) && value.startsWith('#')) value = `#${idMap.get(value.slice(1))}`;
        if (value !== attribute.value) node.setAttribute(attribute.name, value);
      }
    });
    dialog.querySelector('.flow-modal-scroll').replaceChildren(clone);
    dialog.showModal();
  };
  dialog.querySelector('button').onclick = () => dialog.close();
  dialog.addEventListener('click', event => { if (event.target === dialog) dialog.close(); });
  diagram.before(figure);
  if (flow) {
    const heading = document.createElement('div'); heading.className = 'flow-section-heading'; heading.textContent = '单步演示';
    diagram.before(heading);
  }
  paintIcons();
}
async function drawDiagrams(version) {
  for (const [diagramIndex, code] of [...$('article').querySelectorAll('pre code.language-mermaid')].entries()) {
    const pre = code.parentElement;
    const diagram = document.createElement('div'); diagram.className = 'diagram';
    try {
      const { svg } = await mermaid.render(`diagram-${version}-${Math.random().toString(36).slice(2)}`, code.textContent);
      if (version !== diagramVersion) return;
      const flow = flows.find(item => item.chapter === chapters[currentChapter].path && item.diagram === diagramIndex);
      const staticSvg = (await mermaid.render(`static-${version}-${Math.random().toString(36).slice(2)}`, code.textContent)).svg;
      if (version !== diagramVersion) return;
      diagram.innerHTML = svg; pre.replaceWith(diagram);
      addStaticFlow(diagram, staticSvg, flow);
      if (flow) attachFlow(diagram, flow, { openSource, icon: makeIcon, paintIcons });
      else diagram.remove();
    } catch { pre.title = '图表渲染失败，显示原始内容'; }
  }
}
function showChapter(index) {
  currentChapter = index;
  const chapter = chapters[index];
  terminal.setChapter(chapter.path);
  $('top-chapter').textContent = chapter.title;
  $('breadcrumb-title').textContent = chapter.title;
  $('article').innerHTML = marked.parse(chapter.markdown);
  const refs = sourceReferences();
  outline(); renderNavigation(); renderPagination();
  $('reader').scrollTop = 0;
  history.replaceState(null, '', `#chapter=${encodeURIComponent(chapter.path)}`);
  if (refs[0] && window.matchMedia('(min-width: 901px)').matches) openSource(refs[0].filename, refs[0].ranges);
  drawDiagrams(++diagramVersion);
  paintIcons();
}
$('article').addEventListener('click', e => {
  const link = e.target.closest('a');
  if (!link) return;
  const href = link.getAttribute('href');
  if (!href || /^(https?:|mailto:|#)/.test(href)) return;
  const resolved = new URL(href, `https://binderabc.local/${chapters[currentChapter].path}`).pathname.slice(1).split('#')[0];
  const index = chapters.findIndex(chapter => chapter.path === decodeURIComponent(resolved));
  if (index >= 0) { e.preventDefault(); showChapter(index); }
});
const requested = new URLSearchParams(location.hash.slice(1)).get('chapter');
showChapter(Math.max(0, chapters.findIndex(chapter => chapter.path === requested)));
paintIcons();
