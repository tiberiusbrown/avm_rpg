# Map and tile authoring guide

This guide tells agents what each tile means and how to build maps that play and
look right. The file format itself (TMX/TSX rules, CSV layout, exporter errors) is
specified in `docs/tiled_pipeline.md`. Read that first when you need an exact rule.

This guide covers the world tileset and outdoor maps. Houses, caves and dungeons each
have their own tileset. [AUTHORING_INTERIOR.md](AUTHORING_INTERIOR.md) describes the room
model that all three share, and the house tileset. [AUTHORING_CAVE.md](AUTHORING_CAVE.md)
and [AUTHORING_DUNGEON.md](AUTHORING_DUNGEON.md) describe caves and dungeons.

## Files

- `assets/maps/*.tmx`: one finite orthogonal Tiled map per area. `outdoor.tmx` is the
  overworld. `interiors.tmx` holds the inside of every house. `cave.tmx` is Hollow Mine,
  and `dungeon.tmx` is the Old Vault.
- **Each map uses exactly one tileset**, and the game draws and collides each area with
  the tileset of its map. Outdoor maps use `world.tsx`. Tile IDs belong to a tileset, so
  the same ID is a different tile in another tileset.
- `assets/tilesets/world.tsx`: metadata for every atlas cell (symbol, collision, portal).
- `assets/images/tiles16.png`: the 256x256 monochrome atlas.
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
| `0x02` | 2 | `TREE_ROUND` | partly | One isolated round tree. Do not clump it. Blocks its cell without the bottom corners. |
| `0x03` | 3 | `TREE_CLUMP_TOP_LEFT` | partly | Clump piece. The tree continues down and to the right. The top-left quadrant is open. |
| `0x04` | 4 | `TREE_CLUMP_TOP_RIGHT` | partly | Clump piece. The tree continues left and down. The top-right quadrant is open. |
| `0x06` | 6 | `GRASS_06` | yes | Grass floor. Mix it with the other grass cells. |
| `0x07` | 7 | `GRASS_07` | yes | Grass floor. Mix it with the other grass cells. |
| `0x08` | 8 | `GRASS_08` | yes | Sparse grass floor with three tufts. |
| `0x09` | 9 | `GRASS_09` | yes | Sparse grass floor with two tufts. |
| `0x0A` | 10 | `GRASS_0A` | yes | Sparse grass floor with one tuft. |
| `0x0B` | 11 | `HOLE_LADDER` | yes | Hole in the ground with the top of a ladder. See [Entrances](#entrances). |
| `0x0C` | 12 | `ROCK_FLAT` | partly | Low flat rock. Solid on the bottom half. See [Rocks](#rocks). |
| `0x0D` | 13 | `BOULDER_TALL` | no | Tall pointed boulder. |
| `0x0E` | 14 | `ROCK_PILE` | no | Pile of rocks. |
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
| `0x30` | 48 | `SWITCH_RIGHT` | partly | Interactive lever pointing right. Blocks its cell without the top corners, as `SWITCH_LEFT` does. |
| `0x31` | 49 | `SWITCH_LEFT` | partly | Interactive lever pointing left. Blocks its cell without the top corners, as `POT` does. |
| `0x32` | 50 | `POT` | partly | Unbroken pot. Collectible. Blocks its cell without the top corners. |
| `0x33` | 51 | `POT_BROKEN` | partly | Broken pot. The collected state of `POT`. Solid on the bottom half. |
| `0x34` | 52 | `ROCK_SMALL` | partly | Small rock. Solid on the bottom half. |
| `0x35` | 53 | `ROCKS` | no | Two rocks. |
| `0x36` | 54 | `BOULDER` | no | Round boulder. |
| `0x37` | 55 | `BOULDER_CRACKED` | no | Split boulder. |
| `0x41` | 65 | `SIGN` | no | Signpost. The `read_sign` handler belongs on it. |
| `0x42` | 66 | `CHEST_CLOSED` | partly | Closed chest with a lit lid and a lock. Collectible. Blocks its cell without the top corners. |
| `0x43` | 67 | `CHEST_OPEN` | partly | Opened chest with its lid raised. The collected state of `CHEST_CLOSED`. |
| `0x59` | 89 | `GRASS_59` | yes | Sparse grass floor with two tufts. It sits in the center of the cliff block. |

The road, bridge, large boulder, water, raised-area and fence cells are named too. Their
tables are in [Roads](#roads), [Bridges](#bridges), [Rocks](#rocks), [Water](#water),
[Raised areas](#raised-areas), [Entrances](#entrances) and [Fences](#fences).

Forty-two cells are fully walkable: `REEDS` (`0x8E`, see [Animation](#animation)), the eight grass cells (`GRASS_06`, `GRASS_07`,
`GRASS_16`, `GRASS_17`, `GRASS_08`, `GRASS_09`, `GRASS_0A` and `GRASS_59`), `FLOWERS_26`,
`FLOWERS_27`, the twenty-one road cells, the six bridge cells, `HOLE_LADDER` (`0x0B`),
`CLIFF_RAMP` (`0x78`), `CLIFF_CAVE` (`0x79`) and `CLIFF_MINE` (`0x7A`).
Shaped cells are partly walkable, and the shapes are listed in
[What solidity means](#what-solidity-means). Every other cell is solid by default,
including `EMPTY` and the unnamed cells listed below. Check the atlas cell before you
paint it.

The eight grass cells are interchangeable floor, and each one tiles cleanly against every
other. `GRASS_06`, `GRASS_07`, `GRASS_16` and `GRASS_17` are dense. `GRASS_08`, `GRASS_09`,
`GRASS_0A` and `GRASS_59` are sparse, with one to three tufts each. Use the two kinds to
vary how busy the ground looks:

- Open meadow, plateau floors and the ground beside water, cliffs and fences are mostly
  sparse, with a dense cell here and there.
- Overgrown ground, such as forest edges and the gaps between trees, is mostly dense.
- Every area mixes several cells, because a run of one cell repeats visibly.

Choose among them when you paint, because the game does not randomize at runtime and the
exported map looks the same every time.

## Round trees

`TREE_ROUND` (`0x02`) is for isolated single trees. Every other round-tree piece is part
of a tree that crosses a tile edge, so it must join matching pieces across that edge.

Every piece is drawn from one tree: a crown with a lumpy rim, leaf marks inside it, a
second line inside the rim on the side away from the light, and a trunk with a root flare. A
piece holds the trees that cross its sides, each shifted by half a cell, and a tree further
south covers the one behind it. Draw a new piece the same way, from the same tree, so that
its halves meet the halves in the neighboring cells.

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
- **The map edge matches any seam.** The forest continues beyond the map, so a piece on
  the edge may have a seam that faces outside. Half a tree at the edge is correct.
- **A dense forest is `0x11` everywhere.** Around it, each cell beside one or two `0x11`
  cells takes the piece whose seams face those cells: `0x21` below the forest, `0x01`
  above it, `0x12` to its right, `0x10` to its left, and the clump pieces at its corners.
  A cell beside three `0x11` cells has no piece. Make it `0x11` too.
- Never place `TREE_ROUND` next to a piece whose side facing it is a seam. Two
  `TREE_ROUND` tiles never touch orthogonally.

## Roads

Roads are paved with flagstones. They are fully walkable and one tile wide, and their
edges waver so they sit on grass without a hard line. A road cell takes its piece from the
sides on which the road continues. Bridge cells count as road.

The sixteen pieces fill the 4x4 block from `0x44` to `0x77`. A piece's ID is `0x44` plus
its side bits, where north is 1, east is 2, south is 4 and west is 8: the low two bits give
the column and the high two bits give the row.

| Hex | Symbol | Road continues | Use for |
| --- | --- | --- | --- |
| `0x44` | `ROAD_PATCH` | nowhere | One paved cell on its own |
| `0x45` | `ROAD_N` | north | Dead end, such as a door front |
| `0x46` | `ROAD_E` | east | Dead end |
| `0x47` | `ROAD_NE` | north, east | Corner |
| `0x54` | `ROAD_S` | south | Dead end |
| `0x55` | `ROAD_NS` | north, south | Straight road, north to south |
| `0x56` | `ROAD_ES` | east, south | Corner |
| `0x57` | `ROAD_NES` | north, east, south | T junction |
| `0x64` | `ROAD_W` | west | Dead end |
| `0x65` | `ROAD_NW` | north, west | Corner |
| `0x66` | `ROAD_EW` | east, west | Straight road, east to west |
| `0x67` | `ROAD_NEW` | north, east, west | T junction |
| `0x74` | `ROAD_SW` | south, west | Corner |
| `0x75` | `ROAD_NSW` | north, south, west | T junction |
| `0x76` | `ROAD_ESW` | east, south, west | T junction |
| `0x77` | `ROAD_NESW` | all four | Crossing, and the inside of a paved square |

Five more pieces are for paved areas two or more cells wide:

| Hex | Symbol | Use for |
| --- | --- | --- |
| `0x40` | `ROAD_NESW_ALT` | Second pattern for `ROAD_NESW`. Mix it into paved squares. |
| `0x50` | `ROAD_INNER_NW` | Road on all four sides, grass at the top-left diagonal |
| `0x51` | `ROAD_INNER_NE` | Road on all four sides, grass at the top-right diagonal |
| `0x52` | `ROAD_INNER_SW` | Road on all four sides, grass at the bottom-left diagonal |
| `0x53` | `ROAD_INNER_SE` | Road on all four sides, grass at the bottom-right diagonal |

Rules for roads:

- **One piece per cell, from its four neighbors.** A wide road or a square needs no edge
  set: its outer cells are corners and T junctions, and its inner cells are `ROAD_NESW`.
- **Roads are one tile wide.** Use a paved square of 2x2 or 3x3 where streets meet.
- **Roads turn at right angles.** There are no diagonal pieces. Bend a road by stepping
  it sideways one cell every few cells.
- **A road may end.** A dead-end piece closes it with a rounded end. Use that at a door
  front and where a road leaves the settled area.
- **A road meets a bridge end on.** The road cell next to a bridge counts the bridge as
  road, so the paving runs up to the deck.

A street that ends at both sides, with a path up to a door and a 2x2 square, as runtime
IDs. `06` is any grass cell and `DD` is the door.

```text
06 DD 06 06 06 06
06 54 06 56 74 06
46 67 66 67 67 64
```

## Water

Water is solid. It uses the 24 cells `0x18` to `0x3F` in columns `8` to `F` of the atlas, and
the three pile cells under a bridge (see [Bridges](#bridges)).
The bank is drawn inside the water cell, one or two pixels in from each side that faces
land, so a water cell is mostly water. The land cells around water stay ordinary tiles, so
water tiles cleanly against every grass cell.

The water lies lower than the ground, and the view shows it:

- **The shoreline wanders.** The edge of the ground is a white line that steps in and out by
  a pixel. Every side uses the same profile, and it repeats every sixteen pixels, so shores
  join from cell to cell. Where two banks meet, the shore rounds the corner.
- **A north bank shows its face.** It faces the viewer, so two dark rows of earth show below
  the ground's edge, and a broken line below them is the water's edge.
- **A west or east bank shows a narrow face**: one dark column, then a broken waterline.
- **A south bank shows no face.** The viewer looks over it from behind, so its edge is the
  ground's edge alone.
- **The water laps against the shore, very quietly.** The waterline is a broken line of
  dashes. On each side of a cell, the two middle pixels of one dash stand one pixel further
  from the bank for one phase of the four, then return. The move is toward the shore and
  back, never along it. North banks, side banks and south banks each take a different phase,
  so the whole shore never moves at once. Against a south bank the line is short dashes of
  foam. Keep this far quieter than the ripples. Use the same idea for anything that stands
  in water: at the foot of a bridge pile the water steps a pixel out on one side, then on
  the other.
- Ripples are dashes on open water, never shorter than three pixels. Each has its own motion
  through the four phases: some drift east and back, some drift west and back, and some grow
  and shrink in place. Their rows are spread evenly over the cell, and two ripples run off
  one side of the cell and continue on the other, so a body of water shows no grid of cells.
  **Every water cell holds the same two crossing ripples, at the same place and with the
  same motion**, so the part that leaves one cell always meets the part that enters the
  next, in every phase. A ripple shows whole or not at all, so a shore leaves no stray
  pixels, and it shows only where it has room through its whole motion: a ripple that would
  reach the bank in any phase is left out of that cell in every phase, so none vanishes on
  its way to the shore. The ambient effects supply the glints.

Draw a new water cell from these rules, not by hand: every piece must share the profile and
the ripples of the others.

**Every water cell must be part of a 2x2 block of water cells.** Water is never one tile
wide. Every shape that follows this rule has a piece for each of its cells. Lakes can take
any outline, and a river can bend or step sideways one tile at a time.

Choose the piece for each water cell from its eight neighbors:

1. **Sides.** Note which of its four sides touch land. Land is any cell that is not water.
   The 2x2 rule leaves no side, one side, or two sides that meet at a corner.
2. **Inner corners.** Look at the four diagonal neighbors. A diagonal is an inner corner
   when it is land and both sides next to it are water.

| Sides facing land | Inner corners | Piece | Symbol |
| --- | --- | --- | --- |
| top, left | none | `0x18` | `WATER_TOP_LEFT` |
| top | none | `0x19` | `WATER_TOP` |
| top, right | none | `0x1A` | `WATER_TOP_RIGHT` |
| left | none | `0x28` | `WATER_LEFT` |
| none | none | `0x29` or `0x3F` | `WATER` or `WATER_CALM` |
| right | none | `0x2A` | `WATER_RIGHT` |
| bottom, left | none | `0x38` | `WATER_BOTTOM_LEFT` |
| bottom | none | `0x39` | `WATER_BOTTOM` |
| bottom, right | none | `0x3A` | `WATER_BOTTOM_RIGHT` |
| none | bottom-right | `0x1B` | `WATER_INNER_BOTTOM_RIGHT` |
| none | bottom-left | `0x1C` | `WATER_INNER_BOTTOM_LEFT` |
| none | top-right | `0x2B` | `WATER_INNER_TOP_RIGHT` |
| none | top-left | `0x2C` | `WATER_INNER_TOP_LEFT` |
| none | top-right and bottom-left | `0x1F` | `WATER_INNER_TOP_RIGHT_BOTTOM_LEFT` |
| none | top-left and bottom-right | `0x2F` | `WATER_INNER_TOP_LEFT_BOTTOM_RIGHT` |
| top | bottom-left | `0x1D` | `WATER_TOP_INNER_BOTTOM_LEFT` |
| top | bottom-right | `0x1E` | `WATER_TOP_INNER_BOTTOM_RIGHT` |
| bottom | top-left | `0x2D` | `WATER_BOTTOM_INNER_TOP_LEFT` |
| bottom | top-right | `0x2E` | `WATER_BOTTOM_INNER_TOP_RIGHT` |
| left | top-right | `0x3B` | `WATER_LEFT_INNER_TOP_RIGHT` |
| left | bottom-right | `0x3C` | `WATER_LEFT_INNER_BOTTOM_RIGHT` |
| right | top-left | `0x3D` | `WATER_RIGHT_INNER_TOP_LEFT` |
| right | bottom-left | `0x3E` | `WATER_RIGHT_INNER_BOTTOM_LEFT` |

A combination that is not in the table means a water cell near it breaks the 2x2 rule. Widen
the water there.

Three layouts, as runtime IDs. The first is a 4x3 lake. The second is a river two tiles wide
that comes from the west and turns south. The third is a river two tiles wide that runs east
and steps down one tile.

```text
18 19 19 1A      19 19 19 1A      19 19 1A
28 29 3F 2A      39 39 1C 2A      39 39 1F 19 19
38 39 39 3A            28 2A            38 39 39
                       28 2A
```

Rules for water:

- Interior cells take `WATER` or `WATER_CALM`. Paint about one interior cell in three as
  `WATER_CALM`, so wide water does not repeat.
- The edge of the map counts as water, so a river can run off the map without a bank.
- Rivers are two or three tiles wide. They wind as roads do.
- Water blocks the player and NPCs. A [bridge](#bridges) is the only way across. Water
  must not cut walkable ground off from the spawn: an island or a far bank with fully
  walkable cells and no bridge fails the content test.

## Bridges

A bridge carries a road across water. Bridge cells are fully walkable, and each replaces a
water cell. The water cell that it replaces shows under and beside it, so the banks appear at
its two ends.

The two directions are drawn differently, because the view is from the front and above:

- **A bridge that runs east to west** shows its deck from above, with plank seams that run
  north to south. The deck lies in the lower part of its cell: the water behind it shows
  along the top, then the far rail, the planks, and the near rail along the bottom. Its piles
  stand in the row of cells below it, the three `WATER_PILES` cells: the deck's front edge
  runs along their top, and two short piles per cell stand in the water, each lit on its
  left. The piles are eight pixels apart, so they are evenly spaced along the whole bridge.
  The water laps against the foot of each pile.
- **A bridge that runs north to south** shows its deck as a column of planks with seams that
  run west to east. A rail runs down each side, the east one broken because it is in shade.
  At every post a pile descends into the water beside the deck.
- The deck is lit: it faces up, so it is white.

| Hex | Symbol | Place |
| --- | --- | --- |
| `0x60` | `BRIDGE_WEST` | West end of a bridge that runs east to west |
| `0x61` | `BRIDGE_HORIZONTAL` | Middle of a bridge that runs east to west |
| `0x62` | `BRIDGE_EAST` | East end of a bridge that runs east to west |
| `0x70` | `BRIDGE_NORTH` | North end of a bridge that runs north to south |
| `0x71` | `BRIDGE_VERTICAL` | Middle of a bridge that runs north to south |
| `0x72` | `BRIDGE_SOUTH` | South end of a bridge that runs north to south |
| `0x9A` | `WATER_PILES_WEST` | Water with the piles of a bridge. It goes directly below `BRIDGE_WEST` and replaces a `WATER_LEFT`. |
| `0x9B` | `WATER_PILES` | Water with the piles of a bridge. It goes directly below `BRIDGE_HORIZONTAL` and replaces a `WATER` or `WATER_CALM`. |
| `0x9C` | `WATER_PILES_EAST` | Water with the piles of a bridge. It goes directly below `BRIDGE_EAST` and replaces a `WATER_RIGHT`. |

Rules for bridges:

- **Bridges cross straight water only.** An east-to-west bridge crosses a river that runs
  north to south, and the river is straight for the bridge row and one row on each side.
  A north-to-south bridge crosses a river that runs east to west in the same way.
- **A bridge spans the whole river.** Replace every water cell in the row or column,
  bank to bank: the two end pieces, with middle pieces between them on a river three or
  more cells wide. A river two cells wide takes the two end pieces alone.
- **An east-to-west bridge takes a row of piles.** Directly below each of its deck cells goes
  the matching `WATER_PILES` cell. The pile cells are water: solid, and they count as water
  for the pieces around them. They exist for a west bank, open water and an east bank only,
  so the river must run straight for the row below the bridge, with no corner and no inner
  corner there. A pile cell appears nowhere else. A north-to-south bridge needs none: its
  piles are drawn beside its deck.
- **Bridges count as water for the water around them.** Choose the pieces of the
  neighboring water cells as if the bridge cells were water, so no bank is drawn against
  the deck.
- **Bridges count as road for the roads at their ends.** A road cell meets each end.
- Water blocks movement, so a bridge is the only way across. Every river that divides the
  walkable ground needs at least one.

A bridge over a river three cells wide, as runtime IDs, with the road on both sides:

```text
06 28 29 2A 06
66 60 61 62 66
06 9A 9B 9C 06
06 28 29 2A 06
```

## Rocks

Rocks and boulders stand on grass. Use them as obstacles, as scenery beside roads and
water, and as barriers along the map border.

A rock is drawn as a solid seen from above and in front: an outline, a ridge line between
its top and its front, and a second line inside the rim on its right and lower side, away
from the light. The outline is lumpy, never straight: it is the union of a few overlapping
rounded lumps. A few short strokes that run down to the left mark the front. The large
boulders reach the edges of their cells, so a band of them reads as one mass of rock.

| Hex | Symbol | Passable | Meaning |
| --- | --- | --- | --- |
| `0x34` | `ROCK_SMALL` | partly | Small rock. Solid on the bottom half. |
| `0x0C` | `ROCK_FLAT` | partly | Low flat rock. Solid on the bottom half. |
| `0x35` | `ROCKS` | no | Two rocks. |
| `0x36` | `BOULDER` | no | Round boulder. |
| `0x37` | `BOULDER_CRACKED` | no | Split boulder. |
| `0x0D` | `BOULDER_TALL` | no | Tall pointed boulder. |
| `0x0E` | `ROCK_PILE` | no | Pile of rocks. |
| `0x5E`, `0x5F` | `BOULDER_WIDE_LEFT`, `BOULDER_WIDE_RIGHT` | no | A wide boulder, two cells across. |
| `0x6E`, `0x6F`, `0x7E`, `0x7F` | `BOULDER_BIG_TOP_LEFT`, `BOULDER_BIG_TOP_RIGHT`, `BOULDER_BIG_BOTTOM_LEFT`, `BOULDER_BIG_BOTTOM_RIGHT` | no | A large boulder, 2x2 cells. |

- Rocks need no neighbors and may touch each other and anything else.
- The wide and large boulders must be complete: both cells of the wide one, and all four
  of the large one.
- Mix several kinds. A field of one rock repeats visibly.
- `ROCK_SMALL` and `ROCK_FLAT` block only their bottom half, or less. Do not rely on them
  alone to close a gap. Every other rock blocks its whole cell.

## Raised areas

A raised area is a plateau seen from the front and slightly above. Its floor is ordinary
grass. Its outline is drawn on the outermost raised cells: a thin wavering rim along the
back and the sides, and a wall one tile tall along the front. Walls face south only. The
pieces are `0x48` to `0x4D`, `0x58` to `0x5C`, `0x68` to `0x6B` and `0x78` to `0x7A`. Outer
corners are rounded, and the ground outside each rim is drawn as bare grass. Put grass
cells next to the outline, on the lower ground and on the floor.

**Every raised cell must be part of a 2x2 block of raised cells.** Every shape that follows
this rule has a piece for each of its cells. The edge of the map counts as raised.

Decide which cells are raised. Then choose the piece for each raised cell:

1. **Walls.** A raised cell with lower ground directly below it is a wall. It takes
   `CLIFF_WALL` (`0x69`). It takes `CLIFF_WALL_LEFT` (`0x68`) when lower ground is also on
   its left, and `CLIFF_WALL_RIGHT` (`0x6A`) when lower ground is also on its right. In a
   run of walls, paint about one `CLIFF_WALL` in three as `CLIFF_WALL_ALT` (`0x6B`).
2. **Floor.** Every other raised cell is floor. Work out its rims:
   - Top rim: the cell above is lower ground.
   - Left rim: the cell on the left is lower ground or a wall.
   - Right rim: the cell on the right is lower ground or a wall.
   - Top-left notch: the cell has no top rim and no left rim, and the cell diagonally
     above it on the left is lower ground. The top-right notch mirrors it.

| Rims | Piece | Symbol | Walkable part |
| --- | --- | --- | --- |
| none | any grass cell | | all |
| top | `0x49` | `CLIFF_TOP` | bottom half |
| top, left | `0x48` | `CLIFF_TOP_LEFT` | bottom-right quadrant |
| top, right | `0x4A` | `CLIFF_TOP_RIGHT` | bottom-left quadrant |
| left | `0x58` | `CLIFF_LEFT` | right half |
| right | `0x5A` | `CLIFF_RIGHT` | left half |
| top-left notch | `0x4B` | `CLIFF_INNER_TOP_LEFT` | all but the top-left quadrant |
| top-right notch | `0x4C` | `CLIFF_INNER_TOP_RIGHT` | all but the top-right quadrant |
| left, right | `0x4D` | `CLIFF_LEFT_RIGHT` | none |
| left, top-right notch | `0x5B` | `CLIFF_LEFT_INNER_TOP_RIGHT` | bottom-right quadrant |
| right, top-left notch | `0x5C` | `CLIFF_RIGHT_INNER_TOP_LEFT` | bottom-left quadrant |

The last three pieces appear only where a raised area narrows to a diagonal neck. They keep
the outline whole, but they leave little or no room to walk. Widen the area where the player
must cross.

Two layouts, as runtime IDs. The first is a 5x4 area with a ramp. The second is an L-shaped
area, where `4C` is the top-right notch.

```text
48 49 49 49 4A      48 4A
58 59 08 0A 5A      58 4C 49 4A
58 09 59 08 5A      68 69 6B 6A
68 69 78 6B 6A
```

Rules for raised areas:

- **Walls are solid, and rims block their own half of the cell.** The player on the floor
  stops half a tile short of a rim. NPCs treat every wall and rim piece as solid.
- **A ramp gives access.** `CLIFF_RAMP` (`0x78`) is fully walkable. It replaces a
  `CLIFF_WALL` or `CLIFF_WALL_ALT`, never `CLIFF_WALL_LEFT` or `CLIFF_WALL_RIGHT`. The
  cells on both of its sides stay raised. The cell above it is fully walkable floor, and
  the cell below it is walkable lower ground. Ramps exist on the south side only.
- **An area with no ramp is scenery.** Use it to close ground off without a wall of
  trees. The content test accepts unreachable floor only behind an unbroken outline of
  cliff pieces and map edges. A tree, a building or any other tile in place of an outline
  piece breaks the seal, and the test fails.
- **Borders.** Because the map edge counts as raised, one line of pieces closes a map
  edge: `CLIFF_WALL` along the top row, `CLIFF_TOP` along the bottom row, `CLIFF_RIGHT`
  down the left column and `CLIFF_LEFT` down the right column.
- **Terraces.** A raised area can stand on the floor of another. Treat the inner area the
  same way, and keep at least one floor cell between the two outlines.
- Use sparse grass for most of the floor. `GRASS_59` sits in the center of the cliff block
  for this purpose, but any grass cell works.

## Entrances

Three cells mark a way in or down. All three are fully walkable.

| Piece | Symbol | Meaning |
| --- | --- | --- |
| `0x79` | `CLIFF_CAVE` | Cave mouth in a cliff wall. |
| `0x7A` | `CLIFF_MINE` | Timber-framed mine or dungeon doorway in a cliff wall, with rails. |
| `0x0B` | `HOLE_LADDER` | Hole in the ground with the top of a ladder. |

- `CLIFF_CAVE` and `CLIFF_MINE` follow the ramp's placement rule. Each replaces a
  `CLIFF_WALL` or `CLIFF_WALL_ALT`, with raised cells on both sides and walkable lower
  ground below. They exist on the south side only.
- `HOLE_LADDER` goes on open ground, with at least one walkable cell beside it.
- Entering is content code. Put a point with the handler `take_passage` on the cell and on
  the doorway inside, and add the pair to `AreaPassages` in the header of the environment that
  it leads into (`src/logic/area_cave.h` or `src/logic/area_dungeon.h`). The handler calls
  `player_is_walking_onto<POS>()` and then `teleport_player<TARGET>()`. No tile sets `portal`.
  A marker that is missing from the tables fails to compile.
- The outdoor map has three: `CaveMouth` and `CaveHole` lead into `cave.tmx`, and
  `MineEntrance` leads into `dungeon.tmx`. [AUTHORING_CAVE.md](AUTHORING_CAVE.md#exits)
  describes both ends of a passage.
- **Give every entrance a handler that teleports.** The cells have no collision. A cave
  or mine with no handler acts as a ramp, and the player walks through it onto the floor
  above. A hole with no handler is walked over like grass. NPCs can also step onto an
  entrance, so keep routes and roaming areas away from them.
- A trip back out lands on a walkable cell next to the entrance, not on the entrance, so
  the handler does not fire again. For a cave or mine, that is the cell below it.

## Fences

Fences are drawn on grass in ten pieces. Posts are outlined and about half a tile tall,
with two posts per tile in each direction. A horizontal run stands on the bottom half of
its cell, and that half is solid. A vertical run stands on the left half of its cell, and
that half is solid. In a vertical run each post overlaps the post behind it.

| Piece | Symbol | Arms | Solid part |
| --- | --- | --- | --- |
| `0x88` | `FENCE_HORIZONTAL` | west, east | bottom half |
| `0x89` | `FENCE_VERTICAL` | north, south | left half |
| `0x8A` | `FENCE_END_WEST` | east | bottom-right quadrant |
| `0x8B` | `FENCE_END_EAST` | west | bottom half |
| `0x8C` | `FENCE_END_NORTH` | south | bottom-left quadrant |
| `0x8D` | `FENCE_END_SOUTH` | north | left half |
| `0x98` | `FENCE_TOP_LEFT` | east, south | bottom half |
| `0x99` | `FENCE_TOP_RIGHT` | west, south | bottom-left quadrant |
| `0xA8` | `FENCE_BOTTOM_LEFT` | north, east | left half and bottom half |
| `0xA9` | `FENCE_BOTTOM_RIGHT` | north, west | left half |

Rules for fences:

- **Arms match.** An east arm needs a west arm in the cell to its east, and a south arm
  needs a north arm in the cell below. An arm that points at a cell with no matching arm
  shows as a cut rail or a cut post.
- **Runs end in an end piece.** A horizontal run goes `FENCE_END_WEST`, `FENCE_HORIZONTAL`
  cells, `FENCE_END_EAST`. A vertical run goes `FENCE_END_NORTH`, `FENCE_VERTICAL` cells,
  `FENCE_END_SOUTH`. `FENCE_END_WEST` and `FENCE_END_NORTH` hold one post, so those ends
  start half a tile into the cell.
- **Corners turn a run.** The four corner pieces are named for their place in a
  rectangular pen. There are no pieces for three-way or four-way junctions.
- **Fences are half a cell deep.** The top half of a horizontal piece and the right half
  of a vertical piece are walkable grass. The right-hand column of a pen holds its fence on
  the left half, so the right half of those cells lies outside the pen. The player's head
  overlaps a horizontal fence from behind, as it should in this view.
- **Leave a gap.** A fence has no gate piece. End the run on both sides of the opening.
  A pen with fully walkable cells inside needs an opening, because the content test
  requires those cells to be reachable from the spawn.

A 5x4 pen with an opening in its bottom side, as runtime IDs. `06` stands for any grass cell.

```text
98 88 88 88 99
89 06 06 06 89
89 06 06 06 89
A8 8B 06 8A A9
```

## Buildings

A building is base walls with roofs above them. Base wall sections can join at the same
or different y-levels, and one building can mix roof types.

### Base walls

- A wall faces the viewer, so it is dark inside its outline, with a few bricks marked. It
  stands on a plinth whose top is a lit line. The west corner post is lit, and the east one
  is hollow, because it is in shade. Windows have a lit sill, and a door has a lit lintel and
  a lit step.
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

Slanted roofs are only for 3-wide base walls, `0xc0` to `0xc2`. The house shows its gable.
A ridge runs from the back peak down to the gable's peak, with a slope on each side of it,
and the wall shows below the gable's two rakes.

A slanted roof is drawn as the trees and rocks are: dark inside white outlines, never filled
white. Both slopes carry shingles: short slanted strokes, single or in pairs, at irregular
places with a gap around each. A stroke leans the way its slope runs, so the west slope's
lean up to the right and the east slope's lean up to the left. The east slope is in shade:
a second line runs inside its east rim and its back edge.

- **A rake is two pixels thick, thickened toward the wall.** Its inner line ends on the
  corner pixel of the wall cell below it, `0xb1` or `0x80`, so the gable's front edge runs
  unbroken from cell to cell.
- **A window nook stands out of the slope.** Its window faces along the roof, so it is not
  seen. `0x82` and `0x83` show the nook's own small roof from above and, below it, the end
  wall as a triangle, because the wall's foot follows the slope. A nook sits toward the
  ridge, clear of the roof's rim, so `0x83` keeps the east rim's second line as `0xa2` has it.
- **The east slope's second line is unbroken.** It runs two pixels inside the back edge,
  from `0x81` across the top-right corner of the cell below the peak (`0x91` or `0xa1`),
  through `0x92`, and down the inside of the east rim.
- **A chimney's foot follows the slope.** On the east slope its east side meets the tiles
  lower than its west side, on a line that runs parallel to the rake. On a flat roof its foot
  is level.

- The bottom roof tiles `0xb0`, `0xb1` and `0xb2` sit directly above the base wall.
- `0xa1` sits above `0xb1`.
- The top edge is `0x90`, `0x81` and `0x92`: `0x90` above `0xb0`, `0x81` above `0xa1`,
  and `0x92` above `0xb2`.
- To extend the roof deeper, add the middle roof tiles `0xa0`, `0x91` and `0xa2` between
  the bottom roof and the top edge: `0xa0` above `0xb0`, `0x91` above `0xa1`, and `0xa2`
  above `0xb2`. Then place the top edge above them: `0x90` above `0xa0`, `0x81` above
  `0x91`, and `0x92` above `0xa2`.
- Three pieces have a decorative alternative that fits the same place. `0x80` replaces
  `0xb1` and adds a window to the gable. `0x82` replaces `0xa0`, and `0x83` replaces
  `0xa2`; each adds a window to that side of the roof. Use them on some houses for variety.

### Flat roofs

Flat roofs sit above base walls that are two or more tiles wide. They use the 3x3 convex
set from `0xa4` to `0xc6`:

- Corners: `0xa4` top-left, `0xa6` top-right, `0xc4` bottom-left, and `0xc6` bottom-right.
- Edges: `0xa5` top, `0xb4` left, `0xb6` right, and `0xc5` bottom.
- Center: `0xb5`, which has no outline.

The outline sits on the outside of the roof, so choose each tile by the sides that face
outside the roof, as for roads.

A flat roof is dark inside a white outline, and its covering is small slabs of several
sizes: squares, short bars and single pixels, set at irregular places with a gap around each.
Keep the placing natural. A perfect pattern reads as a grid, not as a roof, and a white fill
is too heavy. Its east edge and its front edge take a second line inside the rim, because
they face away from the light.

### One-wide roofs

A one-wide base wall, `0xc3`, takes `0xb3` directly above it, `0xa3` above `0xb3`, and
`0x81` above `0xa3` as the top and back edge.

### Solidity

Walls, doors and roofs are solid, except the shaped top-edge pieces `0x81`, `0x90` and
`0x92`. See [What solidity means](#what-solidity-means).

### Doors and interiors

The player enters a house by standing in front of its door, facing it, and pressing A. The
inside of every house is a room in `interiors.tmx`. Walking onto the room's exit returns
the player to the cell in front of the door. See
[AUTHORING_INTERIOR.md](AUTHORING_INTERIOR.md#the-house-map).

- Put a point named `<House>Door` on the door tile, with the handler `enter_house`.
- Put a point named `<House>Exit` on the room's exit cell in `interiors.tmx`, with the
  handler `leave_house`.
- Add the pair to `HOUSE_LINKS` in `src/logic/area_village.h`. A door or exit that is missing
  from the table fails to compile.
- The cell below the door is where the player returns. Keep it fully walkable.

## Animation

The world moves in three ways. None of them keeps any state: each is computed from the game
tick and a world position, so the animation needs no RAM and is not saved.

### Animated tiles

Every tile can animate through a cycle of four phases, 160 ms each. Phase 0 is the atlas cell.
The other phases are drawn in **layer atlases** beside the atlas, so animation frames take no
atlas cells:

- `assets/images/tiles16_phase1.png`, `tiles16_phase2.png` and `tiles16_phase3.png` have the
  layout of `tiles16.png`. Draw the tile's art for that phase in the tile's own cell.
- A layer cell is fully transparent or fully opaque monochrome. A transparent cell repeats
  the atlas cell in that phase. A tile with two frames draws phases 2 and 3 only, so it shows
  each frame for 320 ms.
- The generator emits a whole sprite sheet per phase, and the game draws a phase by selecting
  its sheet. An animated tile therefore costs nothing per tile at run time, and any number of
  tiles can animate. A phase with no art at all shares an earlier sheet.
- An animated tile keeps one collision mask and one symbol. Animation never changes how a
  tile plays.

Animated tiles so far: all 24 water cells and the three pile cells (the ripples move and the
water laps at the shore and the piles), `FLOWERS_26` and
`FLOWERS_27` (the blossoms sway), the two chimneys (smoke), `TORCH_POST`, `CAMPFIRE`, `FLAG`,
`REEDS` and `SCARECROW`.

| Hex | Symbol | Passable | Meaning |
| --- | --- | --- | --- |
| `0x4E` | `ROOF_FLAT_CHIMNEY` | no | Flat roof center with a smoking chimney. It replaces a `0xb5`. |
| `0x4F` | `ROOF_SLOPE_CHIMNEY` | partly | Slanted roof top-right edge with a smoking chimney. It replaces a `0x92` and has its collision. |
| `0x7B` | `SCARECROW` | partly | Scarecrow with fluttering sleeves. For pens and fields. Blocks the diamond. |
| `0x86` | `TORCH_POST` | partly | Burning torch on a post. It stands on the left of its cell, and only the left half blocks. Put it on grass beside a road or a door path, never on the path. |
| `0x87` | `CAMPFIRE` | partly | Burning campfire. Blocks the diamond. |
| `0x8E` | `REEDS` | yes | Reeds on the bank, swaying. Put it on a land cell that touches water. |
| `0x8F` | `FLAG` | partly | Flag on a pole, waving. The pole stands on the left of its cell, and only the left half blocks. |

Give a house one chimney at most. `REEDS` is the one walkable cell that is not grass, so no
gust bends it and no rabbit sits in it.

### Wind

A gust is a diagonal line of bent grass that rolls across the map toward the south-east.
`assets/images/tiles16_wind.png` holds the bent frame of each grass and flowers cell, in the
atlas layout. Exactly the tiles of the ambient classes `grass` and `flowers` have a wind
frame; the generator rejects anything else.

### Ambient creatures and effects

Each 8x4 chunk picks one of its tiles every 2.56 seconds. The tile's **ambient class**, the
optional `ambient` string property in `world.tsx`, decides what may appear there:

| Class | Tiles | What appears |
| --- | --- | --- |
| `grass` | the eight grass cells | A butterfly, or a rabbit that sits up and looks around |
| `flowers` | `FLOWERS_26`, `FLOWERS_27` | A butterfly or a bee |
| `water` | all water cells | A jumping fish, a ripple ring or a glint |
| `tree` | the round-tree pieces and `TREE` | A falling leaf, or a bird that leaves the crown and returns |
| `perch` | `SIGN`, the solid rocks and boulders | A bird that flies down, perches, pecks and leaves |
| `road` | the 21 road cells | A dry leaf skipping along on the wind |
| `steam` | the chimneys and `CAMPFIRE` | A puff of smoke rising |

Birds also cross the sky above any map whose tileset sets the tileset property `sky`.

The interior tilesets have classes of their own. `floor` is the house's. `cave`, `crypt`,
`drip`, `pool`, `crystal` and `ember` belong to caves and dungeons: bats, wisps, falling
drops, glints and sparks. [AUTHORING_CAVE.md](AUTHORING_CAVE.md#animation) and
[AUTHORING_DUNGEON.md](AUTHORING_DUNGEON.md#animation) list them.

- The sprites are the 16x16 frames of `assets/images/ambient16.png`, in rows of eight.
  `src/ambient.h` names the frames and holds every motion and chance.
- A new tile takes a class when one of these effects suits it. A `perch` tile is solid and
  has its top within three pixels of the top of its cell, because the bird stands there.
  `tree`, `perch` and `water` tiles are never walkable, so no actor stands under an effect
  that belongs on top of the tile.
- Keep maps varied. A screen of one tile class shows one kind of effect.

## Atlas layout

Descriptions of unnamed regions are approximate and come from visual inspection.
Check a cell before you rely on it. A cell with no entry here is solid by default.

- `0x01` to `0x04`, `0x10` to `0x14`, `0x21`, `0x23` and `0x24`: round trees and their
  pieces. Solid, except where a shape in [What solidity means](#what-solidity-means)
  opens part of the cell. The named cells are in the table. `0x05`, `0x15` and `0x22` are
  blank black cells, and solid.
- `0x08` to `0x0A`: sparse grass, walkable. `0x0B`: the hole with a ladder, walkable. See
  [Entrances](#entrances).
- `0x0C` to `0x0E`: rocks. See [Rocks](#rocks). `0x0F`: a blank black cell. Solid.
- `0x18` to `0x1F`, `0x28` to `0x2F` and `0x38` to `0x3F`: water. Solid. See [Water](#water).
- `0x25`: a blank black cell that is solid. It looks like floor but is not.
- `0x34` to `0x37`: rocks and boulders. See [Rocks](#rocks).
- `0x40` and `0x50` to `0x53`: the extra road pieces for paved squares. Walkable.
- `0x44` to `0x47`, `0x54` to `0x57`, `0x64` to `0x67` and `0x74` to `0x77`: the sixteen
  road pieces. Walkable. See [Roads](#roads).
- `0x60` to `0x62` and `0x70` to `0x72`: bridges. Walkable. See [Bridges](#bridges).
- `0x5E`, `0x5F`, `0x6E`, `0x6F`, `0x7E` and `0x7F`: the wide and large boulders.
- `0x48` to `0x4D`, `0x58` to `0x5C`, `0x68` to `0x6B` and `0x78` to `0x7A`: raised areas
  and their entrances. `0x59` is sparse grass. See [Raised areas](#raised-areas).
- `0x4E`, `0x4F`, `0x7B`, `0x86`, `0x87`, `0x8E` and `0x8F`: the animated scenery tiles. See
  [Animation](#animation).
- `0x80` to `0x85`: building fronts, doors and windows. Solid, except `0x81`, which is a
  triangle. `0x80`, `0x82` and `0x83` are decorative roof alternatives. See the shapes list
  and [Buildings](#buildings).
- `0x88` to `0x8D`, `0x98`, `0x99`, `0xA8` and `0xA9`: fences. See [Fences](#fences).
- `0x9A` to `0x9C`: the piles under an east-to-west bridge. Solid. See [Bridges](#bridges).
- `0x90` to `0x97`: wall and window panels, and signboards. Solid, except the ramps `0x90`
  and `0x92`. Decorated base walls and roof edges: see [Buildings](#buildings).
- `0xA0` to `0xA7`, `0xB0` to `0xB7` and `0xC0` to `0xC7`: roofs, hatching, paving and
  framed pavers. Solid. Roofs and base walls: see [Buildings](#buildings).

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
  - `0x01` and `0x0C`: solid on the bottom half (`0xFF00`).
  - `0x03`: top-left quadrant open (`0xFFF0`).
  - `0x04`: top-right quadrant open (`0xFF0F`).
  - `0x13`: bottom-left quadrant open (`0xF0FF`).
  - `0x14`: bottom-right quadrant open (`0x0FFF`).
  - `0x10`: solid on the right half (`0xF0F0`).
  - `0x12`: solid on the left half (`0x0F0F`).
  - `0x20`: solid except a house roof at the top, with 45-degree slopes from the peak at
    top center to both top corners (`0xFFC6`).
  - `0x21`: solid on the top half (`0x00FF`).
  - `0x81`: solid triangle with vertices at the two bottom corners and the center (`0xC600`).
  - `0x90`: ramp, solid below the diagonal from bottom-left to top-right (`0xF660`).
  - `0x92`: ramp, solid below the diagonal from top-left to bottom-right (`0xCF0C`).
  - `0x49`: solid on the top half (`0x00FF`).
  - `0x88`, `0x8B` and `0x98`: solid on the bottom half (`0xFF00`).
  - `0x58`, `0x89`, `0x8D` and `0xA9`: solid on the left half (`0x0F0F`).
  - `0x8C` and `0x99`: solid on the bottom-left quadrant (`0x0F00`).
  - `0x8A`: solid on the bottom-right quadrant (`0xF000`).
  - `0xA8`: top-right quadrant open (`0xFF0F`).
  - `0x5A`: solid on the right half (`0xF0F0`).
  - `0x48` and `0x5B`: bottom-right quadrant open (`0x0FFF`).
  - `0x4A` and `0x5C`: bottom-left quadrant open (`0xF0FF`).
  - `0x4B`: solid on the top-left quadrant (`0x000F`).
  - `0x4C`: solid on the top-right quadrant (`0x00F0`).
- **Objects take a simple convex shape.** A mask only needs to follow its art roughly. An
  object takes one of these shapes, the one that covers its art best:

  | Mask | Shape | For |
  | --- | --- | --- |
  | `0xFFFF` | the whole cell | A piece that fills its cell: a boulder, a table, a crate |
  | `0x93C6` | the diamond, with its points at the middle of each side | A small round piece: a lever, a campfire, a lantern |
  | `0xFFC6` | the cell without its top corners | A piece with a rounded or narrow top: a pot, a chest, a sack |
  | `0x93FF` | the cell without its bottom corners | A piece that narrows to a foot: a round tree, a chair |
  | `0xFF00`, `0x00FF`, `0x0F0F`, `0xF0F0` | a half | A low piece, or a post that stands on one side |
  | `0x0F00`, `0xF000`, `0x000F`, `0x00F0` | a quadrant | A piece in one corner |
  | `0xC600`, `0x0093`, `0x090C`, `0x3060` | half of the diamond, on the bottom, top, left or right edge | A small heap against one edge |
  | `0xF3F6`, `0x9FCF` | the cell without its left or right corners | A piece that narrows to one side |

  - **Avoid concave shapes.** Only terrain that crosses cell corners uses one: the three
    quadrants of a tree clump, a cliff corner or a fence corner. An object never does.
  - **States and pieces of one thing take the same mask.** A lever blocks the same way thrown
    or not, a chest shut or open, and every piece of a long table. A state changes the mask
    only when it changes what is physically there: a gate that opens, a pot that breaks.
  - **A tile is not fully solid only because it is an obstacle.** A piece that leaves much of
    its cell bare takes a smaller shape. Terrain that fills its cell stays fully solid: water,
    cliff walls, buildings and the large boulders.
  - **A post stands on one side of its cell.** `TORCH_POST` and `FLAG` stand on the left, and
    only the left half blocks.
  - The world's objects: `TREE_ROUND` (cell without its bottom corners), `SWITCH_RIGHT` (cell without its top corners), `SWITCH_LEFT` (cell without its top corners), `POT` (cell without its top corners), `POT_BROKEN` (bottom half), `ROCK_SMALL` (bottom half), `ROCK_FLAT` (bottom half), `CHEST_CLOSED` (cell without its top corners), `CHEST_OPEN` (cell without its top corners), `SCARECROW` (diamond), `CAMPFIRE` (diamond), `TORCH_POST` (left half), `FLAG` (left half).
  - Check the red area in `tiles16_collision.png` against the art after you set a mask.
- **Pixel rule.** A pixel is solid when its center lies in a solid triangle. A center on a
  quadrant diagonal lies on the edge of two triangles, and it is solid when either one is.
- **Corners slide.** When terrain blocks the player's next pixel, the player takes one pixel
  sideways toward a position that clears the step, then keeps walking. The sidestep is a
  normal one-pixel move, so a corner a few pixels deep no longer stops smooth movement,
  and a slope's edge is followed the same way. Facing does not change. A deeper overlap
  stays blocked, and NPCs never sidestep.
- **Unpainted cells are outside, not ground.** An empty CSV cell (GID `0`) becomes the
  sentinel, which is not walkable. Paint every cell that should be playable.
- **Interactive tiles block.** Pots, chests and levers take the shapes above, and
  `SIGN` blocks its whole cell. The player interacts by standing next to one and facing it,
  so its handler point goes on the tile itself, as `read_sign` does on `SIGN`. The cave and
  the dungeon use levers and chests: see their guides.
- **Each area has its own tileset.** Collision comes from the tileset of the active area,
  so the masks here apply on maps that use `world.tsx`.
- **Collectibles change state.** A collected `POT` becomes `POT_BROKEN`, and a collected
  `CHEST_CLOSED` becomes `CHEST_OPEN`. The map shows the initial state. Content code must
  record the collected state in a world flag, because the cache is rebuilt from the map
  when it reloads.
  For `open_chest`, add the chest's mechanism row and specialize `ChestReward<positions::Name>`
  in the area's logic header. The shared handler changes the flag and lid after the reward
  succeeds. `ContainerReward` validates and grants the combined gold, supplies and equipment reward.
- **Portals.** No tile sets `portal=true` yet. The flag stops an NPC from spawning
  when its foot centre lies on that cell. Doorways are not driven by tile flags.
  Content code triggers them from a handler with `teleport_player<POS>()`, as the house
  doors do. See [Doors and interiors](#doors-and-interiors).

## Layout principles

These rules describe how roads, villages and trees should look. Follow them when you
author a map by hand.

- **Doors, not walls.** A road touches a building only at its door. The row below a base
  wall is grass, except for the door's front tile. A footpath runs from that tile to the
  road network, and its first tile runs straight south, because the tiles beside it touch
  the wall.
- **Organic villages.** Do not line houses up on one row. Space them at irregular
  intervals and distances from the street. Each door's footpath can be a different length.
- **Roads one tile wide.** Streets and footpaths are one tile wide. A paved square of 2x2
  or 3x3 marks the place where streets meet.
- **Winding, not straight.** Roads step sideways one cell every four to twelve cells.
  Avoid straight runs longer than about fourteen tiles, and zigzags with runs of one or
  two tiles.
- **Roads lead somewhere.** A road ends at a junction, a door, a bridge or the edge of the
  settled area. Not every house needs a road: leave some doors on open grass.
- **Rivers shape a village.** Run a river past or through a settlement, cross it with a
  bridge where a street meets it, and let it continue to the sea or the map edge.
- **Trees around roads.** Scatter single trees and small clumps near roads, two to four
  tiles away, thinning with distance. A single tree never touches another tree.
- **Soft forest edges.** Forest edges are not hard lines. Vary the depth of a forest from
  cell to cell, and put loose trees near it.
- **A varied border.** The map border changes its barrier every few dozen cells: forest,
  cliffs, water and rocks. See [Building a map](#building-a-map).
- **Calm and busy ground.** Vary the grass density from place to place. Open ground is
  mostly sparse grass, and overgrown ground is mostly dense grass.
- **Water with room.** Lakes and rivers are at least two tiles wide everywhere. Rivers wind
  as roads do. Where a river divides the walkable ground, a bridge joins the two sides.
- **Cliffs close ground off.** A raised area with no ramp is a natural barrier. Use it in
  place of a wall of trees. Give an area a ramp only where the player should climb it.
- **Fences mark places.** Use fences for pens, gardens and yard edges near buildings, with
  an opening where the player should enter.
- **Rocks break up open ground.** Scatter rocks of several kinds near water, cliffs and
  roads, and a boulder or two in a meadow.

## Building a map

- Use one Ground tile layer that covers the whole map. Layers overwrite each other
  wherever they are non-zero. Keep one layer unless you have a specific reason.
- Keep the logical size within 240x248. Design rooms in view-sized blocks. The view is
  8x4 tiles (128x64 pixels).
- Align room edges and handler regions to the 8x4 chunk grid. Each chunk dispatches
  its own handlers, and a handler point belongs to the chunk that contains it.
- Map properties: `area_id` must be unique, and the area IDs of all maps must run 1..N with no
  gaps, where N is `NUM_AREAS` in `area_ids.h`. `symbol` must be unique. `spawn`
  names a single named point in the same area. That tile must be passable and must not
  carry a handler.
- Floors use the eight grass cells in a mix of sparse and dense. Add flowers sparingly,
  about one floor cell in eight, and never fill a room with them.
- Lay roads with the road pieces in [Roads](#roads), not with grass cells.
- **The border keeps the player off the map edge.** Close every cell of the perimeter
  with a barrier, and use several kinds around one map:
  - Forest: `0x11` cells two or more deep, with the fringe pieces of
    [Round trees](#round-trees) on the inside.
  - Cliffs: a raised area that runs off the map. See [Raised areas](#raised-areas).
  - Water: a sea or lake two or more cells deep, or a river that runs off the map.
  - Rocks: a band of fully solid rocks and boulders two or more cells deep.
- **Tiling terrain continues beyond the map.** Forest, water and raised ground behave as
  if there were more of the same outside. The map edge counts as water for water pieces,
  as raised for cliff pieces, and as a matching seam for tree pieces. Place half a tree at
  the edge, run a river off it, and leave a plateau open toward it.
- **Change barriers with an overlap.** Where one kind of barrier ends and the next begins,
  put fully solid rocks between them, so no walkable gap reaches the edge. The content
  test floods from the spawn and fails if any edge cell can be reached.
- Outside the settled area the map may stay open grass. Mix sparse and dense grass in
  drifts so large meadows do not look uniform.

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
- **Keep the one view.** Everything is seen from the front and slightly above. An object
  shows its top as well as its front: a chest shows its lid, a pot shows its mouth, and a
  rock shows the ridge between its top and its front. Do not draw an object from the side
  only.
- **Shade the same way everywhere.** A small flat top that faces up, such as a chest's lid or
  a bridge's deck, is lit, so it is filled white. A large surface, such as a roof, stays dark
  inside its outline.
  A front is dark inside its outline. A round dark surface takes a second line inside its
  rim, or a few hatch strokes, on the side away from the light.
- **The light comes from the top left, for everything.** Trees and rocks are shaded on their
  right and lower side. A roof takes a second line inside its east rim and, on a slanted
  roof, inside the back edge of its east slope.
  Draw every new piece, buildings included, under the same light.
- **Never fill an area with a checkerboard stipple.** The view scrolls one pixel at a time,
  and a stipple inverts at every step, so it flickers. The content test rejects a 4x4 block
  of it in any tile or animation frame.
- **Black is the background.** A drawn tile overwrites what is beneath it. White
  carries the detail.
- **Make silhouettes read at 16 pixels.** Use bold shapes. Keep outlines one pixel wide
  and consistent across a set. Avoid speckles that vanish next to solid shapes.
- **Put the bases of solid objects at the bottom of the cell.** The feet stop at the
  bottom edge, so trunks, rocks and walls should meet the ground there.
- **Tile round trees.** Do not place large clumps of `TREE_ROUND` next to each other.
  Tile groups of trees with the plus, split and clump pieces instead. `TREE_ROUND` is
  for isolated single trees.
- **Vary the floor.** Mix the eight grass cells, sparse and dense, and add flowers only as
  occasional accents. Do not use a solid cell that looks like floor (`0x25`) as floor.
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
   `collision` mask and `portal` value. Symbols must be unique. Fit the mask to the art:
   see [What solidity means](#what-solidity-means).
3. Regenerate. Renaming a symbol changes its `TILE_<symbol>` constant, so update any
   code that refers to it.
4. Check every map that uses the cell. Changing `collision` changes how every map plays.

## Before committing a map

- A new animated tile has its frames in the phase layer atlases, not in atlas cells, and
  every grass or flowers tile has a wind frame.
- Chimneys replace `0xb5` or `0x92` only, torches stand beside paths and not on them, and
  every `REEDS` cell touches water.

- The generator, `test_pipeline.py` and `test_content.py` all pass.
- Roads touch a building only at its door front. Every road cell uses the piece that its
  four neighbors select in [Roads](#roads), and roads wind gently.
- Every east-to-west bridge has its row of `WATER_PILES` cells directly below it.
- Every bridge spans its river from bank to bank, over straight water, with a road at
  each end.
- No cell on the map edge can be reached from the spawn, and the border uses several kinds
  of barrier.
- Every house door has a `<House>Door` point with `enter_house`, a matching `<House>Exit`
  in `interiors.tmx`, an entry in `HOUSE_LINKS`, and a walkable cell below it.
- Single trees never touch another tree, and loose trees sit near roads.
- No `EMPTY` (`0x00`) cell is painted, and the floor mixes sparse and dense grass cells.
- The spawn is passable, and every named point and route is reachable from the spawn
  over passable tiles.
- Every fully walkable cell is reachable from the spawn, except floor that an unbroken
  outline of cliff pieces seals off.
- Every water cell is part of a 2x2 block of water and uses the piece that its sides and
  inner corners select in [Water](#water).
- Every raised cell is part of a 2x2 block of raised cells and uses the piece that
  [Raised areas](#raised-areas) selects. Ramps, caves and mines replace a `CLIFF_WALL`.
- Every fence arm meets a matching arm, every run ends in an end piece or a corner, and
  every pen has an opening.
- Every cave, mine and hole has a `take_passage` point on its cell and a row in `AreaPassages`.
- Every object's mask is one of the convex shapes, states of one thing share a mask, and a
  torch stands clear of the path.
- No tile holds a checkerboard stipple, and every piece is lit from the top left.
- No unpainted cell lies inside the playable area.
- Every handler point sits on the tile it describes, and it does not share a cell with
  the spawn.
- Route waypoints are on passable tiles and clear of edges and corners.
- No large clumps of `TREE_ROUND` sit next to each other. Groups use the tiling pieces.
- Each building has exactly one door, on an interior base wall tile (`0x84` or `0x85`).
- Every base wall run starts and ends with a side piece (`0xc0` or `0xc2`, or `0x93` or
  `0x96`), or is a single `0xc3`.
- Every tree seam matches its neighbor. Seams may face outside the map (see Round trees).
- Device QA passes when a change touches NPCs, collisions or sightlines.
