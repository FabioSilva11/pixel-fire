// Setup: npm install --prefix output/browser-qa --no-audit --no-fund playwright
// Run: node scripts/test-character-preview.cjs (installed Edge or Chrome required).
const assert = require('node:assert/strict');
const http = require('node:http');
const fs = require('node:fs/promises');
const path = require('node:path');
const { chromium } = require(process.env.ZZ_PLAYWRIGHT_MODULE || '../output/browser-qa/node_modules/playwright');
const root = path.resolve(__dirname, '..');

(async () => {
  const server = http.createServer(async (req, res) => {
    try {
      const file = path.resolve(root, '.' + decodeURIComponent(new URL(req.url, 'http://localhost').pathname));
      if (!file.startsWith(root + path.sep)) { res.writeHead(403).end(); return; }
      const target = (await fs.stat(file)).isDirectory() ? path.join(file, 'index.html') : file;
      res.setHeader('Content-Type', target.endsWith('.html') ? 'text/html' : target.endsWith('.mjs') ? 'text/javascript' : 'application/octet-stream');
      res.end(await fs.readFile(target));
    } catch { res.writeHead(404).end(); }
  });
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  let browser;
  try {
    for (const channel of ['msedge', 'chrome']) {
      try { browser = await chromium.launch({ channel, headless: true }); break; } catch {}
    }
    assert.ok(browser, 'Install Edge or Chrome to run browser checks');
    const page = await browser.newPage({ viewport: { width: 1280, height: 900 }, deviceScaleFactor: 1 });
    const errors = [];
    page.on('pageerror', error => errors.push(error.message));
    await page.goto(`http://127.0.0.1:${server.address().port}/tools/personagem-voxel/`, { waitUntil: 'networkidle' });
    await page.waitForFunction(() => !!window.__heroi, null, { timeout: 60000 });
    const capture = options => page.evaluate(options => window.__heroi.capture(options), options);
    const ready = await capture({ time: 0, view: 'front' });
    await page.locator('[data-view="side"]').click();
    const side = await page.evaluate(() => window.__heroi.snapshot());
    assert.equal(side.view, 'side');
    assert.ok(side.camera.position.every((value, axis) => Math.abs(value - [62, 19, 0][axis]) < 1e-8), 'view buttons must work while paused');
    const shot = await capture({ time: 0, view: 'front', shotAge: 0 });
    assert.notDeepEqual(shot.gun, ready.gun, 'single shot must move the rendered gun');
    const reload = await capture({ time: 0, view: 'front', reloadProgress: .25 });
    assert.notDeepEqual(reload.bones.LeftHand, ready.bones.LeftHand, 'reload must move the rendered support hand');
    for (const options of [{}, { anim: 'aim', view: 'shoulder', shotAge: .03 }, { reloadProgress: .25 }, { anim: 'unarmed' }, { anim: 'tpose' }]) {
      const first = await capture(options);
      await capture({ anim: 'run', time: 1.7, view: 'side' });
      assert.deepEqual(await capture(options), first, 'capture must ignore prior animation and camera history');
    }
    await capture({ time: 0, view: 'front' });
    const canvas = page.locator('#viewport canvas');
    const firstImage = await canvas.screenshot();
    await page.waitForTimeout(250);
    assert.deepEqual(await canvas.screenshot(), firstImage, 'paused image must remain stable');
    await page.getByRole('button', { name: 'Disparo único', exact: true }).click();
    assert.ok((await page.evaluate(() => window.__heroi.snapshot())).motion.kick > 0);
    await page.getByRole('button', { name: 'Recarregar', exact: true }).click();
    assert.equal((await page.evaluate(() => window.__heroi.snapshot())).motion.reloadProgress, 0);
    await page.getByRole('button', { name: '+1 quadro (30 fps)', exact: true }).click();
    assert.ok((await page.evaluate(() => window.__heroi.snapshot())).motion.reloadProgress > 0);
    await capture({ view: 'persp', reloadProgress: .25 });
    await fs.mkdir(path.join(root, 'output/browser-qa'), { recursive: true });
    await page.screenshot({ path: path.join(root, 'output/browser-qa/character-reload.png') });
    assert.deepEqual(errors, [], 'browser must not report JavaScript errors');
    console.log('PASS: real browser shot/reload controls, transforms, repeated captures and paused pixels');
  } finally {
    if (browser) await browser.close();
    await new Promise(resolve => server.close(resolve));
  }
})().catch(error => { console.error(error); process.exitCode = 1; });
