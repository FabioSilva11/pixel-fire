# Ecos de Eldra - Campaign Screens and Ready-Made Controls Design

## Objective

Turn the current technical prototype into an engaging first campaign chapter without replacing its maps, 18 avatars, animation system, world rendering, or core combat foundation. Add the screens that connect avatar selection to repeatable progression, replace the hand-written camera and player-control layers with established libraries, and keep the Android game usable offline after installation.

## Product decisions

- The experience combines story and progression.
- Players advance through a visual chapter map rather than entering consecutive stages automatically.
- Rewards are stars and crystals. Crystals improve vitality and attack power.
- Story is delivered through short cards before and after stages.
- Combat uses a third-person shooter camera, assisted aiming, and manual firing.
- Progress is local to the device. Accounts and cloud synchronization are excluded.
- The existing mysterious-fantasy identity and all existing game assets remain in use.

## Player flow

The primary flow is:

`Loading -> Home -> Chapter map -> Story -> Mission preparation -> Gameplay -> Result -> Updated chapter map`

Avatar selection is mandatory on the first run and remains accessible from Home. Pause and the large in-stage map are overlays on Gameplay. Defeat returns to retry, preparation, or the chapter map without granting incomplete rewards.

## Screens

### Loading

Keep the current portal presentation but make its progress reflect actual initialization of the local runtime, control libraries, campaign data, save data, and required assets. A failure names the failed stage and offers Retry. The game must not show a blank WebView.

### Home

Home presents the selected avatar in an idle 3D scene in front of the ruins. The single dominant action is **Continue adventure**. It also shows current chapter progress, total stars, crystal balance, and small controls for avatar selection and settings.

On first run, Continue routes to avatar selection and then the first story card. On later runs, it routes to the chapter map with the next playable stage highlighted.

### Avatar selection

Preserve the existing selection of 18 characters and its visual identity. Selecting a character updates the local save. The screen does not assign unique statistics or abilities to avatars in this chapter.

### Chapter map

The first release contains one chapter with six stages. A luminous path crosses a stylized ruin map:

- Completed stages are illuminated and show their best star count.
- The next available stage pulses gently.
- Locked stages appear through fog and show the preceding-stage requirement.
- One star on the preceding stage unlocks the next stage.
- Three stars are optional and never block the main story.

Selecting an unlocked stage opens its pre-mission story card unless the player has chosen to skip previously viewed story, then opens mission preparation.

### Story cards

A story card contains one illustration or staged world image, a title, the speaking character, and no more than three short dialogue blocks. It provides Continue and Skip. Pre-mission cards lead to preparation; post-mission cards lead to the result screen. Viewed state is saved so repeated stages can omit the card by default.

### Mission preparation

Preparation shows the stage objective, target time, potential star conditions, first-clear crystal reward, avatar, and current vitality/power levels. The player can purchase one or more affordable upgrades, then choose **Start mission**.

The first chapter has only two upgrades, each capped at level five:

- **Vitality:** increases maximum player health.
- **Power:** increases damage from the ranged attack.

There is no equipment inventory, weapon rarity, or loadout system in this iteration. The current attack foundation is adapted to one fixed ranged attack with manual firing; a weapon catalog is outside this scope.

### Gameplay HUD

The landscape HUD contains:

- Top-left: compact minimap, avatar, and player health.
- Center: persistent reticle, hit confirmation, and the current target's health when applicable.
- Top-right: concise objective and Pause.
- Bottom-left: NippleJS movement joystick.
- Bottom-right: primary Fire, Reload, and Dodge buttons.
- Temporary feedback: incoming-damage direction, low-health vignette, collected crystals, and objective updates.

The HUD preserves safe-area padding and large touch targets. It does not add jump or crouch controls in this chapter.

### Pause

Pause stops gameplay simulation and exposes Continue, camera sensitivity, sound, Restart stage, and Return to map. Restart and Return to map require a lightweight confirmation only when the current run has unsaved stage progress.

### Victory

Victory reveals earned stars one at a time, counts newly earned crystals, presents the short post-mission story card, and reveals the next map node when applicable. The primary action is **Next stage**; Map is secondary.

### Defeat

Defeat explains the immediate cause, highlights **Try again**, and offers Preparation and Map. It may recommend an affordable vitality or power upgrade but never blocks retry behind a purchase.

## Progression rules

Each stage awards up to three stars:

1. Complete the stage.
2. Finish with at least 50 percent health.
3. Finish within the stage's configured target time.

Crystals come from first completion, newly earned stars, and one-time stage pickups. Replaying a stage cannot repeatedly grant first-clear or already-collected crystal rewards. A better result grants only the difference between the previous and new reward state.

Upgrade costs are explicit campaign data and increase by level. Both upgrades remain optional for completing the first chapter.

## Ready-made control stack

### Camera Controls

Use `camera-controls` as the camera implementation. Configure it for one-finger rotation in the right-side look zone, fixed third-person distance bounds, pitch bounds appropriate for dungeon ceilings, smooth target following, and collision against nearby structural meshes. The adapter updates its target from the player's current position and exposes the camera forward direction to movement and aiming.

### Rapier

Use Rapier's kinematic character controller for player motion and collision. It owns hit-and-slide resolution, slope limits, autostep, snap-to-ground, doors, and stairs. The procedural layout remains the semantic source for spawn, exit, minimap, and enemy navigation, while Rapier colliders become the authority for physical player movement.

### NippleJS

Use NippleJS for the movement joystick. Its normalized output feeds desired motion relative to the camera heading. Touch ownership must allow simultaneous movement, camera dragging, and firing.

### Assisted aiming

Assisted aiming is a focused adapter, not a replacement movement library. It considers only living enemies that are within range, inside a narrow screen-space cone around the reticle, and visible by line of sight. It applies limited camera/reticle magnetism and target retention. It never fires automatically, selects enemies behind the player, or tracks through walls.

## Internal architecture

- **Screen flow:** owns the current screen or overlay and legal transitions.
- **Campaign catalog:** declaratively defines six stages, story cards, objectives, timing, rewards, and unlocks.
- **Save store:** validates, migrates, reads, and atomically writes local progress.
- **Progression service:** calculates unlocks, stars, reward deltas, and upgrade purchases.
- **Camera adapter:** isolates `camera-controls` configuration from game rules.
- **Player controller adapter:** connects NippleJS, Rapier, avatar animation, and camera-relative heading.
- **Aim assist:** selects and retains a valid target within configured limits.
- **Game session:** emits health, objective, enemy, victory, and defeat events without directly navigating screens.
- **Screen views:** render the approved UI and call the flow/progression services through explicit actions.

The main data path is:

`Chapter map -> Preparation -> Game session -> Result -> Progression service -> Save store -> Updated chapter map`

Existing world, map, catalog, avatar, animation, combat, and minimap modules are preserved unless a narrow adapter is required. The existing hand-written camera rig, player joystick implementation, and physical player collision path are retired only after the library-backed equivalents pass the same gameplay scenarios.

## Local packaging

Pin known-compatible versions of Three.js, `camera-controls`, Rapier, and NippleJS. Package their production files, Rapier WebAssembly, and license notices inside Android assets. No game runtime library is fetched from a CDN. This makes the installed campaign start without internet and prevents upstream releases from changing behavior unexpectedly.

## Local save

The versioned save contains:

- selected avatar;
- unlocked and completed stages;
- best stars, best time, and best ending health per stage;
- crystal balance and one-time reward claims;
- vitality and power levels;
- viewed story cards;
- camera sensitivity, sound settings, and story-skip preference.

Writes use a validated replacement record so an interrupted write cannot partially update progression. Migration preserves valid fields. If one field is invalid, that field returns to its default without erasing unrelated progress. Closing the app during a stage retains the previous completed save and grants no partial result.

## Visual direction

Retain deep navy and slate surfaces, violet magic/progression, amber rewards and primary actions, and cyan navigation/aiming. Panels resemble dark stone and metal with moderate translucency. The chapter path, selected stage, rewards, and primary action provide the brightest accents.

Transitions last approximately 200 to 350 milliseconds. Use restrained glows and limited particles; do not add heavyweight background video. Every screen has one obvious primary action. Text and controls remain legible on landscape phones and respect display cutouts.

## Failure handling

- Missing essential library or asset: name the loading stage, allow Retry, and return safely to Home or Map.
- Invalid stage configuration: disable that stage and preserve the save.
- Incompatible save: migrate known fields and default only invalid fields.
- Unavailable WebGL or WebAssembly: show a compatibility screen instead of a blank viewport.
- Avatar load failure: return to selection without modifying progression.
- Camera or physics initialization failure: stop stage entry with an actionable error; do not silently restore the retired hand-written controllers.

## Verification

Automated checks cover:

- legal screen transitions and first-run routing;
- six-stage unlocking, three star conditions, reward deltas, and upgrade costs;
- save creation, atomic replacement, migration, and partial recovery;
- assisted-aim range, cone, line-of-sight, and retention limits;
- Rapier collision, sliding, doors, slopes, stairs, and snap-to-ground;
- camera pitch, distance, following, and wall collision;
- simultaneous joystick, look, and fire touch ownership.

Browser verification covers the complete Home-to-Result loop, all overlays, responsive landscape layout, and console errors. Android verification uses the existing low-memory one-worker build, followed by installation on an authorized device and hands-on checks for touch, frame pacing, app resume, and offline startup.

## Success criteria

The change is complete when a player can start the installed game without internet, select an avatar, continue from Home, advance through the chapter map, view or skip story, buy upgrades, play with library-backed third-person controls and assisted manual firing, receive correct stars and crystals, and resume saved progress after closing the app.

## Non-goals

- Accounts, cloud save, multiplayer, rankings, ads, billing, or daily rewards.
- Equipment inventory, multiple weapons, rarity, crafting, or avatar-specific abilities.
- Cinematic video, voice acting, or long dialogue trees.
- Jumping, crouching, swimming, vehicles, or advanced traversal.
- A rewrite of the existing maps, avatars, world renderer, or procedural generation.

## Library references

- Camera Controls: <https://github.com/yomotsu/camera-controls>
- Rapier character controller: <https://rapier.rs/docs/user_guides/javascript/character_controller/>
- NippleJS: <https://github.com/yoannmoinet/nipplejs>
- Three.js Rapier character-controller example: <https://threejs.org/examples/physics_rapier_character_controller.html>
