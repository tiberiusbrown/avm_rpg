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
returning resumes the caller. Selections, animation position and feedback are local variables.
The title has no menu: it animates a scene (`src/title.h`, stateless rules of a local frame
counter) under the name Venlor, while Call of Venlor (`MUSIC_TITLE`) plays, until A or B, then
continues the saved game if `save_exists` is set and starts a new game otherwise. The area's song
fades in over the title's. A new game over an existing save is not offered.
The pause panel eases halfway in from the left each frame while the world fades to 50% black;
closing reverses both effects. Up/down selects Resume, Save, Party, Journal, Map, Audio or Load. Map is
visible only in the overworld; Load uses the global `save_exists` flag. Party calls status;
Journal and Map are placeholders. A selects and B resumes. Load eases the panel
out while fading fully to black, then returns to exploration to restore and refresh the
saved world while hidden, keeping that work off the menu's stack. The restored world fades
completely back in before simulation resumes.
Audio returns from the pause loop after its panel eases off the left edge, then enters
its own loop with an 88-pixel panel easing in from the right. Both use the battle cursor's
half-distance easing, with a steady half-faded world during handoff. Music and SFX each
have independent 0..30 volume sliders displayed as 0–200%, mapped to native attenuation
0..255. Both default to level 15 (100%, native 128), with shared master gain at its maximum
255. The stored levels retain their prior percentage meaning. Low-pass has
16 strengths, from bypass to coefficient 1. Up/down selects; left/right adjusts and repeats
while held. B closes Audio directly to gameplay, easing the world back to full brightness.
The three levels live in `Game::options` and are saved with progress. Startup applies valid
saved values or defaults; a new game and later loads preserve the current values. Finishing
an outstanding song fade before a music-volume edit prevents its saved gain undoing the edit.

Exploration and battle share `menu_screen`, item filtering and the panel renderer. Battle
passes its existing local `BattleScreen` for the frozen background; there is no global battle
state or separate menu loop. On player turns, B cancels a selected unit or opens the menu
when nothing is selected. Battle items are Resume, Resign, Audio and Pass; Load
remains exclusive to exploration. Pass and Resign open centered black/white/black confirmation
panels over the paused menu. A requires a fresh press after a 12-frame (240 ms) guard;
early presses are consumed, holding A never accepts, and B cancels immediately. The existing
menu loop holds the local confirmation countdown, without nesting another screen loop.
Confirmed Resign sets the existing defeat result and ends the battle without consuming an
activation or changing unit HP. The normal result screen, defeat sound and exit transition
run before returning defeat to the caller. Confirmed Pass consumes one activation and damages
every living teammate by 1, regardless of whether it is voluntary or forced. Paused-board rendering omits
selection-target enumeration to keep the deeper menu path within the guest stack budget.
Story handlers still receive only victory or defeat and need no menu-result handling.
Every AI search ply also lists a voluntary Pass for its acting side. The unused packed
self-target value represents Pass without changing unit-action enumeration or destination
markers. The shared action stack grows from 80 to 82 bytes so two plies of four rogues per
side still fit, including each side's pass. A completed Pass result is accepted by the enemy
controller; it is distinct from an incomplete search by the returned depth.

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
A activates the selected pause item. Startup loads and validates the physical record to set
`save_exists`, then prepares new-game defaults without clearing that flag. Save writes once
and sets the flag immediately, without reading the record back. Menu visibility uses the
flag without further storage access.

Save version 7 places the entire pointer-free `Game` object in `.saved`; the live state is
the save storage, so no duplicate static buffer or serialization step is required. A layout
version is part of `Game`; changes to its byte layout require a version bump. There is no
checksum. Load checks the version, stats, flags, bounds and NPC residences before entering
the world, then reconstructs transient NPC/cache state and repairs blocked player positions.
A rejected load clears the flag and prepares a new game. Existing version-1–6 records are
incompatible. The global flag is ordinary `.data` and is never persisted.

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

## Animated rendering optimization (2026-11-13)

Select the terrain tileset and animation phase before entering the tile loop. Template
specializations make each phase's bounds and frame offset constants, and phase zero omits the
animation test. The ambient pass computes its camera/cache view and flash class-table pointer
once, then shares them with its slots. Birds outside the horizontal view return before their
second hash. Animation formulas, drawing order, assets and gameplay stay unchanged.

Compile-time lookup tables add 896 flash bytes for visible gust runs and 128 bytes for
transposed collision quadrants. Each gust plan is copied as one small block before drawing;
the collision probe reads runs of pixels from a row or column instead of testing each pixel.
Slot-index specialization folds constant seed arithmetic. Keeping generated chunk handlers
out of line avoids cold handler locals on empty chunk visits, and complete cache windows skip
the sentinel clear before copying every cell. Seed-table and chunk-dispatch LUT trials
increased cycles, so only the small gust and collision tables remain.

Measured on the normal and FXC packages with identical initial state and five consecutive
256-frame input sequences. The last sequence crosses the 16-bit animation tick wrap. Values
are average emulated cycles per frame, including audio interrupt costs:

| Workload | Frame before | Frame after | Render/display before | Render/display after |
|---|---:|---:|---:|---:|
| Idle | 338,264 | 296,360 | 233,555 | 195,170 |
| Scroll down | 336,047 | 293,724 | 222,560 | 184,954 |
| Scroll right | 337,919 | 271,968 | 215,768 | 176,385 |
| Scroll up | 319,366 | 279,821 | 220,529 | 183,970 |
| Idle across tick wrap | 304,274 | 264,135 | 222,116 | 184,290 |

Across these equally sized workloads, total frame work falls 14.1% and render/display work
falls 17.0%. The LUT/specialization follow-up cuts total work another 5.5% relative to the
initial optimization pass. Frames above 320,000 cycles fall from 777 to 48 out of 1,280.
The worst observed frame falls from 402,358 to 368,356 cycles (25.15 to 23.02 ms); the strict 20 ms full-device
budget still fails. Profiling measurements retain these misses instead of relaxing that test.
The shipping ELF uses the same 477 static RAM bytes and has a complete 243-byte maximum stack
bound against 256 bytes. Text grows by 3,210 bytes and read-only data by 1,024 bytes.

Verification compares all ordered drawing commands for 1,665,024 synthetic frames, covering
both tilesets, every tile ID and camera pixel alignment, chunk boundaries, all tick values at
eight locations, ambient effects and actor layering. All 857,148,438 command bytes match.
Full-device hashes of the Game and OLED RAM frame sequences also match in both packages.
Source profiles, ELF/resource snapshots, comparison harnesses and frame measurements
are retained under ignored `build/animation-optimization/`.

QA waits for completed frame markers and the following serial update, rather than polling a
16-bit serial that the guest writes one byte at a time. At a carry, serial polling could count
the same frame twice; returning before the update could also expose the previous serial.
The SDK pause check samples frozen ticks after the pause input has been consumed, allowing
an in-flight exploration frame to finish first. The world and NPC guest probes pass (18,538
and 4,264 checks), as do all 17 native/content tests. The native world checks include random
collision masks, map edges, quadrant boundaries and coordinate wrapping. Full-device QA passes
the Audio, pause and initial battle UI checks before stopping at the strict exploration frame budget.

## Environment animation design (2026-11-14)

Goal: atmospheric animation wherever the player goes, with no static RAM and a bounded
per-frame cost. `src/ambient.h` holds every rule as a constant expression of the game tick and a
world position, so nothing is stored or saved and a frozen tick shows a frozen scene.

- **Tile phases use one sheet per phase.** The first version kept the phase frames of a span of
  tiles in the tileset's sheet and tested every drawn tile against that span (specialized per
  tileset and phase in the pass above). The test still cost about 13,000 cycles a frame. Each
  phase now has a whole sheet and the tile loop is the pre-animation loop: 66,600 cycles
  against 65,500 measured before animation. The price is read-only data: three 8,162-byte
  sheets per tileset, 48,972 bytes for the two tilesets in use. A phase with no art shares an
  earlier sheet.
- **One ambient slot per chunk, found by a short loop.** Three slots per chunk with two hash
  rounds each cost about 54,000 cycles a frame. Each chunk in view now has one slot whose pick
  is one add, one shift, one multiply and a fold, with the seed stepped from chunk to chunk.
  Only a slot whose tile is in view calls the out-of-line function that reads the tile's class
  and computes the effect.
- **One bird lane per frame.** Lanes are bands at the top of every 128 pixels of world height,
  so a 64-pixel view shows at most one, and the test for a bird in view is one multiply and a
  few adds before any hash.
- **Gusts are one tile wide and slow.** A line redraws at most five tiles in a frame. The
  renderer rejects a frame with no gust in view from the camera's tile alone, before the call.
- **Organic motion.** Fliers follow two slow waves with per-instance speeds and phases under an
  envelope that pins both ends of the flight to the tile, plus a small quick orbit (the flutter
  of a butterfly, the spiral of a moth, the shiver of a bee). The waves are parabolic arcs, not
  triangles, so turns ease. Birds flap and climb, then glide and sink, and followers repeat
  the leader's rise and fall a little later. A bird that visits a perch arrives as a distant
  speck on a swooping path, so nothing pops into view.
- **Stack.** The render passes are separate out-of-line functions so that only one pass's
  locals are live, and the status screen draws only terrain and actors under its panel. The
  bound is 243 bytes, unchanged by this round.

Measured with full-device QA on the QA guest (average and worst cycles per frame, budget
320,000; "before" is the build without animation):

| Workload | Frame before | Frame now | Render and display before | Render and display now |
|---|---:|---:|---:|---:|
| Idle grove | 207,701 / 236,442 | 246,312 / 278,634 | 105,047 | 147,554 |
| Pixel scroll | 211,580 / 227,802 | 254,830 / 281,052 | 105,294 | 151,988 |
| Dense collision | 210,534 / 221,368 | 250,616 / 263,116 | 101,421 | 145,449 |

Full-device QA passes in the normal and FX-C packages. The remaining cost is mostly drawn
sprites, not selection: a 16x16 plus-mask sprite costs 5,000 to 8,000 cycles, the same as an
actor, and the measured scenes show one or two ambient sprites and often a flock of two or
three birds. The idle ambient loop is about 7,000 cycles and the idle gust and bird tests
under 2,000. The clearest remaining saving is an 8x8 sheet for the small creatures.

Resources of the shipping guest against the build without animation: static RAM 477 bytes
(unchanged), stack bound 243 (was 240), code 37,857 bytes (was 34,764), read-only data 178,827
bytes (was 123,901).

## Further animation optimization (2026-11-14)

This pass starts from `ccfb2ad` with the new phase sheets, wandering flights and bird
flocks already present. It preserves the animation rules, drawing commands, gameplay,
capacities and static RAM. Builds use RelWithDebInfo and full LTO.

The source profiles and guest disassemblies led to these changes:

- Pass the three chunk-coordinate bytes separately inside generated dispatch. Keep the
  `ChunkCoord` overload as an inline interface. This removes aggregate packing, unpacking,
  saved registers and the dispatcher epilogue; handler calls become tail jumps. Stored
  coordinates and code pointers retain their existing representations.
- Filter NPC interaction geometry before fetching the NPC definition from flash. Compute
  facing axes once per interaction pass, preserving range boundaries and nearest/tie rules.
- Clamp the camera once when reloading the game cache. Retain clamping for the public
  arbitrary-window helper. Unroll the common complete-window copy; partial map windows
  retain their sentinel fill and bounded copy.
- Keep partial-mask collision work outside the common empty/full-tile path. A constexpr
  23 by 23 table stores the triangles intersected by every prefix, suffix and full foot
  span. Direct tile-relative offsets select a 16-bit entry without clipping or a row scan.
  The table occupies 1,058 flash bytes. Leading-edge collision probes retain their existing
  row/column tables and behavior.
- Precompute the 256 bird poses as packed vertical offset/frame words, using 512 flash
  bytes. This removes pose arithmetic and lowers the bird pass's local stack use.

A general sine LUT, scaled leaf-wave LUT, manually shifted signed flight divisions,
outlined packed shader returns, packed ambient positions, broader generated handler
shortcuts and redundant-gust calculations were also measured. They increased cost,
regressed some workloads or saved too little to justify the change. In particular, the
first collision LUT formulation increased hot-function spills. The retained formulation
indexes the table directly inside the cold helper and improves both ordinary movement
and repeated shaped collisions.

Timing uses completed QA frame markers and a host instruction-boundary observer, with
no additional guest markers or RAM. Every frame's component cycle counts must sum exactly
to its measured frame work. Native drawing, display transfer and audio interrupt costs are
included; waiting for the next 50 Hz frame boundary is excluded. An independent eight-second
shipping-guest replay profile (after one second of warmup) identifies source hotspots.
Before/after shipping disassemblies and targeted function disassemblies are retained.

Each workload below has 1,024 frames, covering a complete gust period. The first eight are
representative workloads weighted equally in the headline average. Scrolling sequences run
consecutively; the tick-wrap sequence starts at tick 65,400. The field and interior sequences
reset to the same archived initial state and generated map markers. The last row is a
separate forced stress test: reset the player beside an authored shaped tree before every
frame and hold east. This repeatedly exercises collision resolution rather than allowing
the player to walk away. Results are emulated milliseconds at 16 MHz:

| Workload | Mean before | Mean after | Worst before | Worst after |
|---|---:|---:|---:|---:|
| Idle grove | 15.120 | 14.383 | 17.765 | 16.963 |
| Scroll down | 15.674 | 14.616 | 18.750 | 17.863 |
| Scroll right | 14.519 | 13.634 | 16.130 | 15.277 |
| Scroll up | 14.047 | 13.239 | 15.999 | 15.110 |
| Tick wrap | 12.555 | 11.948 | 14.608 | 13.994 |
| Remote field | 13.381 | 12.615 | 15.119 | 14.381 |
| Interior idle | 11.610 | 11.197 | 12.510 | 12.088 |
| Interior scroll | 13.384 | 12.740 | 14.018 | 13.370 |
| Forced shaped collision | 21.085 | 19.349 | 23.233 | 21.550 |

Average frame work over the eight representative workloads falls from **13.7864 to
13.0466 ms (5.37%)**, saving 0.7398 ms. Their worst observed frame falls from 18.750 to
17.863 ms, and both versions have zero frames over 20 ms in these workloads. The
start-to-start period remains 19.9999 ms: the game still runs at its 50 Hz cap.
The forced collision workload improves 8.23%; frames above 20 ms fall from **987 to 165
out of 1,024**, so that stress workload still has budget misses. Normal and FXC packages
were measured independently and produce the same rounded timings and miss counts.

Average frame-time breakdown for the eight representative workloads, in milliseconds:

| Component | Before | After |
|---|---:|---:|
| Other update work | 0.3739 | 0.3741 |
| Tile cache reload | 0.7484 | 0.6090 |
| Chunk handler stages | 2.4562 | 2.0138 |
| Movement, collision and NPCs | 2.1389 | 1.9836 |
| Terrain drawing | 4.0957 | 4.0993 |
| Gusts | 0.1395 | 0.1397 |
| Ambient tile effects | 1.4151 | 1.4171 |
| Actors | 0.5832 | 0.5832 |
| Birds | 0.1947 | 0.1877 |
| Other rendering | 0.2081 | 0.2088 |
| OLED display | 1.4328 | 1.4303 |
| **Total** | **13.7864** | **13.0466** |

Remaining cost is concentrated in the native sprite drawing and display services.
The shipping profile's terrain pass remains approximately 65,000 cycles per frame.
Gameplay chunk dispatch falls from approximately 12,200 to 7,300 cycles per frame,
cache refresh from 14,700 to 11,900, and the common player terrain check from 8,800 to
6,900. These profile figures use the replay's workload mix; the table above uses the
fixed frame workloads instead.

Shipping resources: static RAM **477 to 477 bytes**, complete maximum stack bound **243 to
241 bytes**, text **37,857 to 37,556 bytes**, read-only data **178,827 to 180,397 bytes**.
The lookup tables add 1,570 read-only bytes while text shrinks by 301 bytes. Full LTO
checks retain one global Game, no game-reference helper arguments and no map tile-position
storage. Both playable root packages are updated.

Verification includes all 17 native/content tests, 1,231,526 world checks, 9,227,197
ambient checks and 9,686 NPC dialogue checks. The world checks cover every individual
triangle against all 529 foot windows, random combined masks, map edges and coordinate
wrapping. All ordered render commands match over **1,665,024 synthetic frames and
850,874,601 command bytes**, including both tilesets, all tile IDs, camera alignments,
all tick values at eight locations, ambient effects and actor layering. Game and OLED
sequence hashes match across all nine 1,024-frame workloads in both normal and FXC
packages. Full-device SDK integration also checks world/NPC probes, pause, sign dialogue,
menus, battles, save/restart and scrolling without relaxing its frame-budget assertions.

The archived ELFs, resources, source profiles, disassemblies, observer source and fixtures,
raw frame breakdowns, exact command comparison and `comparison.json`/`comparison.md` are
under ignored `build/exhaustive-optimization/`. Baseline measurements use
`baseline-shipping/`; final measurements use `direct-foot-lut/`. The frame observer can be
rerun with `python3 build/exhaustive-optimization/measure.py LABEL --frames 1024`, adding
`--mode fxc` for FXC. `summarize.py` reconciles components and checks sequence hashes before
writing the comparison.
