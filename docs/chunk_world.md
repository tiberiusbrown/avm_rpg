# Chunk world (M1.5)

The world is authored in finite orthogonal TMX maps with external TSX tilesets.
Tiles are 8x8 pixels. A storage/logic chunk is 16x8 tiles (128x64 pixels).
Chunks are never loaded as RAM objects. All immutable tiles,
collision definitions and sprite data use AVM program address space one.

Each area has independent local coordinates and an explicit stable byte ID.
The maximum logical map is **240x248 tiles**, or 15x31 chunks. Smaller maps pad
to chunk boundaries with tile 255, an impassable, non-portal, undrawn sentinel.
Logical bounds remain separate from padding. For example, the 32x20 prototype
occupies 2x3 storage chunks; its last four stored rows are sentinel tiles.

`TilePos`, `ChunkCoord`, `MapDimensions`, `TileRegion` and cache origins
store byte coordinates/dimensions. `Actor`, `PixelPos` and camera store 16-bit pixels. Header assertions compile on both the
host and AVM. Chunk indices and byte offsets are uint16_t: chunk 464 and its
last tile offset 59519 are valid. Intermediate differences are signed; region
ends, camera extents and address calculations widen before checking/narrowing.
There is no 256-valued dimension or byte-truncated flattened index.

`game.dimensions` caches the active area's width and height in two existing bytes.
Area entry and cross-area teleports populate it before processing the destination;
same-area teleports reuse it. Camera clamping, chunk enumeration, cache loading,
tile setters, player membership, interactions and collision use these RAM values.
`MapDimensions::pixel_bounds()` shares the sprite-origin bounds check. Save
validation reads the candidate area's dimensions before changing active state,
then area entry populates the cache after a valid load. Dimensions are derived
from assets and are not serialized.

## Exact simulation and render order

1. Reset global `frame_pressed`, `teleported` and the interaction hint. Expose
   the previous movement's tile-entry edge and this frame's fresh A edge.
2. Compute a clamped camera and enumerate its logical chunks in row-major order
   (Y, then X). Run `run_chunk<STAGE_GAMEPLAY>()` for every live chunk. Each generated
   per-tile call selects its gameplay tag overload or empty default. Nonblocking
   flag/reward/input logic finishes before any terrain is reconstructed.
3. Compute the render camera from the resulting player position and rebuild
   **all 153 bytes** of the 17x9 cache from baseline PROGMEM. Every row is filled
   with the sentinel, then contiguous program-copy spans split at chunk boundaries.
   The camera remains fixed through movement and rendering unless overlap repair
   falls back to a distant spawn.
4. Enumerate the resulting live chunks and run `run_chunk<STAGE_TILES>()`. The same
   generated handler list invokes tile overloads/defaults, reading gameplay flags
   and the current tick directly from global game. Tile overrides must not change
   flags, time, input, rewards or player state. Gameplay flag changes affect all
   relevant cached tiles in this same update; there is no frozen WorldState.
5. Check the player's cached foot-box terrain. If blocked, try clear positions
   at distances 1..8 pixels, N/E/S/W. An enclosed player returns to the authored
   safe spawn and rebuilds the cache using only the tile stage.
6. Run `run_chunk<STAGE_DIALOGUE>()` over every live chunk. All tile chunks have
   completed before any blocking prompt runs, so dialogue rendering sees a
   complete cache. Dialogue flag changes affect terrain on the next tile pass;
   call `refresh_tile_cache()` explicitly if an immediate refresh is needed.
7. Move the player, checking every intermediate pixel of the 6x5 foot box at
   sprite offset (1,3). A teleport or fresh A interaction pauses movement.
8. Advance the animation tick except during an A interaction. Render background
   only from the cache, then the player with the fixed camera, and display/clear
   the framebuffer. The following update follows the new player position.

The exporter emits `Handler<TilePos const& POS, HandlerStage STAGE>` with forwarding
methods for each authored handler name. `HandlerStage` has STAGE_GAMEPLAY,
STAGE_TILES and STAGE_DIALOGUE. Generated `handler_impl::Tag<POS, STAGE>` types and
Gameplay/Tiles/Dialogue aliases select ordinary handwritten overloads. Generated
generic functions supply empty defaults for missing implementations, including
newly added handler names. Objects sort by name, then unnamed authoring order;
expanded cells remain row-major. All three stages use this exact list. Overlapping objects run
in that order; combine their rules when shared precedence or guards are needed.

`teleport_player<TARGET>(facing)` updates player/area state and sets global
`teleported`. Gameplay and dialogue dispatch check it after each handler and stop
all remaining outgoing calls. The tile stage ignores it and builds the complete
destination view without running destination gameplay or dialogue. A dialogue
teleport also refreshes the destination cache before returning. The next update
resets the flag and runs ordinary destination logic. Same-area teleports use this path.
Loading or teleporting onto a trigger cannot retrigger it without walking out
and back in. No frame-event pointer or null-pointer guard exists.

Area entry/new/load initialize the previous foot tile, so entry is not walking.
Ordinary A interaction runs gameplay, tiles, then dialogue with movement/time paused.
Procedural dialogue loops block outside exploration timing markers; menu/status/
dialogue rendering never invokes handlers. `frame_pressed` and `teleported` are
transient globals outside Game and its serialized save fields.

## Baseline, effective tiles and persistent facts

Only the cache loader reads authored tile data from PROGMEM. Game terrain
consumers use `cached_tile()` and `terrain_clear()`; the latter drives player
movement. Wrong-area and out-of-window reads return the solid TILE_OUTSIDE
sentinel. There is no arbitrary-position tile lookup or projection fallback. Portal entry is tested by native chunk logic; effective collision still
controls whether the player can walk onto a trigger. Collision/portal predicates
are generated from TSX metadata, including sentinel rejection.

`set_tile<POS>` specializes area-local coordinates, validates logical
bounds and clips unsigned cache-relative coordinates. Negative differences wrap
to large uint16_t values, so one upper-bound comparison per axis rejects them.
There is no rectangle-fill operation or offset API. Each handler writes exactly
its bound tile.

Tile setters write the global cache directly and do not know the calling
chunk. They validate area/logical bounds and clip cache-relative coordinates.
There is no context object, null-cache mode, query position or result byte.

The cache is ephemeral and never saved. Lantern/gate/bridge/waystone flags are
persistent facts; native functions project those facts into each new baseline.
The closed gate handler only writes an open path when its flag is set. Clearing
the flag needs no restoration branch. The tidal platform projects path for ticks
0..15 of each 32-step period and otherwise leaves baseline water untouched.

## Global gameplay state and three-stage handlers

`src/game.cpp` defines one zero-initialized `Game game` for native and AVM builds;
`rpg.h` declares it. Gameplay, screens, rendering and saves use that object.
TileContext, ChunkContext, FrameEvents and WorldState have been removed. The
stage template parameter selects tag overloads for gameplay, cache writes or
dialogue, so no snapshot or context plumbing is needed. Tile overloads read
`game.flags` and `game.ticks` after gameplay has finished. Dialogue starts only
after every live chunk's tiles are complete. `process_all_chunks<STAGE>()`
performs one complete stage pass; `refresh_tile_cache()` reloads the baseline and
runs the complete tile pass without replaying gameplay or dialogue.

Native tests reset the singleton with `game_new()` and keep host-only value
snapshots for deterministic comparisons. There is no multi-instance gameplay
API. Saves serialize explicit fields, so their version/layout is unchanged.

Save decoding checks the record format, CRC, fields and map pixel bounds before
changing `game`. Once committed, area entry builds the destination cache from the
loaded flags and tick zero. A position on solid cached terrain is relocated using
N/E/S/W candidates at distances 1..8 pixels, with the authored spawn as fallback.
A spawn fallback rebuilds its view without replaying gameplay. Save loading has no
separate candidate-terrain evaluation or offscreen tile lookup.

## Per-tile compile-time dispatch

The exporter expands every rectangle into individual covered tiles and assigns
each tile only to its own chunk. Generated calls instantiate native templates,
such as Handler<positions::EasternBridge_0, STAGE>{}.update_bridge(). There is no region
ownership calculation, repeated rectangle call, runtime point table or dedup set.
A 3x2 bridge produces six independent template instantiations across four chunks. Every
cell writes only its own cached tile. A partial view processes only the points
belonging to its live chunks; all writes are clipped to the resident window.

All objects use the same format, with optional name and handler. The exporter
adds a generic empty function and a forwarding member for each handler name.
Handwritten overloads may target a stage for every position, a position for every
stage, or an exact position/stage pair. A single generic bridge overload is reused
for all six cells; coordinates and stages remain compile-time arguments. Named tile constants are public; unnamed handlers use private
constexpr constants in handler_defaults.h. Full LTO removes empty defaults,
Handler temporaries, position objects and coordinate arguments.

Named rectangle metadata remains available as NameRegion with constexpr contains.
Game::player_is_in_region supports optional gameplay queries, but chunk dispatch
and tile mutation have no region API. Waystone uses exact per-tile membership,
with one persistent flag guarding both cells' reward calls.

## Regions, flags and save compatibility

`Game::player_is_in_region(TileRegion)` checks the current area, a valid nonempty
logical region and the player's foot-center tile `(x+4,y+6)/8`. Right and bottom
edges are exclusive. The waystone and tile-entry helper share this membership rule. Facing interactions
check exactly one adjacent tile. The position-templated helpers player_is_walking_onto<POS>,
player_just_interacted<POS> and teleport_player<TARGET> are free helpers in chunk.h. The A edge is
consumed at the first matching call, with no persistent object identity. NPC
content, runtime state, collisions and stress targets are deferred.

Flags are explicit stable bit indices declared in `src/flags.h`; append flags
before `FLAG_COUNT`. `FlagSet<FLAG_COUNT>` uses exactly ceil(FLAG_COUNT/8) bytes,
with no 16-bit mask limit. Game provides `get_flag`, `set_flag`, `clear_flag` and
`toggle_flag`. Invalid indices read false and leave storage unchanged. Native tests cover 37 and 257
flags, and the AVM probe exercises bits 16 and 36.

Version 3 stores flag count and the packed bytes explicitly. All numbers are
little-endian. CRC-16/CCITT-FALSE covers the record before its trailing checksum.
The layout is magic[2], version[1], length[1], area[1], facing[1], x[2], y[2],
hp[1], max_hp[1], level[1], reserved-zero[1], gold[2], rng[2], flag_count[2],
flag bytes, zero padding, CRC[2]. Record size is max(24, 22+flag_bytes).
The current four flags need one byte and the record remains 24 bytes.

The decoder accepts valid version-1 and version-2 24-byte records. It maps the
old lantern/gate/bridge/waystone masks to indexed flags and discards the retired
NPC flag. Older version-3 flag counts zero-extend; future counts, unknown versions,
undefined high bits, nonzero padding, invalid fields and CRC failures reject
without mutating Game. Changing the physical `.saved` size in a future release
also requires a platform-level migration because `avm_load` checks record length.

Saved positions must be inside the logical map. Loading builds the cache, then
repairs overlap with solid terrain. An open-gate save preserves its position;
the same position with its gate flag cleared loads at a nearby clear position
or the safe spawn. Structurally invalid saves still reject without mutation.
Cache and animation time are not serialized. Transient time restarts at zero,
giving periodic terrain a deterministic load phase.

The machine contracts in AVM `docs/arch.md`, `docs/stack-analysis.md` and
`docs/profiling.md` are unchanged. See `m1_5_measurements.md` for actual static
RAM, complete stack bounds and emulator cycle evidence.
