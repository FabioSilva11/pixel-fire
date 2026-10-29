# Campaign Screens and Ready-Made Controls Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Deliver a six-stage local campaign shell around the existing game and replace hand-written camera, joystick, and player collision with pinned, locally packaged libraries.

**Architecture:** Pure JavaScript modules own campaign rules, save migration, screen transitions, and aim selection; DOM adapters render the flow. Thin runtime adapters connect Camera Controls, NippleJS, and Rapier to the existing Three.js world without rewriting maps, avatars, animation, enemies, or minimap.

**Tech Stack:** Android WebView, HTML/CSS, ES modules, Three.js `0.185.1`, Camera Controls `3.1.2`, `@dimforge/rapier3d-compat` `0.20.0`, NippleJS `1.0.4`, Node test runner, Kotlin/Gradle.

**Spec:** `docs/superpowers/specs/2026-08-28-campaign-screens-and-controls-design.md`

## Global Constraints

- Preserve the current maps, 18 avatars, animation system, minimap, world renderer, and enemy foundation.
- Package all runtime libraries and Rapier WebAssembly in Android assets; gameplay must not fetch a CDN.
- The first campaign chapter has exactly six sequential stages.
- Progress is local and versioned; no account, cloud, payment, ads, rankings, or multiplayer.
- Camera uses Camera Controls, movement collision uses Rapier, and the mobile joystick uses NippleJS.
- Assisted aiming never fires automatically and never targets through walls.
- Keep landscape touch targets large and honor safe-area insets.
- Run Gradle with `--no-daemon --max-workers=1` on this machine.

---

### Task 1: Package the pinned web runtime locally

**Files:**
- Modify: `package.json`
- Create: `package-lock.json`
- Create: `scripts/vendor-web-runtime.ps1`
- Create: `app/src/main/assets/game/vendor/three.module.js`
- Create: `app/src/main/assets/game/vendor/three/addons/loaders/GLTFLoader.js`
- Create: `app/src/main/assets/game/vendor/three/addons/utils/BufferGeometryUtils.js`
- Create: `app/src/main/assets/game/vendor/camera-controls.module.js`
- Create: `app/src/main/assets/game/vendor/rapier.mjs`
- Create: `app/src/main/assets/game/vendor/rapier_wasm3d_bg.wasm`
- Create: `app/src/main/assets/game/vendor/nipplejs.mjs`
- Create: `app/src/main/assets/notices/CAMERA_CONTROLS_LICENSE.txt`
- Create: `app/src/main/assets/notices/RAPIER_LICENSE.txt`
- Create: `app/src/main/assets/notices/NIPPLEJS_LICENSE.txt`
- Modify: `app/src/main/assets/game/index.html`
- Test: `web-tests/offline-runtime.test.mjs`

**Interfaces:**
- Produces import-map names `three`, `three/addons/`, `camera-controls`, `@dimforge/rapier3d-compat`, and `nipplejs`.
- Produces `scripts/vendor-web-runtime.ps1`, which fails when an expected package file is absent.

- [ ] **Step 1: Write the failing offline-runtime test**

```js
test('import map and CSP use only packaged runtime modules', () => {
  const html = readFileSync('app/src/main/assets/game/index.html', 'utf8');
  assert.doesNotMatch(html, /cdn\.jsdelivr\.net/);
  for (const path of ['./vendor/three.module.js', './vendor/camera-controls.module.js', './vendor/rapier.mjs', './vendor/nipplejs.mjs']) {
    assert.match(html, new RegExp(path.replaceAll('.', '\\.')));
  }
});
```

- [ ] **Step 2: Run the test and confirm it fails on CDN references**

Run: `node --test web-tests/offline-runtime.test.mjs`
Expected: FAIL because `index.html` still references jsDelivr and has no local library mappings.

- [ ] **Step 3: Install exact dependencies and add the vendor script**

Run: `npm install --save-exact three@0.185.1 camera-controls@3.1.2 @dimforge/rapier3d-compat@0.20.0 nipplejs@1.0.4`

The PowerShell script must copy the exact package files listed above with `Copy-Item -LiteralPath`, create target directories, and copy each license. It exits non-zero if any source or output is missing.

- [ ] **Step 4: Generate assets and replace the import map/CSP**

Run: `powershell -ExecutionPolicy Bypass -File scripts/vendor-web-runtime.ps1`

Use this import map:

```json
{
  "imports": {
    "three": "./vendor/three.module.js",
    "three/addons/": "./vendor/three/addons/",
    "camera-controls": "./vendor/camera-controls.module.js",
    "@dimforge/rapier3d-compat": "./vendor/rapier.mjs",
    "nipplejs": "./vendor/nipplejs.mjs"
  }
}
```

- [ ] **Step 5: Run the targeted and full web tests**

Run: `node --test web-tests/offline-runtime.test.mjs web-tests/loading.test.mjs`
Expected: PASS.

- [ ] **Step 6: Commit the offline runtime**

```powershell
git add package.json package-lock.json scripts/vendor-web-runtime.ps1 app/src/main/assets/game/index.html app/src/main/assets/game/vendor app/src/main/assets/notices web-tests/offline-runtime.test.mjs
git commit -m "build: package offline game control runtime"
```

### Task 2: Add campaign catalog and progression rules

**Files:**
- Create: `app/src/main/assets/game/js/campaign.js`
- Create: `app/src/main/assets/game/js/progression.js`
- Test: `web-tests/campaign.test.mjs`
- Test: `web-tests/progression.test.mjs`

**Interfaces:**
- Produces `CAMPAIGN`, `stageById(id)`, `createInitialProgress()`, `evaluateStage(stage, result)`, `applyStageResult(progress, stage, result)`, `upgradeCost(kind, level)`, and `purchaseUpgrade(progress, kind)`.
- A stage is `{ id, number, title, mapId, objective, targetSeconds, firstClearCrystals, storyBefore, storyAfter }`.
- A result is `{ completed, elapsedSeconds, healthRemaining, maxHealth, collectedCrystalIds }`.

- [ ] **Step 1: Write failing catalog tests**

```js
test('chapter contains six sequential stages with complete presentation data', () => {
  assert.deepEqual(CAMPAIGN.stages.map(({ number }) => number), [1, 2, 3, 4, 5, 6]);
  for (const stage of CAMPAIGN.stages) {
    assert.ok(stage.title && stage.objective && stage.targetSeconds > 0);
    assert.ok(stage.storyBefore.lines.length <= 3);
    assert.ok(stage.storyAfter.lines.length <= 3);
  }
});
```

- [ ] **Step 2: Write failing progression tests**

```js
test('completion, health and target time award independent stars', () => {
  const stage = CAMPAIGN.stages[0];
  assert.equal(evaluateStage(stage, { completed: true, elapsedSeconds: stage.targetSeconds - 1, healthRemaining: 50, maxHealth: 100 }).stars, 3);
  assert.equal(evaluateStage(stage, { completed: true, elapsedSeconds: stage.targetSeconds + 1, healthRemaining: 49, maxHealth: 100 }).stars, 1);
});

test('replay grants only newly earned star rewards', () => {
  const stage = CAMPAIGN.stages[0];
  const oneStar = { completed: true, elapsedSeconds: stage.targetSeconds + 20, healthRemaining: 1, maxHealth: 100, collectedCrystalIds: [] };
  const first = applyStageResult(createInitialProgress(), stage, oneStar);
  const replay = applyStageResult(first.progress, stage, oneStar);
  assert.equal(replay.crystalsAwarded, 0);
});
```

- [ ] **Step 3: Run tests and confirm missing-module failures**

Run: `node --test web-tests/campaign.test.mjs web-tests/progression.test.mjs`
Expected: FAIL with module-not-found.

- [ ] **Step 4: Implement frozen catalog and pure progression functions**

Use crystal rewards of `40 + stage.number * 10` for first clears, 15 per newly earned star, and upgrade costs `[50, 90, 140, 200, 280]`. Level zero is the starting level and level five is the cap. Unlock only the next sequential stage after a completion.

- [ ] **Step 5: Run tests**

Run: `node --test web-tests/campaign.test.mjs web-tests/progression.test.mjs`
Expected: PASS.

- [ ] **Step 6: Commit campaign rules**

```powershell
git add app/src/main/assets/game/js/campaign.js app/src/main/assets/game/js/progression.js web-tests/campaign.test.mjs web-tests/progression.test.mjs
git commit -m "feat: add local campaign progression rules"
```

### Task 3: Add versioned local save and screen-flow state

**Files:**
- Create: `app/src/main/assets/game/js/save-store.js`
- Create: `app/src/main/assets/game/js/screen-flow.js`
- Test: `web-tests/save-store.test.mjs`
- Test: `web-tests/screen-flow.test.mjs`

**Interfaces:**
- Produces `createSaveStore(storage, key = 'ecos-de-eldra.save')` with `load()`, `save(progress)`, and `update(updater)`.
- Produces `createScreenFlow(progress)` with `state`, `go(screen, payload)`, `pause()`, `resume()`, and `subscribe(listener)`.

- [ ] **Step 1: Write failing save tests with an in-memory Storage double**

```js
test('invalid fields reset without erasing valid progression', () => {
  storage.setItem(KEY, JSON.stringify({ version: 1, crystals: 95, selectedAvatar: 99, stages: { stage1: { stars: 2 } } }));
  const saved = createSaveStore(storage, KEY).load();
  assert.equal(saved.crystals, 95);
  assert.equal(saved.selectedAvatar, 'a');
  assert.equal(saved.stages.stage1.stars, 2);
});
```

- [ ] **Step 2: Write failing flow tests**

```js
test('first run routes through avatar while returning players reach the map', () => {
  assert.equal(createScreenFlow(createInitialProgress()).state.screen, 'avatar');
  const returning = createInitialProgress(); returning.selectedAvatar = 'b'; returning.hasStarted = true;
  assert.equal(createScreenFlow(returning).state.screen, 'home');
});
```

- [ ] **Step 3: Run tests and confirm failures**

Run: `node --test web-tests/save-store.test.mjs web-tests/screen-flow.test.mjs`
Expected: FAIL with module-not-found.

- [ ] **Step 4: Implement schema validation, migration, and legal transitions**

Save a full validated replacement JSON record. Legal screens are `loading`, `home`, `avatar`, `chapter`, `story`, `preparation`, `gameplay`, `victory`, `defeat`, and `compatibility`; `pause` and `map` are overlays.

- [ ] **Step 5: Run tests and commit**

Run: `node --test web-tests/save-store.test.mjs web-tests/screen-flow.test.mjs`
Expected: PASS.

```powershell
git add app/src/main/assets/game/js/save-store.js app/src/main/assets/game/js/screen-flow.js web-tests/save-store.test.mjs web-tests/screen-flow.test.mjs
git commit -m "feat: add resilient local campaign save"
```

### Task 4: Build the campaign screens and visual flow

**Files:**
- Modify: `app/src/main/assets/game/index.html`
- Modify: `app/src/main/assets/game/css/game.css`
- Create: `app/src/main/assets/game/js/campaign-ui.js`
- Modify: `app/src/main/assets/game/js/selection.js`
- Test: `web-tests/campaign-ui.test.mjs`

**Interfaces:**
- Produces `mountCampaignUI({ root, flow, store, campaign, characters, onStartMission })` with `render()` and `dispose()`.
- Consumes `screen-flow.js`, `save-store.js`, `campaign.js`, and `progression.js`.

- [ ] **Step 1: Write a failing structural test**

```js
test('campaign shell exposes every approved screen and one primary action per screen', () => {
  const html = readFileSync('app/src/main/assets/game/index.html', 'utf8');
  for (const id of ['home-screen', 'chapter-screen', 'story-screen', 'preparation-screen', 'pause-screen', 'victory-screen', 'defeat-screen']) {
    assert.match(html, new RegExp(`id="${id}"`));
  }
  assert.match(html, /data-primary-action/);
});
```

- [ ] **Step 2: Run and confirm missing-screen failures**

Run: `node --test web-tests/campaign-ui.test.mjs`
Expected: FAIL because the approved campaign screens are absent.

- [ ] **Step 3: Add semantic HTML for the approved screens**

Home shows avatar, Continue, chapter progress, stars, crystals, Avatar, and Settings. Chapter renders six buttons from `CAMPAIGN`. Story renders speaker and at most three lines. Preparation renders stars, objective, rewards, two upgrades, and Start. Victory and Defeat expose the approved actions.

- [ ] **Step 4: Add responsive campaign styling**

Reuse the existing CSS variables. Add `.campaign-screen`, `.chapter-path`, `.stage-node`, `.story-card`, `.upgrade-card`, `.result-stars`, and `.pause-panel`; keep transitions between 200 and 350 ms and include `env(safe-area-inset-*)` padding.

- [ ] **Step 5: Implement UI binding and existing avatar reuse**

Render all values through `textContent`; use fixed internal markup only for icons. Extend `mountSelection` with `selectedId` and `onBack` options without removing current selection behavior.

- [ ] **Step 6: Run UI and full pure tests, then commit**

Run: `node --test web-tests/campaign-ui.test.mjs web-tests/selection.test.mjs web-tests/campaign.test.mjs web-tests/progression.test.mjs web-tests/save-store.test.mjs web-tests/screen-flow.test.mjs`
Expected: PASS.

```powershell
git add app/src/main/assets/game/index.html app/src/main/assets/game/css/game.css app/src/main/assets/game/js/campaign-ui.js app/src/main/assets/game/js/selection.js web-tests/campaign-ui.test.mjs web-tests/selection.test.mjs
git commit -m "feat: add chapter campaign screen flow"
```

### Task 5: Replace joystick and player collision with NippleJS and Rapier

**Files:**
- Create: `app/src/main/assets/game/js/player-controller.js`
- Modify: `app/src/main/assets/game/js/world.js`
- Modify: `app/src/main/assets/game/js/main.js`
- Delete: `app/src/main/assets/game/js/input.js`
- Delete: `web-tests/input.test.mjs`
- Test: `web-tests/player-controller.test.mjs`

**Interfaces:**
- Produces `createPlayerController({ RAPIER, nipplejs, joystickZone, world, player, cameraHeading })` with `movement`, `move(delta)`, `setEnabled(value)`, and `dispose()`.
- `world.physicsGeometry()` returns `{ walls, floorHeightAt, canOccupy, gateOpened }` and `world.setPlayerController(controller)`.

- [ ] **Step 1: Write failing adapter tests with Rapier/NippleJS doubles**

```js
test('controller feeds camera-relative desired motion to Rapier', () => {
  const fixture = makeControllerHarness({ stick: { x: 1, y: 0 }, cameraYaw: Math.PI / 2 });
  const controller = createPlayerController(fixture.options);
  controller.move(0.1);
  assert.deepEqual(fixture.characterController.lastDesired, { x: 0, y: 0, z: 0.435 });
});

test('dispose destroys the NippleJS manager and Rapier world', () => {
  const fixture = makeControllerHarness({ stick: { x: 0, y: 0 }, cameraYaw: 0 });
  const controller = createPlayerController(fixture.options);
  controller.dispose();
  assert.equal(fixture.joystickDestroyed(), true);
  assert.equal(fixture.physicsFreed(), true);
});
```

- [ ] **Step 2: Run and confirm missing-module failures**

Run: `node --test web-tests/player-controller.test.mjs`
Expected: FAIL with module-not-found.

- [ ] **Step 3: Expose world wall AABBs and implement Rapier adapter**

Create fixed cuboid colliders from `buildWallColliders(layout)`, a kinematic capsule for the player, and a `world.createCharacterController(0.03)` configured with sliding, `enableAutostep(0.32, 0.18, false)`, and `enableSnapToGround(0.4)`. Each frame calls `computeColliderMovement`, applies `computedMovement`, validates semantic layout/door occupancy, and samples `floorHeightAt` for the map's two-level stair interpolation.

- [ ] **Step 4: Bind NippleJS and remove custom pointer joystick**

Use a static joystick in `#joystick` with one pointer, normalized `data.vector`, and zero motion on `end`. Preserve a separate right-side camera zone and simultaneous fire pointers.

- [ ] **Step 5: Run tests and commit**

Run: `node --test web-tests/player-controller.test.mjs web-tests/map-collision.test.mjs web-tests/phase-layout.test.mjs web-tests/world-navigation.test.mjs`
Expected: PASS.

```powershell
git add app/src/main/assets/game/js/player-controller.js app/src/main/assets/game/js/world.js app/src/main/assets/game/js/main.js web-tests/player-controller.test.mjs
git rm app/src/main/assets/game/js/input.js web-tests/input.test.mjs
git commit -m "feat: use Rapier and NippleJS player controls"
```

### Task 6: Replace the hand-written camera rig with Camera Controls

**Files:**
- Create: `app/src/main/assets/game/js/camera-controller.js`
- Modify: `app/src/main/assets/game/js/world.js`
- Modify: `app/src/main/assets/game/js/main.js`
- Delete: `app/src/main/assets/game/js/camera-rig.js`
- Delete: `web-tests/camera-rig.test.mjs`
- Test: `web-tests/camera-controller.test.mjs`

**Interfaces:**
- Produces `createThirdPersonCamera({ CameraControls, THREE, camera, domElement, player, colliderMeshes, sensitivity })` with `update(delta)`, `heading()`, `setSensitivity(value)`, `setEnabled(value)`, and `dispose()`.
- `world.cameraColliderMeshes` contains the prepared structural meshes.

- [ ] **Step 1: Write failing configuration tests**

```js
test('third-person camera configures distance, pitch, touch rotation and colliders', () => {
  const camera = createThirdPersonCamera(harness());
  assert.equal(harness.controls.minDistance, 0.72);
  assert.equal(harness.controls.maxDistance, 6.25);
  assert.equal(harness.controls.touches.one, harness.CameraControls.ACTION.TOUCH_ROTATE);
  assert.equal(harness.controls.colliderMeshes, harness.colliderMeshes);
});
```

- [ ] **Step 2: Run and confirm missing-module failure**

Run: `node --test web-tests/camera-controller.test.mjs`
Expected: FAIL with module-not-found.

- [ ] **Step 3: Implement the Camera Controls adapter**

Install Three into Camera Controls once, connect only to `#camera-zone`, use min/max polar angles equivalent to the approved dungeon pitch, target player position plus 1.2 metres, and call `controls.update(delta)` each frame. Disable dolly and truck gestures.

- [ ] **Step 4: Replace camera-rig calls in main and world**

Movement reads `cameraController.heading()`. Camera collision comes from `controls.colliderMeshes`; remove `world.resolveCameraPosition` and the hand-written camera math only after the new adapter is active.

- [ ] **Step 5: Run tests and commit**

Run: `node --test web-tests/camera-controller.test.mjs web-tests/player-controller.test.mjs web-tests/world-navigation.test.mjs`
Expected: PASS.

```powershell
git add app/src/main/assets/game/js/camera-controller.js app/src/main/assets/game/js/world.js app/src/main/assets/game/js/main.js web-tests/camera-controller.test.mjs
git rm app/src/main/assets/game/js/camera-rig.js web-tests/camera-rig.test.mjs
git commit -m "feat: use Camera Controls third-person rig"
```

### Task 7: Add manual ranged fire, assisted aim, and mission results

**Files:**
- Create: `app/src/main/assets/game/js/aim-assist.js`
- Modify: `app/src/main/assets/game/js/game-rules.js`
- Modify: `app/src/main/assets/game/js/combat.js`
- Modify: `app/src/main/assets/game/js/main.js`
- Modify: `app/src/main/assets/game/index.html`
- Modify: `app/src/main/assets/game/css/game.css`
- Test: `web-tests/aim-assist.test.mjs`
- Modify: `web-tests/game-rules.test.mjs`

**Interfaces:**
- Produces `selectAssistedTarget({ candidates, maxRange, maxAngle, hasLineOfSight, retainedId })`; candidates contain `{ id, distance, angle }` calculated from the camera ray.
- `CombatSystem.fire(target)` returns `{ fired, hit, defeated, targetId }` and applies the progression power multiplier.
- `createGame` resolves with a mission result on victory or defeat instead of reloading the whole page.

- [ ] **Step 1: Write failing aim tests**

```js
test('assist chooses the nearest visible target in the reticle cone', () => {
  const candidates = [
    { id: 'near-visible', distance: 6, angle: 0.05 },
    { id: 'far-visible', distance: 12, angle: 0.08 },
    { id: 'blocked', distance: 4, angle: 0.02 },
  ];
  const selected = selectAssistedTarget({ candidates, maxRange: 18, maxAngle: 0.12, hasLineOfSight: id => id !== 'blocked' });
  assert.equal(selected.id, 'near-visible');
});

test('assist rejects targets behind walls and never fires', () => {
  let fireCalls = 0;
  const selected = selectAssistedTarget({ candidates: [{ id: 'blocked', distance: 4, angle: 0.01 }], maxRange: 18, maxAngle: 0.12, hasLineOfSight: () => false });
  assert.equal(selected, null);
  assert.equal(fireCalls, 0);
});
```

- [ ] **Step 2: Run and confirm failure**

Run: `node --test web-tests/aim-assist.test.mjs web-tests/game-rules.test.mjs`
Expected: FAIL because ranged targeting is absent.

- [ ] **Step 3: Implement pure target scoring and ranged-hit rules**

Score by angular distance first, world distance second, with a small retained-target bonus. Use a Three.js raycaster against static geometry for line of sight. Fire remains button-driven with a 0.45-second cooldown, 18-metre range, and base damage multiplied by the power upgrade.

- [ ] **Step 4: Wire HUD feedback and mission outcomes**

Add reticle lock class, target health, reload state, hit marker, directional damage indicator, pause button, and collected-crystal counter. Victory supplies elapsed time and health; defeat supplies the cause. Do not call `window.location.reload()` for ordinary replay or stage transitions.

- [ ] **Step 5: Run tests and commit**

Run: `node --test web-tests/aim-assist.test.mjs web-tests/game-rules.test.mjs web-tests/progression.test.mjs`
Expected: PASS.

```powershell
git add app/src/main/assets/game/js/aim-assist.js app/src/main/assets/game/js/game-rules.js app/src/main/assets/game/js/combat.js app/src/main/assets/game/js/main.js app/src/main/assets/game/index.html app/src/main/assets/game/css/game.css web-tests/aim-assist.test.mjs web-tests/game-rules.test.mjs
git commit -m "feat: add assisted manual ranged combat"
```

### Task 8: Integrate campaign lifecycle, compatibility errors, and offline boot

**Files:**
- Modify: `app/src/main/assets/game/js/bootstrap.js`
- Modify: `app/src/main/assets/game/js/loading.js`
- Modify: `app/src/main/assets/game/js/main.js`
- Modify: `app/src/main/assets/game/js/campaign-ui.js`
- Create: `app/src/main/assets/game/js/mission-lifecycle.js`
- Modify: `app/src/main/assets/game/index.html`
- Modify: `app/src/main/assets/game/css/game.css`
- Modify: `README.md`
- Test: `web-tests/loading.test.mjs`
- Test: `web-tests/mission-lifecycle.test.mjs`

**Interfaces:**
- `startApp(loading)` mounts one campaign shell for the whole WebView lifetime.
- `startMission(stageId)` returns `{ outcome, elapsedSeconds, healthRemaining, maxHealth, collectedCrystalIds }`.
- `completeMission(progress, stageId, result)` returns `{ progress, award, nextScreen: 'victory' }`; `routeBootFailure(error)` returns `compatibility` for WebGL/WebAssembly failures.

- [ ] **Step 1: Write failing lifecycle and compatibility tests**

```js
test('a completed mission updates progress then returns to the chapter map', async () => {
  const result = completeMission(createInitialProgress(), 'stage1', { completed: true, healthRemaining: 80, maxHealth: 100, elapsedSeconds: 40, collectedCrystalIds: [] });
  assert.equal(result.progress.stages.stage1.stars, 3);
  assert.equal(result.nextScreen, 'victory');
});

test('missing WebAssembly routes to compatibility instead of blank gameplay', async () => {
  assert.equal(await routeBootFailure(new WebAssembly.CompileError('unsupported')), 'compatibility');
});
```

- [ ] **Step 2: Run and confirm failures**

Run: `node --test web-tests/mission-lifecycle.test.mjs web-tests/loading.test.mjs`
Expected: FAIL because lifecycle and compatibility routing are absent.

- [ ] **Step 3: Mount the persistent campaign shell and local boot sequence**

Initialize local Three.js, Camera Controls, Rapier, save data, and campaign data with named progress stages. Route WebGL/WASM failures to `#compatibility-screen`; other retryable asset failures return to Home or Chapter.

- [ ] **Step 4: Connect victory/defeat to progression and save**

Apply results exactly once, persist before rendering Victory, and reveal the unlocked stage after returning to Chapter. Pause disables physics, camera, combat, and animation updates while keeping UI responsive.

- [ ] **Step 5: Update README verification and offline statement**

Document `npm install`, `scripts/vendor-web-runtime.ps1`, `npm test`, and the low-memory Gradle command. Remove the statement that first startup requires internet.

- [ ] **Step 6: Run the complete web suite and commit**

Run: `npm test`
Expected: all web tests PASS.

```powershell
git add app/src/main/assets/game/js/bootstrap.js app/src/main/assets/game/js/loading.js app/src/main/assets/game/js/main.js app/src/main/assets/game/js/campaign-ui.js app/src/main/assets/game/js/mission-lifecycle.js app/src/main/assets/game/index.html app/src/main/assets/game/css/game.css README.md web-tests/loading.test.mjs web-tests/mission-lifecycle.test.mjs
git commit -m "feat: integrate offline campaign lifecycle"
```

### Task 9: Verify browser, Android build, and device behavior

**Files:**
- Modify only if verification exposes a scoped defect in files from Tasks 1-8.
- Update: `README.md` with final artifact facts only after successful build.

**Interfaces:**
- Produces a tested debug APK at `app/build/outputs/apk/debug/app-debug.apk`.

- [ ] **Step 1: Run static and unit verification**

Run: `npm test`
Expected: all tests PASS with no skipped campaign/control tests.

- [ ] **Step 2: Reload the existing browser preview and exercise the complete loop**

Verify Home, avatar, chapter, story, preparation, gameplay, pause, victory, defeat, replay, and persisted refresh. Inspect console errors and confirm the Network panel has no CDN requests.

- [ ] **Step 3: Build Android with low-memory settings**

Run: `.\gradlew.bat :app:testDebugUnitTest :app:lintDebug :app:assembleDebug --no-daemon --max-workers=1`
Expected: `BUILD SUCCESSFUL` and a fresh debug APK.

- [ ] **Step 4: Verify artifact freshness and hash**

Run: `Get-Item app\build\outputs\apk\debug\app-debug.apk | Select-Object FullName,Length,LastWriteTime; Get-FileHash app\build\outputs\apk\debug\app-debug.apk -Algorithm SHA256`
Expected: timestamp from this build and a non-empty SHA-256.

- [ ] **Step 5: Install only on a currently authorized device**

Run: `adb devices -l`
If exactly one authorized device is listed, resolve its serial and install without a hard-coded historical identifier:

```powershell
$authorizedDevices = @(adb devices | Select-String "`tdevice$" | ForEach-Object { ($_ -split '\s+')[0] })
if ($authorizedDevices.Count -eq 1) { adb -s $authorizedDevices[0] install -r app\build\outputs\apk\debug\app-debug.apk }
```

Verify `Success`. If none is authorized, report build success separately and do not claim installation. If verification exposes a defect, return to the task that owns that behavior, add a reproducing test there, and repeat that task's failing-test, minimal-patch, full-test, and scoped-commit cycle before making a completion claim.
