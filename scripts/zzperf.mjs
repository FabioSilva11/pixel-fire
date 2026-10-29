#!/usr/bin/env node
// Automação de medição de desempenho (tag ZZPerf). Sem dependências. Exige um aparelho no adb e um APK DEBUG.
//   node scripts/zzperf.mjs run     --label baseline --perf 1 --stage 2 [--quality 1] [--zombies -1] [--install] [--runs 3]
//   node scripts/zzperf.mjs bench   --label baseline --stage 2
//   node scripts/zzperf.mjs live
//   node scripts/zzperf.mjs compare perf-results/a.json perf-results/b.json
import { execFileSync, spawn } from 'node:child_process';
import { existsSync, mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { join, resolve } from 'node:path';

const PKG = 'com.fabiosilva.ecosdeeldra';
const ROOT = resolve(new URL('..', import.meta.url).pathname.replace(/^\/([A-Za-z]:)/, '$1'));

function findAdb() {
  const candidates = [];
  for (const env of ['ANDROID_HOME', 'ANDROID_SDK_ROOT']) if (process.env[env]) candidates.push(join(process.env[env], 'platform-tools', 'adb.exe'), join(process.env[env], 'platform-tools', 'adb'));
  const lp = join(ROOT, 'local.properties');
  if (existsSync(lp)) {
    const m = /^sdk\.dir=(.+)$/m.exec(readFileSync(lp, 'utf8'));
    if (m) candidates.push(join(m[1].trim().replace(/\\:/g, ':').replace(/\\\\/g, '\\'), 'platform-tools', 'adb.exe'));
  }
  if (process.env.LOCALAPPDATA) candidates.push(join(process.env.LOCALAPPDATA, 'Android', 'Sdk', 'platform-tools', 'adb.exe'));
  return candidates.find(existsSync) ?? 'adb';
}
const ADB = findAdb();
const adb = (...args) => execFileSync(ADB, args, { encoding: 'utf8', maxBuffer: 64 * 1024 * 1024 });

function requireDevice() {
  const out = adb('devices', '-l').split('\n').slice(1).filter((l) => /\sdevice\s/.test(l));
  if (!out.length) {
    console.error('Nenhum aparelho no adb (adb devices -l vazio). Conecte por USB ou "adb connect IP:5555".');
    process.exit(2);
  }
}

function parseArgs(argv) {
  const o = { _: [] };
  for (let i = 0; i < argv.length; i++) {
    if (argv[i].startsWith('--')) {
      const k = argv[i].slice(2);
      const next = argv[i + 1];
      if (next === undefined || next.startsWith('--')) o[k] = true;
      else { o[k] = next; i++; }
    } else o._.push(argv[i]);
  }
  return o;
}

const KV = /([\w.]+)=(\S+)/g;
function kv(line) {
  const r = {};
  for (const m of line.matchAll(KV)) r[m[1]] = Number.isNaN(Number(m[2])) ? m[2] : Number(m[2]);
  return r;
}

const WIN_FIELDS = ['n', 'fps', 'int.avg', 'int.p50', 'int.p95', 'int.max', 'work.avg', 'work.p95', 'work.max', 's.upd', 's.str', 's.nav', 's.mini', 's.scene', 's.rend', 's.upl', 's.hud',
  'slow33', 'slow50', 'gen.ps', 'mesh.ps', 'up.ps', 'pend', 'pend.max', 'chunks', 'gpuMB', 'dc.chunks', 'dc.chars', 'dc.boxes', 'dc.bb', 'cull'];

function parseLog(text) {
  const out = { windows: [], hitches: [], load: null, run: null, bench: [] };
  for (const raw of text.split('\n')) {
    const i = raw.indexOf('ZZPerf');
    const line = i >= 0 ? raw.slice(i + 6).replace(/^[:\s]+/, '') : raw.replace(/^.*?ZZPerf\s*:?\s*/, '');
    if (line.startsWith('win ')) {
      const w = kv(line);
      const missing = WIN_FIELDS.filter((f) => !(f in w));
      if (missing.length) throw new Error(`linha win sem campos: ${missing.join(', ')}`);
      out.windows.push(w);
    } else if (line.startsWith('hitch ')) out.hitches.push(kv(line));
    else if (line.startsWith('load ')) out.load = kv(line);
    else if (line.startsWith('run done')) out.run = kv(line);
    else if (line.startsWith('bench result') || line.startsWith('bench compare') || line.startsWith('selftest')) out.bench.push(line);
  }
  return out;
}

function deviceInfo() {
  const prop = (p) => adb('shell', 'getprop', p).trim();
  const grab = (cmd) => { try { return adb('shell', cmd).trim(); } catch { return ''; } };
  return { device: prop('ro.product.model'), soc: prop('ro.soc.model'), android: prop('ro.build.version.release'), battery: grab('dumpsys battery | grep temperature'), thermal: grab('dumpsys thermalservice | grep Status') };
}

async function waitFor(marker, timeoutMs) {
  const t0 = Date.now();
  while (Date.now() - t0 < timeoutMs) {
    const log = adb('logcat', '-d', '-s', 'ZZPerf:I');
    if (log.includes(marker)) return log;
    await new Promise((r) => setTimeout(r, 3000));
  }
  throw new Error(`tempo esgotado esperando "${marker}"`);
}

function stamp() {
  const d = new Date();
  const p = (n) => String(n).padStart(2, '0');
  return `${d.getFullYear()}${p(d.getMonth() + 1)}${p(d.getDate())}-${p(d.getHours())}${p(d.getMinutes())}${p(d.getSeconds())}`;
}

function save(label, payload) {
  const dir = join(ROOT, 'perf-results');
  mkdirSync(dir, { recursive: true });
  const file = join(dir, `${label}-${stamp()}.json`);
  writeFileSync(file, JSON.stringify(payload, null, 2));
  console.log('gravado', file);
}

function install() {
  const apk = join(ROOT, 'app', 'build', 'outputs', 'apk', 'debug', 'app-debug.apk');
  console.log(adb('install', '-r', '-t', apk));
}

async function cmdRun(o, bench) {
  requireDevice();
  if (o.install) install();
  const runs = Number(o.runs ?? 1);
  const label = o.label ?? 'run';
  const results = [];
  for (let r = 0; r < runs; r++) {
    adb('logcat', '-c');
    const args = ['shell', 'am', 'start', '-S', '-n', `${PKG}/.MainActivity`, '--ei', 'zz_perf', bench ? '3' : String(o.perf ?? 1), '--ei', 'zz_stage', String(o.stage ?? 2), '--ei', 'zz_seed', String(o.seed ?? 1234567)];
    for (const [flag, extra] of [['quality', 'zz_quality'], ['zombies', 'zz_zombies'], ['frames', 'zz_frames'], ['meters', 'zz_meters'], ['policy', 'zz_policy']]) if (o[flag] !== undefined) args.push('--ei', extra, String(o[flag]));
    adb(...args);
    const log = await waitFor(bench ? 'bench done' : 'run done', 4 * 60 * 1000);
    results.push({ info: deviceInfo(), args, ...parseLog(log) });
    if (r + 1 < runs) { console.log('esfriando 2 min...'); await new Promise((res) => setTimeout(res, 120000)); }
  }
  save(label, { label, runs: results });
}

function median(xs) { const s = [...xs].sort((a, b) => a - b); return s.length ? s[Math.floor(s.length / 2)] : NaN; }

function cmdCompare(files) {
  const metrics = ['int.p50', 'int.p95', 'int.p99', 'int.max', 'work.p95', 'str.p99', 'str.max', 'slow33', 'slow50', 'gen', 'up', 'starved'];
  const load = (f) => JSON.parse(readFileSync(f, 'utf8')).runs.map((r) => r.run).filter(Boolean);
  const [a, b] = files.map(load);
  console.log('metrica'.padEnd(12), 'antes'.padStart(10), 'depois'.padStart(10), 'delta%'.padStart(8));
  for (const m of metrics) {
    const x = median(a.map((r) => r[m]).filter((v) => typeof v === 'number'));
    const y = median(b.map((r) => r[m]).filter((v) => typeof v === 'number'));
    const d = x ? ((y - x) / x) * 100 : NaN;
    const flag = (m === 'int.p95' && d > 10) || (m.endsWith('.max') && y > 2 * x) ? '  REGRESSAO' : '';
    console.log(m.padEnd(12), String(x).padStart(10), String(y).padStart(10), (Number.isNaN(d) ? '-' : d.toFixed(1)).padStart(8) + flag);
  }
}

const [cmd, ...rest] = process.argv.slice(2);
const opts = parseArgs(rest);
try {
  if (cmd === 'run') await cmdRun(opts, false);
  else if (cmd === 'bench') await cmdRun(opts, true);
  else if (cmd === 'live') {
    requireDevice();
    spawn(ADB, ['logcat', '-s', 'ZZPerf:I'], { stdio: 'inherit' });
  } else if (cmd === 'compare') cmdCompare(opts._);
  else console.log('uso: node scripts/zzperf.mjs run|bench|live|compare ...');
} catch (e) {
  console.error(String(e.message ?? e));
  process.exit(1);
}
