// Converte animações FBX do Mixamo para clipes do herói voxel do jogo (app/src/main/assets/anims/*.zza).
//
// O esqueleto do jogo usa os nomes do Mixamo, mas repousa com os braços para baixo (os clipes antigos
// contam com isso). O retarget usa só a diferença de cada osso em relação à T-pose do Mixamo, aplicada
// sobre a T-pose equivalente do herói (braços girados 90° em Z), e grava as rotações locais por quadro.
//
// Uso: node convert.mjs   (lê ~/Downloads e grava em app/src/main/assets/anims)
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import './shims.mjs';
import { createClipSampler, sampleTime, removeHorizontalDrift } from './sampling.mjs';
import * as THREE from 'three';
import { FBXLoader } from 'three/addons/loaders/FBXLoader.js';

const ROOT = path.resolve(path.dirname(new URL(import.meta.url).pathname.replace(/^\/([A-Za-z]:)/, '$1')), '../..');
const OUT = path.join(ROOT, 'app/src/main/assets/anims');
const DL = path.join(os.homedir(), 'Downloads');

// Arquivo do Mixamo -> clipe do jogo. loop: repete; drift: remove o deslocamento horizontal acumulado.
const JOBS = [
  { file:'Standing Aim Idle 01.fbx', id:'hero-aim-idle', loop:true, drift:true },
  { file:'Standing Death Left 01.fbx', id:'die', loop:false, drift:false },
  { file:'Mutant Walking.fbx', id:'zombie-walk', loop:true, drift:true },
  { file:'Bboy Hip Hop Move.fbx', id:'dance', loop:false, drift:true },
  { file:'Swimming.fbx', id:'swim', loop:true, drift:true },
];

// Esqueleto do herói (mesma hierarquia de render/blocky.cpp). Quadril a 12,5 px; 1 px = 0,085 unidade do modelo.
const PX = 0.085;
const HIPS_Y = 12.5 * PX;
const PARENT = { Hips:null, Spine:'Hips', Spine1:'Spine', Spine2:'Spine1', Neck:'Spine2', Head:'Neck', HeadTop_End:'Head', LeftEye:'Head', RightEye:'Head' };
for (const s of ['Left', 'Right']) {
  Object.assign(PARENT, { [s + 'Shoulder']:'Spine2', [s + 'Arm']:s + 'Shoulder', [s + 'ForeArm']:s + 'Arm', [s + 'Hand']:s + 'ForeArm' });
  for (const f of ['Thumb', 'Index', 'Middle', 'Ring', 'Pinky']) for (let i = 1; i <= 4; i++) PARENT[`${s}Hand${f}${i}`] = i === 1 ? s + 'Hand' : `${s}Hand${f}${i - 1}`;
  Object.assign(PARENT, { [s + 'UpLeg']:'Hips', [s + 'Leg']:s + 'UpLeg', [s + 'Foot']:s + 'Leg', [s + 'ToeBase']:s + 'Foot', [s + 'Toe_End']:s + 'ToeBase' });
}
const ORDER = [];
(function walk(n) { ORDER.push(n); for (const [c, p] of Object.entries(PARENT)) if (p === n) walk(c); })('Hips');

// Rotação (espaço do modelo) que leva o osso da pose de repouso do jogo à T-pose.
const Z = new THREE.Vector3(0, 0, 1);
function tposeRef(name) {
  const left = name.startsWith('Left'), right = name.startsWith('Right');
  if ((left || right) && /(Arm|ForeArm|Hand)/.test(name) && !/Shoulder/.test(name)) return new THREE.Quaternion().setFromAxisAngle(Z, left ? Math.PI / 2 : -Math.PI / 2);
  return new THREE.Quaternion();
}

const canon = n => n.replace(/^mixamorig\d*[:_]?/i, '').replace(/:/g, '');
const FPS = 30;
const loader = new FBXLoader();

function convert(job) {
  const src = path.join(DL, job.file);
  const buf = fs.readFileSync(src);
  const root = loader.parse(buf.buffer.slice(buf.byteOffset, buf.byteOffset + buf.byteLength), '');
  const clip = root.animations.filter(a => a.tracks.length).sort((a, b) => b.duration - a.duration)[0];
  if (!clip) throw new Error('sem animação em ' + job.file);
  const bones = new Map();
  root.traverse(o => { if (o.isBone) { const c = canon(o.name); if (PARENT[c] !== undefined && !bones.has(c)) bones.set(c, o); } });
  // Pose de repouso do arquivo = T-pose do Mixamo (conferido com inspect.mjs: braços em ±X).
  root.updateMatrixWorld(true);
  const rootInv = root.getWorldQuaternion(new THREE.Quaternion()).invert();
  const restInv = new Map();
  for (const [c, b] of bones) restInv.set(c, rootInv.clone().multiply(b.getWorldQuaternion(new THREE.Quaternion())).invert());
  const hips = bones.get('Hips');
  const hipsRest = root.worldToLocal(hips.getWorldPosition(new THREE.Vector3()));
  const scale = HIPS_Y / hipsRest.y;

  const sample = createClipSampler(root, clip);
  const frames = Math.max(2, Math.round(clip.duration * FPS) + 1);
  const used = ORDER.filter(n => bones.has(n));
  const rot = new Map(used.map(n => [n, []]));
  const hipsT = [];
  for (let f = 0; f < frames; f++) {
    sample(sampleTime(f, frames, clip.duration, FPS));
    const W = new Map();
    for (const n of ORDER) {
      const parentW = PARENT[n] ? W.get(PARENT[n]) : new THREE.Quaternion();
      const b = bones.get(n);
      const w = b ? rootInv.clone().multiply(b.getWorldQuaternion(new THREE.Quaternion())).multiply(restInv.get(n)).multiply(tposeRef(n)) : parentW.clone();
      W.set(n, w);
      if (b) {
        const local = parentW.clone().invert().multiply(w).normalize();
        const list = rot.get(n);
        const prev = list[list.length - 1];
        if (prev && prev.dot(local) < 0) local.set(-local.x, -local.y, -local.z, -local.w);
        list.push(local);
      }
    }
    const d = root.worldToLocal(hips.getWorldPosition(new THREE.Vector3())).sub(hipsRest).multiplyScalar(scale);
    hipsT.push(d);
  }
  if (job.drift) {
    // Tira a caminhada para a frente (o jogo move o personagem pela física), mantendo o balanço.
    removeHorizontalDrift(hipsT);
  }
  // Formato ZZA1: cabeçalho, ossos (nome + quaternions por quadro) e a translação do quadril.
  const nameBytes = used.map(n => Buffer.from(n, 'ascii'));
  const size = 4 + 16 + used.reduce((s, n, i) => s + 1 + nameBytes[i].length + frames * 16, 0) + frames * 12;
  const out = Buffer.alloc(size);
  let o = out.write('ZZA1', 0, 'ascii');
  out.writeUInt32LE(frames, o); o += 4;
  out.writeFloatLE(FPS, o); o += 4;
  out.writeUInt32LE(used.length, o); o += 4;
  out.writeUInt32LE(job.loop ? 1 : 0, o); o += 4;
  used.forEach((n, i) => {
    out.writeUInt8(nameBytes[i].length, o); o += 1;
    nameBytes[i].copy(out, o); o += nameBytes[i].length;
    for (const q of rot.get(n)) { out.writeFloatLE(q.x, o); out.writeFloatLE(q.y, o + 4); out.writeFloatLE(q.z, o + 8); out.writeFloatLE(q.w, o + 12); o += 16; }
  });
  for (const d of hipsT) { out.writeFloatLE(d.x, o); out.writeFloatLE(HIPS_Y + d.y, o + 4); out.writeFloatLE(d.z, o + 8); o += 12; }
  fs.mkdirSync(OUT, { recursive:true });
  fs.writeFileSync(path.join(OUT, job.id + '.zza'), out);
  return `${job.file} -> ${job.id}.zza: ${frames} quadros, ${used.length} ossos, ${(size / 1024).toFixed(0)} KB, ${clip.duration.toFixed(2)} s`;
}

for (const job of JOBS) {
  try { console.log(convert(job)); } catch (e) { console.error('ERRO', job.file, e.message); process.exitCode = 1; }
}
