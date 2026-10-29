# Procedural Modular Dungeon Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a two-level seeded 50×50 procedural dungeon using all 24 structural GLBs, with correct sockets, collision, camera occlusion, minimap, stair elevation, and character facing.

**Architecture:** Keep generation and navigation independent from Three.js in focused ES modules, then let `world.js` translate generated placements into cloned GLB scenes. `main.js` consumes world navigation/camera APIs without knowing placement details. The generator is deterministic, retries bounded seeds, and falls back to a known-good seed.

**Tech Stack:** JavaScript ES modules, Three.js 0.185.0, GLTFLoader, Node built-in test runner, Android WebView.

**Spec:** `docs/superpowers/specs/2026-08-28-procedural-modular-dungeon-design.md`

## Global Constraints

- Grid is exactly 50×50×2.
- Native cell size is exactly 4 world units and level height is 4.05.
- All 24 structural definitions appear at least once in every successful generation.
- Models use scale 1.0 and rotations in 90-degree increments only.
- Narrow and wide sockets connect only through matching sockets or `corridor-transition`.
- Existing unrelated worktree changes must remain untouched.
- Browser tests precede the low-memory Android command `./gradlew.bat :app:testDebugUnitTest :app:lintDebug :app:assembleDebug --no-daemon --max-workers=1`.

---

### Task 1: Structural catalog and packaged assets

**Files:**
- Create: `app/src/main/assets/game/js/dungeon-catalog.js`
- Modify: `app/src/main/assets/game/js/catalog.js`
- Copy: 17 missing structural GLBs into `app/src/main/assets/game/models/dungeon/`
- Test: `web-tests/dungeon-catalog.test.mjs`

**Interfaces:**
- Produces: `DUNGEON_PIECES`, `pieceById(id)`, `rotatePiece(piece, turns)`, and `STRUCTURAL_MODEL_URLS`.
- Socket shape: `{ side: 'N'|'S'|'E'|'W', width: 'narrow'|'wide', levelDelta: number, state: 'open'|'door'|'blocked' }`.

- [ ] Write a failing catalog test asserting 24 unique ids, required groups, correct special sockets, and quarter-turn rotation.
- [ ] Run `node --test web-tests/dungeon-catalog.test.mjs` and confirm the missing module failure.
- [ ] Implement immutable definitions for 11 corridors, 4 gates, 7 rooms, and 2 stairs with exact footprints and native sockets.
- [ ] Copy only the 17 missing GLBs from `kenney_modular-dungeon-kit_1.0/Models/GLB format/` and point catalog URLs at packaged assets.
- [ ] Run the catalog test and existing catalog test; expect all assertions to pass.
- [ ] Commit only catalog, test, and copied GLBs with `feat: package complete modular dungeon catalog`.

### Task 2: Seeded generator and validation

**Files:**
- Create: `app/src/main/assets/game/js/dungeon-generator.js`
- Replace: `app/src/main/assets/game/js/phase-layout.js`
- Replace: `web-tests/phase-layout.test.mjs`
- Test: `web-tests/dungeon-generator.test.mjs`

**Interfaces:**
- Consumes: `DUNGEON_PIECES`, `rotatePiece`.
- Produces: `generateDungeon(seed)`, `validateDungeon(layout)`, `placementBounds(placement)`, `socketWorldPosition(placement, socket)`, `PHASE_LAYOUT`.
- Layout shape: `{ seed, width: 50, height: 50, cellSize: 4, levelHeight: 4.05, placements, connections, start, exit, zombieSpawns }`.

- [ ] Write failing deterministic-seed, all-24-present, bounds, overlap, socket compatibility, two-level, and reachability tests.
- [ ] Run the generator tests and confirm failures before implementation.
- [ ] Implement a small integer PRNG, rotation transforms, footprint reservation, compatible socket matching, and graph construction.
- [ ] Implement mandatory placement with a connected backbone, both stairs, gate constraints, branch growth, bounded retries, and fallback seed.
- [ ] Replace rectangle helpers with generated placement/navigation helpers while preserving exported `PHASE_LAYOUT`, `canOccupy`, and `layoutIsConnected` compatibility.
- [ ] Run generator and phase-layout tests across at least 25 seeds; expect zero invalid layouts.
- [ ] Commit generator, layout adapter, and tests with `feat: generate connected two-level dungeons`.

### Task 3: World rendering, doors, collision, and elevation

**Files:**
- Modify: `app/src/main/assets/game/js/world.js`
- Test: `web-tests/world-navigation.test.mjs`

**Interfaces:**
- Consumes: generated placements and structural URLs.
- Produces from `createWorld`: `floorHeightAt(position)`, `canOccupy(position, radius)`, `resolveCameraPosition(target, desired)`, `openGate()`, and `layout`.

- [ ] Write failing pure navigation tests for ordinary floors, closed sides, static bars, animated door state, stair interpolation, and two-level separation.
- [ ] Extract pure placement navigation calculations that Node tests can import without Three.js.
- [ ] Replace stretched region placement with cached unique GLB loads, shared clones, rotation, grid translation, and level translation.
- [ ] Maintain nearby occluder roots for camera raycasting and door instances for runtime blocking.
- [ ] Position the final portal after the generated final door and derive spawn positions from the generated graph.
- [ ] Run navigation and generator tests; expect all to pass.
- [ ] Commit world and navigation changes with `feat: render and navigate generated dungeon`.

### Task 4: Character facing and interior camera

**Files:**
- Create: `app/src/main/assets/game/js/movement-heading.js`
- Create: `app/src/main/assets/game/js/camera-rig.js`
- Modify: `app/src/main/assets/game/js/main.js`
- Test: `web-tests/movement-heading.test.mjs`
- Test: `web-tests/camera-rig.test.mjs`

**Interfaces:**
- Produces: `headingFromDirection(x, z)`, `cameraPitch(value)`, `desiredCameraPosition(target, yaw, pitch, distance)`, `cameraDistanceForHit(hitDistance)`.

- [ ] Write failing tests for four cardinal and four diagonal headings plus camera pitch/distance bounds.
- [ ] Implement one vector-to-heading formula and one calibrated native-model forward offset.
- [ ] Implement camera constants: desired distance 5.5, pitch range below wall height, minimum collision distance, and smoothing inputs.
- [ ] Update the frame loop to set player Y from `world.floorHeightAt`, set facing from actual accepted displacement, and ask the world to resolve camera occlusion.
- [ ] Keep camera yaw independent from avatar facing and remove the inconsistent `atan2(...,-z) + π` combination.
- [ ] Run heading, camera, input, combat, and animation tests; expect all to pass.
- [ ] Commit movement and camera changes with `fix: keep avatar and camera aligned inside dungeon`.

### Task 5: Procedural two-level minimap

**Files:**
- Modify: `app/src/main/assets/game/js/minimap.js`
- Replace: `web-tests/minimap.test.mjs`

**Interfaces:**
- Consumes: placement footprints, placement levels, transformed sockets, player Y, and enemy positions.
- Produces: 50×50 exploration state and generated-layout drawing.

- [ ] Write failing tests for 50×50 coordinate conversion, level-aware exploration, placement visibility, and stair markers.
- [ ] Replace fixed 1000-metre math with `layout.width`, `layout.height`, and `layout.cellSize`.
- [ ] Draw generated footprints with distinct room, narrow/wide corridor, gate, and stair styles; emphasize current level.
- [ ] Update `main.js` to create exploration from layout dimensions and pass current level.
- [ ] Run minimap and full Node test suite; expect all tests to pass.
- [ ] Commit minimap changes with `feat: draw generated two-level minimap`.

### Task 6: Browser and Android integration verification

**Files:**
- Modify if required by verified failures: `README.md`, loading copy, and focused runtime files only.

**Interfaces:**
- Validates the complete runtime; produces no new API.

- [ ] Run `node --test web-tests/*.test.mjs`; require zero failures.
- [ ] Serve the local game, generate a dungeon, and inspect console/network logs for model or runtime errors.
- [ ] Verify all 24 ids exist in the active layout, both levels are present, doors animate, stairs change Y, and the exit is reachable.
- [ ] Verify movement in eight directions, camera wall pull-in, and no outside/top-down gameplay view.
- [ ] Verify minimap exploration and level switching on the running build.
- [ ] Run `./gradlew.bat :app:testDebugUnitTest :app:lintDebug :app:assembleDebug --no-daemon --max-workers=1` and inspect the APK artifact.
- [ ] Update README claims only with evidence obtained in this task.
- [ ] Commit verified integration changes with `feat: deliver procedural modular dungeon`.
