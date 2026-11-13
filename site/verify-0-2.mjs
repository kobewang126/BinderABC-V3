import { chromium } from 'playwright';
import { pathToFileURL } from 'node:url';
import path from 'node:path';

const browser = await chromium.launch({ headless: true, executablePath: '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome' });
const page = await browser.newPage({ viewport: { width: 1440, height: 900 } });
const errors = [];
page.on('pageerror', error => errors.push(error.message));
await page.goto(pathToFileURL(path.resolve('dist/BinderABC-V3/index.html')).href);
await page.waitForSelector('.chapter-link');
if (await page.locator('#annotation-toggle, .code-comment').count()) throw new Error('Unexpected embedded line comments');
const dataCheck = await page.evaluate(() => {
  const data = window.BINDERABC_DATA;
  const chapter = data.chapters.find(item => item.path.endsWith('2.7-concurrent-transactions.md'));
  return !('lineComments' in data) && chapter?.markdown.includes('这些问题留到生命周期章节集中讨论') &&
    data.files['examples/2.3/include/mini_binder.h']?.includes('struct mini_binder_transaction_data');
});
if (!dataCheck) throw new Error('V3 chapter or source snapshot mismatch');
const chapterIndices = await page.evaluate(() => window.BINDERABC_DATA.chapters.flatMap((chapter, index) =>
  /^docs\/(?:00-preparation|01-rpc|02-transaction)\//.test(chapter.path) ? [index] : []));
if (chapterIndices.length !== 15) throw new Error('Expected 15 chapters in parts 0–2');
for (let i = 0; i < chapterIndices.length; i++) {
  await page.locator('.chapter-link').nth(chapterIndices[i]).click();
  const expected = await page.evaluate(index => {
    const chapter = window.BINDERABC_DATA.chapters[index];
    return {
      refs: (chapter.markdown.match(/源码：`examples\//g) || []).length,
      diagrams: (chapter.markdown.match(/^(?:```|~~~)mermaid/gm) || []).length
    };
  }, chapterIndices[i]);
  if (await page.locator('.source-ref').count() !== expected.refs) throw new Error(`Source references: chapter ${i}`);
  if (expected.diagrams) await page.waitForFunction(n => document.querySelectorAll('.static-flow-scroll svg').length === n, expected.diagrams, { timeout: 15000 });
  if (expected.diagrams) {
    await page.waitForFunction(n => document.querySelectorAll('.flow-panel').length === n, expected.diagrams, { timeout: 15000 });
    for (const panel of await page.locator('.flow-panel').all()) {
      while (await panel.locator('.flow-next').isEnabled()) {
        await panel.locator('.flow-next').click();
        if (!(await page.locator('.code-line.highlight').count())) throw new Error(`Flow source selection: chapter ${i}`);
      }
    }
  }
  if (await page.locator('.source-ref').count()) {
    await page.locator('.source-ref').first().click();
    if (!(await page.locator('.code-line.highlight').count())) throw new Error(`Source selection: chapter ${i}`);
  }
}
await page.locator('.chapter-link').filter({ hasText: '多笔事务与用户地址' }).click();
await page.waitForSelector('.flow-panel');
if (await page.locator('.flow-panel').first().locator('.flow-progress span').count() !== 4) throw new Error('2.7 should show only normal states');
await page.locator('.flow-panel').last().locator('.flow-next').click();
if (!(await page.locator('#file-path').innerText()).includes('examples/2.7/')) throw new Error('Concurrent flow source navigation');
await page.locator('.chapter-link').filter({ hasText: '使用 BC 和 BR 命令' }).click();
await page.waitForSelector('.flow-panel');
await page.locator('.flow-next').click();
if (!(await page.locator('#file-path').innerText()).includes('examples/2.6/')) throw new Error('BC/BR flow source navigation');
await page.locator('#terminal-chapter').click();
async function command(value) {
  await page.locator('#terminal-input').fill(value);
  await page.locator('#terminal-input').press('Enter');
}
for (const [chapter, programs] of [
  ['0.1', [['p01-call', 'add(1, 2) = 3'], ['p01-fork', 'value=20']]],
  ['0.2', [['p02-syscalls', 'open failed: errno=2']]],
  ['1.1', [['1.1-rpc-pipe', 'remote add(1, 2) = 3']]]
]) {
  const index = await page.evaluate(number => window.BINDERABC_DATA.chapters.findIndex(ch => ch.path.split('/').at(-1).startsWith(`${number}-`)), chapter);
  await page.locator('.chapter-link').nth(index).click();
  await page.locator('#terminal-chapter').click();
  await command('pwd');
  await command('make');
  for (const [program, expected] of programs) {
    await command(`./build/${program}`);
    if (!(await page.locator('#terminal-output').innerText()).includes(expected)) throw new Error(`Early chapter failed: ${chapter}/${program}`);
    if (!(await page.locator(`.generated-item[data-file="examples/${chapter}/build/${program}"]`).count())) throw new Error(`Missing chapter artifact: ${chapter}/${program}`);
  }
  if (!(await page.locator('#terminal-output').innerText()).includes(`/examples/${chapter}`)) throw new Error('Wrong chapter directory');
  await command('clear');
}
await command('cd ../0.1');
await command('make clean');
if (await page.locator('.generated-item[data-file^="examples/0.1/build/"]').count()) throw new Error('Chapter clean kept stale artifacts');
if (!(await page.locator('.generated-item[data-file="examples/0.2/build/p02-syscalls"]').count())) throw new Error('Clean removed another chapter artifacts');
await command('./build/p02-syscalls');
if (!(await page.locator('#terminal-output').innerText()).includes('未模拟构建该程序')) throw new Error('Cross-chapter binary incorrectly runnable');
await command('clear');
await page.locator('.chapter-link').filter({ hasText: '使用 BC 和 BR 命令' }).click();
await page.locator('#terminal-chapter').click();
await command('make');
await command('sudo insmod kernel/mini_binder.ko');
await command('./build/version');
await command('./build/calc_server');
await page.locator('.terminal-tab').nth(1).click();
await command('./build/calc_client add 7 3');
await command('./build/calc_test');
const output = await page.locator('#terminal-output').innerText();
if (!output.includes('add(7, 3) = 10') || !output.includes('arithmetic commands and error recovery passed')) throw new Error(`Terminal output: ${output}`);
await page.locator('.terminal-tab').first().click();
await page.locator('#terminal-input').press('Control+c');
await command('sudo rmmod mini_binder');
await page.locator('.chapter-link').filter({ hasText: '多笔事务与用户地址' }).click();
await page.locator('#terminal-chapter').click();
await command('make');
await command('sudo insmod kernel/mini_binder.ko');
await command('./build/version');
await command('timeout 15 ./build/concurrent_test');
await command('timeout 10 ./build/server_mapping_test');
const concurrentOutput = await page.locator('#terminal-output').innerText();
for (const expected of ['0207', 'two requests held before either reply', 'third request: EBUSY', 'reverse receive order', 'two slots, distinct user addresses, reverse replies, full capacity and reuse passed', 'server mapping ownership, unmap wakeup and remap passed']) {
  if (!concurrentOutput.includes(expected)) throw new Error(`Concurrent terminal output: ${concurrentOutput}`);
}
await page.screenshot({ path: '/tmp/binderabc-reader-0-2.png' });
if (errors.length) throw new Error(errors.join('\n'));
await browser.close();
console.log('Verified 15 chapters, source links, diagrams and calculator terminal.');
