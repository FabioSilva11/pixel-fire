# Zombie Zone Lobby and Campaign Redesign

## Goal

Transform the current home screen into a complete mobile lobby, expose all 20 bundled maps as sequential missions, make hero ownership use the diamond economy, remove redundant pre-mission screens, and pause music whenever the Android app is backgrounded.

## Approved flow

The main route is `Lobby -> Campaign map -> Mission loading -> Gameplay`. Selecting a mission opens its details inside the campaign screen; pressing play starts the 3D load immediately. The mission's short story is displayed inside the existing loading/progress screen while the environment loads, so narrative never adds another navigation step.

The lobby is the persistent hub. It shows the animated selected hero, wallet, total stars, campaign completion, current mission, and direct actions for Play, Heroes, Upgrades, and Settings. The old top banner “A cidade precisa de você!” and the separate preparation screen are removed from the normal route. Vitality and power upgrades move into a lobby-accessible panel.

## Campaign and rewards

- Register exactly 20 stages, one for each `mapa (1).json` through `mapa (20).json`.
- Unlock stages sequentially. Finishing stage N unlocks stage N+1.
- Each stage can award three stars and contains three collectible crystals.
- Each mission card displays earned stars as `x/3` and collected pickups as `x/3`.
- The campaign header displays total stars as `x/60`; the diamond wallet remains a spendable balance and is not confused with pickup completion.
- The campaign path is scrollable and uses real connector segments between neighboring card edges. Connectors cannot pass behind card content.

## Heroes

- Kael (`a`) is unlocked by default.
- The other 17 heroes are visible but locked and display a diamond price.
- Prices increase by tier: 150, 250, 400, and 600 diamonds.
- Buying a hero is permanent, deducts diamonds once, and selects the hero.
- A locked hero cannot be equipped without purchase.
- Existing saves are normalized safely: Kael is always owned; the currently selected hero is preserved as owned to avoid invalidating a user's active save.
- The hero screen has a working Back button and the primary action reads `USAR HERÓI` or `DESBLOQUEAR`, never `ENTRAR NA ZONA`.

## Lobby presentation

Use the existing city background and 3D GLB preview. Make the hero noticeably alive with an action-oriented idle/celebration clip when available, a gentle presentation rotation, and stronger aura motion. Keep the layout readable at 1280x720 and short landscape phone viewports.

## Music lifecycle

The music controller gains an explicit active/inactive lifecycle. `visibilitychange`, `pagehide`, and Android native pause events pause both tracks. Returning to the foreground resumes only the previously selected track when sound is enabled. `MainActivity.onPause()` and `onResume()` notify JavaScript so WebView media cannot continue behind another app.

## Compatibility and preservation

- Keep all 20 existing JSON maps, combat, controls, collisions, save data, and offline assets.
- Do not add network dependencies.
- Use existing raster backgrounds and character portraits; no new generated image is required.
- Update cache-busting query versions for changed runtime modules.

## Verification

Add regression tests for 20 stages, 60-star totals, collectible counts, sequential unlocks, hero purchase/ownership, selection back/action states, loading story content, direct map-to-loading flow, connector markup, and music background pause/resume. Run the complete Node test suite, inspect the redesigned screens at 1280x720 with Playwright, and build the Android debug APK using the validated low-memory Gradle command.
