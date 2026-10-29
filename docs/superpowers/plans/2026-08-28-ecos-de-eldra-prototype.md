# Ecos de Eldra Prototype Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build and visually verify an Android debug APK containing a playable online-Three.js dungeon exploration prototype made from the supplied Kenney assets.

**Architecture:** A minimal Kotlin Android activity hosts a hardened WebView and serves packaged game files with `WebViewAssetLoader`. The game is a set of focused ES modules; Three.js and `GLTFLoader` come from jsDelivr at exactly `0.185.0`, while selected GLBs and all character textures remain local.

**Tech Stack:** Android Gradle Plugin 9.3.0, Gradle 9.3.1, Kotlin 2.3.20, AndroidX WebKit, Kotlin/JUnit, HTML/CSS, ES modules, Three.js 0.185.0, Node test runner, Playwright/Chromium for visual verification.

**Spec:** `docs/superpowers/specs/2026-08-28-ecos-de-eldra-prototype-design.md`

## Global Constraints

- Application ID is `com.fabiosilva.ecosdeeldra` and the temporary visible name is `Ecos de Eldra`.
- Three.js core and addons must both come from jsDelivr version `0.185.0`.
- The game starts only with network access; local gameplay code, GLBs, and textures ship inside the APK.
- The Android activity is landscape-only and the game interface is in Brazilian Portuguese.
- Build with one Gradle worker, no daemon, and Kotlin compilation in process.
- The final result must come from the running renderer, not a mockup.
- Each phase is exactly 1000 by 1000 metres in logical coordinates.
- Room and corridor rectangles must not overlap; both player and enemies use the same occupancy collision test.
- The minimap is dark for unexplored walkable cells, light for explored cells, and opens to a large overlay on tap.
- The test phase quota is six zombies distributed in separate rooms; the exit remains locked until the quota is zero.

---

### Task 1: Android shell and safe local asset host

**Files:**
- Create: `settings.gradle.kts`
- Create: `build.gradle.kts`
- Create: `gradle.properties`
- Create: `gradle/wrapper/gradle-wrapper.properties`
- Create: `gradlew`, `gradlew.bat`, `gradle/wrapper/gradle-wrapper.jar`
- Create: `app/build.gradle.kts`
- Create: `app/src/main/AndroidManifest.xml`
- Create: `app/src/main/java/com/fabiosilva/ecosdeeldra/MainActivity.kt`
- Create: `app/src/main/res/values/strings.xml`
- Create: `app/src/main/res/values/themes.xml`
- Test: `app/src/test/java/com/fabiosilva/ecosdeeldra/NavigationPolicyTest.kt`

**Interfaces:**
- Produces: `NavigationPolicy.isAllowedTopLevel(url: String): Boolean` and an Activity that loads `https://appassets.androidplatform.net/assets/game/index.html`.

- [ ] **Step 1: Write a failing navigation policy test**

```kotlin
@Test fun allowsOnlyPackagedGamePage() {
    assertTrue(NavigationPolicy.isAllowedTopLevel("https://appassets.androidplatform.net/assets/game/index.html"))
    assertFalse(NavigationPolicy.isAllowedTopLevel("https://example.com/"))
    assertFalse(NavigationPolicy.isAllowedTopLevel("file:///android_asset/game/index.html"))
}
```

- [ ] **Step 2: Run the test and verify the missing implementation failure**

Run: `./gradlew.bat :app:testDebugUnitTest --no-daemon --max-workers=1`

Expected: compilation fails because `NavigationPolicy` does not exist.

- [ ] **Step 3: Add the minimal Android project and WebView activity**

Implement `NavigationPolicy` as an exact scheme/host/path allowlist. Configure `WebViewAssetLoader.AssetsPathHandler`, JavaScript, DOM storage, blocked file/content access, blocked mixed content, renderer error logging, fullscreen immersive landscape mode, and back handling.

- [ ] **Step 4: Run Android unit tests**

Run: `./gradlew.bat :app:testDebugUnitTest --no-daemon --max-workers=1`

Expected: `NavigationPolicyTest` passes.

- [ ] **Step 5: Commit the Android shell**

```powershell
git add settings.gradle.kts build.gradle.kts gradle.properties gradle app
git commit -m "feat: add secure Android game shell"
```

### Task 2: Asset catalog and deterministic game rules

**Files:**
- Create: `app/src/main/assets/game/js/catalog.js`
- Create: `app/src/main/assets/game/js/game-rules.js`
- Test: `web-tests/catalog.test.mjs`
- Test: `web-tests/game-rules.test.mjs`
- Create: `package.json`

**Interfaces:**
- Produces: `CHARACTERS: CharacterDefinition[]`, `DUNGEON_ASSETS`, `clampToArena(position)`, `canMeleeHit(attacker, target)`, and `nextEnemyState(enemy, player, delta)`.

- [ ] **Step 1: Write failing Node tests for the 18-character catalog and combat boundaries**

```javascript
test('catalog exposes all supplied characters', () => assert.equal(CHARACTERS.length, 18));
test('melee rejects targets behind the attacker', () => assert.equal(canMeleeHit(attacker, behind), false));
test('arena clamps the player inside walls', () => assert.deepEqual(clampToArena({ x: 99, z: -99 }), { x: 11.5, z: -17.5 }));
```

- [ ] **Step 2: Run tests and verify missing-module failures**

Run: `node --test web-tests/*.test.mjs`

Expected: module-not-found failures.

- [ ] **Step 3: Implement the catalog and pure rules**

Character IDs must be `a` through `r`, with local GLB and preview-texture paths. Combat uses a 2.15-unit radius, a forward dot-product threshold of 0.25, and explicit idle/chase/attack/dead states.

- [ ] **Step 4: Run Node tests**

Run: `node --test web-tests/*.test.mjs`

Expected: all catalog and rule tests pass.

- [ ] **Step 5: Commit deterministic game logic**

```powershell
git add package.json web-tests app/src/main/assets/game/js
git commit -m "feat: add character catalog and game rules"
```

### Task 3: Online runtime, selection interface, and asset packaging

**Files:**
- Create: `app/src/main/assets/game/index.html`
- Create: `app/src/main/assets/game/css/game.css`
- Create: `app/src/main/assets/game/js/loading.js`
- Create: `app/src/main/assets/game/js/selection.js`
- Create: `scripts/copy-assets.ps1`
- Create: `app/src/main/assets/notices/KENNEY_LICENSE.txt`
- Create: `app/src/main/assets/notices/THREE_LICENSE.txt`
- Create: `web-tests/page-structure.test.mjs`

**Interfaces:**
- Consumes: `CHARACTERS` from Task 2.
- Produces: selection event `ecos:character-selected` with `{ id: string }`, loading API `setLoadingProgress(label, percent)`, and packaged asset paths.

- [ ] **Step 1: Write failing structural tests**

```javascript
test('import map pins one Three.js version', () => {
  assert.match(html, /three@0\.185\.0\/build\/three\.module\.js/);
  assert.match(html, /three@0\.185\.0\/examples\/jsm\//);
});
test('page exposes selection, loading, HUD and retry roots', () => {
  for (const id of ['loading-screen','selection-screen','game-hud','retry-button']) assert.match(html, new RegExp(`id="${id}"`));
});
```

- [ ] **Step 2: Run structural tests and verify failure**

Run: `node --test web-tests/page-structure.test.mjs`

Expected: fails because `index.html` does not exist.

- [ ] **Step 3: Implement the stylized selector, loader, and copy script**

Use a violet/amber/cyan palette, responsive landscape layout, 18 selectable character cards, Portuguese messages, retry behavior, and a content-security policy limited to self and jsDelivr. The copy script copies character GLBs/textures plus the selected dungeon GLBs and shared colormap.

- [ ] **Step 4: Run the copy script and structural tests**

Run: `powershell -ExecutionPolicy Bypass -File scripts/copy-assets.ps1`

Run: `node --test web-tests/page-structure.test.mjs`

Expected: assets are present and tests pass.

- [ ] **Step 5: Commit UI and packaged assets**

```powershell
git add app/src/main/assets scripts web-tests
git commit -m "feat: add character selection and packaged assets"
```

### Task 4: Playable Three.js dungeon

**Files:**
- Create: `app/src/main/assets/game/js/animation.js`
- Create: `app/src/main/assets/game/js/input.js`
- Create: `app/src/main/assets/game/js/world.js`
- Create: `app/src/main/assets/game/js/combat.js`
- Create: `app/src/main/assets/game/js/main.js`
- Test: `web-tests/animation.test.mjs`
- Test: `web-tests/input.test.mjs`

**Interfaces:**
- Consumes: catalog, rules, selection event, loading API, local GLBs, Three.js, and `GLTFLoader`.
- Produces: a running `Game` loop with `start(characterId)`, `update(delta)`, `attack()`, `dispose()`, enemy health updates, gate-open state, and completion overlay.

- [ ] **Step 1: Write failing animation and input tests**

```javascript
test('preferred animation falls back to idle', () => assert.equal(pickClip(['idle'], ['attack-melee-right','idle']), 'idle'));
test('normalized joystick never exceeds unit length', () => assert.ok(Math.hypot(...normalizeStick(100, 100, 40)) <= 1.0001));
```

- [ ] **Step 2: Run tests and verify missing exports**

Run: `node --test web-tests/*.test.mjs`

Expected: failures for `pickClip` and `normalizeStick`.

- [ ] **Step 3: Implement animation, touch input, world assembly, AI, combat, gate, and renderer**

Use a capped pixel ratio of 1.5, shadow maps, fog, warm key light, cyan fill light, a procedural floor fallback, smooth third-person camera, joystick movement, drag-look, melee button, two enemies, health HUD, gate `open` clip, and exit completion panel.

- [ ] **Step 4: Run all web tests and syntax checks**

Run: `node --test web-tests/*.test.mjs`

Run: `node --check app/src/main/assets/game/js/main.js`

Expected: all tests and syntax checks pass.

- [ ] **Step 5: Commit the playable game**

```powershell
git add app/src/main/assets/game/js web-tests
git commit -m "feat: add playable dungeon exploration prototype"
```

### Task 5: Build, launch, and verify

**Files:**
- Create: `README.md`
- Modify: `docs/superpowers/plans/2026-08-28-ecos-de-eldra-prototype.md`

**Interfaces:**
- Consumes: the complete Android app and web game.
- Produces: `app/build/outputs/apk/debug/app-debug.apk` and a verified run of the real gameplay flow.

- [ ] **Step 1: Run the complete automated verification**

Run: `node --test web-tests/*.test.mjs`

Run: `./gradlew.bat :app:testDebugUnitTest :app:lintDebug :app:assembleDebug --no-daemon --max-workers=1`

Expected: web tests pass, Android tests pass, lint has no errors, and the debug APK is produced.

- [ ] **Step 2: Launch the game in an available Android target or Chromium**

Prefer an attached device/emulator. If none exists, serve `app/src/main/assets` locally and run the exact HTML/JS renderer in Chromium with touch viewport emulation.

- [ ] **Step 3: Exercise the real flow**

Select a character, wait for GLBs, enter the dungeon, move the player, trigger an attack, and confirm the player, at least one enemy, and the dungeon geometry render correctly on the target.

- [ ] **Step 4: Record the verified commands and limitations**

Record exact build/test results, APK path and size, and whether validation used Android or Chromium. Do not claim device verification if only Chromium was available.

- [ ] **Step 5: Commit delivery evidence**

```powershell
git add README.md docs/superpowers/plans/2026-08-28-ecos-de-eldra-prototype.md
git commit -m "docs: add prototype verification evidence"
```

### Task 6: Expand to kilometre phase, collision graph, and map UI

**Files:**
- Create: `app/src/main/assets/game/js/phase-layout.js`
- Create: `app/src/main/assets/game/js/minimap.js`
- Modify: `app/src/main/assets/game/js/game-rules.js`
- Modify: `app/src/main/assets/game/js/world.js`
- Modify: `app/src/main/assets/game/js/combat.js`
- Modify: `app/src/main/assets/game/js/main.js`
- Modify: `app/src/main/assets/game/index.html`
- Modify: `app/src/main/assets/game/css/game.css`
- Test: `web-tests/phase-layout.test.mjs`
- Test: `web-tests/minimap.test.mjs`

**Interfaces:**
- Produces: `PHASE_SIZE_METERS = 1000`, `PHASE_LAYOUT`, `isWalkable(position, radius)`, `markExplored(position)`, `drawMinimap(context, state)`, and a clickable large-map overlay.

- [ ] **Step 1: Write failing tests for non-overlap, connectivity, collision and map state**
- [ ] **Step 2: Run tests and verify the missing exports fail**
- [ ] **Step 3: Implement deterministic 1000m layout, shared collision occupancy, six-zombie quota, and streamed visual pieces**
- [ ] **Step 4: Implement minimap, explored/unexplored rendering, tap-to-expand behavior and heart avatar icon**
- [ ] **Step 5: Run all web tests, build, install and capture the updated device screen**
- [ ] **Step 6: Commit the expansion**
