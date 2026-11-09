# Map and tile authoring guide

This guide tells agents what each tile means and how to build maps that play and
look right. The file format itself (TMX/TSX rules, CSV layout, exporter errors) is
specified in `docs/tiled_pipeline.md`. Read that first when you need an exact rule.

## Files

- `assets/maps/*.tmx`: one finite orthogonal Tiled map per area.
- `assets/tilesets/world.tsx`: metadata for every atlas cell (symbol, solid, portal).
- `assets/images/tiles16.png`: the 256x256 monochrome atlas. `tiles16.xcf` is its editable source.
- After any change, run `python tools/generate_assets.py`, then `tests/test_pipeline.py`
  and `tests/test_content.py`. The generator writes `generated/`, including
  `tile_ids.h` and `map_positions.h`.

## Tile IDs

- A tile's runtime ID is its atlas cell index: `row * 16 + column`. The top-left
  cell is `0x00`, and `0x41` is the sign.
- In the TMX Ground layer, each CSV value is the runtime ID plus one (`firstgid=1`).
  GID `1` is `EMPTY` (`0x00`), and GID `66` is `SIGN` (`0x41`).
- **Do not paint `0x00`.** It is `EMPTY`, a blank placeholder. It is solid, so it blocks
  movement and reads as a black hole. Paint a grass or other floor cell instead.
- Cell `0xFF` (255) is reserved as the outside sentinel. It is never drawn and never
  walkable. Do not paint it as art.
- Named cells use their symbol. Unnamed cells are `ATLAS_XX`, with two hex digits,
  such as `ATLAS_25`. The generator emits `TILE_<symbol>` constants.

## Named tiles

| Hex | ID | Symbol | Passable | Meaning |
| --- | --- | --- | --- | --- |
| `0x00` | 0 | `EMPTY` | no | Blank placeholder. Never paint it. |
| `0x01` | 1 | `TREE_PLUS_NORTH` | no | Top arm of the round-tree plus. |
| `0x02` | 2 | `TREE_ROUND` | no | One isolated round tree. Do not clump it. |
| `0x03` | 3 | `TREE_CLUMP_TOP_LEFT` | no | Clump piece. The tree continues down and to the right. |
| `0x04` | 4 | `TREE_CLUMP_TOP_RIGHT` | no | Clump piece. The tree continues left and down. |
| `0x06` | 6 | `GRASS_06` | yes | Grass floor. Mix it with the other grass cells. |
| `0x07` | 7 | `GRASS_07` | yes | Grass floor. Mix it with the other grass cells. |
| `0x10` | 16 | `TREE_PLUS_WEST` | no | Left arm of the round-tree plus. |
| `0x11` | 17 | `TREE_PLUS_CENTER` | no | Center of the round-tree plus. |
| `0x12` | 18 | `TREE_PLUS_EAST` | no | Right arm of the round-tree plus. |
| `0x13` | 19 | `TREE_CLUMP_BOTTOM_LEFT` | no | Clump piece. The tree continues right and up. |
| `0x14` | 20 | `TREE_CLUMP_BOTTOM_RIGHT` | no | Clump piece. The tree continues left and up. |
| `0x16` | 22 | `GRASS_16` | yes | Grass floor. Mix it with the other grass cells. |
| `0x17` | 23 | `GRASS_17` | yes | Grass floor. Mix it with the other grass cells. |
| `0x20` | 32 | `TREE` | no | Pine tree obstacle. |
| `0x21` | 33 | `TREE_PLUS_SOUTH` | no | Bottom arm of the round-tree plus. |
| `0x23` | 35 | `TREE_SPLIT_HORIZONTAL` | no | Two half trees in one tile: the right half of a tree on the left side, and the left half of a tree on the right side. |
| `0x24` | 36 | `TREE_SPLIT_VERTICAL` | no | Two half trees in one tile: the bottom half of a tree on the top side, and the top half of a tree on the bottom side. |
| `0x26` | 38 | `FLOWERS_26` | yes | Flowers on grass. Use sparingly among the grass. |
| `0x27` | 39 | `FLOWERS_27` | yes | Flowers on grass. Use sparingly among the grass. |
| `0x30` | 48 | `SWITCH_RIGHT` | no | Interactive lever pointing right. |
| `0x31` | 49 | `SWITCH_LEFT` | no | Interactive lever pointing left. |
| `0x32` | 50 | `POT` | no | Unbroken pot. Collectible. |
| `0x33` | 51 | `POT_BROKEN` | no | Broken pot. The collected state of `POT`. |
| `0x41` | 65 | `SIGN` | no | Signpost. The `read_sign` handler belongs on it. |
| `0x42` | 66 | `CHEST_CLOSED` | no | Closed chest. Collectible. |
| `0x43` | 67 | `CHEST_OPEN` | no | Opened chest. The collected state of `CHEST_CLOSED`. |

Only six cells are walkable: `GRASS_06`, `GRASS_07`, `GRASS_16`, `GRASS_17`,
`FLOWERS_26` and `FLOWERS_27`. Every other cell is solid by default, including
`EMPTY` and the unnamed cells listed below. Check the atlas cell before you paint it.

The four grass cells are interchangeable floor. Paint them in a mix. Choose among them
when you paint, because the game does not randomize at runtime and the exported map
looks the same every time.

## Round trees

`TREE_ROUND` (`0x02`) is for isolated single trees. Every other round-tree piece is part
of a tree that crosses a tile edge, so it must join matching pieces across that edge.

Each piece has a side for every edge a tree crosses. A side is empty, `H` (a horizontal
half tree that continues across a left or right edge), or `V` (a vertical half tree that
continues across a top or bottom edge). Two neighboring tiles join only when their facing
sides match: `H` against `H`, `V` against `V`, or empty against empty. A mismatch shows as
a broken half tree.

| Piece | Left | Right | Top | Bottom |
| --- | --- | --- | --- | --- |
| `0x02` `TREE_ROUND` | empty | empty | empty | empty |
| `0x23` `TREE_SPLIT_HORIZONTAL` | H | H | empty | empty |
| `0x24` `TREE_SPLIT_VERTICAL` | empty | empty | V | V |
| `0x03` `TREE_CLUMP_TOP_LEFT` | empty | H | empty | V |
| `0x04` `TREE_CLUMP_TOP_RIGHT` | H | empty | empty | V |
| `0x13` `TREE_CLUMP_BOTTOM_LEFT` | empty | H | V | empty |
| `0x14` `TREE_CLUMP_BOTTOM_RIGHT` | H | empty | V | empty |
| `0x01` `TREE_PLUS_NORTH` | empty | empty | empty | V |
| `0x10` `TREE_PLUS_WEST` | empty | H | empty | empty |
| `0x11` `TREE_PLUS_CENTER` | H | H | V | V |
| `0x12` `TREE_PLUS_EAST` | H | empty | empty | empty |
| `0x21` `TREE_PLUS_SOUTH` | empty | empty | V | empty |

Rules for tiled trees:

- A horizontal run starts with `0x10`, continues with `0x23`, and ends with `0x12`. A
  vertical run starts with `0x01`, continues with `0x24`, and ends with `0x21`. A 2x2
  clump is `0x03` and `0x04` over `0x13` and `0x14`.
- No seam may face outside the map, because the tree would be cut off. Map edges count as
  empty sides.
- Never place `TREE_ROUND` next to a piece whose side facing it is a seam. Two
  `TREE_ROUND` tiles never touch orthogonally.

## Atlas layout

Descriptions of unnamed regions are approximate and come from visual inspection.
Check a cell before you rely on it. A cell with no entry here is solid by default.

- `0x01` to `0x05`, `0x10` to `0x15` and `0x21` to `0x24`: round trees and their
  pieces. Solid. The named cells are in the table. `0x05`, `0x15` and `0x22` are unnamed
  pieces of the same family.
- `0x08` to `0x0F`: blank black cells. Solid, so check them before you use them.
- `0x25`: a blank black cell that is **solid**. It looks like floor but is not.
- `0x34` to `0x37`: boulders, sacks and speckles. Solid.
- `0x44` to `0x47`: crates and boxes, plus vertical bar patterns. Solid.
- `0x80` to `0x8F`: building fronts, doors and windows. Solid.
- `0x90` to `0x9F`: wall and window panels, and signboards. Solid.
- `0xA0` to `0xC7`: roofs, hatching, paving and framed pavers. Solid.

## What solidity means

- **Collision is per whole cell.** A solid cell blocks the player or an NPC whenever
  the actor's foot box would overlap it.
- **The foot box is the lower half of the cell.** It is an 8x8 box at pixel offset
  `(4, 8)` inside each 16x16 cell, so the bottom 8 rows decide what is blocked. Draw
  the base of a solid object in those rows, where the feet stop.
- **Unpainted cells are outside, not ground.** An empty CSV cell (GID `0`) becomes the
  sentinel, which is not walkable. Paint every cell that should be playable.
- **Interactive tiles are solid.** Switches, pots and chests block movement. The player
  interacts by standing next to one and facing it, so its handler point goes on the
  tile itself, as `read_sign` does on `SIGN`. No handler uses these tiles yet.
- **Collectibles change state.** A collected `POT` becomes `POT_BROKEN`, and a collected
  `CHEST_CLOSED` becomes `CHEST_OPEN`. The map shows the initial state. Content code must
  record the collected state in a world flag, because the cache is rebuilt from the map
  when it reloads.
- **Portals.** No tile sets `portal=true` yet. The flag stops an NPC from spawning
  when its foot centre lies on that cell. Doorways are not driven by tile flags.
  Content code triggers them from a handler with `teleport_player<POS>()`, and no
  content does this yet.

## Building a map

- Use one Ground tile layer that covers the whole map. Layers overwrite each other
  wherever they are non-zero. Keep one layer unless you have a specific reason.
- Keep the logical size within 240x248. Design rooms in view-sized blocks. The view is
  8x4 tiles (128x64 pixels).
- Align room edges and handler regions to the 8x4 chunk grid. Each chunk dispatches
  its own handlers, and a handler point belongs to the chunk that contains it.
- Map properties: `area_id` must be unique in 1..255. `symbol` must be unique. `spawn`
  names a single named point in the same area. That tile must be passable and must not
  carry a handler.
- Floors use the four grass cells in a mix. Add flowers sparingly, about one floor cell
  in eight, and never fill a room with them.
- The map's border is solid. Build it from round trees: the split and clump pieces along
  the edges, with isolated `TREE_ROUND` singles for variety. Keep the playable interior
  inside it.

## Objects

Place objects in the Logic group, with Locations and Routes as the other allowed groups.

- **Points sit on tile centers.** Use pixel coordinates `(16*tx + 8, 16*ty + 8)`.
- **Names are game contracts.** A named point becomes `positions::Name`. Game code
  must refer to positions through these constants and must not hardcode coordinates.
  See the rule in `AGENTS.md`.
- **Rectangles** cover every tile they overlap. They generate `positions::Name_0`,
  `positions::Name_1` and so on, plus a `NameRegion` query.
- **Handler points** name a native handler in the `handler` property. The handler's
  tile should show what it does. `read_sign` belongs on a `SIGN` tile, and an
  interactive or collectible tile takes its handler the same way. A handler point must
  not share a cell with the spawn.
- **Routes** are polylines on the Routes layer, with `mode` set to `once`, `loop` or
  `reverse` (default `once`), and `pause` set from 0 to 255 (default 8). A route needs
  2 to 255 waypoints. Each waypoint must sit inside a tile, not on a tile edge or
  corner. Keep waypoints on passable tiles where an NPC can stand.
- Do not use object types, classes or templates. The exporter rejects them.

## Artistic rules

- **Use 1-bit art.** Pixels are pure black or pure white. Use no grey and no
  anti-aliasing. Cells are opaque.
- **Black is the background.** A drawn tile overwrites what is beneath it. White
  carries the detail.
- **Make silhouettes read at 16 pixels.** Use bold shapes. Keep outlines one pixel wide
  and consistent across a set. Avoid speckles that vanish next to solid shapes.
- **Put the bases of solid objects at the bottom of the cell.** The feet stop at the
  bottom edge, so trunks, rocks and walls should meet the ground there.
- **Tile round trees.** Do not place large clumps of `TREE_ROUND` next to each other.
  Tile groups of trees with the plus, split and clump pieces instead. `TREE_ROUND` is
  for isolated single trees.
- **Vary the floor.** Mix the four grass cells, and add flowers only as occasional
  accents. Do not use a solid cell that looks like floor (`0x25`) as floor.
- **Show state on interactive tiles.** Levers point the way they act. Collectibles show
  their state clearly: whole or broken, closed or open.
- **Plan paths for bodies.** A single 16-pixel tile is wide enough for the player and
  an NPC foot box. Two bodies in a one-tile corridor must yield to each other, so use
  two tiles for paths that NPCs will share.
- **Keep sightlines open.** The camera shows 8x4 tiles. Put the important objects
  inside that view when the player arrives.
- **Avoid unreadable inversions.** Do not hide an exit or sign only in a black cell on
  a black background.

## Adding or changing a tile

1. Draw the cell in `assets/images/tiles16.png`, in an empty cell or in an existing
   cell you are replacing. Keep the cell index equal to the runtime ID.
2. In `world.tsx`, set `runtime_id` to the cell index and give the cell its symbol,
   `solid` and `portal` values. Symbols must be unique.
3. Regenerate. Renaming a symbol changes its `TILE_<symbol>` constant, so update any
   code that refers to it.
4. Check every map that uses the cell. Changing `solid` changes how every map plays.

## Before committing a map

- The generator, `test_pipeline.py` and `test_content.py` all pass.
- No `EMPTY` (`0x00`) cell is painted, and the floor mixes the four grass cells.
- The spawn is passable, and every named point and route is reachable from the spawn
  over passable tiles.
- No unpainted cell lies inside the playable area.
- Every handler point sits on the tile it describes, and it does not share a cell with
  the spawn.
- Route waypoints are on passable tiles and clear of edges and corners.
- No large clumps of `TREE_ROUND` sit next to each other. Groups use the tiling pieces.
- Every tree seam matches its neighbor, and no seam faces outside the map (see Round
  trees).
- Device QA passes when a change touches NPCs, collisions or sightlines.
