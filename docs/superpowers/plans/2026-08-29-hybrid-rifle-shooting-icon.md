# Hybrid Rifle Shooting and Android Icon Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Entregar um rifle low-poly visível em TPS, mira/disparo automático em FPS enquanto `ATIRAR` estiver pressionado, hitscan com dano e recarga, áudio local e um ícone Android original no APK instalável.

**Architecture:** Preservar o único renderer, a única câmera e o loop atual. Novos módulos puros controlam cadência, transição TPS/FPS, raycast e áudio; `main.js` somente conecta esses módulos ao jogador, ao `CombatSystem`, ao HUD e ao ciclo de pausa/descarte. A arma é uma única árvore Three.js reanexada entre sockets TPS e FPS, e o ícone nasce de um foreground original transparente convertido em recursos Android por um script determinístico.

**Tech Stack:** JavaScript ES modules, Three.js 0.185.1, Node.js test runner, HTML/CSS, Android adaptive/legacy launcher resources, PowerShell/System.Drawing, Gradle/Kotlin WebView e ADB.

**Spec:** `docs/superpowers/specs/2026-08-29-hybrid-rifle-shooting-icon-design.md`

## Global Constraints

- O rifle deve permanecer visível e preso ao personagem em terceira pessoa; segurar `ATIRAR` entra em primeira pessoa e dispara automaticamente; soltar retorna à terceira pessoa.
- Use uma única instância procedural de rifle Three.js; não incorporar `FpsRig.glb` nem outro modelo de arma.
- Configuração fixa: 30 tiros, recarga de 1,8 segundo, 600 tiros por minuto e alcance de 45 metros.
- `a-saeed/Blaster` é apenas referência conceitual porque não publica licença; não copiar código, assets ou expressões do repositório.
- `laser.mp3` e `reload.mp3` vêm de `Footprintarts/ThreeJS_FPS_2.0` no commit `e2f3fe94ffe681e331f03790b03c6185e5f80d15`; incluir integralmente o aviso MIT de Copyright 2025 FootPrint Arts.
- Não alterar mapas, modelos de personagens, posições, colisões estruturais, progressão ou decisões da IA zumbi. A única mudança em combate é uma entrada de dano de rifle reutilizando o estado atual dos inimigos.
- Preservar as alterações não commitadas atuais de IA zumbi. Antes de cada commit, usar `git diff --cached --name-only` e incluir somente os caminhos listados na tarefa.
- Respeitar `settings.soundEnabled`; ao desativar, pausar efeitos ativos e impedir novos efeitos.
- Cancelar o gatilho em `pointerup`, `pointercancel`, `lostpointercapture`, `blur`, pausa, vitória, derrota e `dispose`.
- Manter o jogo totalmente offline e atualizar os sufixos `?v=` dos módulos/entrypoints afetados para evitar cache antigo no WebView.
- Android permanece com `minSdk 23`, `targetSdk 36`, pacote `com.fabiosilva.ecosdeeldra` e build de baixa memória com um worker, sem daemon e Kotlin in-process.

## File Structure

- `app/src/main/assets/game/js/weapon-state.js`: munição, recarga manual e recarga automática ao esvaziar.
- `app/src/main/assets/game/js/automatic-fire.js`: ownership do gatilho, intervalo de 100 ms e dispersão crescente/recuperação.
- `app/src/main/assets/game/js/hybrid-aim.js`: máquina de estados e blend de 180 ms entre TPS e FPS.
- `app/src/main/assets/game/js/camera-controller.js`: composição FPS e aplicação do blend à câmera já existente.
- `app/src/main/assets/game/js/rifle-model.js`: geometria procedural, sockets e reanexação da mesma instância.
- `app/src/main/assets/game/js/rifle-effects.js`: clarão, recuo e traço temporário do disparo.
- `app/src/main/assets/game/js/rifle-hitscan.js`: raycast da câmera, dispersão, parede mais próxima e classificação corpo/cabeça.
- `app/src/main/assets/game/js/weapon-audio.js`: pool de tiro, recarga, mute e descarte.
- `app/src/main/assets/game/js/rifle-input.js`: listeners de pressão/soltura e limpeza idempotente.
- `app/src/main/assets/game/js/combat.js`: aplica dano de rifle sem reutilizar o cooldown lento do ataque antigo.
- `app/src/main/assets/game/js/main.js`: composição dos módulos no ciclo da missão.
- `app/src/main/assets/game/index.html` e `css/game.css`: HUD de 30 tiros, estado pressionado e abertura visual da mira.
- `app/src/main/assets/game/audio/{rifle-fire,rifle-reload}.mp3`: efeitos locais licenciados.
- `app/src/main/assets/game/notices/ThreeJS_FPS_2.0-MIT.txt`: atribuição integral da fonte dos efeitos.
- `design-assets/zombie-zone-icon-foreground.png`: fonte 1024×1024 transparente criada por ImageGen.
- `scripts/generate-android-icons.ps1`: conversão determinística para foreground adaptativo e ícones legados.
- `app/src/main/res/drawable-nodpi`, `mipmap-*`, `mipmap-anydpi-v26` e `values/colors.xml`: recursos do launcher.
- `web-tests/*.test.mjs`: cobertura unitária, integração e integridade dos assets.

---

### Task 1: Weapon state and automatic-fire cadence

**Files:**
- Modify: `app/src/main/assets/game/js/weapon-state.js`
- Create: `app/src/main/assets/game/js/automatic-fire.js`
- Modify: `web-tests/weapon-state.test.mjs`
- Create: `web-tests/automatic-fire.test.mjs`

**Interfaces:**
- Consumes: nenhuma interface nova.
- Produces: `createWeaponState(capacity = 30, reloadSeconds = 1.8)` com `ammo`, `capacity`, `reloading`, `consumeShot(): boolean`, `startReload(): boolean`, `update(delta): boolean`; `createAutomaticFire(options)` com `press()`, `release()`, `cancel()`, `update(delta, { ready, fire }): number`, `held`, `spread` e `dispose()`.

- [ ] **Step 1: Write the failing weapon tests**

Adicionar a `web-tests/weapon-state.test.mjs`:

```js
test('rifle usa carregador de 30 e recarga automática em 1,8 segundo', () => {
  const weapon = createWeaponState();
  for (let shot = 0; shot < 30; shot += 1) assert.equal(weapon.consumeShot(), true);
  assert.equal(weapon.ammo, 0);
  assert.equal(weapon.reloading, true);
  assert.equal(weapon.consumeShot(), false);
  weapon.update(1.79);
  assert.equal(weapon.ammo, 0);
  weapon.update(0.02);
  assert.equal(weapon.ammo, 30);
  assert.equal(weapon.reloading, false);
});

test('recarga antecipada não reinicia enquanto já está ativa', () => {
  const weapon = createWeaponState();
  weapon.consumeShot();
  assert.equal(weapon.startReload(), true);
  assert.equal(weapon.startReload(), false);
});
```

Criar `web-tests/automatic-fire.test.mjs`:

```js
import test from 'node:test';
import assert from 'node:assert/strict';
import { createAutomaticFire } from '../app/src/main/assets/game/js/automatic-fire.js';

test('segurar dispara a 600 RPM somente quando FPS está pronto', () => {
  const trigger = createAutomaticFire();
  let shots = 0;
  trigger.press();
  assert.equal(trigger.update(0.18, { ready: false, fire: () => { shots += 1; return true; } }), 0);
  assert.equal(trigger.update(0, { ready: true, fire: () => { shots += 1; return true; } }), 1);
  assert.equal(trigger.update(0.099, { ready: true, fire: () => { shots += 1; return true; } }), 0);
  assert.equal(trigger.update(0.001, { ready: true, fire: () => { shots += 1; return true; } }), 1);
  assert.equal(shots, 2);
});

test('soltar cancela imediatamente e recupera a dispersão', () => {
  const trigger = createAutomaticFire({ spreadPerShot: 0.01, recoverPerSecond: 0.1 });
  trigger.press();
  trigger.update(0, { ready: true, fire: () => true });
  const expanded = trigger.spread;
  trigger.release();
  trigger.update(1, { ready: true, fire: () => true });
  assert.equal(trigger.held, false);
  assert.ok(trigger.spread < expanded);
});
```

- [ ] **Step 2: Run the focused tests to verify RED**

Run: `node --test web-tests/weapon-state.test.mjs web-tests/automatic-fire.test.mjs`

Expected: FAIL porque `automatic-fire.js` ainda não existe e o estado atual não inicia recarga ao zerar.

- [ ] **Step 3: Implement the minimal state machines**

Em `weapon-state.js`, preservar a API existente e iniciar a recarga após o trigésimo consumo:

```js
export function createWeaponState(capacity = 30, reloadSeconds = 1.8) {
  let ammo = capacity;
  let reloadRemaining = 0;
  return {
    get ammo() { return ammo; },
    get capacity() { return capacity; },
    get reloading() { return reloadRemaining > 0; },
    consumeShot() {
      if (reloadRemaining > 0 || ammo <= 0) return false;
      ammo -= 1;
      if (ammo === 0) reloadRemaining = reloadSeconds;
      return true;
    },
    startReload() {
      if (reloadRemaining > 0 || ammo >= capacity) return false;
      reloadRemaining = reloadSeconds;
      return true;
    },
    update(delta) {
      if (reloadRemaining <= 0) return false;
      reloadRemaining = Math.max(0, reloadRemaining - Math.max(0, delta));
      if (reloadRemaining === 0) ammo = capacity;
      return true;
    },
  };
}
```

Criar `automatic-fire.js` com intervalo padrão `60 / 600`, máximo de quatro tiros recuperados por frame para evitar rajadas ilimitadas após travamento e dispersão normalizada em coordenadas de tela:

```js
export function createAutomaticFire({
  roundsPerMinute = 600,
  minSpread = 0.002,
  maxSpread = 0.026,
  spreadPerShot = 0.003,
  recoverPerSecond = 0.04,
} = {}) {
  const interval = 60 / roundsPerMinute;
  let held = false;
  let untilNext = 0;
  let spread = minSpread;

  return {
    get held() { return held; },
    get spread() { return spread; },
    press() { held = true; untilNext = 0; },
    release() { held = false; untilNext = 0; },
    cancel() { held = false; untilNext = 0; },
    update(delta, { ready = false, fire = () => false } = {}) {
      const elapsed = Math.max(0, delta);
      if (!held || !ready) {
        spread = Math.max(minSpread, spread - recoverPerSecond * elapsed);
        return 0;
      }
      untilNext -= elapsed;
      let fired = 0;
      while (untilNext <= 1e-9 && fired < 4) {
        if (!fire(spread)) { untilNext = interval; break; }
        spread = Math.min(maxSpread, spread + spreadPerShot);
        untilNext += interval;
        fired += 1;
      }
      return fired;
    },
    dispose() { held = false; untilNext = 0; },
  };
}
```

- [ ] **Step 4: Run focused tests to verify GREEN**

Run: `node --test web-tests/weapon-state.test.mjs web-tests/automatic-fire.test.mjs`

Expected: PASS, incluindo os dois testes legados de `weapon-state`.

- [ ] **Step 5: Commit only this task**

```powershell
git add app/src/main/assets/game/js/weapon-state.js app/src/main/assets/game/js/automatic-fire.js web-tests/weapon-state.test.mjs web-tests/automatic-fire.test.mjs
git diff --cached --name-only
git commit -m "feat: add automatic rifle cadence and reload"
```

### Task 2: Hybrid TPS/FPS camera transition

**Files:**
- Create: `app/src/main/assets/game/js/hybrid-aim.js`
- Modify: `app/src/main/assets/game/js/camera-controller.js`
- Create: `web-tests/hybrid-aim.test.mjs`
- Modify: `web-tests/camera-controller.test.mjs`

**Interfaces:**
- Consumes: `createThirdPersonCamera(...)` atual.
- Produces: `createHybridAim({ transitionSeconds = 0.18 })`; `firstPersonComposition(position, yaw, pitch, options)`; câmera com `setViewBlend(value)` e `viewBlend()`.

- [ ] **Step 1: Write failing transition and composition tests**

Criar `web-tests/hybrid-aim.test.mjs`:

```js
import test from 'node:test';
import assert from 'node:assert/strict';
import { createHybridAim } from '../app/src/main/assets/game/js/hybrid-aim.js';

test('pressão entra em FPS em 180 ms e soltura volta a TPS', () => {
  const aim = createHybridAim();
  aim.press();
  aim.update(0.09);
  assert.equal(aim.state, 'entering-first-person');
  assert.ok(aim.blend > 0.49 && aim.blend < 0.51);
  assert.equal(aim.readyToFire, false);
  aim.update(0.09);
  assert.equal(aim.state, 'first-person');
  assert.equal(aim.readyToFire, true);
  aim.release();
  aim.update(0.18);
  assert.equal(aim.state, 'third-person');
  assert.equal(aim.blend, 0);
});

test('cancel volta imediatamente ao destino TPS e bloqueia tiro', () => {
  const aim = createHybridAim();
  aim.press();
  aim.update(0.18);
  aim.cancel();
  assert.equal(aim.readyToFire, false);
  assert.equal(aim.state, 'returning-third-person');
});
```

Adicionar a `camera-controller.test.mjs` um teste que cria a câmera atual, chama `setViewBlend(1)`, executa `updateFollow(0)` e verifica distância menor que `0.6` da posição dos olhos; também verificar que `heading()` e `pitch()` não mudam durante o blend.

- [ ] **Step 2: Run focused tests to verify RED**

Run: `node --test web-tests/hybrid-aim.test.mjs web-tests/camera-controller.test.mjs`

Expected: FAIL por módulos/métodos ainda ausentes.

- [ ] **Step 3: Implement the hybrid state and blend in the existing camera**

Criar `hybrid-aim.js`:

```js
export function createHybridAim({ transitionSeconds = 0.18 } = {}) {
  let blend = 0;
  let target = 0;
  let state = 'third-person';
  const speed = 1 / transitionSeconds;
  return {
    get blend() { return blend; },
    get state() { return state; },
    get readyToFire() { return target === 1 && blend >= 0.98; },
    press() { target = 1; state = blend >= 0.98 ? 'first-person' : 'entering-first-person'; },
    release() { target = 0; state = blend <= 0.02 ? 'third-person' : 'returning-third-person'; },
    cancel() { target = 0; state = blend <= 0.02 ? 'third-person' : 'returning-third-person'; },
    update(delta) {
      const step = Math.max(0, delta) * speed;
      blend = target > blend ? Math.min(target, blend + step) : Math.max(target, blend - step);
      if (blend >= 1) state = 'first-person';
      else if (blend <= 0) state = 'third-person';
      return blend;
    },
  };
}
```

Em `camera-controller.js`, exportar a composição abaixo e misturar a posição TPS já resolvida pelo SpringArm com a posição FPS. O aim usa sempre o yaw/pitch existente, portanto não há uma segunda câmera nem um estado concorrente de rotação:

```js
export function firstPersonComposition(position, yaw, pitch, {
  eyeHeight = 2.05, forwardOffset = 0.18, aimDistance = 45,
} = {}) {
  const cosPitch = Math.cos(pitch);
  const forward = { x: Math.sin(yaw) * cosPitch, y: -Math.sin(pitch), z: -Math.cos(yaw) * cosPitch };
  const camera = {
    x: position.x + forward.x * forwardOffset,
    y: position.y + eyeHeight + forward.y * forwardOffset,
    z: position.z + forward.z * forwardOffset,
  };
  return { camera, forward, aim: {
    x: camera.x + forward.x * aimDistance,
    y: camera.y + forward.y * aimDistance,
    z: camera.z + forward.z * aimDistance,
  } };
}
```

Dentro de `createThirdPersonCamera`, manter `let viewBlend = 0`; em `updateFollow`, calcular `firstPersonComposition(...)`, interpolar `camera.position` entre `resolved.position` e `first.camera`, e construir `aimTarget` a partir do `forward` interpolado/normalizado. Acrescentar ao retorno:

```js
setViewBlend(value) { viewBlend = clamp(Number(value) || 0, 0, 1); },
viewBlend() { return viewBlend; },
```

- [ ] **Step 4: Run focused tests to verify GREEN**

Run: `node --test web-tests/hybrid-aim.test.mjs web-tests/camera-controller.test.mjs web-tests/tps-integration.test.mjs`

Expected: PASS e nenhuma regressão no SpringArm/ControllerYaw.

- [ ] **Step 5: Commit only this task**

```powershell
git add app/src/main/assets/game/js/hybrid-aim.js app/src/main/assets/game/js/camera-controller.js web-tests/hybrid-aim.test.mjs web-tests/camera-controller.test.mjs
git diff --cached --name-only
git commit -m "feat: add hybrid first person aim transition"
```

### Task 3: Procedural rifle, sockets, muzzle flash, recoil and tracer

**Files:**
- Create: `app/src/main/assets/game/js/rifle-model.js`
- Create: `app/src/main/assets/game/js/rifle-effects.js`
- Create: `web-tests/rifle-model.test.mjs`
- Create: `web-tests/rifle-effects.test.mjs`

**Interfaces:**
- Consumes: `THREE`, `player.root`, a `THREE.PerspectiveCamera` pertencente à cena.
- Produces: `createRifleModel(THREE) -> { root, recoilGroup, muzzle, dispose }`; `createRifleSockets({ THREE, playerRoot, camera })`; `attachRifle(rifle, socket, mode)`; `createRifleEffects({ THREE, scene, recoilGroup, muzzle })` com `pulseShot(endPoint)`, `update(delta)` e `dispose()`.

- [ ] **Step 1: Write failing model/effect tests**

Em `rifle-model.test.mjs`, exigir nomes `rifle-root`, `rifle-recoil`, `rifle-muzzle`, ao menos seis meshes, materiais `MeshStandardMaterial`, e provar que `attachRifle` move o mesmo objeto do socket TPS para o FPS sem duplicá-lo. Em `rifle-effects.test.mjs`, criar uma cena, disparar `pulseShot(new THREE.Vector3(0, 1, -10))`, verificar clarão visível e um `Line` na cena; após `update(0.08)`, verificar remoção do traço e retorno do recuo.

Código central do teste de identidade:

```js
const rifle = createRifleModel(THREE);
const player = new THREE.Group();
const camera = new THREE.PerspectiveCamera();
const sockets = createRifleSockets({ THREE, playerRoot: player, camera });
attachRifle(rifle, sockets.tps, 'tps');
assert.equal(sockets.tps.children[0], rifle.root);
attachRifle(rifle, sockets.fps, 'fps');
assert.equal(sockets.tps.children.length, 0);
assert.equal(sockets.fps.children[0], rifle.root);
```

- [ ] **Step 2: Run focused tests to verify RED**

Run: `node --test web-tests/rifle-model.test.mjs web-tests/rifle-effects.test.mjs`

Expected: FAIL porque os módulos ainda não existem.

- [ ] **Step 3: Build one low-poly rifle and short-lived effects**

Em `rifle-model.js`, criar `root > recoilGroup > meshes`, usando cores explícitas e sem textura externa. Montar corpo/guarda-mão/coronha/carregador/mira com `BoxGeometry`; cano e muzzle com `CylinderGeometry` rotacionados em X. Configuração mínima:

```js
const materials = [
  new THREE.MeshStandardMaterial({ color: 0x27313a, roughness: 0.5, metalness: 0.55 }),
  new THREE.MeshStandardMaterial({ color: 0x151b20, roughness: 0.72, metalness: 0.25 }),
  new THREE.MeshStandardMaterial({ color: 0x8fd62f, roughness: 0.62, metalness: 0.08 }),
];
const root = new THREE.Group(); root.name = 'rifle-root';
const recoilGroup = new THREE.Group(); recoilGroup.name = 'rifle-recoil'; root.add(recoilGroup);
const muzzle = new THREE.Object3D(); muzzle.name = 'rifle-muzzle'; muzzle.position.set(0, 0.04, -1.72);
recoilGroup.add(muzzle);
```

Os sockets usam offsets distintos e `attachRifle` redefine posição/rotação/escala após `socket.add(rifle.root)`:

```js
const transforms = {
  tps: { position: [0.34, 1.52, -0.2], rotation: [0.02, 0.03, -0.06], scale: 1.05 },
  fps: { position: [0.34, -0.28, -0.72], rotation: [-0.03, 0.02, -0.02], scale: 0.42 },
};
```

Em `rifle-effects.js`, manter um flash emissivo por `0.045` segundo, deslocar `recoilGroup.position.z` para `0.08` e retornar exponencialmente, e criar `THREE.Line` com material laranja `0xffb24a`, duração `0.065` segundo. `dispose()` remove traços e libera suas geometrias/materiais; o dispose do rifle libera todas as geometrias e os três materiais uma única vez.

- [ ] **Step 4: Run focused tests to verify GREEN**

Run: `node --test web-tests/rifle-model.test.mjs web-tests/rifle-effects.test.mjs`

Expected: PASS.

- [ ] **Step 5: Commit only this task**

```powershell
git add app/src/main/assets/game/js/rifle-model.js app/src/main/assets/game/js/rifle-effects.js web-tests/rifle-model.test.mjs web-tests/rifle-effects.test.mjs
git diff --cached --name-only
git commit -m "feat: add procedural rifle visuals"
```

### Task 4: Hitscan, wall blocking and rifle damage

**Files:**
- Create: `app/src/main/assets/game/js/rifle-hitscan.js`
- Modify: `app/src/main/assets/game/js/combat.js`
- Create: `web-tests/rifle-hitscan.test.mjs`
- Modify: `web-tests/zombie-ai-combat.test.mjs`

**Interfaces:**
- Consumes: câmera Three.js, `world.cameraColliderMeshes`, lista atual de inimigos e `CombatSystem`.
- Produces: `resolveRifleHitscan({ THREE, camera, raycaster, wallMeshes, enemies, spread, random, maxRange })`; `combat.applyRifleHit(target, { bodyDamage = 18, headshot = false, headshotMultiplier = 1.75 })`.

- [ ] **Step 1: Write failing raycast and damage tests**

Em `rifle-hitscan.test.mjs`, construir caixas Three.js alinhadas à câmera e cobrir: parede a 5 m bloqueia zumbi a 10 m; sem parede, mesh do inimigo é retornado; impacto acima de `enemy.root.position.y + 1.7` é `head`; miss termina exatamente a 45 m. Usar `camera.updateMatrixWorld(true)`, `root.updateMatrixWorld(true)` e `random: () => 0.5` para eliminar dispersão.

Adicionar a `zombie-ai-combat.test.mjs`:

```js
test('dano de rifle ignora cooldown do ataque e aplica multiplicador de cabeça', () => {
  const { combat, enemy } = fixture();
  combat.playerAttackCooldown = 0.5;
  const first = combat.applyRifleHit(enemy, { bodyDamage: 20 });
  assert.equal(first.hit, true);
  assert.equal(enemy.health, 50);
  const headshot = combat.applyRifleHit(enemy, { bodyDamage: 20, headshot: true, headshotMultiplier: 1.5 });
  assert.equal(headshot.damage, 30);
  assert.equal(enemy.health, 20);
});
```

- [ ] **Step 2: Run focused tests to verify RED**

Run: `node --test web-tests/rifle-hitscan.test.mjs web-tests/zombie-ai-combat.test.mjs`

Expected: FAIL por módulo/método ausente.

- [ ] **Step 3: Implement nearest-impact resolution and damage**

Em `rifle-hitscan.js`, usar `raycaster.setFromCamera({ x, y }, camera)`, com `x/y = (random() * 2 - 1) * spread` e `raycaster.far = maxRange`. Encontrar a parede mais próxima por `intersectObjects(wallMeshes, false)[0]`; para cada inimigo vivo, obter `intersectObject(enemy.root, true)[0]`; aceitar o inimigo somente quando sua distância for menor que a da parede. Retornar sempre:

```js
{
  target: enemyOrNull,
  zone: enemyOrNull ? (hit.point.y - enemy.root.position.y >= 1.7 ? 'head' : 'body') : null,
  point: hitPointOrRangeEnd,
  direction: normalizedWorldDirection,
  distance: impactDistance,
}
```

Em `CombatSystem`, adicionar sem tocar em `update()` ou nas transições da IA:

```js
applyRifleHit(target, { bodyDamage = 18, headshot = false, headshotMultiplier = 1.75 } = {}) {
  if (!target || target.health <= 0 || this.player.health <= 0) {
    return { hit: false, defeated: false, targetId: target?.id ?? null, damage: 0 };
  }
  const damage = Math.round(bodyDamage * (this.player.powerMultiplier ?? 1) * (headshot ? headshotMultiplier : 1));
  target.health = Math.max(0, target.health - damage);
  if (target.health > 0) alertZombie(target.ai, this.player.root.position);
  const defeated = target.health === 0;
  if (defeated) target.animations.play(['die', 'idle'], { once: true, fade: 0.1 });
  this.notify();
  return { hit: true, defeated, targetId: target.id ?? null, damage };
}
```

- [ ] **Step 4: Run focused tests to verify GREEN**

Run: `node --test web-tests/rifle-hitscan.test.mjs web-tests/zombie-ai-combat.test.mjs web-tests/zombie-ai-line-of-sight.test.mjs`

Expected: PASS; a parede ganha sempre por distância e a percepção da IA continua intacta.

- [ ] **Step 5: Commit only this task**

```powershell
git add app/src/main/assets/game/js/rifle-hitscan.js app/src/main/assets/game/js/combat.js web-tests/rifle-hitscan.test.mjs web-tests/zombie-ai-combat.test.mjs
git diff --cached --name-only
git commit -m "feat: add blocked hitscan and rifle damage"
```

### Task 5: Licensed weapon sounds and audio pool

**Files:**
- Create: `app/src/main/assets/game/audio/rifle-fire.mp3`
- Create: `app/src/main/assets/game/audio/rifle-reload.mp3`
- Create: `app/src/main/assets/game/notices/ThreeJS_FPS_2.0-MIT.txt`
- Create: `app/src/main/assets/game/js/weapon-audio.js`
- Create: `web-tests/weapon-audio.test.mjs`
- Modify: `web-tests/zombie-zone-assets.test.mjs`

**Interfaces:**
- Consumes: `settings.soundEnabled` e HTML `Audio`.
- Produces: `createWeaponAudio({ fireUrl, reloadUrl, enabled, volume, poolSize, audioFactory })` com `playFire()`, `playReload()`, `setEnabled(value)`, `dispose()`.

- [ ] **Step 1: Write failing audio/controller and asset tests**

Criar doubles com `paused`, `currentTime`, contadores `play/pause`, `removeAttribute` e `load`. Verificar: cinco disparos alternam por pool de quatro sem lançar; `setEnabled(false)` pausa todas as cinco instâncias; recarga usa áudio separado; `dispose()` limpa `src` e impede novas reproduções.

Adicionar ao teste de assets:

```js
for (const file of ['rifle-fire.mp3', 'rifle-reload.mp3']) {
  assert.ok(statSync(new URL(`../app/src/main/assets/game/audio/${file}`, import.meta.url)).size > 10_000);
}
const notice = readFileSync(new URL('../app/src/main/assets/game/notices/ThreeJS_FPS_2.0-MIT.txt', import.meta.url), 'utf8');
assert.match(notice, /Copyright \(c\) 2025 FootPrint Arts/);
assert.match(notice, /THE SOFTWARE IS PROVIDED "AS IS"/);
```

- [ ] **Step 2: Run focused tests to verify RED**

Run: `node --test web-tests/weapon-audio.test.mjs web-tests/zombie-zone-assets.test.mjs`

Expected: FAIL por módulo/assets ausentes.

- [ ] **Step 3: Copy exact MIT assets and implement audio pooling**

Copiar dos clones de referência já auditados:

```powershell
$referenceRoot = Join-Path $env:TEMP 'zombie-zone-shooting-references\ThreeJS_FPS_2.0'
Copy-Item -LiteralPath (Join-Path $referenceRoot 'public\sounds\laser.mp3') -Destination 'app/src/main/assets/game/audio/rifle-fire.mp3'
Copy-Item -LiteralPath (Join-Path $referenceRoot 'public\sounds\reload.mp3') -Destination 'app/src/main/assets/game/audio/rifle-reload.mp3'
Copy-Item -LiteralPath (Join-Path $referenceRoot 'LICENSE') -Destination 'app/src/main/assets/game/notices/ThreeJS_FPS_2.0-MIT.txt'
```

Se o diretório temporário não existir, clonar exatamente `https://github.com/Footprintarts/ThreeJS_FPS_2.0.git`, confirmar `git rev-parse HEAD` igual a `e2f3fe94ffe681e331f03790b03c6185e5f80d15` e então copiar os mesmos três arquivos.

Em `weapon-audio.js`, criar quatro `Audio(fireUrl)` e um `Audio(reloadUrl)`, definir `preload='auto'`, volumes `volume` e `volume * 0.8`, e avançar índice circular. Antes de `play()`, definir `currentTime = 0`; envolver retorno em `Promise.resolve(...).catch(() => false)`. `setEnabled(false)` e `dispose()` pausam toda a coleção; somente `dispose()` remove `src` e chama `load()`.

- [ ] **Step 4: Run focused tests to verify GREEN**

Run: `node --test web-tests/weapon-audio.test.mjs web-tests/zombie-zone-assets.test.mjs`

Expected: PASS e tamanhos maiores que 10 kB.

- [ ] **Step 5: Commit only this task**

```powershell
git add app/src/main/assets/game/audio/rifle-fire.mp3 app/src/main/assets/game/audio/rifle-reload.mp3 app/src/main/assets/game/notices/ThreeJS_FPS_2.0-MIT.txt app/src/main/assets/game/js/weapon-audio.js web-tests/weapon-audio.test.mjs web-tests/zombie-zone-assets.test.mjs
git diff --cached --name-only
git commit -m "feat: add licensed rifle audio"
```

### Task 6: Pointer lifecycle and complete gameplay integration

**Files:**
- Create: `app/src/main/assets/game/js/rifle-input.js`
- Create: `web-tests/rifle-input.test.mjs`
- Modify: `app/src/main/assets/game/js/main.js`
- Modify: `app/src/main/assets/game/index.html`
- Modify: `app/src/main/assets/game/css/game.css`
- Modify: `web-tests/tps-integration.test.mjs`
- Modify: `web-tests/zombie-zone-shell.test.mjs`

**Interfaces:**
- Consumes: todos os módulos das Tasks 1–5, `world.cameraColliderMeshes`, `settings.soundEnabled`, `setPaused` e `dispose`.
- Produces: `bindRifleInput({ button, windowRef, onPress, onRelease }) -> { cancel(), dispose() }`; `activeGame.setSoundEnabled(value)`.

- [ ] **Step 1: Write failing lifecycle and integration tests**

Criar `rifle-input.test.mjs` usando `EventTarget`; provar exatamente uma chamada de press em `pointerdown`, uma release no `pointerup`, cancelamento em `pointercancel` e `blur`, e nenhuma chamada após `dispose()`.

Em `tps-integration.test.mjs`, substituir a asserção do antigo `const fire` por asserções estruturais novas:

```js
assert.match(main, /createHybridAim\(/);
assert.match(main, /createAutomaticFire\(/);
assert.match(main, /resolveRifleHitscan\(/);
assert.match(main, /attachRifle\(rifle, sockets\[nextRifleMode\], nextRifleMode\)/);
assert.match(main, /automaticFire\.update\(delta/);
assert.match(main, /hybridAim\.readyToFire/);
assert.match(main, /input\.dispose\(\)/);
assert.doesNotMatch(main, /combat\.fire\(selectedEnemy\)/);
```

Em `zombie-zone-shell.test.mjs`, exigir `30/30`, `aria-label="Atirar"` e a classe de mira que usa a custom property `--spread`.

- [ ] **Step 2: Run focused tests to verify RED**

Run: `node --test web-tests/rifle-input.test.mjs web-tests/tps-integration.test.mjs web-tests/zombie-zone-shell.test.mjs`

Expected: FAIL porque a integração ainda usa o disparo único antigo.

- [ ] **Step 3: Implement input ownership and wire the mission loop**

Criar `rifle-input.js` com um único `pointerId` ativo. Em `pointerdown`, chamar `setPointerCapture`, `onPress()` e `preventDefault`; `pointerup`, `pointercancel`, `lostpointercapture` e `blur` chamam uma função `release()` idempotente. `dispose()` remove todos os listeners e chama `release()`.

Em `main.js`:

1. Importar os sete módulos novos com `?v=rifle-1`.
2. Após criar a câmera, executar `world.scene.add(camera)`.
3. Criar `hybridAim`, `automaticFire`, `weapon = createWeaponState()`, rifle, sockets, effects, áudio, raycaster e input.
4. Em `onPress`, chamar `hybridAim.press()`, `automaticFire.press()` e adicionar `attackButton.classList.add('is-active')`; no release, desfazer os três.
5. Criar `performRifleShot(spread)` exatamente com esta ordem:

```js
const performRifleShot = (spread) => {
  const wasReloading = weapon.reloading;
  if (!weapon.consumeShot()) return false;
  const shot = resolveRifleHitscan({
    THREE, camera, raycaster, wallMeshes: world.cameraColliderMeshes,
    enemies, spread, maxRange: 45,
  });
  if (shot.target) {
    const result = combat.applyRifleHit(shot.target, { headshot: shot.zone === 'head' });
    if (result.hit) {
      reticle.classList.add('hit');
      window.setTimeout(() => reticle.classList.remove('hit'), 110);
    }
  }
  effects.pulseShot(shot.point);
  weaponAudio.playFire();
  if (!wasReloading && weapon.reloading) weaponAudio.playReload();
  paintWeapon();
  return true;
};
```

6. No loop, depois de atualizar movimento/mundo e antes de `renderer.render`: `hybridAim.update(delta)`, `cameraController.setViewBlend(hybridAim.blend)`, definir `const nextRifleMode = hybridAim.blend >= 0.5 ? 'fps' : 'tps'` e, quando mudar, chamar `attachRifle(rifle, sockets[nextRifleMode], nextRifleMode)`. Ocultar `player.root` apenas quando `blend >= 0.92`, chamar `cameraController.updateFollow(delta)`, executar `automaticFire.update(delta, { ready: hybridAim.readyToFire, fire: performRifleShot })`, `effects.update(delta)` e atualizar `--spread`.
7. A recarga manual chama som somente se `weapon.startReload()` retornar `true`.
8. `setPaused(true)` chama `input.cancel()`, `automaticFire.cancel()` e `hybridAim.cancel()` antes de desativar controladores.
9. `dispose()` chama `input.dispose()`, `automaticFire.dispose()`, `weaponAudio.dispose()`, `effects.dispose()`, `rifle.dispose()` e remove a câmera da cena.
10. Expor `setSoundEnabled(value) { weaponAudio.setEnabled(value); }` e chamar `activeGame?.setSoundEnabled(event.target.checked)` no handler global da configuração de som.

Em `index.html`, iniciar munição em `30/30` e manter o texto `ATIRAR`. Em `game.css`, fazer a abertura depender de `--spread`:

```css
.reticle {
  --spread: 0px;
  width: calc(28px + var(--spread));
  height: calc(28px + var(--spread));
}
.reticle.fps { border-color: var(--amber); }
```

Atualizar cache tags do `main.js`, CSS e bootstrap em `index.html` sem alterar a estrutura de telas.

- [ ] **Step 4: Run focused integration tests to verify GREEN**

Run: `node --test web-tests/rifle-input.test.mjs web-tests/tps-integration.test.mjs web-tests/zombie-zone-shell.test.mjs web-tests/weapon-state.test.mjs web-tests/camera-controller.test.mjs`

Expected: PASS; nenhum listener antigo `pointerdown -> fire` permanece.

- [ ] **Step 5: Commit only this task**

```powershell
git add app/src/main/assets/game/js/rifle-input.js web-tests/rifle-input.test.mjs app/src/main/assets/game/js/main.js app/src/main/assets/game/index.html app/src/main/assets/game/css/game.css web-tests/tps-integration.test.mjs web-tests/zombie-zone-shell.test.mjs
git diff --cached --name-only
git commit -m "feat: integrate hybrid automatic rifle gameplay"
```

### Task 7: Generate and package the original Android launcher icon

**Files:**
- Create: `design-assets/zombie-zone-icon-foreground.png`
- Create: `scripts/generate-android-icons.ps1`
- Create: `app/src/main/res/values/colors.xml`
- Create: `app/src/main/res/drawable-nodpi/ic_launcher_foreground.png`
- Create: `app/src/main/res/mipmap-mdpi/ic_launcher.png`
- Create: `app/src/main/res/mipmap-hdpi/ic_launcher.png`
- Create: `app/src/main/res/mipmap-xhdpi/ic_launcher.png`
- Create: `app/src/main/res/mipmap-xxhdpi/ic_launcher.png`
- Create: `app/src/main/res/mipmap-xxxhdpi/ic_launcher.png`
- Create: matching `ic_launcher_round.png` in the five density directories
- Create: `app/src/main/res/mipmap-anydpi-v26/ic_launcher.xml`
- Create: `app/src/main/res/mipmap-anydpi-v26/ic_launcher_round.xml`
- Modify: `app/src/main/AndroidManifest.xml`
- Create: `web-tests/android-icon-assets.test.mjs`

**Interfaces:**
- Consumes: arte PNG transparente 1024×1024 gerada por ImageGen.
- Produces: launcher legado e adaptativo referenciado por `android:icon` e `android:roundIcon`.

- [ ] **Step 1: Write failing Android icon asset tests**

Criar helper Node que lê os primeiros 24 bytes do PNG e extrai largura/altura big-endian nos offsets 16/20. Exigir dimensões `48, 72, 96, 144, 192` para `mdpi..xxxhdpi`, 432×432 para o foreground adaptativo, os dois XMLs adaptativos e estas referências no manifest:

```js
assert.match(manifest, /android:icon="@mipmap\/ic_launcher"/);
assert.match(manifest, /android:roundIcon="@mipmap\/ic_launcher_round"/);
assert.match(adaptive, /@color\/ic_launcher_background/);
assert.match(adaptive, /@drawable\/ic_launcher_foreground/);
```

- [ ] **Step 2: Run the icon test to verify RED**

Run: `node --test web-tests/android-icon-assets.test.mjs`

Expected: FAIL porque nenhum recurso launcher existe.

- [ ] **Step 3: Generate original transparent foreground with ImageGen**

Antes desta ação, ler e seguir integralmente a skill `imagegen`. Gerar PNG quadrado 1024×1024 com transparência real usando este prompt fechado:

```text
Original mobile game app icon foreground, transparent background. Centered low-poly green zombie head with friendly but determined expression, angular faceted shapes, bright lime highlights and deep forest-green shadows. A bold orange circular crosshair surrounds the head without covering the face. No text, no letters, no border, no scenery, no blue background, no photorealism. Symmetrical readable silhouette, chunky shapes, high contrast, Android adaptive icon safe zone with all important details inside the central 66 percent. Export as a clean 1024 by 1024 transparent PNG.
```

Salvar exatamente em `design-assets/zombie-zone-icon-foreground.png`, abrir a imagem em detalhe original e rejeitar/regenerar se houver texto, fundo opaco, membros cortados ou elementos importantes fora da zona central.

- [ ] **Step 4: Create deterministic density resources and adaptive XML**

Criar `scripts/generate-android-icons.ps1` com parâmetro obrigatório `-Source`. Usar `System.Drawing` com `InterpolationMode.HighQualityBicubic`, canvas `#08111f`, foreground central ocupando 76% dos ícones legados, e canvas transparente com foreground ocupando 66% no arquivo adaptativo. Gerar tamanhos nesta tabela:

```powershell
$sizes = [ordered]@{ mdpi = 48; hdpi = 72; xhdpi = 96; xxhdpi = 144; xxxhdpi = 192 }
```

O script cria diretórios, salva `ic_launcher.png` e `ic_launcher_round.png` em cada densidade e `drawable-nodpi/ic_launcher_foreground.png` em 432×432. Para o round legado, usar `GraphicsPath.AddEllipse(...)`, `SetClip(...)`, preencher `#08111f` e desenhar o foreground; nunca simplesmente renomear o quadrado.

Executar:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/generate-android-icons.ps1 -Source design-assets/zombie-zone-icon-foreground.png
```

Criar `values/colors.xml` com `<color name="ic_launcher_background">#08111F</color>` e os dois XMLs adaptativos com `<background android:drawable="@color/ic_launcher_background"/>` e `<foreground android:drawable="@drawable/ic_launcher_foreground"/>`. Modificar `<application>` no manifest para conter:

```xml
android:icon="@mipmap/ic_launcher"
android:roundIcon="@mipmap/ic_launcher_round"
```

- [ ] **Step 5: Run test, inspect icons and commit only this task**

Run: `node --test web-tests/android-icon-assets.test.mjs`

Expected: PASS.

Abrir `mipmap-mdpi/ic_launcher.png`, `mipmap-xxxhdpi/ic_launcher.png` e `drawable-nodpi/ic_launcher_foreground.png`; verificar visualmente legibilidade, transparência do foreground e ausência de halo/corte.

```powershell
git add design-assets/zombie-zone-icon-foreground.png scripts/generate-android-icons.ps1 app/src/main/res app/src/main/AndroidManifest.xml web-tests/android-icon-assets.test.mjs
git diff --cached --name-only
git commit -m "feat: add Zombie Zone Android launcher icon"
```

### Task 8: Full regression, browser proof, APK build and device installation

**Files:**
- Modify only if verification reveals an in-scope defect.
- Create: `docs/verification/2026-08-29-hybrid-rifle.md`

**Interfaces:**
- Consumes: entrega integrada das Tasks 1–7.
- Produces: evidência reproduzível de testes, navegador, APK e instalação; não produz nova mecânica.

- [ ] **Step 1: Run the complete web regression suite**

Run: `npm test`

Expected: todos os testes passam com zero falhas. Registrar contagem, duração e comando no relatório; não reduzir/remover testes para obter verde.

- [ ] **Step 2: Verify asset closure and source hygiene**

Run:

```powershell
rg -n "Blaster|FpsRig\.glb|https?://" app/src/main/assets/game app/src/main/res
git diff --check
git status --short
```

Expected: nenhuma dependência remota ou arquivo do Blaster/FpsRig; URLs encontradas somente em textos de licença quando aplicável; nenhuma textura/som/icon faltante nos testes; nenhuma mudança em modelos, mapas JSON ou colisões além das alterações de IA que já existiam antes deste plano.

- [ ] **Step 3: Exercise the real browser gameplay path**

Ler e usar a skill de browser disponível. Abrir a prévia local existente em viewport landscape e executar: entrar na fase; confirmar rifle TPS; manter `ATIRAR` pressionado por pelo menos 1 segundo; confirmar FPS, rajada próxima de 10 tiros, clarão/recuo/traços e munição reduzida; esvaziar 30 tiros; confirmar `.../30` por 1,8 segundo; soltar e confirmar retorno TPS; atirar contra parede com zumbi atrás e confirmar ausência de dano; acertar corpo/cabeça; pausar durante rajada; retomar sem disparo preso; desativar som e confirmar silêncio. Inspecionar console e rede, exigindo zero erros e zero requests externos.

Registrar no relatório os estados observados, valores de munição e qualquer limitação que não possa ser observada diretamente. Não tratar somente testes unitários como prova visual.

- [ ] **Step 4: Build the APK with low-memory settings**

Run:

```powershell
$env:GRADLE_OPTS = '-Dorg.gradle.jvmargs=-Xmx1024m -Dfile.encoding=UTF-8 -Dorg.gradle.workers.max=1 -Dkotlin.compiler.execution.strategy=in-process'
.\gradlew.bat --no-daemon --max-workers=1 :app:assembleDebug
```

Expected: `BUILD SUCCESSFUL`. Em seguida registrar caminho, tamanho, horário e hash fresco:

```powershell
$apk = Resolve-Path 'app/build/outputs/apk/debug/app-debug.apk'
Get-Item -LiteralPath $apk | Select-Object FullName,Length,LastWriteTime
Get-FileHash -Algorithm SHA256 -LiteralPath $apk
```

- [ ] **Step 5: Install on the currently authorized device and verify package/icon**

Run `adb devices -l`; usar somente um dispositivo atual com estado `device`. Se o dispositivo Wi-Fi histórico não estiver listado, redescobrir com `adb mdns services` antes de conectar; não assumir serial antigo sem confirmação.

```powershell
$deviceSerial = adb devices | Select-Object -Skip 1 | ForEach-Object {
  if ($_ -match '^(\S+)\s+device$') { $matches[1] }
} | Select-Object -First 1
if (-not $deviceSerial) { throw 'Nenhum dispositivo ADB autorizado está no estado device.' }
adb -s $deviceSerial install -r app/build/outputs/apk/debug/app-debug.apk
adb -s $deviceSerial shell pm path com.fabiosilva.ecosdeeldra
adb -s $deviceSerial shell dumpsys package com.fabiosilva.ecosdeeldra | Select-String 'lastUpdateTime|versionName|versionCode'
```

Expected: instalação `Success`, package path presente e `lastUpdateTime` correspondente ao build atual. Abrir pelo launcher, confirmar o novo ícone e repetir no aparelho: rifle visível TPS, segurar entra FPS/dispara, soltar volta TPS, recarga de 30 e áudio/mute.

- [ ] **Step 6: Apply verification skill, write evidence report and commit it**

Antes de qualquer afirmação de conclusão, ler e seguir `superpowers:verification-before-completion`. Criar `docs/verification/2026-08-29-hybrid-rifle.md` com: contagem dos testes; cenários do navegador; console/rede; hash/tamanho/data do APK; serial/modelo confirmado; saída `Success`; package/version/lastUpdateTime; verificação do ícone; limitações reais.

```powershell
git add docs/verification/2026-08-29-hybrid-rifle.md
git diff --cached --name-only
git commit -m "test: verify hybrid rifle Android delivery"
git status --short
```

Expected: as mudanças anteriores de IA zumbi aparecem somente se ainda não tiverem sido commitadas por suas próprias tarefas; nenhum arquivo alheio fica staged. Não fazer push para GitHub sem um pedido explícito do usuário.
