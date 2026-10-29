# Character parity and animation validation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development. Steps use checkbox syntax for tracking.

**Goal:** Improve the native voxel character and its testing page using evidence from 15 comparison workers and 15 online research workers.

**Architecture:** Preserve the restored OpenGL renderer and existing voxel rig. Fix measurable animation defects, add reproducible browser comparisons, and validate the existing Downloads FBX conversion rather than replacing matching geometry.

**Tech Stack:** C++17, Android/Kotlin, Three.js 0.180.0, Node tests, NDK.

**Spec:** User instructions in this conversation: remove cinematic/Cofre/autofire, install and push main, improve test-page/native character parity, use Downloads animations, research improvements without further questions.

## Global Constraints

- Preserve removal of cinematic, Cofre and automatic firing.
- Keep progress and saved character appearance compatible.
- Three auxiliary agents concurrently; 30 requested workers run in waves.
- One native/Gradle compiler at a time on the 4 GB development PC.
- Downloads currently contains five FBX clips, all already integrated; no bow clip found.
- Keep current OpenGL implementation; renderer conversion was explicitly reverted.
- Only publish verified changes; user authorized installation and main push.

## Review Focus

- Interrupted animation fades must retain the displayed pose.
- Nonlooping converted clips must retain their actual final pose.
- Repeated preview snapshots must yield the same pose and camera.
- Reload/single shot must remain manual and preserve hand contacts.
- Degenerate pose inputs must not produce nonfinite transforms.

### Task 1: Complete removal baseline

- [x] Remove three features and their supporting code.
- [x] Assemble both Android ABIs and run JVM tests.
- [x] Install on Wi-Fi device and push main: `0cdffce`.

### Task 2: Compare reference and implementation

- [x] Complete 15 comparison/implementation/review workers, record evidence and actual changes in the audit report.
- [x] Preserve matching torso/head/leg geometry; prioritize demonstrated defects.

### Task 3: Animation continuity

Files: `render/character.cpp`, `render/character.h`, new native animation tests and runner.

- [x] Write and run a failing interrupted-fade regression.
- [x] Preserve the displayed source pose when interrupting a fade, including repeated switches without evaluation.
- [x] Verify normal fades, no-fade playback and invalid channels.

### Task 4: Deterministic test page

Files: `tools/personagem-voxel/index.html`, optional pure state helper, Node tests.

- [x] Add manual single shot and reload with native-equivalent motion.
- [x] Add pause, frame step, time/reload scrub and deterministic snapshot diagnostics.
- [x] Verify fixed-state images and state transitions; inspect page visually. Recheck final small accessibility/timing changes after compilation.

### Task 5: FBX conversion correctness

Files: `tools/anim-converter/convert.mjs`, converter helper/tests, existing animation assets.

- [x] Test endpoint suspicion: looping error refuted; synthetic clip confirms rounded sample misses subframe endpoint.
- [x] Sample true endpoints and fix mutable root-drift baseline; regenerate the five Downloads clips.
- [x] Verify nonloop endpoints and finite normalized samples without changing format/rig conventions.

### Task 6: Research and targeted follow-up

- [x] Complete 15 online research workers, use primary sources, record links and applicability.
- [x] Implement compatible findings with focused tests; reject renderer replacement and speculative geometry changes.
- [x] Independently review each implemented area and whole diff.

### Task 7: Deliver

- [x] Run Node/native/JVM checks and assemble updated APK.
- [x] Install and check process/crash logs. Browser UI inspected; Android screen dozing prevents gameplay screenshot validation.
- [ ] Commit/push verified improvements to main and report exact limitations.

## Execution ledger

- Ruling: user explicitly opted out of questions; proceed using current platform and independent reviews.
- Baseline removal: build successful (24m29s), native ARM64 syntax checks, Node terrain tests, JVM tests passed; install successful. Device screen dozing, so screenshot is black; no runtime crash in checked logs.
- Comparison workers 01–15 and research workers 01–15 complete. Independent per-area and integrated reviews complete.
- Ruling: fix authored idle playback speed locally/remotely; movement speed scaling remains unchanged.
- Root motion regression confirmed mutable endpoint aliasing; fixed with endpoint snapshots and regenerated actual Downloads assets.
- Browser integration passed actual rendered transforms, buttons, repeated state captures and paused pixel stability. Final small accessibility/timing changes await isolated rerun because concurrent browser/Gradle exhausted host memory.
- Isolated browser rerun passed after final accessibility/timing changes, including view buttons while paused.
- Updated APK build successful (both ABIs, 8m9s), 12 Node tests passed, JVM checks up-to-date from 247 passing baseline tests. Native transition regression passed on Android arm64.
- Updated APK installed successfully; process 28578 started, checked crash logs empty. Screen remained dozing; no claim of complete gameplay visual validation.
