# Essential NPCs (M1.6)

The engine has one `Game game`, one 17x9 effective tile cache, and an eight-slot
essential-character pool. No population registry is walked each frame. Only
generated visible-chunk calls and the eight live slots participate. Stable byte
IDs in `npc_types.h` are independent of object order, chunk, coordinates and slot.
Zero means an empty slot; up to 255 distinct essential identities are available.

## Definition, assignment and live state

`NpcDefinition` records live in PROGMEM under `src/logic/npcs`. A direct identity switch selects a ROM
record containing behavior, facing, movement period, pause/walk durations, leash,
priority and default route. Several characters may share one definition. Text,
routes and sprite frames also stay in program memory. Adding definitions adds no
per-character RAM. There is no heap, virtual dispatch, recursion or script VM.

Enum-valued fields and APIs use their byte-backed domain types: `NPC_BEHAVIOR`,
`NPC_ROUTE_MODE`, `NPC_ID`, `NPC_STATE` and `DIRECTION`. State masks have typed
bitwise operators. Actor facing and dialogue prompts likewise use their enums;
serialized facing bytes are validated before conversion. Definition/live-record
sizes and the save format remain unchanged.

`npc_placement(id)` derives the canonical placement from quest flags, independently
of visible chunks. `npc_home(id, placement)` returns generated Tiled constants
through a native identity switch. That switch contains names, never handwritten
chunk ownership or coordinate literals. Mara's assignment follows
`FLAG_GUARD_RELOCATED`. Pip's dispatched/arrived flags identify a durable journey
and its destination. Rowan can become absent through `FLAG_ROWAN_AWAY`; an absent
assignment returns area zero. Other characters have one placement. These queries
are implemented by each character's logic, not by the movement engine. A native
quest changes assignment with `game.set_flag(...)`.

## Character lifecycle hooks

`src/logic/npcs/roster.inc` associates each stable identity with a native logic
type. `src/logic/npc_logic.cpp` expands that single roster into direct switches
for definitions, placement, home, activation, events, retirement and dialogue.
Add a character header and roster entry without editing `src/npc.cpp`. Shared
types support families of characters, as demonstrated by the hall occupants.
`Defaults` supplies empty optional hooks. Full LTO removes unused defaults;
there is no callback table, event queue, virtual dispatch or per-character RAM.
Only requests and the eight active slots participate during normal frames.

Activation runs after a slot is initialized and may select its initial behavior.
`NPC_EVENT` reports route start, route completion, invalid routes, target arrival
and target failure. Route start fires exactly once on the first eligible simulated
update, after validation; activation, rendering, paused frames and load do not
dispatch it. A pending bit uses the last bit of the existing state byte. Explicit
route commands rearm it; target commands cancel it. Completion/failure hooks see
an already stopped actor and may start another movement via `npc_move_to` or
`npc_follow_route`; the replacement runs on a later update. Route completion
retains the completed route ID for the handler to identify the relevant journey.
Loops/reversals do not emit completion. There is no recursive event queue.

Retirement receives the still-valid outgoing actor and a typed reason:
`NPC_RETIRED_OFFSCREEN`, `NPC_RETIRED_TERRAIN` or `NPC_RETIRED_RESET`. The slot is
cleared only after notification. After a pool reset, each logic type's `restore`
hook runs once, even if no actor existed (as after save decoding). These hooks
run only during reconstruction, not every frame, and must be idempotent.
They derive durable outcomes from quest state independently of visible chunks.

All lifecycle hooks are synchronous and nonblocking. They may update quest
flags; activation/movement hooks may change their actor's behavior through the
movement APIs. They must not reset the pool, teleport, show dialogue or recursively
simulate the world. Retirement hooks observe a const actor. Blocking dialogue
uses the separate `talk` hook after the complete cache and movement pass.

`Courier::event` dispatches and completes only `routes::CourierWalk`, so other
routes and other actors using the same polyline cannot trigger delivery flags.
Its offscreen/reset retirement finishes dispatched travel. Terrain retirement
instead cancels dispatch and retries from the source; unsafe recovery does not
count as arrival. Its restore hook completes saved dispatched journeys without
needing a live slot. Guard relocation and Rowan's absence likewise live entirely
in their own logic headers. The engine contains no character IDs, quest flags,
authored positions or route names.

An active `Npc` is **18 bytes** on both host and AVM: six-byte actor, four-byte
union for a target pixel position or route ID, and eight behavior/identity bytes.
The old placement byte belongs to the actor's physical incarnation, so it can
remain visible at the old post after the canonical assignment changes. All eight
records plus a bounded eight-ID request queue and four bookkeeping bytes occupy
**156 bytes**. Slot indices are never saved or used as character identity.

## Authoring and generated calls

Add a centered point to `Logic` or `Locations`, such as `VillageGuardHome` at
`(8*tx+4, 8*ty+4)`, and set `handler=update_village_guard`. Add the corresponding
native overload in `src/logic/handlers.h`:

```cpp
template <TilePos POS> void update_village_guard(Gameplay<POS>) {
    request_essential_npc<NPC_GUARD, POS>();
}
```

The exporter emits `positions::VillageGuardHome` and a direct
`Handler<positions::VillageGuardHome, STAGE>{}.update_village_guard()` call only in
its owning chunk. Moving it across a chunk boundary automatically changes both.
Requests are accepted only when the authored anchor matches the current global
assignment and intersects the camera. They express eligibility; they never reset
an existing actor's position, timers, route or target.

Routes are separate Tiled **polyline objects in a `Routes` layer**. They have a
unique name, optional `mode` of `once`, `loop` or `reverse`, and optional byte
`pause` (default eight simulation frames). They have no type/class, handler or
`route_id` property. The exporter sorts route names and assigns IDs 1..255.
Reordering objects does not change those IDs; adding/renaming routes can renumber
them. Native code always uses generated names such as `routes::CourierWalk`.
Route IDs are transient implementation details, never saved character identity.

Use tile-interior waypoint markers like location objects: **center points at
`(8*tx+4, 8*ty+4)`, never tile corners**. Tiled polyline points are relative to
the object's X/Y. The exporter adds that origin and selects each containing tile
with floor(X/8), floor(Y/8), just like location objects. Moving a marker within
its tile does not change the waypoint. Edges/corners, nonfinite/out-of-map points,
duplicate names, unsupported shapes/properties and fewer than two or more than
255 points reject generation. There can be at most 255 routes.

`generated/npc_routes.h` contains named generated ID constants; `npc_routes.cpp`
contains `constexpr RoutePoint PROGMEM[]`, compact descriptors and direct lookup.
Each waypoint is **two bytes: byte tile X and byte tile Y**; the descriptor stores
the shared area. Only the current waypoint is widened and multiplied by eight
for pixel steering. Actors still move smoothly in one-pixel steps. Neither route
traversal nor lookup scans the authored world. Area/tile/chunk coordinates and
map dimensions retain their byte contracts. `CourierWalk` demonstrates once with
pauses and crosses x=128. `CourierLoop` and `CourierReverse` exercise reusable
endpoint policies in host and guest tests.

## Frame ownership, visibility and travel

Each exploration update clears the request queue, runs every visible chunk's
gameplay stage once, rebuilds all 153 tile entries, and completes every tile
handler. Player overlap repair precedes reconciliation. The pool retires fully
offscreen actors, admits safe requested actors, repairs invalid terrain positions,
then updates each remaining cached actor **once**. NPC interaction precedes the
ordinary dialogue stage; player movement follows it. Rendering uses that same
camera/cache. No gameplay dispatch is repeated to refresh NPCs or terrain.

Active sprites intersect the exact 128x64 camera rectangle; there is no additional
simulation halo. Feet must fit the effective cache before movement/recovery.
A partially clipped sprite whose feet are no longer cached freezes until cached
again or completely offscreen. This avoids snapping against the solid outside
sentinel. Every physical NPC step uses `terrain_clear`; no off-window projection
or full-chunk RAM data exists. Uncached terrain is always solid. A route toward an
uncached edge waits if a sprite is still visible; physical simulation never reads
beyond the effective window.

Visible actors retain their position when assignment changes. Mara has no suitable
inter-area walking route, so her physical relocation waits until offscreen or an
area transition. The destination handler subsequently requests the same identity
at its canonical post. It cannot create a second visible Mara. Pip follows an
authored route while visible. His first simulated route update sets the saved
dispatch flag and makes the destination canonical; completion sets his arrival flag. Once his dispatched
journey leaves the active view or an area is left, it finishes logically at the
destination without simulating intervening terrain. That flag persists even when
the destination chunk has never executed. An unstarted offscreen courier is idle
at his initial assignment until that anchor becomes eligible and simulation begins.
A save during travel retains dispatch; loading completes that journey logically at
the destination instead of restarting at the source. Rowan's requested absence also
defers visible departure until he is offscreen, then suppresses reconstruction.

Teleports clear old actors/requests and finish a dispatched courier journey. They
preserve the existing rule of destination **tiles only** during the outgoing
update. Destination actors reconstruct on its first ordinary gameplay update;
loading/explicit `world_enter` reconstructs without advancing NPC movement or RNG.
Transient target/route overrides and wandering positions reconstruct from their
canonical assignment. Story-specific travel requiring intermediate persistent
checkpoints would need another explicit quest flag/stage; exact live pixels are
intentionally absent from saves.

## Admission and capacity

Requests deduplicate by identity and sort by descending ROM priority, then ascending
stable ID. Existing visible actors have retention priority. Fully offscreen actors
retire before admission; no visible actor is evicted to make room. Admission
requires a clear home footprint, no player/actor overlap, a visible anchor and no
portal tile. A blocked home defers activation until a later request; it never
searches arbitrary offscreen terrain or materializes inside another actor.

More than eight simultaneous eligible characters is an authoring capacity error.
`capacity_faults` saturates at 255 for queue/pool overflow, and the deterministic
excess requests defer. Visible incumbents remain present. `activation_deferrals`
records unsafe admission/recovery. Tests exercise full-pool retention and explicit
overflow reporting; all authored gameplay scenes must produce zero capacity
faults. The hall authors eight characters and tests actual simultaneous admission.
Increasing authored demand requires increasing capacity and rerunning AVM budgets;
the engine cannot display nine distinct colliding bodies using eight slots.

## Movement and collision

Standing, wandering, target steering and route traversal are native finite-state
behaviors. Movement periods are powers of two (one or two frames in the demo).
Wandering alternates pause/walk phases, takes deterministic game RNG choices and
enforces ROM X/Y pixel leashes around the actor's physical placement. Movement
targets are distinct from canonical assignment. The target API is `npc_move_to(id, x, y)` and `npc_move_to<positions::Name>(id)`;
`npc_follow_route(id, routes::Name)` starts an explicit override.

Steering tries at most two needed axes, alternating preference when obstructed.
It uses no search/grid. After 24 blocked updates a target stops with
`NPC_TARGET_FAILED`; a route waits for its configured pause and retries the same
segment. Routes support completion, looping, reversal and per-waypoint pause.
They cannot navigate arbitrary concave obstacles: author a suitable route.

All bodies use the player's 6x5 foot box at offset (1,3). Each normal displacement
checks terrain and other actors in one-pixel increments. NPCs never displace the
player. A head-on collision initially blocks without moving the NPC.

After **eight sustained push frames in one direction**, the engine checks whether
the player can walk around the blocking body. This checks two fixed three-segment
lanes: sidestep beyond the NPC foot box, move forward past it, then return to the
original line. Every intermediate player footprint must clear effective terrain
and other NPCs. This is a bounded local clearance test, not a pathfinding grid or
search. If a lane is clear, the NPC holds its position and ordinary collision;
the player can take that route around it. Continued head-on pushing does not
make it step aside or become passable. Clearance is checked again every eight
push frames so changed terrain/actors cannot leave a stale obstruction decision.

If neither walkaround lane is clear, the NPC plans a perpendicular escape of at
most eight pixels, checking terrain, the player, other actors and its wander leash.
The collision call records a plan without changing the NPC position. Starting on
the next update, it **walks one pixel per simulation frame**, with facing/walking
animation, until the attempted player position is clear. Its normal behavior is
paused during the step; its target/route and pause timer are preserved. Direction
and remaining distance temporarily share its otherwise unused stuck-counter byte,
so the live record remains 18 bytes. No instant side displacement occurs.

If no safe side step exists, or dynamic collision blocks a planned step, that
player/NPC relationship permits pass-through. Each NPC has its own pressure and
bypass state; multiple blockers cannot continually reset one another's timeout.
Direction changes or releasing the push reset pressure. During overlap, only the
affected player/NPC relationship bypasses actor collision; terrain and NPC/NPC
collision still apply. Normal collision returns after separation. No pushback
loop exists. Thus ordinary bodies cannot permanently obstruct traversable terrain:
either a validated local walkaround remains available, or delayed yielding/
pass-through opens the direct route. Story barriers belong in solid tiles/quests.

If effective terrain closes on an NPC, recovery checks N/E/S/W at distances 1..8,
respecting other actors and leashes. It never moves the player. If no safe cached
position exists, the unsafe actor retires and reconstruction defers. This exceptional
terrain recovery can move/remove a visible body; quest relocation and pool overflow
cannot. A dynamically enclosing tile change therefore cannot leave a solid actor
inside a wall or force the player into terrain.

## Rendering, interaction and persistence

NPCs use the existing eight-frame facing/walking actor atlas and clipped AVM sprite
calls, with a small identifying head bar. Tiles draw first, NPCs then the player.
Dialogue/menu/status backgrounds use this same renderer, without simulation.
Overlap draws the player on top; general depth sorting is not implemented.

Facing targets within 12 forward and five transverse pixels are ordered by distance,
then stable ID. A targeted NPC consumes fresh A before tile dialogue handlers.
Held A never synthesizes a press. Character dialogue lives alongside each character's rules in `src/logic/npcs`; the NPC engine only
selects/consumes the interaction and calls `npc_talk(id)`. Native identity dialogue includes Mara's quest
choice and changed reply. An A interaction pauses player motion, NPC updates, RNG
and exploration time; blocking dialogue/menu/status loops never update the world.

Save version 3 and its physical **24-byte** record remain unchanged. The four new
flags append at indices 4..7, within the existing single flag byte. Old v3 flag
counts zero-extend; v1/v2 migrations remain intact. Actors, requests, pressure,
route indices, animation and targets are transient. The saved quest flags and RNG
reconstruct the correct characters, with safe activation deferred when occupied.

## Demo and verification

Iona stands north of the starting path; Rowan wanders around the southern garden.
Mara stands north of the house, west of the gate, and accepts reassignment to the
house's northeast post. Rowan's native choice can send him away; clearing the
quest flag makes him eligible again. Pip follows the delivery polyline east of the house.
Walking onto `NpcHallDoor` at outdoor tile (26,10) enters a narrow hall; walking
west onto its exit returns outside. It contains eight named essential characters,
including one wanderer, and can always be crossed by sustained movement.

`tests/npc_scenarios.h` is compiled unchanged into the native NPC suite and
`m16_probe`. It checks stable IDs, duplicate requests, camera retirement, visible
relocation deferral, offscreen destinations, save/load, blocked admission, leashed
wandering, pauses/facing, targets/stuck fallback, all route endpoint modes, chunk
crossing, one update per frame, dynamic terrain recovery, interaction precedence,
eight-slot saturation, open-ground refusal to yield, delayed one-pixel alcove
yielding, dynamically blocked yield recovery, corridor progress and area reconstruction. Corridor cases
cover stationary, wandering, two-body congestion and a boundary-adjacent actor.
The full-device test additionally exercises real buttons, blocking NPC dialogue
freeze, eight active actors, steering, corridor passage, visible rendering and
physical FX save followed by a fresh emulator restart.

`tests/test_npc_hooks.cpp` links the production mechanics with an independent
character policy. It verifies activation overrides, single route-start delivery,
replacing a route from its start hook, chaining a target after completion,
target/route failures, all retirement reasons, and restore with an empty pool.
The shared host/AVM scenarios also check that another identity or route cannot
trigger courier quest flags and that terrain retirement retries delivery.

See [measured evidence](m1_6_measurements.md) for current RAM/stack/cycle values,
hashes, test totals and exact reproduction commands. Physical hardware,
audio-enabled workloads and universal worst-case execution time remain unverified.
