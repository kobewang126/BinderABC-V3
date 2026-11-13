import { chromium } from 'playwright';
import { pathToFileURL } from 'node:url';
import path from 'node:path';

const browser = await chromium.launch({ headless: true, executablePath: process.env.CHROME_PATH || '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome' });
const page = await browser.newPage({ viewport: { width: 1440, height: 900 } });
const errors = [];
page.on('pageerror', error => errors.push(error.message));
await page.goto(pathToFileURL(path.resolve('dist/BinderABC-V3/index.html')).href);
await page.waitForSelector('.chapter-link');
const downloadEvent = page.waitForEvent('download');
await page.locator('#download-source').click();
const sourceDownload = await downloadEvent;
if (sourceDownload.suggestedFilename() !== 'BinderABC-V3-source.zip' || await sourceDownload.failure()) {
  throw new Error('Source ZIP download failed');
}
const counts = await page.evaluate(() => ({ chapters: window.BINDERABC_DATA.chapters.filter(ch => !ch.path.includes('/appendices/')).length, entries: window.BINDERABC_DATA.chapters.length }));
if (counts.chapters !== 39 || counts.entries !== 49) throw new Error(`Incomplete navigation: ${JSON.stringify(counts)}`);
// 所有章节共用顶部悬浮目录；展开、收起不能改变正文或滚动位置。
async function articleLayout() {
  return page.evaluate(() => {
    const rect = document.getElementById('article').getBoundingClientRect();
    return { x: rect.x, y: rect.y, width: rect.width, scroll: document.getElementById('reader').scrollTop };
  });
}
function unchangedLayout(before, after, index) {
  if (Object.keys(before).some(key => Math.abs(before[key] - after[key]) > 1)) {
    throw new Error(`Outline moved article ${index}: ${JSON.stringify({ before, after })}`);
  }
}
for (let index = 0; index < counts.entries; index++) {
  await page.locator('.chapter-link').nth(index).click();
  await page.evaluate(() => document.getElementById('reader').scrollTo({ top: 0, behavior: 'instant' }));
  const headings = await page.locator('#article h2[id], #article h3[id]').count();
  if (await page.locator('#section-list a').count() !== headings) throw new Error(`Outline count: ${index}`);
  if (await page.locator('#navigation #section-list').count()) throw new Error('Outline remained in chapter sidebar');
  if (!headings) continue;
  const details = page.locator('#chapter-outline');
  if (await details.evaluate(node => node.open)) await details.locator('summary').click();
  if (await page.locator('#section-list').isVisible()) throw new Error('Outline did not collapse');
  const before = await articleLayout();
  await details.locator('summary').click();
  if (!(await page.locator('#section-list').isVisible())) throw new Error('Outline did not expand');
  unchangedLayout(before, await articleLayout(), index);
  await details.locator('summary').click();
  unchangedLayout(before, await articleLayout(), index);
  await details.locator('summary').click();
  const target = await page.locator('#section-list a').last().getAttribute('href');
  if (!(await page.locator(target).count())) throw new Error(`Outline missing heading: ${index}`);
  await page.locator('#section-list a').last().click();
  if (await details.evaluate(node => node.open)) throw new Error('Outline did not close after navigation');
}
const specs = await page.evaluate(() => window.BINDERABC_DATA.extraFlows.map(flow => ({ path: flow.chapter, count: flow.steps.length, diagram: flow.diagram })));
for (const spec of specs) {
  const index = await page.evaluate(path => window.BINDERABC_DATA.chapters.findIndex(chapter => chapter.path === path), spec.path);
  await page.locator('.chapter-link').nth(index).click();
  await page.waitForFunction(n => document.querySelectorAll('.flow-panel').length > n, spec.diagram || 0);
  const panel = page.locator('.flow-panel').nth(spec.diagram || 0);
  await page.locator('.flow-diagram').nth(spec.diagram || 0).locator('.flow-event').first().click({ force: true });
  if (!(await page.locator('.code-line.highlight').count())) throw new Error(`Diagram click did not select source: ${spec.path}`);
  for (let step = 0; step < spec.count; step++) {
    await panel.locator('.flow-source').click();
    if (!(await page.locator('.code-line.highlight').count())) throw new Error(`Missing flow source: ${spec.path} step ${step}`);
    if (step < spec.count - 1) await panel.locator('.flow-next').click();
  }
}
async function command(text) {
  await page.locator('#terminal-input').fill(text);
  await page.locator('#terminal-input').press('Enter');
}
for (const [chapter, program, expected] of [
  ['3.3', 'object_parcel_test', 'offsets[1]=20 -> HANDLE'],
  ['4.2', 'manager_test --demo', 'client: get calculator through handle 0 -> local handle 1'],
  ['3.6', 'transfer_test', 'translated to recipient handle'],
  ['4.4', 'workers_test', 'queued before workers'],
  ['5.2', 'target_wakeup_test', 'target wake events 1; other wake events 0'],
  ['6.1', 'fd_demo', 'independent open: ab'],
  ['6.3', 'fd_failure_test --fd-full', 'first reservation rolled back'],
  ['6.4', 'fd_reply_demo', 'reply fd 4 read: data'],
  ['4.3', 'thread_test --demo', 'workers 3 and 2'],
  ['5.1', 'thread_reply_test --demo', 'reverse replies and thread isolation passed'],
  ['5.2', 'thread_reply_test --demo', 'transaction 2'],
  ['5.3', 'thread_exit_test --demo', 'next caller reused it'],
  ['7.1', 'object_demo', 'same local object reused'],
  ['7.2', 'object_demo', 'same local object reused'],
  ['7.3', 'looper_demo', 'BC_EXIT_LOOPER completed'],
  ['7.4', 'write_read_test', 'exactly once']
]) {
  const index = await page.evaluate(number => window.BINDERABC_DATA.chapters.findIndex(ch => ch.path.split('/').at(-1).startsWith(`${number}-`)), chapter);
  await page.locator('.chapter-link').nth(index).click();
  await page.locator('#terminal-chapter').click();
  await command('make');
  if (!['3.3', '6.1'].includes(chapter)) {
    await command('sudo insmod kernel/mini_binder.ko');
    await command('./build/version');
  }
  await command(`${chapter === '5.2' ? 'sudo ' : ''}./build/${program}`);
  const output = await page.locator('#terminal-output').innerText();
  if (!output.includes(expected) || !output.includes('模拟回放')) throw new Error(`Terminal ${chapter}: ${output}`);
  if (!['3.3', '6.1'].includes(chapter)) await command('sudo rmmod mini_binder');
  await command('clear');
}
await page.locator('.chapter-link').filter({ hasText: '一个服务端，两个用户态对象' }).click();
await page.locator('#terminal-chapter').click();
await command('make');
await command('sudo insmod kernel/mini_binder.ko');
await command('./build/object_server');
await page.locator('.terminal-tab').nth(1).click();
await command('./build/object_client 1 sub 7 3');
await command('./build/object_client 2 sub 7 3');
await command('./build/object_client 1 add 7 3');
await command('./build/object_test');
const objectOutput = await page.locator('#terminal-output').innerText();
for (const expected of ['object 1: sub(7, 3) = 4', 'object 2: sub(7, 3) = -4', 'object 1: add(7, 3) = 10', 'two objects, invalid target and code, recovery passed']) {
  if (!objectOutput.includes(expected)) throw new Error(`Object terminal: ${objectOutput}`);
}
await page.locator('.terminal-tab').first().click();
await page.locator('#terminal-input').press('Control+c');
await command('sudo rmmod mini_binder');
const finalIndex = await page.evaluate(() => window.BINDERABC_DATA.chapters.findIndex(ch => ch.path.includes('/7.4-')));
await page.locator('.chapter-link').nth(finalIndex).click();
await page.locator('#terminal-chapter').click();
await command('make');
await command('sudo insmod kernel/mini_binder.ko');
await command('./build/servicemanager');
await page.locator('.terminal-tab').nth(1).click();
await page.locator('#terminal-chapter').click();
await command('./build/calculator_server');
await page.locator('.terminal-tab').nth(2).click();
await page.locator('#terminal-chapter').click();
await command('./build/calculator_client missing');
await command('./build/calculator_client');
await command('./build/service_parcel_test');
let finalOutput = await page.locator('#terminal-output').innerText();
for (const text of ['getService(missing): -2', 'remote add(7, 3) = 10', 'remote sub(7, 3) = 4', 'valid call recovered']) {
  if (!finalOutput.includes(text)) throw new Error(`7.4 terminal missing ${text}: ${finalOutput}`);
}
await command('./build/calculator_server');
if (!(await page.locator('#terminal-output').innerText()).includes('addService failed: -17')) throw new Error('Duplicate registration not rejected');
await page.locator('.terminal-tab').nth(1).click();
if (!(await page.locator('#terminal-output').innerText()).includes('add(7, 3) = 10')) throw new Error('Server did not log call');
await page.locator('#terminal-input').press('Control+c');
await page.locator('.terminal-tab').first().click();
if (!(await page.locator('#terminal-output').innerText()).includes('getService(calculator): return HANDLE object')) throw new Error('Manager did not log query');
await page.locator('#terminal-input').press('Control+c');
await command('sudo rmmod mini_binder');
await page.screenshot({ path: '/tmp/binderabc-v3-site.png' });
const overviewIndex = await page.evaluate(() => window.BINDERABC_DATA.chapters.findIndex(ch => ch.path.endsWith('7.5-driver-api.md')));
await page.locator('.chapter-link').nth(overviewIndex).click();
await page.locator('#terminal-chapter').click();
await command('pwd');
if (!(await page.locator('#terminal-output').innerText()).includes('/examples/7.4')) throw new Error('Overview must reuse 7.4 sources');
await page.locator('#chapter-outline summary').click();
await page.screenshot({path:'/tmp/binderabc-v3-overview-outline.png'});
await page.locator('#chapter-outline summary').click();
await page.locator('.chapter-link').filter({ hasText: '第五篇自测' }).click();
if (!(await page.locator('#article').innerText()).includes('参考答案')) throw new Error('Missing review answers');
if (errors.length) throw new Error(errors.join('\n'));
await browser.close();
console.log(`Verified ${specs.length} additional interactive diagrams, source links, review and terminal workflows.`);
