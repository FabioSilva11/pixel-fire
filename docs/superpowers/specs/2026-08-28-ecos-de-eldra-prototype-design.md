# Ecos de Eldra - Android/Three.js Prototype Design

## Purpose

Build a small Android prototype that proves the available Kenney characters and modular dungeon assets can support a third-person exploration game with light combat. The prototype is a technical and gameplay foundation, not a finished game.

## Player experience

1. Launch the app in landscape orientation.
2. Wait for the online Three.js runtime to load.
3. Pick one of the 18 available blocky characters from a visual selector.
4. Enter a compact dungeon assembled from the available modular GLB models.
5. Move with a virtual joystick, drag to rotate the camera, and use an attack button.
6. Defeat two lightweight enemies.
7. Open or pass through an animated gate and reach the exit marker.

The temporary product name is **Ecos de Eldra**. The application ID is `com.fabiosilva.ecosdeeldra`.

## Scope

### Included

- Android app written in Kotlin with one full-screen WebView activity.
- Local HTML, CSS, JavaScript, character GLBs, dungeon GLBs, and textures.
- Three.js `0.185.0` and its `GLTFLoader` loaded from the same jsDelivr CDN version.
- A network/loading screen with retry behavior when the CDN is unavailable.
- Character selection for all 18 Kenney characters.
- Third-person camera and touch controls designed for landscape phones.
- Idle, walk, melee attack, and death animations, with safe fallbacks if a clip is missing.
- Simple wall boundaries, player/enemy hit radii, health, enemy pursuit, melee range, and damage cooldowns.
- Two enemies using suitable blocky character variants.
- Animated dungeon gate and a clear prototype completion state.
- Local license notices for the CC0 Kenney assets and MIT Three.js runtime.

### Excluded

- Accounts, cloud saves, ads, billing, analytics, multiplayer, procedural worlds, inventory, quests, audio, and production monetization.
- Large physics engines or external gameplay services.
- A fully offline fallback for Three.js. The first load requires internet as requested.

## Architecture

### Android shell

The Android module owns lifecycle, orientation, WebView setup, error recovery, and back-button behavior. `WebViewAssetLoader` exposes packaged files through `https://appassets.androidplatform.net/assets/` instead of a `file://` origin. JavaScript is enabled only for the local game page. File and content access are disabled, mixed content is blocked, and external top-level navigation is rejected.

The manifest declares internet access and locks the prototype activity to landscape. The app targets the locally installed Android SDK while keeping a broadly compatible minimum SDK.

### Web runtime

`index.html` defines an import map for:

- `https://cdn.jsdelivr.net/npm/three@0.185.0/build/three.module.js`
- `https://cdn.jsdelivr.net/npm/three@0.185.0/examples/jsm/`

All Three.js modules come from that single pinned CDN/version, matching the official Three.js installation guidance. The game shell is split into focused JavaScript modules for asset loading, animation, input, world construction, combat, and the main loop.

### Assets

Only the GLB distribution is copied into the Android app; FBX, OBJ, marketing previews, URL shortcuts, and source-package duplicates are excluded. All 18 character GLBs and their texture files are included for selection. The prototype includes the subset of dungeon pieces necessary for one coherent level plus their shared colormap texture. Original Kenney license files are preserved under notices.

### Game loop

The renderer uses WebGL with a capped device pixel ratio to protect performance on mobile devices. A fixed-step-friendly update loop applies input, movement, simple collision constraints, camera smoothing, enemy AI, combat cooldowns, animation transitions, and gate state before rendering.

Player attacks use the supplied `attack-melee-right` clip and affect living enemies within a forward cone and short radius. Enemies alternate between idle, pursuit, attack, hit cooldown, and death. When both enemies die, the gate plays its `open` animation and the exit becomes reachable.

## Interface

The visual direction is stylized mysterious fantasy: deep violet, warm amber, soft cyan, and dark slate. The selection screen uses large character cards and a prominent play button. During play, the HUD contains only a health bar, objective text, virtual joystick, camera drag zone, and attack button. Controls use large translucent surfaces with safe-area padding.

Loading failures show a clear Portuguese message and a retry button. Loading progress names the current stage so a blank WebView is never mistaken for a crash.

## Error handling

- CDN failure: show the connection panel and allow retry.
- Individual optional dungeon asset failure: use a generated primitive fallback and continue.
- Required player model failure: return to selection with an actionable error.
- Missing animation: fall back to idle or a short procedural action without stopping the loop.
- WebGL unavailable: show a compatibility message instead of a blank screen.

## Verification

- Static JavaScript syntax checks where supported by the local runtime.
- Android unit checks for URL/navigation policy where practical.
- Low-memory Gradle build using one worker and no daemon.
- Install or launch in an available emulator/device when accessible.
- Browser/WebView visual run verifying character selection, level load, movement HUD, enemies, and gate.
- Final verification performed on the running game, not from a design mockup.

## Success criteria

The prototype is complete when a debug APK builds, the game starts with internet access, a player can select any character, enter the dungeon, move and rotate the camera, perform melee attacks, defeat two enemies, and see the gate open. The real running gameplay must be exercised and reported with the result.

## Phase and map expansion

Each phase is a logical square of exactly 1000 by 1000 metres. A deterministic room-and-corridor graph connects the constructions; rooms are placed from non-overlapping rectangles and corridors are explicit traversable links. Visual GLB instances stream around the player, while the complete graph remains available for navigation and the minimap.

The top-left HUD contains a square minimap. Walkable layout cells are dark until revealed, then become light; the player is cyan and nearby zombies are visible only after their cells are explored. Tapping the minimap opens a large map overlay, and a close control returns to gameplay.

Collision uses the same room/corridor occupancy data for player and zombie movement. A circular actor radius is tested against solid borders and blocked cells before every movement step, so neither actor can walk through a wall or leave the phase. Each phase has a configured zombie quota; this test phase uses six zombies distributed across separated rooms, and the exit objective is unavailable until the quota is defeated.

The HUD avatar marker uses a heart icon instead of a star.
