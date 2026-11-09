export function attachFlow(diagram, flow, { openSource, icon, paintIcons }) {
  const steps = flow.steps;
  const svg = diagram.querySelector('svg');
  const candidates = [...svg.querySelectorAll(flow.kind === 'concept' ? '.node' : '.messageText')];
  const used = new Set();
  const labels = flow.events.map(event => {
    const label = candidates.find(candidate => !used.has(candidate) && candidate.textContent.trim() === event);
    if (label) used.add(label);
    return label;
  });
  const arrows = flow.kind === 'sequence' ? [...svg.querySelectorAll('.messageLine0, .messageLine1')] : [];
  if (labels.some(label => !label) || (flow.kind === 'sequence' && arrows.length !== flow.events.length)) throw new Error(`流程图事件不匹配: ${flow.chapter}`);
  diagram.classList.add('flow-diagram');
  diagram.setAttribute('aria-label', `${flow.title}，事件可点击`);
  const panel = document.createElement('section');
  panel.className = 'flow-panel';
  panel.setAttribute('aria-label', `${flow.title}单步演示`);
  panel.innerHTML = `
    <div class="flow-heading"><div><span class="flow-kicker">INTERACTIVE TRACE</span><strong class="flow-name"></strong></div><span class="flow-badge">教学回放</span></div>
    <div class="flow-progress" aria-hidden="true"></div>
    <div class="flow-info"><div class="flow-info-top"><span class="flow-count"></span><span class="flow-context"></span></div><h3 class="flow-title"></h3><p class="flow-description"></p><button class="flow-source" type="button"></button></div>
    <div class="flow-controls"><button class="flow-reset" type="button" title="回到第一步" aria-label="回到第一步">${icon('rotate-ccw')}</button><span class="flow-notice"></span><div class="flow-navigation"><button class="flow-previous" type="button">${icon('arrow-left')} 上一步</button><button class="flow-next" type="button">下一步 ${icon('arrow-right')}</button></div></div>`;
  panel.querySelector('.flow-name').textContent = `${flow.title} · 流程演示`;
  panel.querySelector('.flow-notice').textContent = flow.kind === 'concept' ? '概念示意 · 并非执行轨迹' : '正常路径示例 · 非真实调试';
  diagram.after(panel);
  const progress = panel.querySelector('.flow-progress');
  steps.forEach(() => { const bar = document.createElement('span'); progress.append(bar); });
  const progressItems = [...progress.children];
  const previous = panel.querySelector('.flow-previous');
  const next = panel.querySelector('.flow-next');
  const source = panel.querySelector('.flow-source');
  let index = 0;
  function select(i, reveal = true) {
    index = Math.max(0, Math.min(steps.length - 1, i));
    const step = steps[index];
    panel.querySelector('.flow-count').textContent = `${String(index + 1).padStart(2, '0')} / ${steps.length}`;
    panel.querySelector('.flow-context').textContent = step.context;
    panel.querySelector('.flow-context').classList.toggle('kernel', step.context.includes('内核态'));
    panel.querySelector('.flow-title').textContent = step.title;
    panel.querySelector('.flow-description').textContent = step.detail;
    source.textContent = `${step.file}  ·  ${step.ranges.map(([a, b]) => a === b ? a : `${a}–${b}`).join('、')} 行 ↗`;
    previous.disabled = index === 0;
    next.disabled = index === steps.length - 1;
    progressItems.forEach((bar, n) => bar.classList.toggle('done', n <= index));
    labels.forEach((label, n) => { label.classList.toggle('flow-active', n === step.message); label.setAttribute('aria-current', n === step.message ? 'step' : 'false'); });
    arrows.forEach((arrow, n) => arrow.classList.toggle('flow-active', n === step.message));
    const activeRect = labels[step.message].getBoundingClientRect();
    const diagramRect = diagram.getBoundingClientRect();
    diagram.scrollTop += activeRect.top - diagramRect.top - diagram.clientHeight / 2;
    diagram.scrollLeft += activeRect.left + activeRect.width / 2 - diagramRect.left - diagram.clientWidth / 2;
    if (reveal) openSource(step.file, step.ranges, { explanation: { label: `步骤解读 · ${step.context}`, text: step.detail } });
  }
  labels.forEach((label, messageIndex) => {
    label.classList.add('flow-event');
    label.setAttribute('role', 'button');
    label.setAttribute('tabindex', '0');
    label.setAttribute('aria-label', `查看步骤：${label.textContent}`);
    const open = () => select(steps.findIndex(step => step.message === messageIndex));
    label.addEventListener('click', open);
    label.addEventListener('keydown', event => { if (event.key === 'Enter' || event.key === ' ') { event.preventDefault(); open(); } });
    if (arrows[messageIndex]) { arrows[messageIndex].classList.add('flow-arrow'); arrows[messageIndex].addEventListener('click', open); }
  });
  previous.onclick = () => select(index - 1);
  next.onclick = () => select(index + 1);
  panel.querySelector('.flow-reset').onclick = () => select(0);
  source.onclick = () => openSource(steps[index].file, steps[index].ranges, { explanation: { label: `步骤解读 · ${steps[index].context}`, text: steps[index].detail } });
  paintIcons();
  select(0, false);
}
