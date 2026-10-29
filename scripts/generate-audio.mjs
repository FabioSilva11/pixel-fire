// Sintetiza todos os efeitos sonoros e músicas do Zombie Zone (áudio 100% original).
// Uso: node scripts/generate-audio.mjs <pasta-wav-temporária> [nomes...]
// (com nomes, grava só esses arquivos; a síntese das outras faixas roda igual para manter o RNG estável)
// Depois converta para OGG com ffmpeg (ver scripts/build-audio.ps1).
import { mkdirSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';

const OUT = process.argv[2] ?? 'build/audio-wav';
const ONLY = new Set(process.argv.slice(3));
mkdirSync(OUT, { recursive: true });

let seed = 1234567;
const rand = () => { seed = (seed * 1664525 + 1013904223) >>> 0; return seed / 4294967296; };
const noise = () => rand() * 2 - 1;
const TAU = Math.PI * 2;

function buffer(seconds, rate = 44100) { return { rate, data: new Float32Array(Math.ceil(seconds * rate)) }; }
const env = (t, a, d) => (t < a ? t / a : Math.exp(-(t - a) / d));
const osc = {
  sine: (p) => Math.sin(TAU * p),
  square: (p) => ((p % 1) < 0.5 ? 1 : -1),
  saw: (p) => 2 * (p % 1) - 1,
  tri: (p) => 1 - 4 * Math.abs((p % 1) - 0.5),
};

function lowpass(data, rate, cutoff) {
  const rc = 1 / (TAU * cutoff), dt = 1 / rate, a = dt / (rc + dt);
  let y = 0;
  for (let i = 0; i < data.length; i++) { y += a * (data[i] - y); data[i] = y; }
  return data;
}
function highpass(data, rate, cutoff) {
  const rc = 1 / (TAU * cutoff), dt = 1 / rate, a = rc / (rc + dt);
  let y = 0, prev = 0;
  for (let i = 0; i < data.length; i++) { const x = data[i]; y = a * (y + x - prev); prev = x; data[i] = y; }
  return data;
}
// Passa-faixa biquad (formantes de voz dos zumbis).
function bandpass(input, rate, freq, q) {
  const out = new Float32Array(input.length);
  const w0 = TAU * freq / rate, alpha = Math.sin(w0) / (2 * q);
  const b0 = alpha, b2 = -alpha, a0 = 1 + alpha, a1 = -2 * Math.cos(w0), a2 = 1 - alpha;
  let x1 = 0, x2 = 0, y1 = 0, y2 = 0;
  for (let i = 0; i < input.length; i++) {
    const x = input[i];
    const y = (b0 * x + b2 * x2 - a1 * y1 - a2 * y2) / a0;
    out[i] = y; x2 = x1; x1 = x; y2 = y1; y1 = y;
  }
  return out;
}
function normalize(data, peak = 0.9) {
  let m = 0;
  for (const v of data) m = Math.max(m, Math.abs(v));
  if (m > 0) for (let i = 0; i < data.length; i++) data[i] *= peak / m;
  return data;
}
function fadeEdges(data, rate, ms = 6) {
  const n = Math.min(data.length >> 1, Math.floor(rate * ms / 1000));
  for (let i = 0; i < n; i++) { const g = i / n; data[i] *= g; data[data.length - 1 - i] *= g; }
  return data;
}
function finish(b, peak = 0.9, ms = 6) { normalize(b.data, peak); fadeEdges(b.data, b.rate, ms); return b; }
function writeWav(name, { rate, data }) {
  if (ONLY.size && !ONLY.has(name)) return;
  const bytes = Buffer.alloc(44 + data.length * 2);
  bytes.write('RIFF', 0); bytes.writeUInt32LE(36 + data.length * 2, 4); bytes.write('WAVE', 8);
  bytes.write('fmt ', 12); bytes.writeUInt32LE(16, 16); bytes.writeUInt16LE(1, 20); bytes.writeUInt16LE(1, 22);
  bytes.writeUInt32LE(rate, 24); bytes.writeUInt32LE(rate * 2, 28); bytes.writeUInt16LE(2, 32); bytes.writeUInt16LE(16, 34);
  bytes.write('data', 36); bytes.writeUInt32LE(data.length * 2, 40);
  for (let i = 0; i < data.length; i++) bytes.writeInt16LE(Math.max(-32767, Math.min(32767, Math.round(data[i] * 32767))), 44 + i * 2);
  writeFileSync(join(OUT, `${name}.wav`), bytes);
  console.log('gerado', name, (data.length / rate).toFixed(2) + 's');
}
const note = (n) => 440 * Math.pow(2, (n - 69) / 12);

// ------------------------------------------------------------------ efeitos

function shot() {
  const b = buffer(0.32); const { rate, data } = b;
  for (let i = 0; i < data.length; i++) {
    const t = i / rate;
    const crack = noise() * env(t, 0.001, 0.035);
    const body = Math.sin(TAU * (90 - t * 120) * t) * env(t, 0.002, 0.07) * 0.9;
    const tail = noise() * env(t, 0.01, 0.12) * 0.25;
    data[i] = crack + body + tail;
  }
  lowpass(data, rate, 5200);
  return finish(b, 0.95);
}

function click(data, rate, at, freq, gain = 1) {
  const start = Math.floor(at * rate);
  for (let i = 0; i < rate * 0.06 && start + i < data.length; i++) {
    const t = i / rate;
    data[start + i] += (noise() * 0.6 + Math.sin(TAU * freq * t) * 0.8) * env(t, 0.0005, 0.012) * gain;
  }
}

// Raspão metálico (pente deslizando, ferrolho correndo).
function scrape(data, rate, at, length, from, to, gain) {
  const start = Math.floor(at * rate), n = Math.floor(length * rate);
  let phase = 0;
  for (let i = 0; i < n && start + i < data.length; i++) {
    const t = i / rate, k = t / length;
    phase += (from + (to - from) * k) / rate;
    const amp = Math.sin(Math.PI * k) * gain;
    data[start + i] += (noise() * 0.55 + osc.saw(phase) * 0.25) * amp;
  }
}

// Batida grave (pente encaixando na arma).
function thunk(data, rate, at, freq, gain) {
  const start = Math.floor(at * rate);
  for (let i = 0; i < rate * 0.12 && start + i < data.length; i++) {
    const t = i / rate;
    data[start + i] += (Math.sin(TAU * freq * t) * 0.9 + noise() * 0.5) * env(t, 0.0008, 0.03) * gain;
  }
}

// Recarga do fuzil: trava solta, pente sai raspando, pente novo entra e bate no lugar.
function reload() {
  const b = buffer(1.05); const { rate, data } = b;
  click(data, rate, 0.02, 2400, 0.8);
  scrape(data, rate, 0.05, 0.16, 1400, 900, 0.45);
  thunk(data, rate, 0.24, 160, 0.5);
  scrape(data, rate, 0.52, 0.12, 900, 1500, 0.4);
  thunk(data, rate, 0.66, 210, 1.0);
  click(data, rate, 0.67, 2900, 1.0);
  click(data, rate, 0.74, 1300, 0.5);
  highpass(data, rate, 140);
  return finish(b, 0.9);
}

// Ferrolho: puxado para trás e solto — o "clac-clac" de arma carregada.
function rack() {
  const b = buffer(0.5); const { rate, data } = b;
  click(data, rate, 0.0, 2000, 0.7);
  scrape(data, rate, 0.01, 0.1, 1800, 1100, 0.6);
  thunk(data, rate, 0.11, 240, 0.7);
  click(data, rate, 0.115, 3400, 0.9);
  scrape(data, rate, 0.19, 0.06, 1200, 2200, 0.5);
  thunk(data, rate, 0.25, 300, 1.0);
  click(data, rate, 0.252, 4200, 1.0);
  highpass(data, rate, 160);
  return finish(b, 0.9);
}

function hit() {
  const b = buffer(0.18); const { rate, data } = b;
  for (let i = 0; i < data.length; i++) {
    const t = i / rate;
    data[i] = noise() * env(t, 0.001, 0.03) * 0.8 + Math.sin(TAU * 140 * t) * env(t, 0.001, 0.05);
  }
  lowpass(data, rate, 1800);
  return finish(b, 0.85);
}

function headshot() {
  const b = buffer(0.5); const { rate, data } = b;
  const h = hit().data;
  for (let i = 0; i < data.length; i++) {
    const t = i / rate;
    const ding = (Math.sin(TAU * 1568 * t) + 0.4 * Math.sin(TAU * 3136 * t) + 0.2 * Math.sin(TAU * 2350 * t)) * env(t, 0.002, 0.12) * 0.45;
    data[i] = (h[i] ?? 0) * 0.8 + ding;
  }
  return finish(b, 0.85);
}

function groan(base, length, wobble) {
  const b = buffer(length); const { rate, data } = b;
  const src = new Float32Array(data.length);
  let phase = 0;
  for (let i = 0; i < src.length; i++) {
    const t = i / rate;
    const f = base * (1 + 0.08 * Math.sin(TAU * wobble * t) + 0.03 * noise()) * (1 - 0.18 * t / length);
    phase += f / rate;
    const amp = Math.sin(Math.PI * Math.min(1, t / length)) ** 0.7;
    src[i] = (osc.saw(phase) * 0.7 + noise() * 0.18) * amp;
  }
  const f1 = bandpass(src, rate, 420, 4), f2 = bandpass(src, rate, 980, 6), f3 = bandpass(src, rate, 2300, 8);
  for (let i = 0; i < data.length; i++) data[i] = f1[i] * 1.0 + f2[i] * 0.6 + f3[i] * 0.2 + src[i] * 0.08;
  lowpass(data, rate, 3000);
  return finish(b, 0.8, 30);
}

function zombieDie() {
  const b = groan(110, 1.0, 5);
  const { rate, data } = b;
  for (let i = 0; i < data.length; i++) {
    const t = i / rate;
    data[i] *= 1 - t * 0.6;
    if (t > 0.65) data[i] += Math.sin(TAU * 70 * (t - 0.65)) * env(t - 0.65, 0.002, 0.08) * 0.8;
  }
  return finish(b, 0.85, 20);
}

function zombieAttack() {
  const b = groan(150, 0.45, 11);
  const { rate, data } = b;
  for (let i = 0; i < data.length; i++) {
    const t = i / rate;
    data[i] = data[i] * 0.7 + noise() * Math.exp(-((t - 0.2) ** 2) / 0.003) * 0.6;
  }
  return finish(b, 0.85, 15);
}

function hurt() {
  const b = buffer(0.35); const { rate, data } = b;
  let phase = 0;
  const src = new Float32Array(data.length);
  for (let i = 0; i < src.length; i++) {
    const t = i / rate;
    phase += (220 - 260 * t) / rate;
    src[i] = osc.saw(phase) * env(t, 0.01, 0.1);
  }
  const f = bandpass(src, rate, 700, 3);
  for (let i = 0; i < data.length; i++) {
    const t = i / rate;
    data[i] = f[i] + Math.sin(TAU * 80 * t) * env(t, 0.001, 0.05) * 0.5;
  }
  return finish(b, 0.85);
}

function arpeggio(notes, step, decay, wave = 'sine', length = 1.0, gain = 0.6) {
  const b = buffer(length); const { rate, data } = b;
  notes.forEach((n, k) => {
    const start = Math.floor(k * step * rate), f = note(n);
    for (let i = 0; start + i < data.length; i++) {
      const t = i / rate;
      const v = (osc[wave](f * t) * 0.7 + Math.sin(TAU * f * 2 * t) * 0.2) * env(t, 0.003, decay) * gain;
      data[start + i] += v;
    }
  });
  return finish(b, 0.8);
}

function portal() {
  const b = buffer(1.8); const { rate, data } = b;
  let p1 = 0, p2 = 0;
  for (let i = 0; i < data.length; i++) {
    const t = i / rate;
    const f = 180 + 520 * (t / 1.8) ** 1.5;
    p1 += f / rate; p2 += (f * 1.503) / rate;
    const amp = Math.sin(Math.PI * Math.min(1, t / 1.8));
    data[i] = (Math.sin(TAU * p1) * 0.5 + Math.sin(TAU * p2) * 0.3 + noise() * 0.25 * (1 - t / 1.8)) * amp;
  }
  lowpass(data, rate, 4000);
  return finish(b, 0.8, 20);
}

function swish(length, cutoff, gain) {
  const b = buffer(length); const { rate, data } = b;
  for (let i = 0; i < data.length; i++) {
    const t = i / rate;
    data[i] = noise() * Math.sin(Math.PI * Math.min(1, t / length)) * gain;
  }
  lowpass(data, rate, cutoff);
  return finish(b, gain);
}

function step() {
  const b = buffer(0.09); const { rate, data } = b;
  for (let i = 0; i < data.length; i++) { const t = i / rate; data[i] = noise() * env(t, 0.002, 0.02); }
  lowpass(data, rate, 1400); highpass(data, rate, 120);
  return finish(b, 0.5);
}

function splash() {
  const b = buffer(0.7); const { rate, data } = b;
  for (let i = 0; i < data.length; i++) { const t = i / rate; data[i] = noise() * env(t, 0.005, 0.12); }
  lowpass(data, rate, 2500);
  for (let k = 0; k < 7; k++) {
    const start = Math.floor((0.08 + rand() * 0.45) * rate), f = 500 + rand() * 900;
    for (let i = 0; i < rate * 0.05 && start + i < data.length; i++) {
      const t = i / rate;
      data[start + i] += Math.sin(TAU * (f + 3000 * t) * t) * env(t, 0.002, 0.012) * 0.4;
    }
  }
  return finish(b, 0.75);
}

function emptyClick() {
  const b = buffer(0.1); const { rate, data } = b;
  click(data, rate, 0.0, 3200, 1);
  return finish(b, 0.6);
}

function uiClick() {
  const b = buffer(0.06); const { rate, data } = b;
  for (let i = 0; i < data.length; i++) { const t = i / rate; data[i] = osc.square(1050 * t) * env(t, 0.001, 0.012) * 0.5; }
  lowpass(data, rate, 4000);
  return finish(b, 0.5, 2);
}

function jingle(notes, step, length, wave = 'square', decay = 0.18) {
  return arpeggio(notes, step, decay, wave, length, 0.5);
}

// ------------------------------------------------------------------ músicas

function music({ bpm, bars, chords, rate = 32000, seconds, dark }) {
  const beat = 60 / bpm;
  const total = seconds ?? bars * 4 * beat;
  const b = buffer(total, rate); const { data } = b;
  const add = (start, len, fn) => {
    const s = Math.floor(start * rate), n = Math.floor(len * rate);
    for (let i = 0; i < n; i++) {
      const idx = (s + i) % data.length; // envolve para loop perfeito
      data[idx] += fn(i / rate);
    }
  };
  const barLen = beat * 4;
  for (let bar = 0; bar < bars; bar++) {
    const chord = chords[bar % chords.length];
    const t0 = bar * barLen;
    // Pad
    for (const n of chord) {
      const f = note(n);
      add(t0, barLen + 0.4, (t) => (osc.tri(f * t) * 0.5 + Math.sin(TAU * f * 1.003 * t) * 0.5) * Math.min(1, t / 0.4) * Math.exp(-t * 0.25) * (dark ? 0.05 : 0.065));
    }
    // Baixo
    const root = chord[0] - 12;
    for (let k = 0; k < (dark ? 8 : 4); k++) {
      const f = note(root + (dark && k % 4 === 3 ? 7 : 0));
      const len = dark ? beat / 2 : beat;
      add(t0 + k * len, len, (t) => {
        const v = dark ? osc.saw(f * t) * 0.6 + osc.square(f * 0.5 * t) * 0.2 : Math.sin(TAU * f * t);
        return v * env(t, 0.005, dark ? 0.12 : 0.35) * (dark ? 0.16 : 0.14);
      });
    }
    // Arpejo
    const arp = [chord[0] + 12, chord[1] + 12, chord[2] + 12, chord[1] + 12, chord[0] + 24, chord[2] + 12, chord[1] + 12, chord[2] + 12];
    for (let k = 0; k < 8; k++) {
      if (!dark && k % 2 === 1 && bar % 2 === 0) continue;
      const f = note(arp[k]);
      add(t0 + k * beat / 2, beat / 2, (t) => (dark ? osc.square(f * t) * 0.4 : osc.tri(f * t)) * env(t, 0.004, dark ? 0.08 : 0.16) * (dark ? 0.06 : 0.075));
    }
    // Bateria
    for (let k = 0; k < 4; k++) {
      const bt = t0 + k * beat;
      if (dark || k % 2 === 0) add(bt, 0.3, (t) => Math.sin(TAU * (55 + 90 * Math.exp(-t * 30)) * t) * env(t, 0.001, 0.09) * (dark ? 0.5 : 0.25));
      if (k % 2 === 1) add(bt, 0.25, (t) => noise() * env(t, 0.001, dark ? 0.06 : 0.04) * (dark ? 0.22 : 0.1));
      add(bt + beat / 2, 0.06, (t) => noise() * env(t, 0.0005, 0.012) * (dark ? 0.09 : 0.05));
    }
  }
  highpass(data, rate, 30);
  lowpass(data, rate, dark ? 6000 : 5000);
  normalize(data, 0.7);
  return b;
}

const A = 57, C = 60, D = 62, E = 64, F = 65, G = 67;
writeWav('sfx_shot', shot());
writeWav('sfx_reload', reload());
writeWav('sfx_hit', hit());
writeWav('sfx_headshot', headshot());
writeWav('sfx_groan1', groan(98, 1.3, 4));
writeWav('sfx_groan2', groan(124, 1.0, 7));
writeWav('sfx_zombie_die', zombieDie());
writeWav('sfx_zombie_attack', zombieAttack());
writeWav('sfx_hurt', hurt());
writeWav('sfx_pickup', arpeggio([84, 88, 91, 96], 0.06, 0.18, 'sine', 0.6));
writeWav('sfx_heal', arpeggio([72, 76, 79, 84, 88], 0.07, 0.25, 'tri', 0.9));
writeWav('sfx_portal', portal());
writeWav('sfx_jump', swish(0.14, 2200, 0.5));
writeWav('sfx_step', step());
writeWav('sfx_empty', emptyClick());
writeWav('sfx_splash', splash());
writeWav('ui_click', uiClick());
writeWav('sfx_reward', jingle([83, 88], 0.08, 0.45, 'square', 0.15));
writeWav('sfx_victory', jingle([72, 76, 79, 84, 79, 84], 0.13, 1.5, 'square', 0.22));
writeWav('sfx_defeat', jingle([69, 65, 62, 57], 0.22, 1.6, 'tri', 0.35));
writeWav('sfx_unlock', arpeggio([76, 79, 83, 88, 91, 95], 0.07, 0.3, 'square', 1.1, 0.45));
writeWav('music_menu', music({ bpm: 88, bars: 16, chords: [[A, C, E], [F - 12 + 12, A, C], [C, E, G], [G - 12, 59, D]] }));
writeWav('music_battle', music({ bpm: 124, bars: 16, chords: [[A, C, E], [A, C, E], [F, A, C + 12], [G, 71, D + 12]], dark: true }));

// ------------------------------------------------------------------ habilidades dos zumbis

function throwWhoosh() {
  const b = buffer(0.32); const { rate, data } = b;
  for (let i = 0; i < data.length; i++) {
    const t = i / rate;
    const sweep = Math.sin(Math.PI * Math.min(1, t / 0.32)) ** 2;
    data[i] = noise() * sweep * (0.6 + 0.4 * Math.sin(TAU * 18 * t));
  }
  bandpass(data, rate, 900, 1.2).forEach((v, i) => { data[i] = v; });
  return finish(b, 0.7);
}

function fireBurst() {
  const b = buffer(1.4); const { rate, data } = b;
  for (let i = 0; i < data.length; i++) {
    const t = i / rate;
    const glass = t < 0.12 ? (Math.sin(TAU * 3100 * t) + Math.sin(TAU * 4700 * t) * 0.6) * env(t, 0.001, 0.03) * 0.5 : 0;
    const whoosh = noise() * env(t, 0.03, 0.45) * 0.9;
    const crackle = rand() < 0.004 ? noise() * 2.5 : 0;
    data[i] = glass + whoosh + crackle * Math.exp(-t * 1.5);
  }
  lowpass(data, rate, 3800);
  return finish(b, 0.85, 20);
}

function explosion() {
  const b = buffer(1.8); const { rate, data } = b;
  for (let i = 0; i < data.length; i++) {
    const t = i / rate;
    const boom = Math.sin(TAU * (60 - 30 * Math.min(1, t)) * t) * env(t, 0.002, 0.35) * 1.2;
    const blast = noise() * env(t, 0.001, 0.25);
    const rumble = noise() * env(t, 0.05, 0.7) * 0.4;
    data[i] = boom + blast + rumble;
  }
  lowpass(data, rate, 2200);
  return finish(b, 0.95, 30);
}

function knife() {
  const b = buffer(0.3); const { rate, data } = b;
  let phase = 0;
  for (let i = 0; i < data.length; i++) {
    const t = i / rate;
    phase += (2600 + 2200 * t) / rate;
    const ring = Math.sin(TAU * phase) * env(t, 0.004, 0.06) * 0.35;
    const swish = noise() * Math.sin(Math.PI * Math.min(1, t / 0.18)) * 0.6;
    data[i] = ring + swish;
  }
  highpass(data, rate, 1200);
  return finish(b, 0.75);
}

function scream() {
  const b = buffer(1.3); const { rate, data } = b;
  const src = new Float32Array(data.length);
  let phase = 0;
  for (let i = 0; i < src.length; i++) {
    const t = i / rate;
    const f = (520 + 260 * Math.sin(Math.PI * Math.min(1, t / 0.5))) * (1 + 0.04 * Math.sin(TAU * 9 * t) + 0.02 * noise());
    phase += f / rate;
    const amp = Math.sin(Math.PI * Math.min(1, t / 1.3)) ** 0.5;
    src[i] = (osc.saw(phase) * 0.7 + noise() * 0.3) * amp;
  }
  const f1 = bandpass(src, rate, 1100, 3), f2 = bandpass(src, rate, 2600, 5);
  for (let i = 0; i < data.length; i++) data[i] = f1[i] + f2[i] * 0.7 + src[i] * 0.05;
  return finish(b, 0.85, 25);
}

function rockThud() {
  const b = buffer(0.45); const { rate, data } = b;
  for (let i = 0; i < data.length; i++) {
    const t = i / rate;
    data[i] = Math.sin(TAU * (110 - 60 * t) * t) * env(t, 0.001, 0.08) + noise() * env(t, 0.001, 0.05) * 0.7 + noise() * env(t, 0.02, 0.12) * 0.15;
  }
  lowpass(data, rate, 1600);
  return finish(b, 0.85);
}

function spit() {
  const b = buffer(0.5); const { rate, data } = b;
  for (let i = 0; i < data.length; i++) {
    const t = i / rate;
    const gurgle = Math.sin(TAU * (180 + 60 * Math.sin(TAU * 23 * t)) * t) * env(t, 0.01, 0.12) * 0.5;
    data[i] = noise() * env(t, 0.005, 0.09) * 0.8 + gurgle;
  }
  bandpass(data, rate, 1400, 0.9).forEach((v, i) => { data[i] = v; });
  return finish(b, 0.75);
}

writeWav('sfx_throw', throwWhoosh());
writeWav('sfx_fire', fireBurst());
writeWav('sfx_explosion', explosion());
writeWav('sfx_knife', knife());
writeWav('sfx_scream', scream());
writeWav('sfx_rock', rockThud());
writeWav('sfx_chest', arpeggio([79, 83, 86, 91, 95, 98], 0.05, 0.22, 'square', 0.9, 0.42));
writeWav('sfx_spit', spit());
writeWav('sfx_rack', rack());

// ------------------------------------------------------------------ armas novas, troca de arma e ondas
// (no fim do arquivo para não mudar o RNG dos sons anteriores)

function gunshot({ seconds, crackDecay, bodyFreq, bodyDecay, tailDecay, tailGain, cutoff }) {
  const b = buffer(seconds); const { rate, data } = b;
  for (let i = 0; i < data.length; i++) {
    const t = i / rate;
    const crack = noise() * env(t, 0.001, crackDecay);
    const body = Math.sin(TAU * (bodyFreq - t * bodyFreq * 1.2) * t) * env(t, 0.002, bodyDecay) * 0.9;
    const tail = noise() * env(t, 0.01, tailDecay) * tailGain;
    data[i] = crack + body + tail;
  }
  lowpass(data, rate, cutoff);
  return b;
}


function shotgunShot() {
  const b = gunshot({ seconds: 0.75, crackDecay: 0.05, bodyFreq: 58, bodyDecay: 0.12, tailDecay: 0.22, tailGain: 0.45, cutoff: 3600 });
  // Bomba: "chk-chk" depois do disparo.
  scrape(b.data, b.rate, 0.42, 0.08, 900, 500, 0.25);
  click(b.data, b.rate, 0.5, 1800, 0.45);
  scrape(b.data, b.rate, 0.55, 0.07, 500, 900, 0.22);
  click(b.data, b.rate, 0.63, 2200, 0.5);
  return finish(b, 0.97, 10);
}

function equip() {
  const b = buffer(0.36); const { rate, data } = b;
  click(data, rate, 0.0, 1300, 0.6);
  scrape(data, rate, 0.05, 0.12, 600, 1000, 0.25);
  click(data, rate, 0.2, 2100, 0.8);
  return finish(b, 0.7);
}

// Corneta da horda (estilo "uma grande onda de zumbis está chegando").
function waveHorn() {
  const b = buffer(2.4); const { rate, data } = b;
  let phase = 0, phase2 = 0;
  for (let i = 0; i < data.length; i++) {
    const t = i / rate;
    const blast = t < 1.0 ? 0 : 1;
    const local = blast ? t - 1.05 : t;
    const len = blast ? 1.3 : 0.9;
    const k = Math.max(0, Math.min(1, local / len));
    const f = (blast ? 98 : 110) * (1 + 0.45 * Math.min(1, k * 3)) * (1 + 0.012 * Math.sin(TAU * 6 * t));
    phase += f / rate;
    phase2 += f * 1.5 / rate;
    const amp = local < 0 ? 0 : Math.sin(Math.PI * k) ** 0.4;
    data[i] = (osc.saw(phase) * 0.6 + osc.saw(phase2) * 0.3 + noise() * 0.05) * amp;
  }
  lowpass(data, rate, 1800);
  for (const at of [0.0, 0.45, 1.05, 1.5, 1.95]) thunk(data, rate, at, 55, 0.9);
  return finish(b, 0.85, 40);
}

writeWav('sfx_shotgun', shotgunShot());
writeWav('sfx_equip', equip());
writeWav('sfx_wave', waveHorn());
