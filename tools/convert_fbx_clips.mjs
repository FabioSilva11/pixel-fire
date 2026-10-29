#!/usr/bin/env node
// Retargets the upstream Bip01 FBX motion clips onto Zombie Zone's seven-joint blocky rig.
// Usage: node tools/convert_fbx_clips.mjs <fbx-animation-dir> <output-file> <three-package-dir>

import fs from 'node:fs';
import path from 'node:path';
import { pathToFileURL } from 'node:url';

const [sourceDir, outputFile, threeRoot] = process.argv.slice(2);
if (!sourceDir || !outputFile || !threeRoot) {
  console.error('Usage: node tools/convert_fbx_clips.mjs <fbx-animation-dir> <output-file> <three-package-dir>');
  process.exit(2);
}

// FBXLoader also creates texture objects; the source animations do not use those images.
globalThis.window = { URL: { createObjectURL: () => 'blob:ignored-animation-texture' } };
globalThis.document = {
  createElementNS: () => ({ addEventListener() {}, removeEventListener() {}, setAttribute() {}, style: {} }),
  createElement: () => ({ addEventListener() {}, removeEventListener() {}, setAttribute() {}, style: {} }),
};

const threeUrl = pathToFileURL(path.join(threeRoot, 'build', 'three.module.js')).href;
const loaderUrl = pathToFileURL(path.join(threeRoot, 'examples', 'jsm', 'loaders', 'FBXLoader.js')).href;
const THREE = await import(threeUrl);
const { FBXLoader } = await import(loaderUrl);

const clipsToConvert = [
  { name: 'idle', file: 'newtest_idle.fbx', duration: 3.2 },
  { name: 'walk', file: 'newtest_walk.fbx', duration: 1.0 },
  { name: 'walk-back', file: 'newtest_walk_back.fbx', duration: 1.0 },
  { name: 'run-back', file: 'newtest_run_back.fbx', duration: 0.62 },
  { name: 'crouch-walk', file: 'newtest_crouch_walk.fbx', duration: 1.1 },
  { name: 'crouch-back', file: 'newtest_crouch_walk.fbx', duration: 1.1, reverse: true },
  { name: 'crouch-idle', file: 'newtest_crouch_idle.fbx', duration: 3.2 },
  { name: 'knife-walk', file: 'newtest_melee_walk.fbx', duration: 0.8 },
  { name: 'knife-back', file: 'newtest_melee_walk_back.fbx', duration: 0.8 },
  { name: 'knife-sprint', file: 'newtest_melee_run.fbx', duration: 0.62 },
  { name: 'knife-strafe-left', file: 'newtest_melee_strafe_left.fbx', duration: 0.9 },
  { name: 'knife-strafe-right', file: 'newtest_melee_strafe_right.fbx', duration: 0.9 },
  { name: 'strafe-left', file: 'newtest_strafe_left.fbx', duration: 0.9 },
  { name: 'strafe-right', file: 'newtest_strafe_right.fbx', duration: 0.9 },
  { name: 'sprint', file: 'newtest_run.fbx', duration: 0.62 },
  { name: 'jump', file: 'newtest_jump.fbx', duration: 0.82 },
];

// Target node numbers from blocky.cpp: root, torso, head, right/left arms, right/left legs.
// Each mapping collapses a section of the source Bip01 chain onto one rigid target joint.
const targetMappings = [
  { target: 0, parent: 'Bip01', end: 'Bip01_Pelvis' },
  { target: 1, parent: 'Bip01_Pelvis', end: 'Bip01_Spine1' },
  { target: 2, parent: 'Bip01_Spine1', end: 'Bip01_Head' },
  { target: 3, parent: 'Bip01_Spine1', end: 'Bip01_R_Forearm' },
  { target: 4, parent: 'Bip01_Spine1', end: 'Bip01_L_Forearm' },
  { target: 5, parent: 'Bip01_Pelvis', end: 'Bip01_R_Calf' },
  { target: 6, parent: 'Bip01_Pelvis', end: 'Bip01_L_Calf' },
];
const fps = 24;

function sampleQuaternion(track, time) {
  if (!track || track.times.length === 0) return null;
  const times = track.times;
  const values = track.values;
  if (time <= times[0] || times.length === 1) return new THREE.Quaternion().fromArray(values, 0);
  const last = times.length - 1;
  if (time >= times[last]) return new THREE.Quaternion().fromArray(values, last * 4);
  let lo = 0;
  let hi = last;
  while (hi - lo > 1) {
    const mid = (lo + hi) >> 1;
    if (times[mid] <= time) lo = mid; else hi = mid;
  }
  const q0 = new THREE.Quaternion().fromArray(values, lo * 4);
  const q1 = new THREE.Quaternion().fromArray(values, hi * 4);
  const span = times[hi] - times[lo];
  return q0.slerp(q1, span > 1e-7 ? (time - times[lo]) / span : 0).normalize();
}

function serializeClip(clip) {
  const bytes = [];
  const pushU8 = value => bytes.push(value & 255);
  const pushU16 = value => { bytes.push(value & 255, (value >>> 8) & 255); };
  const pushF32 = value => {
    const temp = Buffer.allocUnsafe(4);
    temp.writeFloatLE(value);
    bytes.push(...temp);
  };
  const name = Buffer.from(clip.name, 'utf8');
  if (name.length > 255 || clip.channels.length > 255) throw new Error(`Clip ${clip.name} exceeds format limits`);
  pushU8(name.length);
  bytes.push(...name);
  pushF32(clip.duration);
  pushU8(clip.channels.length);
  for (const channel of clip.channels) {
    pushU8(channel.node);
    pushU8(1); // quaternion rotation
    pushU16(channel.keys.length);
    for (const key of channel.keys) {
      pushF32(key.time);
      for (const value of key.quaternion.toArray()) pushF32(value);
    }
  }
  return Buffer.from(bytes);
}

const converted = [];
for (const spec of clipsToConvert) {
  const bytes = fs.readFileSync(path.join(sourceDir, spec.file));
  const arrayBuffer = bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength);
  const object = new FBXLoader().parse(arrayBuffer, `${sourceDir}${path.sep}`);
  const sourceClip = object.animations?.[0];
  if (!sourceClip) throw new Error(`No animation clip in ${spec.file}`);

  const nodesByName = new Map();
  object.traverse(node => {
    if (node.isBone && !nodesByName.has(node.name)) nodesByName.set(node.name, node);
  });
  const tracksByName = new Map(sourceClip.tracks.map(track => [track.name, track]));
  const bindWorld = new Map();
  const animatedWorld = new Map();

  function worldQuaternion(node, time, cache) {
    if (cache.has(node)) return cache.get(node).clone();
    const track = tracksByName.get(`${node.name}.quaternion`);
    const local = time === null || !track
      ? node.quaternion.clone()
      : (sampleQuaternion(track, time) ?? node.quaternion.clone());
    const world = node.parent ? worldQuaternion(node.parent, time, cache).multiply(local).normalize() : local;
    cache.set(node, world.clone());
    return world;
  }

  const bindRelative = new Map();
  for (const mapping of targetMappings) {
    const parent = nodesByName.get(mapping.parent);
    const end = nodesByName.get(mapping.end);
    if (!parent || !end) throw new Error(`${spec.file} is missing the ${mapping.parent} -> ${mapping.end} bone chain`);
    const parentBind = worldQuaternion(parent, null, bindWorld);
    const endBind = worldQuaternion(end, null, bindWorld);
    bindRelative.set(mapping.target, parentBind.invert().multiply(endBind).normalize());
  }

  const duration = spec.duration;
  const keyCount = Math.max(2, Math.ceil(duration * fps) + 1);
  const channels = targetMappings.map(mapping => {
    const parent = nodesByName.get(mapping.parent);
    const end = nodesByName.get(mapping.end);
    const keys = [];
    for (let i = 0; i < keyCount; i += 1) {
      const progress = i / (keyCount - 1);
      const sourceTime = sourceClip.duration * (spec.reverse ? 1 - progress : progress);
      const parentWorld = worldQuaternion(parent, sourceTime, animatedWorld);
      const endWorld = worldQuaternion(end, sourceTime, animatedWorld);
      const animatedRelative = parentWorld.invert().multiply(endWorld).normalize();
      const delta = bindRelative.get(mapping.target).clone().invert().multiply(animatedRelative).normalize();
      keys.push({ time: progress * duration, quaternion: delta });
      animatedWorld.clear();
    }
    return { node: mapping.target, keys };
  });

  converted.push({ name: spec.name, duration, channels });
  console.log(`${spec.name}: ${spec.file} -> ${keyCount} frames, ${sourceClip.duration.toFixed(2)}s retimed to ${duration.toFixed(2)}s`);
}

const chunks = [Buffer.from('ZZAN'), Buffer.from([1, 0, converted.length & 255, (converted.length >>> 8) & 255])];
for (const clip of converted) chunks.push(serializeClip(clip));
fs.mkdirSync(path.dirname(path.resolve(outputFile)), { recursive: true });
fs.writeFileSync(outputFile, Buffer.concat(chunks));
console.log(`Wrote ${outputFile} (${Buffer.concat(chunks).length} bytes)`);
