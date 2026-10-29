# Lobby X-Ray Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Apply the lobby visual corrections approved by the user after inspecting Free Fire and Zombie Zone captures.

**Architecture:** Keep existing native Kotlin lobby actions and existing block character. Give each UI region explicit final dimensions instead of the global 0.6 transforms; change only the home branch of the 3D preview camera.

**Tech Stack:** Kotlin Views, C++17/OpenGL ES, Android Gradle/JDK 17.

**Spec:** `screenshots/xray-lobby/xray-report.md`, diagnosis and proposed visual priorities, explicitly approved by user: “Alinhado: seguir com as correções visuais”.

## Global Constraints

- Preserve all existing lobby destinations, progress, rewards and gameplay.
- Do not add Free Fire social, paid, shop, chat or pass features.
- Preserve unrelated working-tree changes; no reset, stash, whole-repo commit or new checkout that loses ongoing changes.
- Only one Gradle/native build at a time, with existing 512 MB heap / serial configuration.
- No new unit tests that merely assert layout constants; validate this reversible visual edit by compilation, existing tests, real device screenshots and taps.
- Originals stay unchanged; every screenshot revision uses a new directory.

## Review Focus

- All five left-menu actions remain visible with readable text; chest countdown/video subtitle fit.
- Existing three bottom actions retain their destinations; profile long names are constrained rather than extending over currencies.
- Phase/zone text fits the selector, and JOGAR remains reachable at the bottom right.
- Editor and inventory camera branches remain unchanged.
- Home character remains fully visible; other landscape widths do not overlap the main action regions.

## Task 1: Correct the lobby composition

**Files:** `app/src/main/java/com/fabiosilva/ecosdeeldra/ui/HomeScreen.kt`; home-camera branch of `app/src/main/cpp/game/game.cpp`.

**Interfaces:** Existing MainActivity callbacks and progress fields are unchanged. Header.resources is shared by other screens; compact resources should be local to HomeScreen.

- [x] Record baseline existing test result and preserve copies/diffs of touched source files.
- [x] Remove HomeScreen's shrinkLobbyControls helper and its calls. Use final per-region sizes.
- [x] Compact profile: avatar 26 dp, vertical padding 2 dp, name 11–12 sp, subtitle 7–8 sp, single-line/ellipsized name; compact lobby currency row with its own chip sizes.
- [x] Use approximately 40 dp outer horizontal inset, reduced on narrow screens if needed. Left menu starts near 29% of usable height, final rows 29 dp, gaps 5 dp, width 120 dp, icons 16 dp, titles 11–12 sp. Keep all current actions and badges.
- [x] Bottom navigation: final height 31 dp, about 330 dp total width for three equally spaced existing actions, icons 18 dp, labels 11–12 sp, bottom inset 10 dp. Limit width if needed on compact screens.
- [x] Start dock: selector approximately 202 dp wide, button 160 dp aligned to its right; button 41 dp tall with 24 sp title; selector about 40 dp tall, gap 4 dp, bottom inset 6 dp. Keep text readable with truncation for long phase names.
- [x] Progress: compact panel approximately 100 dp wide, values 10–11 sp, reduced labels/padding, retaining all three rows. Keep wordmark and gear consistent with compact header.
- [x] Home camera only: use initial target offset `(-0.1,0.92,0)`, eye Z-distance 2.7, horizontal oscillation amplitude 0.07, vertical oscillation 0.02, eye Y-offset 0.3; preserve FOV 0.85 and editor/inventory branches.
- [x] Review task diff for spec compliance and code quality before device validation.

## Task 2: Build and validate the visual result

**Files:** generated APK; `screenshots/xray-lobby/04-implementacao/r01/`; update `screenshots/xray-lobby/xray-report.md`.

- [x] Run existing JVM suite and build arm64 debug sequentially: `./gradlew.bat :app:testDebugUnitTest :app:assembleDebug -Pandroid.injected.build.abi=arm64-v8a --no-daemon --max-workers=1`. Distinguish baseline failures from new failures.
- [ ] Install with replacement preserving app data on the current authorized device, then open home.
- [ ] Capture clean home, compare with approved reference and previous home; refine clipping/overlap locally if needed.
- [ ] Smoke-test representative navigation (Cofre, Armas, Fases, Ajustes, Missões) without claiming rewards, watching ads, purchases or modifying save data.
- [ ] Save native captures, screenshot comparison and verification record; request the required X-Ray visual review of the implementation.

## Rulings and progress

- Ruling: reuse the current checkout — the ongoing uncommitted terrain/HUD/weapon changes are necessary to build the current app; a fresh HEAD checkout would omit them. Only source edits in this plan are authorized here.
- Ruling: the approved X-Ray report and visual priorities serve as the design; implementation dimensions within the documented ranges are routine choices already delegated by the user.
- Ruling: do not create unit tests of static UI values; compilation, existing regression tests and on-device rendered review are the meaningful checks for this visual change.

- Task 1: source complete, scoped review clean (spec compliance and code quality); no commits created in shared dirty checkout.
- Baseline: 252 JVM tests passed across 30 suites, zero failures/errors.
- Build attempt 1: failed before source compilation because PowerShell split the unquoted ABI property; retried with quoted property.

- Task 2: build passed in 4m34s; 252 tests passed, zero failures/errors. Installation/screenshots pending: phone became offline and refused the current ADB endpoint. User asked to reactivate wireless debugging.
