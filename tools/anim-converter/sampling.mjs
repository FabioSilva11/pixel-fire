import * as THREE from 'three';

export function createClipSampler(root, clip) {
  const mixer = new THREE.AnimationMixer(root);
  const action = mixer.clipAction(clip);
  action.setLoop(THREE.LoopOnce, 1);
  action.clampWhenFinished = true;
  action.play();
  return time => {
    action.time = time;
    mixer.update(0);
    root.updateMatrixWorld(true);
  };
}

export function sampleTime(frame, frames, duration, fps) {
  return frame === frames - 1 ? duration : Math.min(duration, frame / fps);
}

export function removeHorizontalDrift(positions) {
  // Snapshot endpoints before mutating the first frame in place.
  const a = positions[0].clone(), b = positions[positions.length - 1].clone();
  positions.forEach((d, i) => {
    const k = i / (positions.length - 1);
    d.x -= a.x + (b.x - a.x) * k;
    d.z -= a.z + (b.z - a.z) * k;
  });
}

