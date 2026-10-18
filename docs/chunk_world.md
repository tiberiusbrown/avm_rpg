# Chunk world — 16x16 conversion

The world uses finite TMX areas with 16x16 terrain and characters. Each immutable
storage/logic chunk covers 8x4 tiles (128x64 pixels), with 32 byte tile IDs in
PROGMEM. Chunks never become RAM objects. A 9x5 effective tile cache covers the
display plus a partial edge row and column: 45 entries and two byte origins,
47 bytes total, replacing the former 155-byte cache.

Area IDs, tile coordinates, chunk coordinates and logical dimensions remain
bytes; actor/camera coordinates remain 16-bit pixels. Maximum logical dimensions
remain 240x248 tiles. The largest chunk grid is 30x62: final chunk index 1859,
final stored byte offset 59519. Chunk indices and offsets widen to uint16_t.
Map padding, out-of-window and wrong-area reads use TILE_OUTSIDE=0xFF. Usable
atlas IDs are 0x00..0xFE; the final atlas cell is reserved and has no sprite frame.

`game.dimensions` caches the active logical width/height. Camera, movement, bounds,
cache loading, tile setters and save validation use those dimensions rather than
padding. Camera centering accounts for a 16-pixel character. The loader fills
every cache row with the sentinel, then copies contiguous PROGMEM spans split
at 8-tile chunk boundaries. All consumers read through `cached_tile()`; there
is no offscreen projection or arbitrary PROGMEM tile lookup API.

## Frame order

1. Reset fresh input, teleport state, interaction hint and NPC requests. Expose
   the previous movement's tile-entry information.
2. Enumerate live chunks in row-major order and finish STAGE_GAMEPLAY. Native
   handlers request NPCs and perform nonblocking changes.
3. Recompute the camera and reload all 45 baseline cache entries.
4. Finish STAGE_TILES over all live chunks, clipping overrides to the cache.
5. Repair blocked player terrain; reconcile/repair NPCs and update each active
   NPC once. Fresh A freezes ordinary simulation. NPC targeting can consume A.
6. Finish STAGE_DIALOGUE after the complete cache exists. Blocking dialogue
   uses local screen loops. Flag changes affect the next tile pass unless a
   handler explicitly refreshes the cache.
7. Move the player two pixels per update, checking every intermediate pixel
   against terrain and NPCs. Fresh A or a teleport pauses movement/time.
8. Advance the tick and render opaque terrain, then all alpha-masked actors
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

Named rectangles additionally export query-only TileRegion metadata. `set_tile`
validates logical area/bounds and clips against the resident cache. A scrolling
viewport touches at most four chunks. Teleports stop outgoing gameplay/dialogue,
clear the NPC pool and rebuild destination tiles; destination gameplay begins
on the next update. Save/load uses the same cache-based terrain repair.

## Content reset and save boundary

The former gate/bridge/platform, interior, corridor, courier and relocation quests
are removed. The sample has one clearing, one sign dialogue and two NPC anchors.
The generic flags, staged handlers, movement, route and save mechanisms remain.
There is one reserved flag bit and no current quest state.

SAVE_VERSION is now 4. Versions 1–3 are deliberately rejected because their
area layouts, pixel coordinates and quest flags describe the removed content.
New saves retain the explicit 24-byte record with version, CRC, position/stats,
RNG and flags. Invalid records leave Game unchanged. Valid blocked positions
are repaired only after candidate bounds/structure pass and the area cache loads.
