# Essential NPC mechanics

One eight-slot pool and the 9x5 effective terrain cache bound NPC RAM/work.
IDs are independent of object order, slot and coordinates. `Npc` remains 18
bytes and the pool/request scratch remains 156 bytes. Definitions, routes,
character sprites and dialogue remain in PROGMEM.

The replacement sample defines NPC_VILLAGER (standing, actor row 1) and
NPC_WANDERER (bounded wandering, row 2) in `src/logic/npc_logic.cpp`. The player's
row is 0. They occupy generated `VillagerHome`/`WandererHome` positions. Neither
has a conversation; the sign is the sample's only dialogue. The old per-character
quest headers/roster, assignments and offscreen courier story are removed.

`NpcDefinition` is an 11-byte ROM record: behavior/facing, period, pause/walk,
leash, priority, route, sprite_row and talkable. Rows are 0..15; `talkable=false`
excludes a character from A targeting, leaving nearby tile interactions available.
Rows do not add per-live-actor RAM. Both NPCs and player select directional frames
through `actor_frame`: north/east/south/west groups of three, idle 0, gait 0,1,0,2.

## Admission and lifecycle

Native chunk handlers request characters through `request_essential_npc<ID, POS>`.
Policy functions select definitions, placement and canonical home. Requests
deduplicate, prioritize definition priority then ID, and report capacity faults
with saturating counters. Visible incumbents keep their slots/physical positions;
offscreen actors retire and are reconstructed from canonical policy when eligible.
Occupied, solid or uncached home positions defer activation.

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

Player and NPC collision use a 12x10 foot box at (2,6), rather than the whole
16x16 artwork. A partly clipped actor freezes when its feet leave cached terrain;
it is never simulated against arbitrary offscreen tiles. Terrain repair tries
nearby positions before retiring a blocked actor.

After eight sustained push frames, two short walkaround lanes are checked. Where
the player can detour, the NPC holds position. Otherwise a safe short side step
is animated; if none fits or a yield becomes blocked, only that player/NPC pair
may overlap until separation. This preserves progress through one-tile corridors.
Native and guest fixtures cover wandering, scaled footprints, targeting, clipping,
terrain repair, sign input priority and corridor passage.

Only player state and flags are saved. Live NPC pixels/timers are transient and
reconstructed on load. The new content boundary rejects all pre-version-4 saves.
