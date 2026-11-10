# Map and tile authoring guide

This guide tells agents what each tile means and how to build maps that play and
look right. The file format itself (TMX/TSX rules, CSV layout, exporter errors) is
specified in `docs/tiled_pipeline.md`. Read that first when you need an exact rule.

## Files

- `assets/maps/*.tmx`: one finite orthogonal Tiled map per area.
- `assets/tilesets/world.tsx`: metadata for every atlas cell (symbol, collision, portal).
- `assets/images/tiles16.png`: the 256x256 monochrome atlas. `tiles16.xcf` is its editable source.
- `assets/images/tiles16_collision.png`: the atlas with solid collision pixels tinted red, for
  checking shapes. The generator writes it on every run, so open it after changing a mask.
- `assets/images/<SYMBOL>_preview.png`: a 1x preview of each map, drawn from the atlas. Open
  it to check the whole layout at once.
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
| `0x01` | 1 | `TREE_PLUS_NORTH` | partly | Top arm of the round-tree plus. Solid on the bottom half. |
| `0x02` | 2 | `TREE_ROUND` | no | One isolated round tree. Do not clump it. |
| `0x03` | 3 | `TREE_CLUMP_TOP_LEFT` | partly | Clump piece. The tree continues down and to the right. The top-left quadrant is open. |
| `0x04` | 4 | `TREE_CLUMP_TOP_RIGHT` | partly | Clump piece. The tree continues left and down. The top-right quadrant is open. |
| `0x06` | 6 | `GRASS_06` | yes | Grass floor. Mix it with the other grass cells. |
| `0x07` | 7 | `GRASS_07` | yes | Grass floor. Mix it with the other grass cells. |
| `0x10` | 16 | `TREE_PLUS_WEST` | partly | Left arm of the round-tree plus. Solid on the right half. |
| `0x11` | 17 | `TREE_PLUS_CENTER` | no | Center of the round-tree plus. |
| `0x12` | 18 | `TREE_PLUS_EAST` | partly | Right arm of the round-tree plus. Solid on the left half. |
| `0x13` | 19 | `TREE_CLUMP_BOTTOM_LEFT` | partly | Clump piece. The tree continues right and up. The bottom-left quadrant is open. |
| `0x14` | 20 | `TREE_CLUMP_BOTTOM_RIGHT` | partly | Clump piece. The tree continues left and up. The bottom-right quadrant is open. |
| `0x16` | 22 | `GRASS_16` | yes | Grass floor. Mix it with the other grass cells. |
| `0x17` | 23 | `GRASS_17` | yes | Grass floor. Mix it with the other grass cells. |
| `0x20` | 32 | `TREE` | partly | Pine tree obstacle. Solid except for a house-shaped roof at the top. |
| `0x21` | 33 | `TREE_PLUS_SOUTH` | partly | Bottom arm of the round-tree plus. Solid on the top half. |
| `0x23` | 35 | `TREE_SPLIT_HORIZONTAL` | no | Two half trees in one tile: the right half of a tree on the left side, and the left half of a tree on the right side. |
| `0x24` | 36 | `TREE_SPLIT_VERTICAL` | no | Two half trees in one tile: the bottom half of a tree on the top side, and the top half of a tree on the bottom side. |
| `0x26` | 38 | `FLOWERS_26` | yes | Flowers on grass. Use sparingly among the grass. |
| `0x27` | 39 | `FLOWERS_27` | yes | Flowers on grass. Use sparingly among the grass. |
| `0x30` | 48 | `SWITCH_RIGHT` | partly | Interactive lever pointing right. Solid on the bottom half. |
| `0x31` | 49 | `SWITCH_LEFT` | partly | Interactive lever pointing left. Solid on the bottom half. |
| `0x32` | 50 | `POT` | no | Unbroken pot. Collectible. |
| `0x33` | 51 | `POT_BROKEN` | partly | Broken pot. The collected state of `POT`. Solid on the bottom half. |
| `0x41` | 65 | `SIGN` | no | Signpost. The `read_sign` handler belongs on it. |
| `0x42` | 66 | `CHEST_CLOSED` | no | Closed chest. Collectible. |
| `0x43` | 67 | `CHEST_OPEN` | no | Opened chest. The collected state of `CHEST_CLOSED`. |

Nineteen cells are fully walkable: `GRASS_06`, `GRASS_07`, `GRASS_16`, `GRASS_17`,
`FLOWERS_26`, `FLOWERS_27`, `0x34` to `0x36`, `0x44` to `0x46`, `0x54` to `0x56`, and
`0x65`, `0x66`, `0x75` and `0x76`.
Shaped cells are partly walkable, and the shapes are listed in
[What solidity means](#what-solidity-means). Every other cell is solid by default,
including `EMPTY` and the unnamed cells listed below. Check the atlas cell before you
paint it.

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

## Roads

Road pieces are walkable and sit on grass. There are two sets:

- The 3x3 block, `0x34` to `0x36`, `0x44` to `0x46` and `0x54` to `0x56`, holds the
  convex edges and corners of the road. Its center, `0x45`, is entirely road.
- The 2x2 block, `0x65`, `0x66`, `0x75` and `0x76`, holds the concave (inside) corners,
  where the road turns inward around grass.

Use the tiling pieces when authoring roads when feasible. For footpaths and other narrow
roads, you may use `0x45` without edge tiling.

Each road tile in the 3x3 set takes the piece that matches the sides facing grass:

| Piece | Sides facing grass | Use for |
| --- | --- | --- |
| `0x34` | top, left | Top-left corner |
| `0x35` | top | Top edge |
| `0x36` | top, right | Top-right corner |
| `0x44` | left | Left edge |
| `0x45` | none | Road interior |
| `0x46` | right | Right edge |
| `0x54` | bottom, left | Bottom-left corner |
| `0x55` | bottom | Bottom edge |
| `0x56` | bottom, right | Bottom-right corner |

A road tile with grass on opposite sides, or on three or four sides, has no matching
piece. Use `0x45` for it.

The 2x2 block holds the diagonal edges. Each piece is a 45-degree edge through the tile:

- `0x65`: road to the north and west, grass to the southeast.
- `0x76`: grass to the northwest, road to the southeast.
- `0x66`: road to the northeast, grass to the southwest.
- `0x75`: not a diagonal piece. Do not use it for roads.

Use them at concave corners. A grass cell with road on two orthogonal sides takes the piece
that faces that road: road north and east gives `0x66`, road east and south gives `0x76`,
and road west and north gives `0x65`. Road west and south has no piece, so leave that cell
as grass.

A diagonal road that runs from the bottom-left to the top-right (`/`) takes the diagonal
pieces on both sides automatically: `0x76` on the northwest side and `0x65` on the southeast
side. The 2x2 pieces sit on grass, so where trees line one side of the road instead, the road
tiles on that side take 3x3 pieces. A road that runs from the top-left to the bottom-right
(`\`) has only one matching piece, `0x66`, so its other side becomes a staircase. Avoid `\`
diagonals.

## Buildings

A building is base walls with roofs above them. Base wall sections can join at the same
or different y-levels, and one building can mix roof types.

### Base walls

- Empty base walls are 3 wide: `0xc0` is the left end, `0xc1` is interior, and `0xc2` is
  the right end.
- Decorated base walls with windows are 4 wide: `0x93` is the left end, `0x94` and `0x95`
  are interior, and `0x96` is the right end.
- A one-wide base wall is `0xc3`. Attach it to another building as a side extension, at
  the same or a different y-level from the wall it joins.
- Door tiles are `0x84` and `0x85`. They are interior pieces, so a door wall needs a side
  piece at each end. Each building has exactly one door, on one of these tiles. The tile in
  front of the door is the only road tile that touches the base wall (see
  [Layout principles](#layout-principles)).

### Slanted roofs

Slanted roofs are only for 3-wide base walls, `0xc0` to `0xc2`.

- The bottom roof tiles `0xb0`, `0xb1` and `0xb2` sit directly above the base wall.
- `0xa1` sits above `0xb1`.
- The top edge is `0x90`, `0x81` and `0x92`: `0x90` above `0xb0`, `0x81` above `0xa1`,
  and `0x92` above `0xb2`.
- To extend the roof deeper, add the middle roof tiles `0xa0`, `0x91` and `0xa2` between
  the bottom roof and the top edge: `0xa0` above `0xb0`, `0x91` above `0xa1`, and `0xa2`
  above `0xb2`. Then place the top edge above them: `0x90` above `0xa0`, `0x81` above
  `0x91`, and `0x92` above `0xa2`.

### Flat roofs

Flat roofs sit above base walls that are two or more tiles wide. They use the 3x3 convex
set from `0xa4` to `0xc6`:

- Corners: `0xa4` top-left, `0xa6` top-right, `0xc4` bottom-left, and `0xc6` bottom-right.
- Edges: `0xa5` top, `0xb4` left, `0xb6` right, and `0xc5` bottom.
- Center: `0xb5`, which has no outline.

The outline sits on the outside of the roof, so choose each tile by the sides that face
outside the roof, as for roads.

### One-wide roofs

A one-wide base wall, `0xc3`, takes `0xb3` directly above it, `0xa3` above `0xb3`, and
`0x81` above `0xa3` as the top and back edge.

### Solidity

Walls, doors and roofs are solid, except the shaped top-edge pieces `0x81`, `0x90` and
`0x92`. See [What solidity means](#what-solidity-means). A door does not open by itself.
Entering a building is handled by content code, not by the tile (see **Portals** in
that section).

## Atlas layout

Descriptions of unnamed regions are approximate and come from visual inspection.
Check a cell before you rely on it. A cell with no entry here is solid by default.

- `0x01` to `0x05`, `0x10` to `0x15` and `0x21` to `0x24`: round trees and their
  pieces. Solid, except where a shape in [What solidity means](#what-solidity-means)
  opens part of the cell. The named cells are in the table. `0x05`, `0x15` and `0x22` are
  unnamed pieces of the same family.
- `0x08` to `0x0F`: blank black cells. Solid, so check them before you use them.
- `0x25`: a blank black cell that is solid. It looks like floor but is not.
- `0x34` to `0x36`: road pieces, walkable. See [Roads](#roads).
  `0x37`: boulders, sacks and speckles. Solid.
- `0x44` to `0x46`: road pieces, walkable. `0x47`: crates and boxes, plus vertical bar
  patterns. Solid.
- `0x54` to `0x56`: road pieces, walkable.
- `0x65`, `0x66`, `0x75` and `0x76`: road pieces, walkable.
- `0x80` to `0x8F`: building fronts, doors and windows. Solid, except `0x81`, which is a
  triangle. See the shapes list and [Buildings](#buildings).
- `0x90` to `0x9F`: wall and window panels, and signboards. Solid, except the ramps `0x90`
  and `0x92`. Decorated base walls and roof edges: see [Buildings](#buildings).
- `0xA0` to `0xC7`: roofs, hatching, paving and framed pavers. Solid. Roofs and base
  walls: see [Buildings](#buildings).

## What solidity means

- **Every tile has a 16-bit collision mask**, set as the `collision` property in
  `world.tsx` (0 to 65535). `0` is passable. `65535` is fully solid. NPCs, navigation and
  cache checks treat any nonzero mask as entirely solid.
- **The player tests the mask pixel by pixel.** The foot box is the 8x8 area at pixel
  offset `(4, 8)` inside each 16x16 cell, so the bottom half of the cell decides most
  blocks. A foot pixel is blocked when its tile's mask marks that pixel solid.
- **The mask is sixteen triangles.** Split the tile into four 8x8 quadrants, in the order
  top-left, top-right, bottom-left and bottom-right. Each quadrant has four triangles that
  share its center and two adjacent corners. Each quadrant takes one nibble of the mask,
  and its bits run north, east, south, west:

  | Bits | Quadrant | Bits | Quadrant |
  | --- | --- | --- | --- |
  | 0 to 3 | top-left | 8 to 11 | bottom-left |
  | 4 to 7 | top-right | 12 to 15 | bottom-right |

  `0x00FF` is the top half. `0xF000` is the bottom-right quadrant. `0xF600` is a 45-degree
  corner solid below the diagonal that runs from the bottom-left to the top-right corner.
- **Shapes assigned so far.** Each mask is written in decimal in `world.tsx`, and the hex
  values here show the quadrant bits.
  - `0x01`: solid on the bottom half (`0xFF00`).
  - `0x03`: top-left quadrant open (`0xFFF0`).
  - `0x04`: top-right quadrant open (`0xFF0F`).
  - `0x13`: bottom-left quadrant open (`0xF0FF`).
  - `0x14`: bottom-right quadrant open (`0x0FFF`).
  - `0x10`: solid on the right half (`0xF0F0`).
  - `0x12`: solid on the left half (`0x0F0F`).
  - `0x20`: solid except a house roof at the top, with 45-degree slopes from the peak at
    top center to both top corners (`0xFFC6`).
  - `0x21`: solid on the top half (`0x00FF`).
  - `0x30`: solid on the bottom half (`0xFF00`).
  - `0x31`: solid on the bottom half (`0xFF00`).
  - `0x33`: solid on the bottom half (`0xFF00`).
  - `0x81`: solid triangle with vertices at the two bottom corners and the center (`0xC600`).
  - `0x90`: ramp, solid below the diagonal from bottom-left to top-right (`0xF660`).
  - `0x92`: ramp, solid below the diagonal from top-left to bottom-right (`0xCF0C`).
- **Pixel rule.** A pixel is solid when its center lies in a solid triangle. A center on a
  quadrant diagonal lies on the edge of two triangles, and it is solid when either one is.
- **Corners slide.** When terrain blocks the player's next pixel, the player takes one pixel
  sideways toward a position that clears the step, then keeps walking. The sidestep is a
  normal one-pixel move, so a corner a few pixels deep no longer stops smooth movement,
  and a slope's edge is followed the same way. Facing does not change. A deeper overlap
  stays blocked, and NPCs never sidestep.
- **Unpainted cells are outside, not ground.** An empty CSV cell (GID `0`) becomes the
  sentinel, which is not walkable. Paint every cell that should be playable.
- **Interactive tiles are solid.** Pots and chests block movement fully. `SWITCH_LEFT` is
  solid on its bottom half, like `SWITCH_RIGHT` and `POT_BROKEN`. The player
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

## Layout principles

These rules describe how roads, villages and trees should look. Follow them when you
author a map by hand.

- **Doors, not walls.** A road touches a building only at its door. The row below a base
  wall is grass, except for the door's front tile. A footpath runs from that tile to the
  road network, and its first tile runs straight south, because the tiles beside it touch
  the wall.
- **Organic villages.** Do not line houses up on one row. Space them at irregular
  intervals and distances from the street. Each door's footpath can be a different length.
- **Roads about two tiles wide.** Main roads are two tiles wide, and footpaths are one
  tile wide. A diagonal road is one diagonal line of road tiles, which has about the same
  width as a two-tile road.
- **Winding, not straight.** Roads bend gently, changing direction every four to nine tiles.
  Avoid straight runs longer than about fourteen tiles, and sharp zigzags with runs of one or
  two tiles.
- **No dead ends.** Every road ends at a junction, a door footpath, a village exit or the
  map's edge. A road that stops anywhere else is a bug.
- **Diagonal edges, not staircases.** Diagonal roads use the 2x2 diagonal pieces along their
  grass edges. See the rules for `/` and `\` diagonals under Roads.
- **Trees around roads.** Scatter single trees and small clumps near roads, two to four
  tiles away, thinning with distance. A single tree never touches another tree.
- **Soft forest edges.** Forest edges are not hard lines. Density falls off over a few
  tiles, with loose trees marking the transition. The map's border is solid at its core and
  fades gradually inward.

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
- Lay roads with the road pieces in [Roads](#roads), not with grass cells.
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
  `reverse` (default `once`), and `pause` set from 0 to 255 (default 16). A route needs
  2 to 255 waypoints. Each waypoint must sit inside a tile, not on a tile edge or
  corner. Keep waypoints on passable tiles where an NPC can stand.
- **Test anchors.** `TestField` is open meadow for the native scenario tests, which paint
  their own ground over it. Scenario positions are offsets from named points, such as
  `TestField` or a resident's home. Test code never uses absolute map coordinates.
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
   `collision` mask and `portal` value. Symbols must be unique.
3. Regenerate. Renaming a symbol changes its `TILE_<symbol>` constant, so update any
   code that refers to it.
4. Check every map that uses the cell. Changing `collision` changes how every map plays.

## Before committing a map

- The generator, `test_pipeline.py` and `test_content.py` all pass.
- Roads touch a building only at its door front, and every road ends at a junction, a
  door footpath, a village exit or the map's edge.
- Diagonal road edges that border grass use `0x65`, `0x66` and `0x76`, with no staircases.
  Roads are about two tiles wide and wind gently.
- Single trees never touch another tree, and loose trees sit near roads.
- No `EMPTY` (`0x00`) cell is painted, and the floor mixes the four grass cells.
- The spawn is passable, and every named point and route is reachable from the spawn
  over passable tiles.
- No unpainted cell lies inside the playable area.
- Every handler point sits on the tile it describes, and it does not share a cell with
  the spawn.
- Route waypoints are on passable tiles and clear of edges and corners.
- No large clumps of `TREE_ROUND` sit next to each other. Groups use the tiling pieces.
- Roads use the road pieces in [Roads](#roads), except narrow roads and roads next to
  buildings, which use `0x45` alone.
- Each building has exactly one door, on an interior base wall tile (`0x84` or `0x85`).
- Every base wall run starts and ends with a side piece (`0xc0` or `0xc2`, or `0x93` or
  `0x96`), or is a single `0xc3`.
- Every tree seam matches its neighbor, and no seam faces outside the map (see Round
  trees).
- Device QA passes when a change touches NPCs, collisions or sightlines.
