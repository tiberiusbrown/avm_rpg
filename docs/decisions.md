# Technical decisions

## M1.5 world architecture

Current world behavior is specified in [chunk_world.md](chunk_world.md) and
authoring/build behavior in [tiled_pipeline.md](tiled_pipeline.md). Tiled is the
map authority; generated native C++ calls associate each footprint with all
intersecting chunks. Moving an anchor moves its constants and dispatch without
handwritten ownership changes. No custom interpreter or runtime registry exists.

The sole resident tile cache is 11x7 bytes (the 9x5 view span plus a margin tile on every
side) with byte origins, fully reloaded every
simulation frame. Map tiles remain row-major in PROGMEM; 8x4 chunks determine
handler dispatch, independently of tile storage. Byte tile/chunk/map dimensions enforce
the 240x248 limit, while pixels and flattened indices use uint16_t. Logical
bounds clip map reads and camera edges. Active area dimensions are cached in
Game's existing two-byte MapDimensions on entry or
cross-area teleport. Runtime camera, chunk, cache, setter and membership bounds
use those RAM values; save validation still checks the candidate area before
committing. The cache adds no RAM or save fields. Every live chunk completes its
`STAGE_GAMEPLAY` gameplay stage before cache reload and the `STAGE_TILES` terrain
stage. Only after every tile chunk completes does `STAGE_DIALOGUE` run potentially
blocking interactions. The exporter generates Tag<POS, STAGE>, stage aliases,
generic empty handler functions, dependent dispatch helpers and a forwarding
Handler<POS, STAGE> interface. Ordinary overloads in handler_impl supply behavior
for a stage, position or exact pair; unimplemented combinations remain no-ops.
Tile setters and input/teleport helpers access globals directly.
TileContext, ChunkContext and WorldState have been removed. The tile stage reads
the gameplay flags/time and produces consistent terrain during the same update.
Dialogue terrain changes take effect on the next tile pass unless explicitly
refreshed with refresh_tile_cache().

Tile reads are cache-only; misses return the solid outside sentinel. Rectangles
expand into independent tile specializations, with no runtime region ownership
or dedup table. All three stages share one generated ordered handler list. Overlapping
handlers execute in that order; persistent guards protect shared rewards.
Global `teleported` ends outgoing gameplay; destination tiles still all run.

M1.6 essential NPC ownership and implementation are documented in [essential_npcs.md](essential_npcs.md). Ordinary chunk
handlers retain the lantern choice, gate/bridge controls and paired teleports.
Map objects carry optional names and native handler references, with no types.
All coordinates are TilePos values. C++20 structural non-type template parameters
bind each tile by value; named rectangles also retain optional contains-query metadata.
Equal area/X/Y values share a specialization, and inline or local constexpr
TilePos arguments do not require an exported object's identity or linkage.
Unnamed handlers use private constexpr positions with no public location names. Walking-entry, facing-A and
teleport helpers replace exported portal routing and interaction registries. Point markers
are centered; tile regions include all tiles partially overlapped by a rectangle.
Generated area IDs use AREA_. Chunk origins trust validated exporter output.

Indexed quest flags use a bitset sized by FLAG_COUNT, with getter/setter/clearer/
toggler methods; player region checks share Game::player_is_in_region. Save
version 3 serializes the count and packed bytes and migrates validated v1/v2
records, dropping the retired NPC flag. The tile cache and timer are never saved.
See current [measurements](m1_6_measurements.md) for resource proofs and margins.

Gameplay uses one unconditional `Game game`, defined in game.cpp and declared in
rpg.h, in native and AVM builds. Engine, screen, render and save functions access
it directly. Tests reset the singleton and use host-only snapshots for
deterministic comparisons. Frame events are transient globals outside Game.
Single-tile projection queries and direct PROGMEM tile lookup APIs remain absent.
Saves validate structure/bounds, then load the cache and repair blocked positions.
See [three-stage handlers](chunk_world.md#global-gameplay-state-and-three-stage-handlers).
Full LTO checks the singleton and rejects surviving Game-reference helpers or
stored map position objects. The save schema is independent of this API change.

## Historical M0/M1 decisions

The following records describe the preserved starting point. M1.5 supersedes
their direct-map rendering, actor simulation, save schema, explicit Game arguments
and no-cache decisions.

## Shared deterministic C++17 rules

`Game` owns durable character/world fields and fixed-capacity transient actors.
`game_update(Game&, held_buttons)` advances exploration rules without drawing,
waiting or performing IO. Screen functions call save/load directly. The same `game.cpp`, `world.cpp`,
`dialogue.cpp`, `save.cpp` are linked into native tests and the guest. Areas and NPCs
have explicit stable IDs. Immutable ROM definitions are read directly, so map
size affects program space rather than guest RAM. Only the active area's NPCs
are simulated. Player/actor facing, walking and coordinates are independent.

Pass state by `Game&` / `const Game&` to keep native tests independent. The AVM
guest has one global `Game`, so full LTO specializes the rules to it. Guest links
inspect optimized LLVM IR and fail if a game-reference argument survives. In the
linked guest, source/debug names still describe C++ reference signatures while
the actual calls omit the game reference. Exact retained signatures are recorded
in `build/artifacts/*.elf.lto.json`. The save format is independent of struct layout.

Use `PROGMEM` for assets and `F()` for flash strings. Native builds define these
macros in `rpg.h`. Structs use CamelCase, enum types/values and constants use
ALL_CAPS, and functions/variables use snake_case. `.clang-format` encodes the
four-space formatting rules in `AGENTS.md`.

## Pixel exploration and pacing

Positions are unsigned 16-bit pixels; player speed is 1 px per 20 ms frame
(50 px/s), NPC speed 1 px per walking step. Movement tests every intermediate
pixel, including all terrain tiles touched by a 6×5-foot collision box at
sprite offset (1,3). It cannot tunnel even if a future action requests a large
step. Opposing directions cancel per axis; if both remaining axes are held,
vertical takes precedence. There is no diagonal speed boost. Facing changes
when blocked; walking animation occurs only on successful displacement.

Use SDK `avm_set_frame_rate(50)` and `avm_next_frame`, rather than an additional
timer. Motion is independent of workload while frames meet the 20 ms deadline.
There is no catch-up after an overloaded frame; blocking safe-point operations
may stall simulation. The 20 FPS M1 workload, including 12 NPCs, met its deadline.
The 50 FPS device QA requires every exploration frame under the 320,000-cycle
budget, so additional timing infrastructure is unwarranted.

## Camera, content and actors

The outdoor is 256×160 px; the interior is 128×80. Camera follows at approximately
(60,28), clamps to world edges, and renders partial tiles with signed coordinates
and AVM sprite clipping. Background assets are 8×8 program sprites. Empty paths
and interior floors are skipped on the cleared buffer. NPCs draw a small overhead
bar to distinguish them from the player. Simple actor animation uses two leg
phases per facing. No caching/compression/dirty rectangles were needed to pass
50 FPS; decorative detail can be added only with new measurements.

Active capacity is 12 actors, eight guest bytes each. Normal gameplay uses three
outdoor walkers and one stationary interior caretaker. The stress build activates
12 outdoor walkers. Walkers choose facing/wait/walk on 48..63-frame timers, then
advance pixel by pixel until blocked or out of their authored rectangle. They
never occupy the required doorway or its approach. Collision includes player
and other NPCs; blocked walkers wait then reconsider. No pathfinding, recursion,
off-area schedules or persistent ambient positions are necessary. Area entry
reconstructs from definitions and relocates spawns if they overlap the player.
Rowan's roaming rectangle includes a two-tile stone post; tests verify stopping
at the post and walking around its side.

Paired portals trigger on the player's foot-center tile. The destination is a
passable position outside the return portal. A latch must first observe a
non-portal position before another transition, including after a load. Stats,
flags and RNG survive transitions; NPC scratch does not.

## UI and saves

`title_screen`, `explore`, `menu_screen`, and `status_screen` own their input,
rendering, and display loops. Exploration calls the menu, which calls status;
returning resumes the caller. Selections and feedback are local variables.
`Game` contains no mode, pending command, or input/UI bookkeeping.
`input_wait_press` requires release before a fresh press; `input_wait_frame`
uses the SDK frame limiter. Exploration uses `input_pressed` for actions and
`input_held` for movement. Returning from a screen synchronizes button edges.

`dialogue_show(game, F("..."))` and `dialogue_ask(game, F("..."))` are blocking
calls usable from game logic. They render a fixed lower
overlay over the frozen world, wrap and paginate flash text using the runtime
font, and return to their caller. Text types out letter by letter with a blip per
letter. A during typing finishes the current page; A at a prompt advances or
confirms, but is ignored for half a second after the prompt appears. B dismisses
or declines at a prompt. Each prompt requires a fresh press. Five-byte text scratch lives in shallow
helpers that return before input waits; no page buffer or glyph table is stored.
Facing plus proximity identifies an NPC on an A edge; `npc_talk` records the request
and `npc_dialogue` shows that NPC's line in the Dialogue phase. Iona's ordinary function
calls show an introduction, ask a question, and apply its result; accepting sets
the lantern flag and grants five gold once. Menu B opens/closes;
A selects Status/Save/Return. Startup allows New or Continue only after validating
the physical record. No inventory/quest VM/battle scaffolding is implemented.

Save version 1 is exactly 24 defined bytes with CRC, stable area/flag values and
little-endian numbers. It never saves structs or pointers. Invalid data is
rejected before state mutation. UI reports checked writes after physical readback.
Format migrations must be explicit; asset/source order must never assign IDs.
Ambient simulation is reconstructed on load: the persistent RNG continues, but
NPC positions/timers and the animation tick are intentionally transient. Future
rules requiring exact ambient continuation would need extra serialized fields.

## Future strategic combat boundary (M3)

Add an encounter-entry function that receives a stable encounter ID and captures
the exploration area, pixel return position and facing. Call a tactical screen
that owns its input/display loop and returns when finished; exploration stays
paused in its caller. Battle rules operate on a separate
small structure with grid-cell unit coordinates, turn state and explicit input
actions; they share character statistics, flags, RNG and stable entity IDs with
the durable world. Returning applies native outcomes and restores exploration.
Do not convert overworld actors into board cells or add tile-stepped exploration.

The platform adapter can reuse button abstraction, sprites, text and rectangle
services. Shared pure rules remain host-testable. Save-format changes must decide
whether saving is restricted to exploration safe points or serializes a bounded
battle state. Turn order, AP, range, damage and initiative are deliberately open.
Likely costs are unit occupancy, range/line-of-sight scratch, combat UI, extra
stack depth and audio interrupts. A lifetime-scoped union can eventually share NPC
scratch with battle scratch once their lifetimes are tested. Use the current
link-time static and stack reports when planning additions; measure each battle
path and avoid deep action/AI dispatch chains.

## Font preference

Use the SDK's BR5D variable-width font for game UI and the M0 proof. Its compact
glyph advances allow more text within the 128-pixel display; descenders and a
seven-pixel line height preserve readability. Dialogue measures chunks with the
runtime text renderer and reads its line height from the SDK font. There is no
copied font-metrics fixture or native test for authored dialogue fit.

## M1.6 essential character decisions

Stable IDs select PROGMEM definitions; quest flags derive canonical named placement
even when destination chunks are offscreen. Native generated chunk handlers only
request admission. Eight 18-byte live records preserve physical state across
repeated frames and chunk crossings. Visible incumbents are retained; requests
prioritize definition priority then ID, with explicit saturating overflow evidence.

Use authored tile-coordinate polyline routes with exporter-assigned named IDs and two-axis steering with bounded stuck
time. Finish dispatched courier travel logically when it leaves view; defer visible
inter-area guard relocation until offscreen. Retain transient live pixels only in RAM.
Four appended quest flags (relocation, arrival, absence and dispatch) fit the
existing save byte/version/24-byte record. Saving a dispatched journey completes
it logically at the destination on reconstruction.

Preserve terrain authority and the 17x9 cache. After sixteen push frames, check two
short player walkaround lanes. If either is clear, the NPC holds its position and
ordinary collision. Otherwise plan a side step and animate one pixel per update;
if no step is safe or it becomes blocked, allow only that player/NPC relationship
to overlap until separation. This guarantees progress without immediate jumping.
Story barriers use terrain/quest logic. Character definitions, placement, initial
behavior, dialogue and lifecycle handlers belong in `src/logic/npcs`. A single
native roster produces direct identity dispatch without callbacks stored in RAM.
The engine reports typed movement events and retirement reasons; it retains
generic movement, targeting and input consumption. Restore hooks resolve durable
quest state after pool reconstruction, including saves with no live actor.
No enemy, ambient spawning, general pathfinding or scripting systems are introduced.
Current measurements and exact commands are in
[m1_6_measurements.md](m1_6_measurements.md).

## 16x16 content reset (2026-10-18)

This decision supersedes the M1.5/M1.6 map/cache/art/content examples above.
Adopt supplied tiles16.png and actors16.png at native resolution. Use 8x4 tile
chunks, a 9x5 scrolling cache, 12x10 actor foot boxes and 16-pixel tile coordinates.
Reserve atlas cell 255 for TILE_OUTSIDE; retain black cells and row-major IDs.
Actor masks come from per-frame alpha, including opaque black silhouette pixels.
Use character rows and the 1,2,1,3 directional gait for both player and NPCs.

Replace all demonstration maps/quests with Nocturne Grove, its sign (0x41) and two
simple villagers. Keep generic staged handlers, NPC movement/routes, flags and
saves. SAVE_VERSION=4 deliberately rejects prior worlds instead of attempting to
reinterpret their pixels/flags. No schema migration is promised for this content
reset. The cache alone saves 108 RAM bytes; current link/emulator evidence is in
tiles16_measurements.md. Performance comparisons also reflect the smaller content
and different NPC workload, so they do not isolate the effect of tile size.

## Smaller foot boxes and actor depth (2026-10-18)

Use an 8x8 collision footprint at (4,8) within every 16x16 character sprite.
Derive bounds and center from its dimensions; player and NPC terrain/collision,
cache clipping and detour checks share the same constants. Keep the full 15-pixel
packed yield range because an offset eight-pixel contact can need that much space.
Aligned actors may now walk around each other within a single 16-pixel floor lane.

Draw player and visible NPCs in ascending Y through stable insertion sort of at
most nine byte slot indices. Equal-depth ties keep NPC slot order, then player.
This is bounded to 36 comparisons/shifts and nine scratch bytes without moving
the simulation pool or adding persistent state. Equal sprite heights make origin
Y equivalent to foot Y. Saves retain version 4 and the existing Game layout.

## Cache margin for NPC exits

The effective cache is 11x7 tiles: the 9x5 view span plus one margin tile on every side.
Its origin trails the clamped camera by one tile, and its far edge may extend past the map,
where the sentinel tile remains. A walking NPC simulates only inside the cache, so the
former 9x5 cache stalled NPCs at its edge while the player stood still. With the margin, an
NPC reaches the view edge first and leaves view by itself. The renderer reads the view from
inside the cache, and the host chunk trace holds nine chunks per window.

Measured in the device QA (30 FPS, 533k cycles per frame): `idle_grove` averages 236k cycles
before and 247k after, reloads grow from about 9.9k to 12.2k cycles, and RAM grows by 32 bytes.
Pixel and dense scrolling change by about one percent. The NPC QA shuttle now passes with the
player standing still. Current evidence is regenerated by `tools/collect_evidence.py`.

## NPC continuity and scene-scoped presence (2026-11-09)

Pops and teleports came from four places. Spawns and admissions were gated on visibility, so an
NPC appeared on screen without moving. An active NPC that left view was retired and later
re-admitted at its home or residence. Terrain repair moved an NPC up to a tile in one jump. A
cross-area destination removed a visible NPC. The sample wanderer showed the first case: it
walked off the left edge, was retired, and its chunk respawned it at home on the visible edge.

Options considered:

1. Park an NPC when it leaves view and re-admit it at the parked point. Rejected: it needs a
   stored position per NPC, and re-entry can collide with whatever now occupies the spot.
2. Keep view-gated spawns and hide the seam with a fade or mask. Rejected: a fade is still an
   appearance, and the view gate itself is the cause.
3. Simulate off-screen NPCs against full terrain. Rejected: terrain beyond the cache is unknown,
   and the cost would exceed the AVM budget.
4. Forbid tile edits under NPCs. Rejected for now: it couples terrain content to NPC positions and
   needs deferred writes. Escape steps handle the case instead.
5. Scene-scoped presence: every NPC of the area is cast on entry, a cast after entry happens only
   out of view, removal happens only at scene exit, and every position change is a one-pixel step.
   Chosen. The four properties are listed in `essential_npcs.md`.

Consequences. The dormant index, the pending mask, the view snapshot, and the request queue are
gone. `request_essential_npc` is removed, and the sample handler dropped its call. `NpcPool` is
156 bytes. `NPC_RETIRED_OFFSCREEN` and `NPC_RETIRED_TERRAIN` are gone. A home outside the cache at
entry is cast once the cache covers it, out of view. A cross-area destination leaves the NPC
standing until the scene ends. Lifecycle and detour helpers are kept out of line, so the AVM
stack budget still holds (guest bounds 183, 178 and 232 bytes against 256).

Known limits. NPCs do not path around obstacles, and there is no API for removing an NPC mid-scene.

Verification. `tests/test_npc_continuity.cpp` checks the four properties on every frame of 16,120
frames of random play with terrain edits under NPCs. A planted one-pixel-jump fault is caught. The
AVM probes pass (world 106 checks, NPC 4,264 checks, no failures).

## NPC detours around obstacles (2026-11-09)

A blocked NPC used to stand still until its target failed, or wait on the spot for a route
segment to retry. The sample villager stopped at a tree clump on its way to the east corner and
never arrived.

Choice. A blocked steering NPC waits 12 frames, then sidesteps around the obstacle, toward its
target across the way ahead when that side is clear, and rejoins the way ahead as soon as it is
clear again. A wanderer does the same on its heading, toward home. Every sidestep is one pixel,
so the continuity guarantees in `essential_npcs.md` still hold. The leash and the terrain cache are
limits, not obstacles: an NPC waits at them instead of detouring, so the cache edge does not make
NPCs jitter out of sight.

Alternatives considered:
- Route planning on a grid around the NPC. Rejected for now: the search needs stack and RAM the
  AVM budget does not have to spare, and the leash and cache already bound the problem.
- Wall-following without a side choice. Rejected: it cannot pick the side that leads toward the
  target, so it wanders.

Budget. The detour state packs into the record's former reserved byte, so `Npc` stays 18 bytes
and `NpcPool` stays 156. A sidestep is at most 15 pixels, and a leg allows three detours. That
bounds the loop of a target inside an obstacle, which fails after its third detour.

Verification. `tests/test_npc_detour.cpp` (1,815 checks): a villager goes around a tree after a
12-frame pause, a target inside a tree fails within its budget, a boxed-in NPC holds its place and
fails, and a wanderer goes under a tree within its leash. The continuity property test still
passes (5,447 moves in 16,120 frames). The AVM probes pass, and the NPC probe's stack bound is
243 bytes against 256.
