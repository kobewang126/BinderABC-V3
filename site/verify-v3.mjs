import { chromium } from 'playwright';
import { pathToFileURL } from 'node:url';
import path from 'node:path';

const browser = await chromium.launch({ headless: true, executablePath: process.env.CHROME_PATH || '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome' });
const page = await browser.newPage({ viewport: { width: 1440, height: 900 } });
const errors = [];
page.on('pageerror', error => errors.push(error.message));
await page.goto(pathToFileURL(path.resolve('dist/BinderABC-V3/index.html')).href);
await page.waitForSelector('.chapter-link');
const specs = await page.evaluate(() => window.BINDERABC_DATA.extraFlows.map(flow => ({ path: flow.chapter, count: flow.steps.length })));
for (const spec of specs) {
  const index = await page.evaluate(path => window.BINDERABC_DATA.chapters.findIndex(chapter => chapter.path === path), spec.path);
  await page.locator('.chapter-link').nth(index).click();
  await page.waitForSelector('.flow-panel');
  const panel = page.locator('.flow-panel').first();
  await page.locator('.flow-diagram .flow-event').first().click({ force: true });
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
  ['3.3', 'object_parcel_test', 'object offsets'],
  ['3.6', 'transfer_test', 'translated to recipient handle'],
  ['4.4', 'workers_test', 'queued before workers'],
  ['5.2', 'target_wakeup_test', 'target wake events 1; other wake events 0'],
  ['6.1', 'fd_demo', 'independent open: ab'],
  ['6.4', 'fd_reply_demo', 'reply fd 4 read: data']
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
await page.locator('.chapter-link').filter({ hasText: '第五篇自测' }).click();
if (!(await page.locator('#article').innerText()).includes('参考答案')) throw new Error('Missing review answers');
if (errors.length) throw new Error(errors.join('\n'));
await browser.close();
console.log(`Verified ${specs.length} additional interactive diagrams, source links, review and terminal workflows.`);
