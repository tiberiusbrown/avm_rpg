# Essential NPC mechanics

One eight-slot pool and the 11x7 effective terrain cache bound NPC RAM/work.
Each NPC ID owns at most one slot, so no slot is ever evicted, and occupied slots form a
prefix: scans stop at the first empty slot. IDs are independent of object order, slot and
coordinates. `Npc` is 18 bytes. `NpcPool` is 157 bytes on AVM, and 158 on the host, whose
alignment pads it. Definitions, routes, character sprites and dialogue remain in PROGMEM.

The replacement sample defines NPC_VILLAGER (standing, actor row 1) and NPC_WANDERER
(bounded wandering, row 2) in `src/logic/npc_logic.cpp`. The player's row is 0. The
villager is dynamic: it roams between two residences at the ends of the clearing, named
by the map markers `VillagerWest` and `VillagerEast`. The wanderer is static: it stays
at its generated `WandererHome`. Each has one line of talk dialogue, shown by
`npc_dialogue` after `npc_talk`; the sign is the only other dialogue. Its welcome is
followed by a battle and an outcome line.

`NpcDefinition` is an 11-byte ROM record: behavior/facing, period, pause/walk, leash,
priority, route, sprite_row and talkable. `period` is the number of frames per movement
step; the sample NPCs use 2, so they walk at half the player's speed. Pause and walk are
frame counts. Rows are 0..15; `talkable=false` excludes a character from A targeting,
leaving nearby tile interactions available. Both NPCs and player select directional
frames through `actor_frame`: north/east/south/west groups of three, idle 0, gait 0,1,0,2.

## Presence: NPCs are cast when a scene starts

A scene is an area entry (`world_enter`, which calls `npc_reset`). Presence is decided per
scene, and only two things can change it:

- **Casting.** A reconcile casts each NPC whose home (static) or residence (dynamic) lies
  in the current area. The NPC is placed at that home when its foot box is cached, clear
  of terrain, the player and other NPCs, and not on a portal. On entry the cast may land
  in view, because the whole screen changes at that moment. After entry a cast is allowed
  only out of view: the sprite box must lie outside the view grown by one tile. A cast
  that is blocked, or in view, waits and is retried each frame.
- **Scene exit.** `npc_reset` retires every present NPC with `NPC_RETIRED_RESET`. Nothing
  else removes an NPC.

So an NPC is never removed while its scene lasts, and it appears on screen only by walking
there, by the camera moving onto it, or as part of the scene entry itself.

Static NPCs keep their home for the scene, and `npc_set_destination` refuses them. Dynamic
NPCs have a residence (`NpcPool::residence`, indexed by ID), saved with the game.
`npc_set_destination` changes the residence and the behavior only; it never moves an NPC at
once. For an NPC in the current area the behavior becomes `NPC_TRAVEL`, which walks it toward
the residence. A destination in another area leaves the NPC standing where it is until the
scene ends, and then it is cast at its residence in that area.

## Motion

Every position change is one pixel. Walking, targets, routes, yields and terrain escapes all
go through `step()` or the escape step, and a frame moves an NPC at most once.

- **Cadence.** An NPC steps once every `period` frames. Timers, pauses and stuck limits count
  frames: a blocked attempt adds the NPC's period to its stuck count.
- **Escape.** A tile change can leave an NPC overlapping solid terrain. It then walks out one
  pixel at a time toward the nearest clear spot along a cardinal ray, up to a tile away. The
  distance to that spot falls by one per step, so the walk never jumps. If it cannot move,
  its ordinary logic still runs, so a blocked target can still fail.
- **Walk pose.** The walk pose holds between steps, so a walking NPC does not flicker.
- **Detours.** A steering NPC walks straight at its target. Blocked for `NPC_DETOUR_FRAMES`
  (12 frames), it sidesteps around the obstacle: toward the target across its way ahead, or
  away from it when that side is blocked. Each step it tries to rejoin the way ahead, and it
  sidesteps again while the detour's budget of `NPC_DETOUR_MAX` (15) pixels lasts. A leg
  (a route segment or a target) allows `NPC_DETOURS_PER_LEG` (3) detours. Arrival, a new
  target and a long stall start a fresh budget. A wanderer does the same on its heading,
  sidestepping toward its home, and it stops walking only when no detour is possible. The
  leash and the terrain cache are limits, not obstacles: at either one an NPC waits, or a
  wanderer stops, rather than detouring.

Wandering obeys a pixel leash around its home. Steering tries the axis toward the target
first, then the other, and a blocked NPC detours as described above. A frame, menu or
blocking dialogue freezes simulation.

## Cache margin and freezing

An NPC simulates only while its feet are inside the effective terrain cache, and tiles
outside the cache are solid for collision. The cache is 11x7 tiles: the 9x5 view span plus
one margin tile on every side, so it always extends at least 16 pixels past the view. An NPC
that walks out of view keeps walking inside that margin, then freezes in place at the cache
edge. Its sprite is entirely out of view there, and it resumes from the same pixel when the
camera brings the cache back over it. The device QA shuttle checks this. The margin costs
32 bytes of RAM and roughly 15–23% more reload work per frame (see `decisions.md`).

## Movement and collision

Route waypoints remain two-byte tile coordinates expanded by TILE=16. Stand, wander, target
and once/loop/reverse routes remain supported. Each live actor updates once after the full
terrain cache, regardless of how many chunks are visible.

Player and NPC collision use an 8x8 foot box at (4,8), rather than the whole 16x16 artwork.
A partly clipped actor freezes when its feet leave cached terrain; it is never simulated
against arbitrary offscreen tiles.

After sixteen sustained push frames, two short walkaround lanes are checked. Where the player
can detour, the NPC holds position. Otherwise a safe short side step is animated; if none fits
or a yield becomes blocked, only that player/NPC pair may overlap until separation. The 8x8
footprints can fit side by side in a 16-pixel lane when aligned appropriately, so the NPC holds
still and permits an actual player detour there. Centered tight lanes still yield or allow
temporary passage. Yield search checks up to 15 pixels to cover contacts offset by as much as
seven pixels.

Rendering sorts the visible NPC slots together with the player by ascending Y. A stable
insertion sort stores at most nine byte indices on the stack, leaving the simulation pool
untouched. Lower actors draw last; equal Y preserves slot order and places the player last.

## Guarantees and how they are checked

The rules above make these properties hold for every NPC, not just the sample ones:

1. No NPC is removed while its scene lasts.
2. No NPC moves more than one pixel in a frame, except at a scene entry.
3. No NPC is cast where the player can see it, except at a scene entry.
4. An NPC comes into view only by walking there or by the camera moving.

`tests/test_npc_continuity.cpp` checks each property on every frame of random play, with
terrain edits under the NPCs. A planted one-pixel-jump fault in the escape step is caught by
it. The AVM stack budget (256 bytes) constrains the code: the lifecycle helpers and the
detour and yield helpers are kept out of line, so their frames do not grow the frames on the
deepest paths.

## Limits

- NPCs detour around an obstacle but do not plan a route. A detour sidesteps at most 15
  pixels, three times per leg. A target that lies inside an obstacle, or beyond a longer
  one, fails.
- There is no API for removing an NPC mid-scene, so a quest that needs one must end the scene
  first.
- A scene cut is not a pop. Entering an area, or teleporting the player across the map, changes
  the whole screen at once, and any NPC on the new screen appears with it.

## Hooks

The generic engine calls activation, movement-event, retirement and restore hooks declared in
`logic/npc_logic.h`. The sample hooks are empty. Events cover route start/completion/invalidity
and target reached/failed. Retirement is reported only at scene exit. Independent native policy
fixtures exercise route replacement, pauses, endpoints and targets without quest code inside
`src/npc.cpp`.

Saved state includes each dynamic NPC's residence (`SAVE_VERSION` 5). Live NPC pixels, timers
and stuck counts are transient and are rebuilt by casting on load. Only player state and flags
are saved. The new content boundary rejects all pre-version-4 saves.
