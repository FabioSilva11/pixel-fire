# Procedural Modular Dungeon Design

## Objective

Replace the stretched rectangle-based dungeon with a seeded procedural map that uses all 24 structural Kenney GLB pieces correctly on a 50×50 grid with two elevation levels. Keep the camera inside the dungeon and make the character face its actual movement direction in every cardinal and diagonal direction.

## World coordinates

- Grid: 50 columns × 50 rows × 2 levels.
- Native cell size: 4 world units; total horizontal envelope: 200×200 units.
- Level height: 4.05 world units, matching the stair landing.
- Models use uniform scale 1.0. Placement may only translate and rotate by multiples of 90 degrees.
- Logical footprints ignore decorative trim up to 0.2 units beyond a cell boundary.

## Structural catalog

The catalog contains 24 required structural definitions:

- 11 corridors: straight, corner, end, junction, intersection, transition, and the five wide variants.
- 4 gates: open frame, static metal bars, animated solid door, and animated windowed door.
- 7 rooms: corner, large, large variation, small, small variation, wide, and wide variation.
- 2 stairs: narrow and wide.

Each definition records the GLB URL, logical footprint, native sockets, socket width (`narrow` or `wide`), rotation, level delta, traversal state, and placement role. `room-corner` exposes only north/west sockets. `room-wide-variation` exposes only east/west sockets. The transition connects a narrow west socket to a wide east socket. Stairs connect a low north socket to a high south socket. Gates are thin edge structures rather than ordinary floor tiles.

## Procedural generation

Generation is deterministic for a seed and bounded in cost:

1. Start from a central safe room.
2. Create a connected main route across level zero.
3. Place both stairs and continue a route on level one.
4. Place every one of the 24 structural definitions at least once.
5. Grow branches using compatible sockets and 90-degree rotations.
6. Reject footprint overlaps, out-of-grid placements, mismatched narrow/wide sockets, mismatched levels, and connections into closed sides.
7. Keep static metal bars off every required start-to-exit route.
8. Put the animated final door on the route to the exit portal.
9. Validate reachability of start, enemies, final door, and portal.
10. Retry with derived seeds up to a fixed limit, then use a bundled known-good seed.

The generated result contains placements, transformed sockets, navigation regions, graph edges, start/exit positions, enemy spawn candidates, and its seed. Generation must not depend on Three.js so it can be unit tested in Node.

## Rendering and asset reuse

- Copy all missing structural GLBs from the supplied Kenney pack into the packaged dungeon asset folder.
- Load each unique GLB once and clone its prepared scene for each placement.
- Share geometry and materials between clones.
- Apply only uniform scale, grid-aligned translation, level translation, and quarter-turn rotation.
- Limit expensive shadows to nearby or important pieces.
- Keep the portal procedural geometry, positioned after the final animated door.

## Navigation, collision, and elevation

- Derive walkable regions from generated placement footprints and open sockets, not arbitrary hand-written rectangles.
- Closed sides and static metal bars block player and enemy movement.
- Animated doors block until their runtime state becomes open.
- Stair navigation interpolates Y from 0 to 4.05 along the stair axis.
- Ordinary pieces use their placement level as their floor Y.
- Enemy movement is restricted to reachable navigation regions and uses stair links to change level.

## Camera

- Third-person desired distance: approximately 5.5 units.
- Clamp pitch so camera height stays below native wall tops.
- Raycast from the player look target toward the desired camera position against nearby dungeon occluders.
- Pull the camera in front of the nearest wall with a safety margin, then smoothly restore distance when clear.
- Do not allow gameplay camera positions outside the navigable dungeon envelope.
- Top-down presentation remains exclusive to the minimap.

## Character facing

Calculate facing from the normalized movement vector exactly once. Use a single calibrated model-forward offset rather than mixing an inverted Z argument with an additional π rotation. Cover north, south, east, west, and all four diagonals with unit tests.

## Minimap

- Draw generated placements rather than the removed room/corridor rectangles.
- Track exploration on the 50×50 grid.
- Render only the player's current level prominently; show stairs and the other-level connection distinctly.
- Derive bounds and centering from generated placements.
- Show narrow/wide corridors, rooms, gates, stairs, player, and discovered enemies.

## Failure handling

- A missing optional model omits only its placements and reports progress failure.
- A missing required model stops world creation with a clear loading error.
- Generator failure uses the known-good fallback seed and records the reason in a warning.
- Runtime door and stair state must never create an unreachable respawn position.

## Verification

- Catalog test: exactly 24 structural definitions and correct socket metadata.
- Generator tests over multiple seeds: all 24 present, no overlaps, in bounds, compatible sockets, both levels used, and connected start-to-exit path.
- Navigation tests: walls, static bars, doors, and stair elevation.
- Facing tests for eight directions.
- Camera tests for pitch/distance bounds and wall pull-in.
- Minimap tests for 50×50 exploration and level selection.
- Browser verification: load a generated game, inspect the console, capture gameplay/minimap evidence, and verify camera behavior.
- Android verification uses the existing low-memory one-worker build commands after browser tests pass.

## Non-goals

- No online multiplayer.
- No editor UI for hand-authoring maps in this change.
- No arbitrary-angle placement or non-uniform model scaling.
- No additional third elevation level.
