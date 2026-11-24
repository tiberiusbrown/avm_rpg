# Chunk world — 16x16 conversion

The world uses finite TMX areas with 16x16 terrain and characters. Immutable
map data stores one byte per tile in contiguous rows in PROGMEM, without padding.
Logic chunks cover 8x4 tiles (128x64 pixels) and determine handler dispatch only;
they never become RAM objects. An 11x7 effective tile cache covers the 9x5 view span
plus one margin tile on every side: 77 entries and two byte origins, 79 bytes total.
The margin lets walking NPCs leave view before the cache edge. It replaces the former
155-byte cache.

Area IDs, tile coordinates, chunk coordinates and logical dimensions remain
bytes; actor/camera coordinates remain 16-bit pixels. Maximum logical dimensions
remain 240x248 tiles. The largest chunk grid is 30x62: final chunk index 1859,
final stored byte offset 59519. Chunk indices and map offsets widen to uint16_t.
Outside-map, out-of-window and wrong-area reads use TILE_OUTSIDE=0xFF. Usable
atlas IDs are 0x00..0xFE; the final atlas cell is reserved and has no sprite frame.

`game.dimensions` caches the active logical width/height. Camera, movement, bounds,
cache loading, tile setters and save validation use those dimensions.
Camera centering accounts for a 16-pixel character. The loader fills the cache
with the sentinel, then computes the clipped row count, byte count and starting
pointers once. Each row uses one PROGMEM copy with source stride equal to map
width and destination stride equal to CACHE_WIDTH. All consumers read through
`cached_tile()`; there is no offscreen projection or arbitrary PROGMEM tile lookup API.

## Frame order

1. Reset fresh input, teleport state and interaction hint. Expose the previous
   movement's tile-entry information.
2. Enumerate live chunks in row-major order and finish STAGE_GAMEPLAY. Native
   handlers perform nonblocking changes.
3. Recompute the camera and reload all 77 cache entries.
4. Finish STAGE_TILES over all live chunks, clipping overrides to the cache.
5. Repair blocked player terrain; complete `STAGE_NPCS` over cached chunks, then
   update each active NPC once. Fresh A freezes ordinary simulation. NPC targeting
   can consume A.
6. Finish STAGE_DIALOGUE after the complete cache exists. Blocking dialogue
   uses local screen loops. Flag changes affect the next tile pass unless a
   handler explicitly refreshes the cache.
7. Move the player one pixel per update, checking every intermediate pixel
   against terrain and NPCs. Fresh A or a teleport pauses movement/time.
8. Advance the tick and render opaque terrain from the sheet of the current animation phase,
   then the wind gust and the ambient effects that lie on the terrain, then all alpha-masked actors
   in ascending foot Y, from the fixed camera/cache. Lower actors cover higher
   actors, including the player. The next frame follows the updated player.

The collision footprint is 8x8 pixels at sprite offset (4,8), inclusive
bounds (4..11,8..15): centered horizontally and flush with the sprite bottom.
Tile membership and facing interactions use foot center
(8,12). Overlap repair tries N/E/S/W at distances 1..16, then the authored safe
spawn. NPC yielding retains its packed nibble distance (at most 15 pixels),
enough to separate even offset 8x8 contacts; unsafe yields allow temporary passage until
separation. Cache clipping, bounds and camera edges are covered by host and AVM tests.

Actor rendering uses stable insertion sort over at most nine byte slot indices
(eight NPCs and the player). Only visible NPCs enter the list. Equal Y preserves
NPC slot order and puts the player last. Nine local scratch bytes and at most
36 comparisons/shifts avoid heap or persistent RAM and leave NPC simulation order
unchanged. All sprites have equal height, so sorting origin Y also sorts feet Y.

## Compile-time dispatch and singleton state

`src/game.cpp` owns one `Game game`. Helpers access it directly; no Game-reference
context is passed through engine/screen APIs. Generated `Handler<POS, STAGE>`
methods select ordinary `handler_impl` tag overloads with empty fallbacks.
Rectangles expand into independently owned tile calls. Full LTO verifies one
Game, no retained Game-reference helper arguments and no stored TilePos constants.

Python exports `AllMapHandlers` in `map_handler_catalog.h` and compile-time area dimensions
in `world_assets.h`. `src/chunk_dispatch.cpp` combines these with the automatically enumerated
NPC policies. C++20 folds filter sites by chunk; `if constexpr` retains only occupied function
specializations. Each area/stage has a dense immutable ROM function-pointer table; lookup
validates byte coordinates and uses a 16-bit flattened index, including index 1859.
Generated fallback handlers return a marker, while authored overloads return void. Dependent
call traits omit no-op stages without a separately authored stage mask. Authored ordering and
teleport short-circuiting are preserved. Tile and NPC stages always finish destination work.

Named rectangles additionally export query-only TileRegion metadata. `set_tile`
validates logical area/bounds and clips against the resident cache. A scrolling
viewport touches at most nine chunks. Gameplay derives its window from the current
player camera. Tile, NPC, and dialogue stages use the loaded cache origin. Teleports stop outgoing gameplay/dialogue,
clear the NPC pool and rebuild destination tiles and the full NPC stage; destination gameplay begins
on the next update. Save/load uses the same cache-based terrain repair.

The window's chunks do not change from frame to frame, so each stage does not walk them. The
`ChunkPlan` in the runtime workspace lists, per stage and in chunk order, the window's chunks that
have a handler. It is keyed by area and cache origin, and `plan_window` rebuilds it when the key
changes: one read of the block's four occupancy masks (compile-time, per chunk, covering that chunk
and the eight after it) and one read of the matching ordered list per occupied stage. A window
reaches a third chunk column from its sixth tile of a chunk and a third row from its third, which
selects the masks that apply. A stage then calls `functions[first + offset]` for each listed chunk,
so the order, teleport short-circuiting and handler tables are unchanged. The lists hold at most
`CHUNK_PLAN_CAPACITY` (6) chunks per stage, checked at compile time against every window of every
area. Native builds still record the requested window's chunks, empty ones included, for tests.

## Encounter and save boundary

Enemy catalog sites join essential NPC policy sites in the compile-time NPC stage. Active
actors and the player's movement can report hostile contact. Exploration handles the lowest
contacting identity after the full world update returns. The battle screen gives back the
result while the display is black. Victory records defeat and retires the actor before
the world cache is rebuilt and faded in. Defeat ends exploration and returns through the
outer loop to the title.

Save version 21 stores the live pointer-free `Game` in `.saved`. It includes defeated-enemy
flags and a 16-bit countdown. Older records are rejected. The tile window
and camera are not saved: they live in the runtime workspace (`src/runtime_workspace.h`) and
are rebuilt from the world and flags. Loads validate version, stats, party, flags and bounds
before placing the player. Ticks, active slots, assigned-site snapshots and candidate scratch
are rebuilt through ordinary scene-entry processing. Blocked player positions use cache-based
overlap repair.
