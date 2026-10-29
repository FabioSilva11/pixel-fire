// Mostra o que há em cada FBX do Mixamo: clipes, ossos, malha com pele e direção dos braços na pose padrão.
import fs from 'node:fs';
import path from 'node:path';
import './shims.mjs';
import * as THREE from 'three';
import { FBXLoader } from 'three/addons/loaders/FBXLoader.js';

const loader = new FBXLoader();
for (const file of process.argv.slice(2)) {
  const buf = fs.readFileSync(file);
  const root = loader.parse(buf.buffer.slice(buf.byteOffset, buf.byteOffset + buf.byteLength), '');
  const bones = [];
  let skinned = 0;
  root.traverse(o => { if (o.isBone) bones.push(o); if (o.isSkinnedMesh) skinned++; });
  root.updateMatrixWorld(true);
  const get = n => bones.find(b => b.name.replace(/^mixamorig\d*[:_]?/i, '') === n);
  const dir = (a, b) => { const pa = get(a).getWorldPosition(new THREE.Vector3()); const pb = get(b).getWorldPosition(new THREE.Vector3()); return pb.sub(pa).normalize().toArray().map(v => v.toFixed(2)).join(','); };
  const hips = get('Hips').getWorldPosition(new THREE.Vector3());
  const head = get('HeadTop_End')?.getWorldPosition(new THREE.Vector3());
  console.log(path.basename(file));
  console.log('  ossos', bones.length, 'malhas com pele', skinned, 'escala raiz', root.scale.toArray().join(','));
  console.log('  clipes', root.animations.map(a => `${a.name} ${a.duration.toFixed(2)}s ${a.tracks.length} trilhas`).join(' | '));
  console.log('  quadril y', hips.y.toFixed(1), 'topo da cabeça y', head ? head.y.toFixed(1) : '?');
  console.log('  braço esq', dir('LeftArm', 'LeftForeArm'), ' braço dir', dir('RightArm', 'RightForeArm'), ' coxa esq', dir('LeftUpLeg', 'LeftLeg'));
  const skin = []; root.traverse(o => { if (o.isSkinnedMesh) skin.push(o); });
  if (skin.length) {
    // Pose de bind (T-pose do Mixamo) a partir das matrizes inversas do esqueleto.
    const sk = skin[0].skeleton;
    const i = sk.bones.findIndex(b => /LeftArm$/.test(b.name)), j = sk.bones.findIndex(b => /LeftForeArm$/.test(b.name));
    const pa = new THREE.Vector3().setFromMatrixPosition(sk.boneInverses[i].clone().invert());
    const pb = new THREE.Vector3().setFromMatrixPosition(sk.boneInverses[j].clone().invert());
    console.log('  bind braço esq', pb.sub(pa).normalize().toArray().map(v => v.toFixed(2)).join(','));
  }
}
