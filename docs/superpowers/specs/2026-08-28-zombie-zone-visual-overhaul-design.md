# Zombie Zone Visual Overhaul Design

## Goal

Transform the current campaign shell into a coherent, child-friendly 3D zombie adventure named **Zombie Zone**, while fixing movement through the real NippleJS 1.0 event path and preserving the existing maps, avatars, enemies, progression, and offline Android runtime.

## Product identity

Zombie Zone is energetic rather than frightening. Its visual language combines dark navy environments with radioactive green, electric violet, warm orange, and cyan highlights. Rounded panels, chunky display lettering, sticker-like badges, luminous outlines, and playful hazard graphics repeat across every screen.

The title is always rendered as live text for reliable spelling and localization. Generated raster art contains no words, logos, UI, watermarks, or fake buttons.

## Generated artwork

Create three original landscape backgrounds at a 16:9 composition:

1. **Zombie city:** colorful abandoned city at sunset, glowing green mist, distant friendly blocky zombie silhouettes, and clear negative space for the home avatar and title.
2. **Haunted playground map:** playful ruined amusement park and winding streets viewed from a slightly elevated angle, with six recognizable zones and room for the campaign path overlay.
3. **Zombie laboratory:** comic laboratory interior with glowing specimen tanks, pipes, warning lights, and clear central negative space for story and preparation cards.

All images use a polished stylized 3D/cartoon look suitable for children, with no gore, realistic injuries, weapons pointed at the viewer, or embedded text.

## Shared screen system

Every non-gameplay screen uses the same header, resource badges, panel radius, button hierarchy, typography, and spacing scale. Primary buttons are radioactive green with dark labels; secondary buttons are violet outlines. Decorative elements never compete with text or touch targets.

The screens are:

- Loading: Zombie Zone title, animated hazard emblem, clear progress.
- Home: large live 3D selected avatar, Zombie Zone title, chapter progress, stars, crystals, Continue, Avatar, and Settings.
- Avatar: the existing 18 characters with a larger 3D preview of the selected model.
- Campaign: six visually distinct nodes over the playground/city background with clear locked, available, and completed states.
- Story: laboratory or city artwork, speaking character, no more than three short lines, Continue and Skip.
- Preparation: large live 3D avatar, objective, star conditions, vitality/power upgrades, and Start.
- Gameplay HUD: minimap, health, objective, reticle, target health, movement, Fire, Reload, Dodge, crystals, Pause, and damage feedback.
- Pause and Settings: consistent modal panels with camera and sound controls.
- Victory: large 3D avatar celebration, stars, crystals, Next stage, Map.
- Defeat: readable cause, Retry, Preparation, Map.
- Compatibility: friendly actionable error instead of a blank screen.

## Live 3D presentation

Home, avatar selection, preparation, and victory reuse the selected local GLB model through a lightweight `avatar-preview` adapter. One transparent Three.js renderer is reused across screens. It loads the same character catalog entry as gameplay, plays an idle animation on Home/Preparation, rotates gently on Selection, and plays a celebration-compatible animation or energetic idle on Victory.

Static character PNGs remain only as fast thumbnails inside the 18-character grid. They are not used as the principal character presentation.

The preview renderer pauses when hidden and disposes its model, mixer, renderer, geometry references, observers, and listeners when the app shell is disposed.

## Movement correction

The outer movement zone is renamed from the generic `.joystick` class to `.movement-zone`. This avoids colliding with the `.joystick` element created internally by NippleJS and removes the duplicate hand-built ring and knob.

NippleJS 1.0 sends one event object whose normalized vector is stored in `event.data.vector`. The controller consumes that format, retains a safe fallback for missing intermediate data, and feeds camera-relative desired motion to Rapier.

Verification must prove the full path, not only pure math:

1. A NippleJS-compatible event changes the adapter movement state.
2. Rapier returns non-zero resolved movement for a valid direction.
3. The player's measured world position changes in the browser after a real drag.
4. Movement is tested again on the installed Android build.

No success claim is allowed from a unit test alone if browser or device position remains unchanged.

## Background and accessibility behavior

Generated backgrounds are decorative and have dark overlays that guarantee text contrast. CSS supplies gradients if an image fails to load. Touch targets remain at least 48 CSS pixels, landscape safe-area insets are preserved, and essential state is expressed with text or shape as well as color.

Motion respects a reduced-motion media query by disabling decorative drift and reducing transitions. The game remains offline: all generated images and JavaScript dependencies are packaged in Android assets.

## Data and lifecycle

The saved schema, stars, crystals, stage unlocks, upgrades, selected avatar, viewed stories, and settings remain compatible. Renaming the game does not reset local progression.

The campaign shell owns screen changes; the avatar preview follows the visible screen. Game sessions continue to dispose minimap listeners, camera controls, Rapier objects, NippleJS managers, renderers, materials, and event handlers on replay or map return.

## Review deliverables

Review every final landscape screen in the live build: Loading, Home, Avatar, Campaign, Story, Preparation, Gameplay HUD, Map overlay, Pause, Settings, Victory, Defeat, and Compatibility. Where Victory or Defeat is expensive to reach manually, use an explicit development-only route or controlled state injection that is excluded from the packaged production build.

Every reviewed screen must show the same visual system and contain no loading errors, clipped primary actions, or stale Ecos de Eldra wording. No image files are stored in the repository.

## Verification

- Pure and adapter tests for campaign, save, progression, camera, movement, weapon, minimap, and screen structure.
- Browser walkthrough of every reachable screen with fresh console-log filtering and offline network inspection.
- Browser joystick drag with before/after player coordinates.
- Visual review of every screen at the landscape target size.
- Low-memory Android unit test, lint, and debug APK build with one Gradle worker.
- Fresh APK hash, authorized-device installation, package verification, launch, and device movement check.

## Out of scope

No accounts, cloud sync, ads, payments, multiplayer, realistic gore, new weapon catalog, or replacement of the existing 18 avatar/map asset foundation.
