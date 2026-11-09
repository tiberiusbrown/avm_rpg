# Essential NPC mechanics

One eight-slot pool and the 11x7 effective terrain cache bound NPC RAM/work.
IDs are independent of object order, slot and coordinates. `Npc` is 18 bytes and
`NpcPool` is 176 bytes on both host and AVM, so the QA reader's layout holds.
Definitions, routes, character sprites and dialogue remain in PROGMEM.

The replacement sample defines NPC_VILLAGER (standing, actor row 1) and
NPC_WANDERER (bounded wandering, row 2) in `src/logic/npc_logic.cpp`. The player's
row is 0. The villager is dynamic: it roams between two residences at the ends of
the clearing, named by the map markers `VillagerWest` and `VillagerEast`. The wanderer is static: its chunk handler spawns it at the generated
`WandererHome`. Each
has one line of talk dialogue, shown by `npc_dialogue` after `npc_talk`; the sign
is the only other dialogue. The old per-character
quest headers/roster, assignments and offscreen courier story are removed.

`NpcDefinition` is an 11-byte ROM record: behavior/facing, period, pause/walk,
leash, priority, route, sprite_row and talkable. Rows are 0..15; `talkable=false`
excludes a character from A targeting, leaving nearby tile interactions available.
Rows do not add per-live-actor RAM. Both NPCs and player select directional frames
through `actor_frame`: north/east/south/west groups of three, idle 0, gait 0,1,0,2.

## Static and dynamic NPCs

`npc_is_dynamic(id)` decides which kind an NPC is. Both kinds are admitted only
when visible, and both wander, walk and yield like any actor while spawned.

- **Static NPCs** have no enduring location. A chunk handler requests them through
  `request_essential_npc<ID, POS>` at their generated home. Requests deduplicate,
  prioritize definition priority then ID, and report capacity faults with
  saturating counters. An offscreen static NPC retires and returns only when its
  chunk requests it again. `npc_set_destination` refuses static NPCs.
- **Dynamic NPCs** have a residence (`NpcPool::residence`, indexed by ID). While
  not in a slot they are dormant, in a residence index sorted by area, chunk row,
  and chunk column. Admission looks only at dormant entries inside the new visible
  tile rectangle, then checks each tile is visible and clear. A blocked visible
  admission is retried each frame until placed or out of view. An active dynamic
  NPC that leaves view teleports to its residence. `npc_set_destination` moves the
  residence, reindexes a dormant NPC, or sets an active NPC walking with
  `NPC_TRAVEL`, which steers toward the residence each frame and never gives up
  while visible.

Visible incumbents keep their slots and physical positions. Occupied, solid or
uncached spawn tiles defer activation.

Saved state includes each dynamic NPC's residence (`SAVE_VERSION` 5). Live NPC
pixels, timers and the residence index are reconstructed on load.

**Cache margin.** An NPC simulates only while its feet are inside the effective
terrain cache, and tiles outside the cache are solid for collision. The cache is 11x7
tiles: the 9x5 view span plus one margin tile on every side. A walking NPC therefore
reaches the view edge before the cache edge and leaves view by itself, even while the
player stands still. The device QA shuttle checks this. The margin costs 32 bytes of
RAM and roughly 15–23% more reload work per frame (see `decisions.md`).

The generic engine calls activation, movement-event, retirement and restore hooks
declared in `logic/npc_logic.h`. The sample hooks are empty. Events cover route
start/completion/invalidity and target reached/failed; retirement distinguishes
offscreen, terrain failure and reset. Independent native policy fixtures exercise
route replacement, pauses, endpoints, targets and retirement without quest code
inside `src/npc.cpp`.

## Movement and collision

Movement is pixel-based; route waypoints remain two-byte tile coordinates expanded
by TILE=16. Stand, wander, target and once/loop/reverse routes remain supported.
Wandering obeys a pixel leash. Steering tries two axes and has a bounded stuck
fallback. Each live actor updates once after the full terrain cache, regardless of
how many chunks are visible. A frames, menus and blocking dialogue freeze simulation.

Player and NPC collision use an 8x8 foot box at (4,8), rather than the whole
16x16 artwork. A partly clipped actor freezes when its feet leave cached terrain;
it is never simulated against arbitrary offscreen tiles. Terrain repair tries
nearby positions before retiring a blocked actor.

After eight sustained push frames, two short walkaround lanes are checked. Where
the player can detour, the NPC holds position. Otherwise a safe short side step
is animated; if none fits or a yield becomes blocked, only that player/NPC pair
may overlap until separation. The 8x8 footprints can fit side by side in a 16-pixel lane
when aligned appropriately, so the NPC holds still and permits an actual player
detour there. Centered tight lanes still yield or allow temporary passage. Yield
search checks up to 15 pixels to cover contacts offset by as much as seven pixels.
Native and guest fixtures cover wandering, scaled footprints, targeting, clipping,
terrain repair, sign input priority, walkaround, eight-pixel animated yields
and tight-corridor passage. Both host and AVM probes check exact foot-box edges.

Rendering sorts the visible NPC slots together with the player by ascending Y.
A stable insertion sort stores at most nine byte indices on the stack, leaving
the simulation pool untouched. Lower actors draw last; equal Y preserves slot
order and places the player last. Full-device tests verify masked overlap pixels
with the player between two NPCs deliberately stored in reverse depth order.

Only player state and flags are saved. Live NPC pixels/timers are transient and
reconstructed on load. The new content boundary rejects all pre-version-4 saves.
