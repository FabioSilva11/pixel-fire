# Zombie Zone Lobby and Campaign Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the approved complete lobby, 20-stage campaign path, hero shop, loading-screen stories, and correct Android music lifecycle.

**Architecture:** Keep the current vanilla HTML/CSS/Three.js structure and isolate domain changes in campaign, progression, selection, loading, and music modules. UI controllers consume pure view models so progression and lifecycle behavior stay testable without a browser.

**Tech Stack:** JavaScript ES modules, Three.js GLB previews, HTML/CSS, Android Kotlin WebView, Node test runner, Playwright CLI.

**Spec:** `docs/superpowers/specs/2026-08-29-zombie-zone-lobby-campaign-design.md`

## Global Constraints

- Preserve all maps, combat, controls, collisions, save data, and offline operation.
- Exactly 20 sequential stages map one-to-one to `mapa (1)` through `mapa (20)`.
- Kael is free; locked heroes use the existing diamond wallet.
- Story text appears during 3D loading and never creates an extra navigation screen.
- Backgrounding the Android app must pause all music.

---

### Task 1: Twenty-stage campaign and completion data

**Files:**
- Modify: `app/src/main/assets/game/js/campaign.js`
- Modify: `app/src/main/assets/game/js/campaign-ui.js`
- Test: `web-tests/campaign.test.mjs`
- Test: `web-tests/campaign-ui.test.mjs`
- Test: `web-tests/map-appearance.test.mjs`

**Interfaces:**
- Produces: `CAMPAIGN.stages` with 20 entries and `createChapterView()` entries containing `stars`, `collectibles`, and `status`.

- [ ] Write tests expecting 20 one-to-one map IDs, 60 possible stars, and per-stage collectible totals.
- [ ] Run the focused tests and confirm they fail because only six stages exist.
- [ ] Add the 20 stage definitions and collectible view data.
- [ ] Run focused tests until green.

### Task 2: Hero ownership and diamond purchases

**Files:**
- Modify: `app/src/main/assets/game/js/catalog.js`
- Modify: `app/src/main/assets/game/js/progression.js`
- Modify: `app/src/main/assets/game/js/save-store.js`
- Modify: `app/src/main/assets/game/js/selection.js`
- Test: `web-tests/progression.test.mjs`
- Test: `web-tests/save-store.test.mjs`
- Test: `web-tests/selection.test.mjs`

**Interfaces:**
- Produces: `unlockedAvatarIds: string[]`, `purchaseAvatar(progress, id)`, and selection cards with `owned`, `price`, and `selected`.

- [ ] Write failing ownership, migration, insufficient-funds, and one-time purchase tests.
- [ ] Run focused tests and verify expected failures.
- [ ] Implement prices, save normalization, and purchase rules.
- [ ] Update selection view models and run focused tests until green.

### Task 3: Lobby and screen flow

**Files:**
- Modify: `app/src/main/assets/game/index.html`
- Modify: `app/src/main/assets/game/css/game.css`
- Modify: `app/src/main/assets/game/js/screen-flow.js`
- Modify: `app/src/main/assets/game/js/campaign-ui.js`
- Modify: `app/src/main/assets/game/js/main.js`
- Test: `web-tests/screen-flow.test.mjs`
- Test: `web-tests/zombie-zone-shell.test.mjs`
- Test: `web-tests/tps-integration.test.mjs`

**Interfaces:**
- Lobby actions: `data-home-continue`, `data-home-avatar-button`, `data-home-upgrades`, and `data-open-settings`.
- Hero actions: `data-avatar-back`, `data-play-button`.

- [ ] Write failing shell and flow tests for a lobby-first boot, Back, upgrades overlay, and absence of the redundant preparation route.
- [ ] Run focused tests and confirm the old flow fails them.
- [ ] Replace the home markup with the complete lobby and move upgrades into a lobby overlay.
- [ ] Wire hero purchase/equip and direct campaign navigation.
- [ ] Run focused tests until green.

### Task 4: Connected 20-node campaign map

**Files:**
- Modify: `app/src/main/assets/game/index.html`
- Modify: `app/src/main/assets/game/css/game.css`
- Modify: `app/src/main/assets/game/js/campaign-ui.js`
- Test: `web-tests/campaign-ui.test.mjs`
- Test: `web-tests/zombie-zone-shell.test.mjs`

**Interfaces:**
- Produces alternating `.stage-row` groups and `.stage-connector` elements placed between card edges.

- [ ] Write failing tests for 20 nodes, nineteen connectors, `x/3` stars, and `x/3` collectibles.
- [ ] Run focused tests and verify failures against the six-card bar.
- [ ] Render a scrollable serpentine path with edge-to-edge connector elements and an in-map mission detail panel.
- [ ] Run focused tests until green.

### Task 5: Story inside 3D loading

**Files:**
- Modify: `app/src/main/assets/game/index.html`
- Modify: `app/src/main/assets/game/js/loading.js`
- Modify: `app/src/main/assets/game/js/campaign-ui.js`
- Modify: `app/src/main/assets/game/js/main.js`
- Test: `web-tests/loading.test.mjs`
- Test: `web-tests/campaign-ui.test.mjs`

**Interfaces:**
- Produces: `loading.setStory(story)` and direct `onStartMission(stage)` from campaign details.

- [ ] Write failing tests for loading story normalization and direct mission start.
- [ ] Run focused tests and confirm failure.
- [ ] Add story speaker/lines to the progress screen and remove story/preparation navigation from stage selection.
- [ ] Run focused tests until green.

### Task 6: WebView music lifecycle

**Files:**
- Modify: `app/src/main/assets/game/js/music-controller.js`
- Modify: `app/src/main/assets/game/js/main.js`
- Modify: `app/src/main/java/com/fabiosilva/ecosdeeldra/MainActivity.kt`
- Test: `web-tests/music-controller.test.mjs`
- Test: `app/src/test/java/com/fabiosilva/ecosdeeldra/MainActivityTest.kt`

**Interfaces:**
- Produces: `music.setAppActive(boolean)` and native `zombie-zone-app-pause` / `zombie-zone-app-resume` events.

- [ ] Write failing tests showing inactive music pauses and foreground resume restores only the current track.
- [ ] Run focused tests and verify failure.
- [ ] Add JS lifecycle listeners and Kotlin event dispatch on `onPause`/`onResume`.
- [ ] Run JavaScript and Android unit tests until green.

### Task 7: Visual and delivery verification

**Files:**
- Modify: runtime cache query versions in `index.html`, `bootstrap.js`, and `main.js`.

**Interfaces:**
- Produces: verified lobby, hero shop, and campaign flow plus a fresh debug APK.

- [ ] Run `node --test web-tests/*.test.mjs` and require zero failures.
- [ ] Use Playwright at 1280x720 to verify lobby, 20-stage scrolling map, locked hero purchase states, Back, and loading story.
- [ ] Run `$env:GRADLE_OPTS='-Xmx1024m -Dorg.gradle.workers.max=1 -Dkotlin.compiler.execution.strategy=in-process'; .\gradlew.bat assembleDebug --no-daemon --max-workers=1`.
- [ ] Record the APK timestamp, size, and SHA-256 without installing or publishing unless the user asks.
