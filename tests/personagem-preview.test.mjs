import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import vm from 'node:vm';

const html = readFileSync(new URL('../tools/personagem-voxel/index.html', import.meta.url), 'utf8');
const block = html.match(/\/\/ BEGIN PREVIEW TRANSPORT\r?\n([\s\S]*?)\/\/ END PREVIEW TRANSPORT/);
test('preview exposes a deterministic transport', () => assert.ok(block, 'missing preview transport'));
if (block) {
  const context = vm.createContext({});
  vm.runInContext(block[1] + '\nglobalThis.transport = createPreviewTransport();', context);
  const t = context.transport;
  test('single shot decays at native rifle rate and never repeats', () => {
    t.seek(0); t.scrubReload(-1); t.shoot();
    assert.equal(t.sample().kick, .05);
    t.advance(.05);
    assert.ok(Math.abs(t.sample().kick - .015) < 1e-12);
    t.advance(1);
    assert.equal(t.sample().kick, 0);
  });
  test('reload follows native envelope and left-hand magazine excursion', () => {
    t.reload(); t.advance(.5);
    assert.equal(t.sample().reloadProgress, .25);
    t.scrubReload(.25);
    assert.ok(Math.abs(t.sample().reload - Math.SQRT1_2) < 1e-12);
    assert.equal(t.sample().magazine, 1);
    assert.equal(t.shoot(), false);
    t.scrubReload(.75);
    assert.equal(t.sample().magazine, 0);
    t.advance(1);
    assert.equal(t.sample().reloadProgress, -1);
  });
  test('pause stops playback while explicit frame advance and seek are exact', () => {
    t.seek(2); t.paused = true; t.tick(.5);
    assert.equal(t.time, 2);
    t.advance(1 / 30);
    assert.equal(t.time, 2 + 1 / 30);
    t.seek(3.25);
    assert.equal(t.time, 3.25);
    assert.throws(() => t.advance(-1), /tempo/);
  });
  test('seek clears manual weapon events for repeatable pose-time selection', () => {
    t.shoot(); t.scrubReload(.4); t.seek(2);
    assert.equal(t.sample().kick, 0);
    assert.equal(t.sample().reloadProgress, -1);
  });
}
