# Technical decisions

## M1.5 world architecture

Current world behavior is specified in [chunk_world.md](chunk_world.md) and
authoring/build behavior in [tiled_pipeline.md](tiled_pipeline.md). Tiled is the
map authority; generated native C++ calls associate each footprint with all
intersecting chunks. Moving an anchor moves its constants and dispatch without
handwritten ownership changes. No custom interpreter or runtime registry exists.

The sole resident tile cache is 17x9 bytes with byte origins, fully reloaded every
simulation frame. Chunks remain in PROGMEM. Byte tile/chunk/map dimensions enforce
the 240x248 limit, while pixels and flattened indices use uint16_t. Logical
bounds clip padded storage and camera edges. Active area dimensions are cached in
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

NPC content and runtime are deferred to a separate update. Ordinary chunk
handlers retain the lantern choice, gate/bridge controls and paired teleports.
Map objects carry optional names and native handler references, with no types.
All coordinates are TilePos values. C++17 reference template parameters bind
each tile; named rectangles also retain optional contains-query metadata.
Unnamed handlers use private constexpr positions with no public location names. Walking-entry, facing-A and
teleport helpers replace exported portal routing and interaction registries. Point markers
are centered; tile regions include all tiles partially overlapped by a rectangle.
Generated area IDs use AREA_. Chunk origins trust validated exporter output.

Indexed quest flags use a bitset sized by FLAG_COUNT, with getter/setter/clearer/
toggler methods; player region checks share Game::player_is_in_region. Save
version 3 serializes the count and packed bytes and migrates validated v1/v2
records, dropping the retired NPC flag. The tile cache and timer are never saved.
See current [measurements](m1_5_measurements.md) for resource proofs and margins.

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

Positions are unsigned 16-bit pixels; player speed is 2 px per 50 ms step
(40 px/s), NPC speed 1 px per walking step. Movement tests every intermediate
pixel, including all terrain tiles touched by a 6×5-foot collision box at
sprite offset (1,3). It cannot tunnel even if a future action requests a large
step. Opposing directions cancel per axis; if both remaining axes are held,
vertical takes precedence. There is no diagonal speed boost. Facing changes
when blocked; walking animation occurs only on successful displacement.

Use SDK `avm_set_frame_rate(20)` and `avm_next_frame`, rather than an additional
timer. Motion is independent of workload while frames meet the 50 ms deadline.
There is no catch-up after an overloaded frame; blocking safe-point operations
may stall simulation. The measured M1 workload, including 12 NPCs, meets the
deadline, so additional timing infrastructure is unwarranted.

## Camera, content and actors

The outdoor is 256×160 px; the interior is 128×80. Camera follows at approximately
(60,28), clamps to world edges, and renders partial tiles with signed coordinates
and AVM sprite clipping. Background assets are 8×8 program sprites. Empty paths
and interior floors are skipped on the cleared buffer. NPCs draw a small overhead
bar to distinguish them from the player. Simple actor animation uses two leg
phases per facing. No caching/compression/dirty rectangles were needed to pass
20 FPS; decorative detail can be added only with new measurements.

Active capacity is 12 actors, eight guest bytes each. Normal gameplay uses three
outdoor walkers and one stationary interior caretaker. The stress build activates
12 outdoor walkers. Walkers choose facing/wait/walk on 12..35-step timers, then
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
font, and return to their caller. A advances or confirms; B dismisses or declines.
Each prompt requires a fresh press. Five-byte text scratch lives in shallow
helpers that return before input waits; no page buffer or glyph table is stored.
Facing plus proximity identifies an NPC on an A edge. Iona's ordinary function
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
