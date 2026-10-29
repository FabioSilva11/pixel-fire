# Zombie Zone Visual Overhaul Implementation Plan

> **For Codex:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Use superpowers:test-driven-development for each behavior change, imagegen for the three raster backgrounds, superpowers:systematic-debugging for any failing movement proof, superpowers:verification-before-completion before success claims, and superpowers:requesting-code-review after implementation.

**Goal:** Rename the game to Zombie Zone, repair real browser and Android character movement, and deliver a coherent child-friendly visual system with generated backgrounds and reusable live 3D avatar previews across every campaign screen.

**Architecture:** Preserve the existing offline Android WebView/Three.js campaign and save schema. Keep gameplay, progression, maps, and character catalog intact; add narrowly scoped presentation adapters (`avatar-preview.js`, `brand.js`) and a localhost-only preview hook. NippleJS exclusively owns its generated joystick DOM inside a renamed outer movement zone. All new art remains packaged in Android assets and every browser iteration uses explicit cache-version tags.

**Tech Stack:** Android/Kotlin WebView, HTML/CSS, JavaScript ES modules, Three.js 0.185.1, GLTFLoader, Camera Controls 3.1.2, Rapier 0.20.0, NippleJS 1.0.4, Node test runner, Gradle, ADB, built-in ImageGen, in-app browser, Playwright CLI for stable visual review runs.

**Spec:** `docs/superpowers/specs/2026-08-28-zombie-zone-visual-overhaul-design.md`

## Global Constraints

- Preserve all user work and do not restore or stage the pre-existing deleted map-editor files.
- Use `apply_patch` for hand-authored file changes and scope any Git staging to exact files.
- Keep all runtime dependencies and generated art local; no CDN or runtime network dependency.
- Do not change the persisted save shape or reset existing stars, crystals, upgrades, avatar, story, or settings.
- Do not claim movement fixed until a real browser drag changes the measured world coordinates and the installed Android build is exercised.
- Keep the in-app browser open and show progress there after meaningful milestones.
- Bump `?v=` asset/module cache tags when browser-visible code or CSS changes.

### Task 1: Repair the complete NippleJS-to-Rapier movement path

**Files:**
- Modify: `app/src/main/assets/game/index.html:108`
- Modify: `app/src/main/assets/game/css/game.css:84`
- Modify: `app/src/main/assets/game/js/player-controller.js:1`
- Modify: `web-tests/player-controller.test.mjs`

**Step 1: Write the failing structural test**

Add a test that reads `index.html` and `game.css` and asserts the outer zone uses `class="movement-zone"`, contains no hand-authored `.joystick-ring`/`.joystick-knob`, and the stylesheet does not define a generic `.joystick { ... }` rule.

**Step 2: Run the targeted test to verify RED**

Run: `node --test web-tests/player-controller.test.mjs`

Expected: FAIL because the current shell still uses `.joystick` and duplicate visual children.

**Step 3: Apply the minimum DOM/CSS fix**

Rename the outer control class to `.movement-zone`, remove its ring/knob children, style only the outer zone and NippleJS-generated `.back`/`.front` descendants, and preserve the 48-pixel touch target and safe-area placement.

**Step 4: Keep the event-adapter integration test explicit**

Retain or add a fake NippleJS manager that emits `{ data: { vector: { x, y } } }`, assert that `applyNippleVector` updates the normalized stick state, step the controller through fake Rapier resolution, and assert non-zero player displacement.

**Step 5: Run targeted tests to verify GREEN**

Run: `node --test web-tests/player-controller.test.mjs`

Expected: PASS.

**Step 6: Prove the browser path**

Refresh `http://127.0.0.1:4173/` after a cache-tag bump, enter gameplay, record `#game-hud[data-player-position]`, perform a real pointer drag inside `#joystick`, and record the changed coordinates. Inspect the fresh console for errors.

Expected: at least one X or Z coordinate changes; no NippleJS or Rapier error appears.

### Task 2: Establish the Zombie Zone brand contract

**Files:**
- Create: `app/src/main/assets/game/js/brand.js`
- Create: `web-tests/brand.test.mjs`
- Modify: `app/src/main/assets/game/index.html`
- Modify: `app/src/main/res/values/strings.xml`
- Modify: `package.json`
- Modify: `README.md`

**Step 1: Write the failing brand test**

Test `GAME_BRAND.name === 'Zombie Zone'`, assert Android `app_name`, HTML `<title>`, root aria-label, and production UI copy contain Zombie Zone, and assert no `Ecos de Eldra` remains in production app files.

**Step 2: Run the test to verify RED**

Run: `node --test web-tests/brand.test.mjs`

Expected: FAIL on the old name.

**Step 3: Implement the centralized brand constant and rename surfaces**

Export an immutable `GAME_BRAND` object, update Android/HTML/package/README labels, and keep story/progression identifiers unchanged so saves remain compatible.

**Step 4: Run the brand test to verify GREEN**

Run: `node --test web-tests/brand.test.mjs`

Expected: PASS.

### Task 3: Generate and package three original Zombie Zone backgrounds

**Files:**
- Create: `app/src/main/assets/game/images/zombie-zone-city-bg.png`
- Create: `app/src/main/assets/game/images/zombie-zone-campaign-bg.png`
- Create: `app/src/main/assets/game/images/zombie-zone-lab-bg.png`
- Create: `web-tests/zombie-zone-assets.test.mjs`

**Step 1: Write the failing asset-presence test**

Assert all three files exist, are PNGs, exceed 100 KB, and are referenced by `game.css` using local relative URLs only.

**Step 2: Run the test to verify RED**

Run: `node --test web-tests/zombie-zone-assets.test.mjs`

Expected: FAIL because the files do not exist.

**Step 3: Generate the city background with ImageGen**

Prompt: “Landscape 16:9 polished stylized 3D cartoon background for a children’s zombie adventure game: colorful abandoned city at sunset, deep navy buildings, radioactive green mist, electric violet and warm orange rim lights, distant friendly blocky zombie silhouettes, playful not scary, large clear negative space in the center-left for a live 3D avatar and title overlay, cinematic depth, no gore, no realistic injuries, no text, no logo, no UI, no buttons, no watermark.”

Inspect the generated image, then copy it to `zombie-zone-city-bg.png`.

**Step 4: Generate the campaign background with ImageGen**

Prompt: “Landscape 16:9 polished stylized 3D cartoon campaign-map background for a children’s zombie adventure: haunted playground and playful ruined amusement park connected to winding city streets, slightly elevated view, six visually recognizable zones and clear open areas for a path and level-node overlay, deep navy, radioactive green, electric violet, cyan and warm orange lighting, friendly blocky zombie mood, no gore, no text, no logo, no UI, no buttons, no watermark.”

Inspect the generated image, then copy it to `zombie-zone-campaign-bg.png`.

**Step 5: Generate the laboratory background with ImageGen**

Prompt: “Landscape 16:9 polished stylized 3D cartoon zombie laboratory interior for children: glowing specimen tanks, chunky pipes, playful warning lights, green bubbling energy, violet and cyan accents, central negative space for story and mission-preparation cards, adventurous and mysterious but not frightening, no gore, no realistic injuries, no text, no logo, no UI, no buttons, no watermark.”

Inspect the generated image, then copy it to `zombie-zone-lab-bg.png`.

**Step 6: Run the asset test to verify GREEN**

Run: `node --test web-tests/zombie-zone-assets.test.mjs`

Expected: PASS after CSS references are added in Task 5.

### Task 4: Build one reusable live 3D avatar preview

**Files:**
- Create: `app/src/main/assets/game/js/avatar-preview.js`
- Create: `web-tests/avatar-preview.test.mjs`
- Modify: `app/src/main/assets/game/index.html`
- Modify: `app/src/main/assets/game/js/main.js`
- Modify: `app/src/main/assets/game/js/bootstrap.js`

**Step 1: Write failing adapter tests**

Test a pure `resolvePreviewMode(screen)` mapping (`home`/`preparation` to idle, `avatar` to rotate, `victory` to celebrate), test that `show()` reparents one renderer canvas into the active `[data-avatar-preview]` host, and test that `hide()` pauses rendering while `dispose()` releases the mixer, model, renderer, observer, and animation frame.

**Step 2: Run the targeted tests to verify RED**

Run: `node --test web-tests/avatar-preview.test.mjs`

Expected: FAIL because the module does not exist.

**Step 3: Implement the adapter**

Create one transparent Three.js renderer with a small scene, camera, hemisphere/key/rim lights, GLTFLoader model loading through the existing character catalog URL, animation selection with idle fallback, gentle selection rotation, resize observation, stale-load cancellation, hidden-screen pause, and complete disposal.

**Step 4: Replace principal static portraits**

Add `[data-avatar-preview]` hosts to Home, Avatar, Preparation, and Victory; retain static PNGs only inside the 18-avatar thumbnail grid. Wire screen and selected-character changes from `main.js` to the preview adapter.

**Step 5: Run targeted tests to verify GREEN**

Run: `node --test web-tests/avatar-preview.test.mjs web-tests/selection.test.mjs`

Expected: PASS.

**Step 6: Browser-check the model reuse**

Walk Home → Avatar → Preparation → Gameplay → Victory preview route and confirm the selected GLB remains consistent, fills the panel, animates, pauses when hidden, and produces no duplicate WebGL canvas or console error.

### Task 5: Rebuild the shared screen system and every game screen

**Files:**
- Modify: `app/src/main/assets/game/index.html`
- Modify: `app/src/main/assets/game/css/game.css`
- Modify: `app/src/main/assets/game/js/main.js`
- Modify: `app/src/main/assets/game/js/campaign-ui.js`
- Modify: `app/src/main/assets/game/js/screen-flow.js`
- Create: `web-tests/zombie-zone-shell.test.mjs`

**Step 1: Write the failing shell contract test**

Assert the shell exposes Loading, Home, Avatar, Campaign, Story, Preparation, Gameplay HUD, Pause, Settings, Victory, Defeat, and Compatibility landmarks; shared header/resource badges; one primary-action hierarchy; local background classes; and no principal portrait `<img>` outside the thumbnail grid.

**Step 2: Run the test to verify RED**

Run: `node --test web-tests/zombie-zone-shell.test.mjs`

Expected: FAIL against the inconsistent current shell.

**Step 3: Introduce shared design tokens**

Define navy/green/violet/orange/cyan colors, panel overlays, spacing, radius, typography, focus rings, 48-pixel controls, safe-area behavior, dark background overlays, image-load gradient fallbacks, and reduced-motion rules.

**Step 4: Redesign campaign-shell screens**

Recompose Loading, Home, Avatar, Campaign, Story, and Preparation with the shared header, background layers, large readable cards, consistent buttons, live avatar hosts, six clear node states, concise Portuguese copy, and unclipped landscape/mobile layout.

**Step 5: Redesign runtime and result screens**

Align Gameplay HUD, Map overlay, Pause, Settings, Victory, Defeat, and Compatibility with the same visual language while preserving all existing control IDs and event bindings. Keep reticle, objective, target health, Fire, Reload, Dodge, crystals, Pause, and damage feedback visible but non-overlapping.

**Step 6: Run shell and existing UI tests to verify GREEN**

Run: `node --test web-tests/zombie-zone-shell.test.mjs web-tests/campaign-ui.test.mjs web-tests/screen-flow.test.mjs`

Expected: PASS.

### Task 6: Add a localhost-only deterministic screen-preview harness

**Files:**
- Create: `app/src/main/assets/game/js/preview-harness.js`
- Create: `web-tests/preview-harness.test.mjs`
- Modify: `app/src/main/assets/game/js/main.js`

**Step 1: Write the failing preview-gate tests**

Test that `isPreviewAllowed(location)` returns true only for `127.0.0.1`/`localhost`, that the route enumerates exactly the 13 preview states, and that non-local Android asset URLs cannot install the global preview API.

**Step 2: Run the test to verify RED**

Run: `node --test web-tests/preview-harness.test.mjs`

Expected: FAIL because the module does not exist.

**Step 3: Implement deterministic local state injection**

Expose `window.__zombieZonePreview.show(name)` only on local hosts. Reuse the real shell and UI renderers; provide stable sample mission/reward/error data; never package a reachable production shortcut that mutates save progress.

**Step 4: Run the test to verify GREEN**

Run: `node --test web-tests/preview-harness.test.mjs`

Expected: PASS.

### Task 7: Visually review all final screens

**Files:**
- Modify only the CSS or copy that fails review; no image files are produced or stored.

**Step 1: Run the build in the review browser**

Use the provided Playwright wrapper/CLI against `http://127.0.0.1:4173/` at a fixed 1280×720 landscape viewport. Keep the in-app browser open for live progress.

**Step 2: Walk through all 13 states**

Visit loading, home, avatar, campaign, story, preparation, gameplay, map overlay, pause, settings, victory, defeat, and compatibility in the live build.

**Step 3: Inspect every state**

Check consistency, avatar scale, text contrast, safe-area padding, button visibility, no stale branding, no blank background, and no clipped controls. Patch CSS and re-check any failure.

**Step 4: Inspect browser diagnostics**

Run a fresh console-error filter and network inspection after the walkthrough; verify runtime libraries, GLBs, and generated images load locally with no CDN request.

### Task 8: Run the full web regression suite and lifecycle checks

**Files:**
- Modify only if failures reveal in-scope regressions.

**Step 1: Run the full test suite**

Run: `npm test`

Expected: all tests pass with zero failures.

**Step 2: Exercise repeated lifecycle flows**

In the browser run Home → Campaign → Preparation → Gameplay → Map → Gameplay → Pause → Settings → Resume → Map → replay. Confirm only one joystick, one avatar canvas, one minimap listener set, and one active renderer remain.

**Step 3: Re-prove real movement after the visual overhaul**

Record before/after player coordinates from a real browser joystick drag and save the values in the final verification notes.

### Task 9: Build, install, and verify the Android application

**Files:**
- Modify only if Android build/lint discovers an in-scope defect.

**Step 1: Confirm an authorized device**

Run: `adb devices -l`

Expected: at least one `device` entry; use the currently authorized serial returned by the command.

**Step 2: Run low-memory Android checks**

Run from the repository root with one worker, no daemon, about 1 GB heap, and Kotlin in-process:

`./gradlew.bat testDebugUnitTest lintDebug assembleDebug --no-daemon --max-workers=1 -Dorg.gradle.jvmargs=-Xmx1024m -Dkotlin.compiler.execution.strategy=in-process`

Expected: BUILD SUCCESSFUL and a fresh `app-debug.apk`.

**Step 3: Record fresh artifact evidence**

Run `Get-FileHash -Algorithm SHA256` and record the APK timestamp, size, and hash.

**Step 4: Install and verify package identity**

Run `adb -s <serial> install -r <absolute-apk-path>`, confirm `Success`, verify the package with `pm path`, then launch the main activity.

**Step 5: Verify the device presentation and movement**

Confirm Zombie Zone branding and live selected avatar appear, enter a mission, drag the movement zone on the phone, and verify visible world-position change on the device screen.

### Task 10: Independent review and final evidence

**Files:**
- Modify only for verified review findings.

**Step 1: Request an independent code review**

Use `superpowers:requesting-code-review` over the implementation diff, emphasizing movement input, preview disposal, offline assets, responsive UI, local-only preview gating, and preservation of existing user work.

**Step 2: Address all Critical and Important findings**

Use `superpowers:receiving-code-review` before applying non-trivial feedback. Add or update a regression test for each accepted fix, then rerun targeted and full verification.

**Step 3: Run verification-before-completion**

Re-run `npm test`, the relevant Gradle tasks, browser console/network checks, browser before/after movement proof, APK hash/install/package checks, and confirm all 13 preview states render correctly.

**Step 4: Deliver concise final evidence**

Report the movement coordinates, web test count, Android build result, fresh APK hash/install status, and any limitation that could not be directly observed. Do not stage or commit unrelated dirty-worktree files.
