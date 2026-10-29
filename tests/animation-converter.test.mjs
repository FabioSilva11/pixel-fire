import test from 'node:test';
import assert from 'node:assert/strict';
import * as THREE from '../tools/anim-converter/node_modules/three/build/three.module.js';
import { createClipSampler, sampleTime, removeHorizontalDrift } from '../tools/anim-converter/sampling.mjs';

test('horizontal drift removal preserves sway and vertical motion from an offset start', () => {
  const swayX = [0, 0.3, -0.2, 0.1, 0];
  const swayZ = [0, -0.1, 0.4, -0.2, 0];
  const height = [1, 1.2, 0.8, 1.1, 0.9];
  const positions = height.map((y, i) => new THREE.Vector3(4 + 8 * i / 4 + swayX[i], y, -3 + 6 * i / 4 + swayZ[i]));
  removeHorizontalDrift(positions);
  positions.forEach((p, i) => {
    assert.ok(Math.abs(p.x - swayX[i]) < 1e-12, `frame ${i} horizontal X sway`);
    assert.ok(Math.abs(p.z - swayZ[i]) < 1e-12, `frame ${i} horizontal Z sway`);
    assert.equal(p.y, height[i]);
  });
});

test('sampling preserves the real endpoint of a non-looping clip', () => {
  const root = new THREE.Object3D();
  const clip = new THREE.AnimationClip('endpoint', 1, [
    new THREE.VectorKeyframeTrack('.position', [0, 1], [0, 0, 0, 10, 2, 4]),
  ]);
  const sample = createClipSampler(root, clip);
  sample(0);
  sample(1);
  assert.deepEqual(root.position.toArray(), [10, 2, 4]);
  sample(0.5);
  assert.deepEqual(root.position.toArray(), [5, 1, 2]);
});

test('last frame samples exact duration when duration falls between 30 fps ticks', () => {
  const duration = 1.01;
  const frames = Math.max(2, Math.round(duration * 30) + 1);
  assert.equal(sampleTime(frames - 1, frames, duration, 30), duration);
  assert.equal(sampleTime(1, frames, duration, 30), 1 / 30);
});


test('generated clips contain finite normalized poses', async () => {
  const fs = await import('node:fs');
  const os = await import('node:os');
  const path = await import('node:path');
  const clips = new Map();
  for (const id of ['hero-aim-idle', 'die', 'zombie-walk', 'dance', 'swim']) {
    const data = fs.readFileSync(new URL(`../app/src/main/assets/anims/${id}.zza`, import.meta.url));
    assert.equal(data.toString('ascii', 0, 4), 'ZZA1');
    const frames = data.readUInt32LE(4), count = data.readUInt32LE(12);
    assert.equal(data.readFloatLE(8), 30);
    let offset = 20;
    const bones = new Map();
    for (let b = 0; b < count; b++) {
      const length = data.readUInt8(offset++);
      const name = data.toString('ascii', offset, offset + length);
      offset += length;
      const poses = [];
      for (let f = 0; f < frames; f++) {
        const q = Array.from({ length: 4 }, (_, i) => data.readFloatLE(offset + i * 4));
        offset += 16;
        assert.ok(q.every(Number.isFinite), `${id}/${name}/${f} finite`);
        assert.ok(Math.abs(Math.hypot(...q) - 1) < 1e-6, `${id}/${name}/${f} normalized`);
        poses.push(q);
      }
      bones.set(name, poses);
    }
    const translations = [];
    for (let f = 0; f < frames; f++) {
      const p = Array.from({ length: 3 }, (_, i) => data.readFloatLE(offset + i * 4));
      offset += 12;
      assert.ok(p.every(Number.isFinite));
      translations.push(p);
    }
    assert.equal(offset, data.length);
    clips.set(id, { bones, translations });
  }
});

test('dance endpoint matches its original FBX source', async t => {
  const fs = await import('node:fs');
  const os = await import('node:os');
  const path = await import('node:path');
  await import('../tools/anim-converter/shims.mjs');
  const { FBXLoader } = await import('../tools/anim-converter/node_modules/three/examples/jsm/loaders/FBXLoader.js');
  const source = path.join(os.homedir(), 'Downloads', 'Bboy Hip Hop Move.fbx');
  if (!fs.existsSync(source)) {
    t.skip('Source endpoint check requires Downloads/Bboy Hip Hop Move.fbx.');
    return;
  }
  const bytes = fs.readFileSync(source);
  const root = new FBXLoader().parse(bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength), '');
  const clip = root.animations.filter(a => a.tracks.length).sort((a, b) => b.duration - a.duration)[0];
  let hips;
  root.traverse(b => { if (b.isBone && /^(mixamorig\d*[:_]?)?Hips$/i.test(b.name)) hips = b; });
  root.updateMatrixWorld(true);
  const inverse = root.getWorldQuaternion(new THREE.Quaternion()).invert();
  const rest = inverse.clone().multiply(hips.getWorldQuaternion(new THREE.Quaternion())).invert();
  const restPosition = root.worldToLocal(hips.getWorldPosition(new THREE.Vector3()));
  // Evaluate the source using the independent mixer clock API.
  const sourceMixer = new THREE.AnimationMixer(root);
  const sourceAction = sourceMixer.clipAction(clip);
  sourceAction.setLoop(THREE.LoopOnce, 1);
  sourceAction.clampWhenFinished = true;
  sourceAction.play();
  sourceMixer.setTime(clip.duration);
  root.updateMatrixWorld(true);
  const expected = inverse.clone().multiply(hips.getWorldQuaternion(new THREE.Quaternion())).multiply(rest).normalize();
  const data = fs.readFileSync(new URL('../app/src/main/assets/anims/dance.zza', import.meta.url));
  const frames = data.readUInt32LE(4);
  const nameLength = data.readUInt8(20);
  assert.equal(data.toString('ascii', 21, 21 + nameLength), 'Hips');
  const rotationOffset = 21 + nameLength + (frames - 1) * 16;
  const actual = new THREE.Quaternion().fromArray(Array.from({ length: 4 }, (_, i) => data.readFloatLE(rotationOffset + i * 4)));
  const translation = Array.from({ length: 3 }, (_, i) => data.readFloatLE(data.length - 12 + i * 4));
  assert.ok(Math.abs(actual.dot(expected)) > 1 - 1e-6);
  const expectedY = 1.0625 + (root.worldToLocal(hips.getWorldPosition(new THREE.Vector3())).y - restPosition.y) * (1.0625 / restPosition.y);
  assert.ok(Math.abs(translation[1] - expectedY) < 1e-6);
  assert.ok(Math.abs(translation[0]) < 1e-6);
  assert.ok(Math.abs(translation[2]) < 1e-6);
});
