# Interior map and tile authoring guide

This guide tells agents how to build houses, caves and dungeons with the interior tileset.
It sits beside [AUTHORING.md](AUTHORING.md), which covers the outdoor tileset. The rules
there for tile IDs, collision masks, objects, handlers and routes apply here too. Read it
first.

Houses come first in this guide. [Caves](#caves) and [Dungeons](#dungeons) use the same room
model with their own pieces.

## Status

**Interior maps do not export yet.** The exporter draws every map from `tiles16.png`, and
all tilesets share one runtime ID space. `interior.tsx` reuses IDs 0 to 254, so a map that
references it fails with `conflicting runtime tile IDs across tilesets`. The game needs to
choose a tileset per area before an interior map can ship. Until then the generator checks
`interior.tsx` and its atlas on every run and writes the collision overlay. This guide
describes how to author interior maps for when that support exists.

## Files

- `assets/tilesets/interior.tsx`: metadata for every interior atlas cell.
- `assets/images/tiles16_interior.png`: the 256x256 monochrome interior atlas.
- `assets/images/tiles16_interior_collision.png`: the atlas with solid collision pixels
  tinted red. The generator writes it on every run.
- An interior map is its own TMX file, with `interior.tsx` as its only tileset. Do not mix
  it with `world.tsx` in one map.

## Tile IDs

- A tile's runtime ID is its atlas cell index, `row * 16 + column`, as outdoors.
- `0x00` is `EMPTY`. Never paint it. `0xFF` is reserved.
- Named cells use their symbol. Unnamed cells are `ATLAS_XX` and are solid.
- Interior symbols repeat some outdoor names, such as `POT` and `CHEST_CLOSED`. They are
  separate cells in a separate atlas.

## How a room is drawn

A room is seen from the front and slightly above. The floor is black with a pattern. Walls
are white bands three pixels thick around it. The north wall also shows its face, one tile
tall, which carries windows, pictures and tall furniture. Everything outside is black void.

Every cell of an interior map is one of four kinds:

- **Room cell.** A floor cell, a rug cell, or anything that stands on the floor. Furniture
  is opaque, so it hides the floor under it.
- **Wall row cell.** A cell that is not a room cell and has a room cell directly below it.
  It shows the north wall from the front.
- **Edge cell.** Any other cell that touches the room. It is void with a band on each side
  that faces the room.
- **Void.** Everything else. Leave it unpainted.

Build a room in this order:

1. Paint the room cells with floor.
2. Paint the wall row. Each wall row cell takes a wall face from [Wall faces](#wall-faces),
   or a piece from [Wall decorations](#wall-decorations) or
   [Doors and stairs](#doors-and-stairs).
3. Paint the edge cells with the pieces that [Edges](#edges) selects.
4. Place rugs, furniture and objects on the floor.
5. Add doorways, then the handler points that make them work.

A cottage with a 6x3 floor, as runtime IDs. `..` is void.

```text
03 10 20 37 10 24 10 02
03 64 65 42 42 5A 75 02
03 74 40 4E 41 58 40 02
03 41 40 4C 42 4E 7A 02
09 01 01 0D 01 01 01 08
```

Its wall row holds a window, a bookcase and a sconce. A bed, a dresser, a chair, a set
table, a plant and a chest stand on a wood floor. A doormat lies above the doorway in the
south wall.

## Edges

Edge cells are solid. Work out the bands of each one:

- **Top band:** the cell above is a room cell.
- **Left band:** the cell on the left is a room cell or a wall row cell.
- **Right band:** the cell on the right is a room cell or a wall row cell.
- **Top-left dot:** the cell has no top band and no left band, and the cell diagonally
  above it on the left is a room cell. The top-right dot mirrors it.

| Hex | Symbol | Bands | Use |
| --- | --- | --- | --- |
| `0x01` | `EDGE_TOP` | top | South wall of the room above it. |
| `0x02` | `EDGE_LEFT` | left | East wall of the room on its left. |
| `0x03` | `EDGE_RIGHT` | right | West wall of the room on its right. |
| `0x04` | `EDGE_TOP_LEFT` | top, left | Wall corner that juts into a room, with the room above and on the left. |
| `0x05` | `EDGE_TOP_RIGHT` | top, right | Wall corner that juts into a room, with the room above and on the right. |
| `0x06` | `EDGE_LEFT_RIGHT` | left, right | Wall one cell thick between two rooms side by side. |
| `0x07` | `EDGE_TOP_LEFT_RIGHT` | top, left, right | Top of a wall stub one cell wide. |
| `0x08` | `EDGE_DOT_LEFT` | top-left dot | Bottom-right outer corner of a room. |
| `0x09` | `EDGE_DOT_RIGHT` | top-right dot | Bottom-left outer corner of a room. |
| `0x0A` | `EDGE_DOT_BOTH` | both dots | Outer corner of two rooms at once. |
| `0x0B` | `EDGE_LEFT_DOT_RIGHT` | left, top-right dot | East wall of one room and outer corner of another. |
| `0x0C` | `EDGE_RIGHT_DOT_LEFT` | right, top-left dot | West wall of one room and outer corner of another. |

An edge cell with no band and no dot is void. These twelve pieces cover every room shape,
including walls one cell thick. A wall row cell needs no edge cell above it, because the
wall face carries its own top band. A wall one cell thick between a room above and a room
below is a row of wall faces: their top band is the south wall of the upper room.

## Wall faces

A wall face has the wall top band on its top three rows, the face below it and a skirting
line on its bottom row. Wall faces are solid. There are three styles. Use one style for
all the walls of a room.

| Hex | Symbol | Room cell on its left or right | Meaning |
| --- | --- | --- | --- |
| `0x10` | `WALL` | neither | Plaster wall. |
| `0x11` | `WALL_END_LEFT` | left | Plaster wall with its left end exposed. |
| `0x12` | `WALL_END_RIGHT` | right | Plaster wall with its right end exposed. |
| `0x13` | `WALL_PILLAR` | both | Plaster wall one cell wide. |
| `0x14` | `WALL_WOOD` | neither | Wood-panelled wall. |
| `0x15` | `WALL_WOOD_END_LEFT` | left | Wood-panelled wall with its left end exposed. |
| `0x16` | `WALL_WOOD_END_RIGHT` | right | Wood-panelled wall with its right end exposed. |
| `0x17` | `WALL_WOOD_PILLAR` | both | Wood-panelled wall one cell wide. |
| `0x18` | `WALL_STONE` | neither | Stone block wall, for cellars and halls. |
| `0x19` | `WALL_STONE_END_LEFT` | left | Stone wall with its left end exposed. |
| `0x1A` | `WALL_STONE_END_RIGHT` | right | Stone wall with its right end exposed. |
| `0x1B` | `WALL_STONE_PILLAR` | both | Stone wall one cell wide. |

The end and pillar pieces close the side of a wall that stands next to a room cell. That
happens where a room is L-shaped, or where a wall stub juts into a room.

## Wall decorations

Each of these replaces a plain wall face in the wall row. All of them are solid. They are
drawn without a wall pattern, so they suit all three wall styles.

| Hex | Symbol | Passable | Meaning |
| --- | --- | --- | --- |
| `0x20` | `WINDOW` | no | Four-pane window. |
| `0x21` | `WINDOW_CURTAINS` | no | Window with curtains drawn back. |
| `0x22` | `PAINTING_LANDSCAPE` | no | Framed landscape painting. |
| `0x23` | `PAINTING_PORTRAIT` | no | Framed portrait. |
| `0x24` | `SCONCE` | no | Wall sconce with a lit candle. |
| `0x25` | `MIRROR` | no | Round mirror. |
| `0x26` | `CLOCK` | no | Wall clock with a pendulum. |
| `0x27` | `BANNER` | no | Hanging banner. |
| `0x28` | `SHELF_BOTTLES` | no | Wall shelf with bottles and jars. |
| `0x29` | `WEAPONS` | no | Shield over crossed swords. |
| `0x2A` | `TROPHY` | no | Mounted antlers. |
| `0x2B` | `HANGING_PANS` | no | Rail with a pan and ladles. Kitchen walls. |
| `0x35` | `FIREPLACE` | no | Unlit fireplace. |
| `0x36` | `FIREPLACE_LIT` | no | Fireplace with a fire. |
| `0x37` | `BOOKCASE` | no | Tall bookcase, full. |
| `0x38` | `BOOKCASE_SPARSE` | no | Tall bookcase with gaps on its shelves. |
| `0x39` | `CUPBOARD` | no | Kitchen cupboard with dishes. |
| `0x3A` | `WARDROBE` | no | Wardrobe with two doors. |

Rules for the wall row:

- A decoration needs wall row cells or edge cells on both sides. Only the plain wall faces
  have end pieces, so a decoration cannot stand next to a room cell.
- Tall furniture belongs here. `BOOKCASE`, `CUPBOARD`, `WARDROBE` and the fireplaces stand
  against the north wall and take no floor space.
- Leave plain wall between decorations. Two or three decorated cells in a wall of six is
  enough.
- Put a `SCONCE` or a window in every room, so the room has a visible light source.

## Doors and stairs

These cells connect a room to another place. All but `DOOR_CLOSED` are fully walkable.
Caves and dungeons add their own doorways, listed in their sections. The rules here apply
to those too.

Wall row pieces:

| Hex | Symbol | Passable | Meaning |
| --- | --- | --- | --- |
| `0x30` | `DOOR_CLOSED` | no | Closed door in the north wall. A locked or decorative door, or one opened by interaction. |
| `0x31` | `DOOR_OPEN` | yes | Open doorway in the north wall that leads somewhere else. |
| `0x32` | `ARCHWAY` | yes | Open passage through the north wall to the room directly above. It breaks the wall top band. |
| `0x33` | `STAIRS_UP` | yes | Stairs that climb into the north wall. |
| `0x34` | `LADDER_UP` | yes | Ladder up through a hatch in the ceiling. |

Edge pieces:

| Hex | Symbol | Replaces | Meaning |
| --- | --- | --- | --- |
| `0x0D` | `EDGE_TOP_DOOR` | `EDGE_TOP` | Doorway in a south wall. This is the usual way out of a house. |
| `0x0E` | `EDGE_LEFT_DOOR` | `EDGE_LEFT` | Doorway in the east wall of the room on its left. |
| `0x0F` | `EDGE_RIGHT_DOOR` | `EDGE_RIGHT` | Doorway in the west wall of the room on its right. |
| `0x1C` | `EDGE_LEFT_RIGHT_DOOR` | `EDGE_LEFT_RIGHT` | Open passage through a wall one cell thick, between two rooms side by side. |

Floor pieces `STAIRS_DOWN` and `TRAPDOOR` are listed under [Floors](#floors).

- **A doorway is content code.** Put a handler point on the cell. The handler calls
  `player_is_walking_onto<POS>()` and then `teleport_player<TARGET>()`. No tile sets
  `portal`. See the entrance rules in [AUTHORING.md](AUTHORING.md#entrances).
- **Give every walkable doorway a handler that teleports.** The cells have no collision.
  Without a handler the player walks onto the cell and stops in the wall.
- **`ARCHWAY` and `EDGE_LEFT_RIGHT_DOOR` need no handler.** They join two rooms of the
  same map, and the player walks through.
- **The way out is at the bottom.** A house has one `EDGE_TOP_DOOR` in its south wall. It
  teleports to the cell below the building's door on the outdoor map. The outdoor door
  handler teleports to the floor cell above the `EDGE_TOP_DOOR`.
- **`DOOR_CLOSED` is solid.** Its handler, if it has one, uses
  `player_just_interacted<POS>()`.
- Keep the floor cell in front of every doorway clear of furniture.

## Floors

Floor cells are fully walkable.

| Hex | Symbol | Passable | Meaning |
| --- | --- | --- | --- |
| `0x40` | `FLOOR_WOOD_A` | yes | Wood planks, with a board end. Mix the four wood cells. |
| `0x41` | `FLOOR_WOOD_B` | yes | Wood planks, with two seam fragments. |
| `0x42` | `FLOOR_WOOD_C` | yes | Wood planks, with a board end. |
| `0x4E` | `FLOOR_WOOD_D` | yes | Wood planks, with two seam fragments. |
| `0x43` | `FLOOR_TILE_A` | yes | Floor tiles marked by small crosses where tiles meet. It tiles with itself. |
| `0x44` | `FLOOR_TILE_B` | yes | Small floor tiles marked by a grid of dots. It tiles with itself. |
| `0x45` | `FLOOR_CHECKER` | yes | Checkered tiles. Dense. It tiles with itself. |
| `0x46` | `FLOOR_STONE_A` | yes | Flagstones, with one joint. Mix the two stone cells. |
| `0x47` | `FLOOR_STONE_B` | yes | Flagstones, with one joint. |
| `0x48` | `FLOOR_PLAIN_A` | yes | Bare floor with a few specks. Mix the two plain cells. |
| `0x49` | `FLOOR_PLAIN_B` | yes | Bare floor. |
| `0x4A` | `STAIRS_DOWN` | yes | Stairwell down through the floor. |
| `0x4B` | `TRAPDOOR` | yes | Trapdoor with a ring pull. |
| `0x4C` | `DOORMAT` | yes | Doormat. Put it on the floor cell above an `EDGE_TOP_DOOR`. |
| `0x4D` | `RUG_SMALL` | yes | Round rug, one cell. |

- **Floors are quiet on purpose.** Wood, tile, stone, plain and cave floors are a few
  marks on black. Furniture cells are opaque and carry their own black background, so
  they sit on these floors without a visible edge.
- **Use one floor family per room:** wood for living rooms and bedrooms, tile for kitchens
  and shops, stone for cellars and halls, plain for sheds and storerooms.
- **Mix the cells of the wood, stone and plain families,** because a run of one cell
  repeats visibly. Wood planks run left to right, and the marks are pieces of the seams
  between boards.
- **Do not mix the tile cells.** `FLOOR_TILE_A` and `FLOOR_TILE_B` are two patterns. Fill a
  room with one of them.
- **`FLOOR_CHECKER` is the one dense floor.** A piece that does not fill its cell, such
  as a chair, a stool, a plant or a vase, shows a black square around it on a checkered
  floor. Use the checker in kitchens and similar rooms, where the pieces on it are
  counters, stoves, barrels and crates that fill their cells. Use `FLOOR_TILE_A` or
  `FLOOR_TILE_B` where chairs and small objects stand on the floor.
- `STAIRS_DOWN` and `TRAPDOOR` are doorways. They follow the rules in
  [Doors and stairs](#doors-and-stairs).

## Rugs

A rug of two or more cells in each direction uses the 3x3 block. Choose each cell by its
place in the rug, as for roads and flat roofs outdoors.

| Hex | Symbol | Place in the rug | Passable |
| --- | --- | --- | --- |
| `0x50` | `RUG_TOP_LEFT` | top-left corner | yes |
| `0x51` | `RUG_TOP` | top edge | yes |
| `0x52` | `RUG_TOP_RIGHT` | top-right corner | yes |
| `0x60` | `RUG_LEFT` | left edge | yes |
| `0x61` | `RUG_CENTER` | interior, with a diamond | yes |
| `0x62` | `RUG_RIGHT` | right edge | yes |
| `0x70` | `RUG_BOTTOM_LEFT` | bottom-left corner | yes |
| `0x71` | `RUG_BOTTOM` | bottom edge | yes |
| `0x72` | `RUG_BOTTOM_RIGHT` | bottom-right corner | yes |
| `0x53` | `RUG_CENTER_PLAIN` | interior, plain | yes |

- A rug is at least 2x2. For one cell, use `RUG_SMALL` or `DOORMAT`.
- Mix `RUG_CENTER` and `RUG_CENTER_PLAIN` in the interior of a large rug.
- Furniture is opaque. A table on a rug hides that rug cell, so keep the rug's outline
  cells free.

## Furniture

Furniture is solid and stands on room cells. Each piece is drawn with its base at the
bottom of its cell.

Tables and seats:

| Hex | Symbol | Passable | Meaning |
| --- | --- | --- | --- |
| `0x54` | `TABLE` | no | Table, one cell. |
| `0x55` | `TABLE_LEFT` | no | Left end of a long table. |
| `0x56` | `TABLE_MIDDLE` | no | Middle of a long table. Repeat it for longer tables. |
| `0x57` | `TABLE_RIGHT` | no | Right end of a long table. |
| `0x58` | `TABLE_SET` | no | One-cell table set with a plate and a mug. |
| `0x59` | `TABLE_CANDLE` | no | One-cell table with a candle. |
| `0x5A` | `CHAIR_SOUTH` | no | Chair that faces south, toward the viewer. It goes north of a table. |
| `0x5B` | `CHAIR_NORTH` | no | Chair that faces north, seen from behind. It goes south of a table. |
| `0x5C` | `CHAIR_WEST` | no | Chair that faces west. It goes east of a table. |
| `0x5D` | `CHAIR_EAST` | no | Chair that faces east. It goes west of a table. |
| `0x5E` | `STOOL` | no | Round stool. |
| `0x5F` | `BENCH` | no | Short bench. |

Bedroom and storage:

| Hex | Symbol | Passable | Meaning |
| --- | --- | --- | --- |
| `0x64` | `BED_HEAD` | no | Head of a bed, with the pillow. `BED_FOOT` goes directly below it. |
| `0x74` | `BED_FOOT` | no | Foot of a bed. |
| `0x65` | `DRESSER` | no | Chest of drawers. |
| `0x66` | `CABINET` | no | Low cabinet with two doors. |
| `0x67` | `SHELF_LOW` | no | Low bookshelf that stands on the floor. |

Kitchen and stores:

| Hex | Symbol | Passable | Meaning |
| --- | --- | --- | --- |
| `0x68` | `STOVE` | no | Cooking stove with a pot. |
| `0x69` | `COUNTER_LEFT` | no | Left end of a counter. |
| `0x6A` | `COUNTER_MIDDLE` | no | Middle of a counter. Repeat it for longer counters. |
| `0x6B` | `COUNTER_RIGHT` | no | Right end of a counter. |
| `0x6C` | `COUNTER_SINK` | no | Counter middle with a basin and a tap. |
| `0x6D` | `BARREL` | no | Barrel. |
| `0x6E` | `CRATE` | no | Crate. |
| `0x6F` | `SACK` | no | Sack. |
| `0x7C` | `BUCKET` | no | Bucket. |
| `0x7D` | `CAULDRON` | no | Cauldron on legs. |

Other objects:

| Hex | Symbol | Passable | Meaning |
| --- | --- | --- | --- |
| `0x75` | `PLANT` | no | Potted plant. |
| `0x76` | `VASE` | no | Vase. |
| `0x77` | `CANDELABRA` | no | Standing candelabra. |
| `0x78` | `POT` | no | Unbroken pot. Collectible, as outdoors. |
| `0x79` | `POT_BROKEN` | partly | Broken pot. The collected state of `POT`. Solid on the bottom half. |
| `0x7A` | `CHEST_CLOSED` | no | Closed chest. Collectible, as outdoors. |
| `0x7B` | `CHEST_OPEN` | no | Opened chest. The collected state of `CHEST_CLOSED`. |

Rules for furniture:

- **Long tables and counters run left to right.** A table is `TABLE_LEFT`, any number of
  `TABLE_MIDDLE`, then `TABLE_RIGHT`. A counter is `COUNTER_LEFT`, any number of
  `COUNTER_MIDDLE` or `COUNTER_SINK`, then `COUNTER_RIGHT`.
- **A bed is two cells tall.** `BED_HEAD` goes above `BED_FOOT`. Put the head against the
  wall row.
- **Chairs face their table.** Choose the chair by the side of the table it stands on.
- **Interactive tiles are solid.** A pot or chest takes its handler point on its own
  cell, and the player interacts from the next cell while facing it. Content code records
  the collected state, as outdoors.

## Caves

A cave uses the room model from [How a room is drawn](#how-a-room-is-drawn). Its edge
cells and wall faces show rough rock in place of straight walls. Choose every piece by
the same rules, then take the cave piece.

Cave edges are solid. The bands and dots are worked out as in [Edges](#edges).

| Hex | Symbol | Bands | House piece it matches |
| --- | --- | --- | --- |
| `0x81` | `CAVE_EDGE_TOP` | top | `EDGE_TOP` |
| `0x82` | `CAVE_EDGE_LEFT` | left | `EDGE_LEFT` |
| `0x83` | `CAVE_EDGE_RIGHT` | right | `EDGE_RIGHT` |
| `0x84` | `CAVE_EDGE_TOP_LEFT` | top, left | `EDGE_TOP_LEFT` |
| `0x85` | `CAVE_EDGE_TOP_RIGHT` | top, right | `EDGE_TOP_RIGHT` |
| `0x86` | `CAVE_EDGE_LEFT_RIGHT` | left, right | `EDGE_LEFT_RIGHT` |
| `0x87` | `CAVE_EDGE_TOP_LEFT_RIGHT` | top, left, right | `EDGE_TOP_LEFT_RIGHT` |
| `0x88` | `CAVE_EDGE_DOT_LEFT` | top-left dot | `EDGE_DOT_LEFT` |
| `0x89` | `CAVE_EDGE_DOT_RIGHT` | top-right dot | `EDGE_DOT_RIGHT` |
| `0x8A` | `CAVE_EDGE_DOT_BOTH` | both dots | `EDGE_DOT_BOTH` |
| `0x8B` | `CAVE_EDGE_LEFT_DOT_RIGHT` | left, top-right dot | `EDGE_LEFT_DOT_RIGHT` |
| `0x8C` | `CAVE_EDGE_RIGHT_DOT_LEFT` | right, top-left dot | `EDGE_RIGHT_DOT_LEFT` |

Cave wall row pieces. All are solid except the two doorways.

| Hex | Symbol | Passable | Meaning |
| --- | --- | --- | --- |
| `0x90` | `CAVE_WALL` | no | Rock face. Mix it with `CAVE_WALL_ALT`. |
| `0x94` | `CAVE_WALL_ALT` | no | Rock face with different cracks. |
| `0x91` | `CAVE_WALL_END_LEFT` | no | Rock face with a room cell on its left. |
| `0x92` | `CAVE_WALL_END_RIGHT` | no | Rock face with a room cell on its right. |
| `0x93` | `CAVE_WALL_PILLAR` | no | Rock face one cell wide, with room cells on both sides. |
| `0x95` | `CAVE_WALL_ORE` | no | Rock face with ore nuggets. |
| `0x96` | `CAVE_WALL_CRYSTAL` | no | Rock face with crystals. |
| `0x97` | `CAVE_WALL_CRACKED` | no | Rock face with a deep crack. A weak wall that content code can open. |
| `0x98` | `CAVE_WALL_TIMBER` | no | Mine support frame of two posts and a beam. |
| `0x99` | `CAVE_WALL_TORCH` | no | Rock face with a lit torch. |
| `0x9A` | `CAVE_TUNNEL` | yes | Tunnel mouth in the rock face. A doorway. |
| `0x9B` | `CAVE_LADDER_UP` | yes | Ladder up through the rock. A doorway. It pairs with `HOLE_LADDER` outdoors. |

Exits through a cave edge. All are fully walkable.

| Hex | Symbol | Replaces | Meaning |
| --- | --- | --- | --- |
| `0x8D` | `CAVE_EDGE_TOP_EXIT` | `CAVE_EDGE_TOP` | Way out through the south side of a cave. |
| `0x8E` | `CAVE_EDGE_LEFT_EXIT` | `CAVE_EDGE_LEFT` | Way out through the east side of the cave on its left. |
| `0x8F` | `CAVE_EDGE_RIGHT_EXIT` | `CAVE_EDGE_RIGHT` | Way out through the west side of the cave on its right. |
| `0x9C` | `CAVE_EDGE_LEFT_RIGHT_EXIT` | `CAVE_EDGE_LEFT_RIGHT` | Open passage through rock one cell thick, between two chambers side by side. |

Cave floor pieces. All are fully walkable.

| Hex | Symbol | Passable | Meaning |
| --- | --- | --- | --- |
| `0xA0` | `FLOOR_CAVE_A` | yes | Bare ground with pebbles. Mix the three cave floor cells. |
| `0xA1` | `FLOOR_CAVE_B` | yes | Bare ground with pebbles. |
| `0xA2` | `FLOOR_CAVE_C` | yes | Bare ground with pebbles. |
| `0xB3` | `PUDDLE` | yes | Shallow puddle. Decoration. |
| `0xB4` | `BONES` | yes | Skull and bone. Decoration. |
| `0xB6` | `LADDER_DOWN` | yes | Hole with the top of a ladder. A doorway to a lower level. |
| `0xA3` | `RAIL_HORIZONTAL` | yes | Mine rails that run west to east. |
| `0xA4` | `RAIL_VERTICAL` | yes | Mine rails that run north to south. |
| `0xA5` | `RAIL_WEST_SOUTH` | yes | Rail curve that joins the west and south sides. |
| `0xA6` | `RAIL_EAST_SOUTH` | yes | Rail curve that joins the east and south sides. |
| `0xA7` | `RAIL_WEST_NORTH` | yes | Rail curve that joins the west and north sides. |
| `0xA8` | `RAIL_EAST_NORTH` | yes | Rail curve that joins the east and north sides. |

Cave objects. All are solid.

| Hex | Symbol | Passable | Meaning |
| --- | --- | --- | --- |
| `0xAA` | `BOULDER` | no | Boulder. |
| `0xAB` | `RUBBLE` | no | Pile of small rocks. |
| `0xAC` | `STALAGMITE` | no | One tall stalagmite. |
| `0xAD` | `STALAGMITES` | no | Cluster of stalagmites. |
| `0xAE` | `CRYSTALS` | no | Crystal cluster. |
| `0xAF` | `MUSHROOMS` | no | Cave mushrooms. |
| `0xB5` | `CAMPFIRE` | no | Burning campfire. |
| `0xA9` | `MINECART` | no | Mine cart on west-to-east rails. |
| `0xB7` | `PIT` | no | Deep pit. Solid, so nothing walks into it. |

A pool of two or more cells in each direction uses its 3x3 block, as a rug does. Pools are
solid.

| Hex | Symbol | Place in the pool | Passable |
| --- | --- | --- | --- |
| `0xB0` | `POOL_TOP_LEFT` | top-left corner | no |
| `0xB1` | `POOL_TOP` | top edge | no |
| `0xB2` | `POOL_TOP_RIGHT` | top-right corner | no |
| `0xC0` | `POOL_LEFT` | left edge | no |
| `0xC1` | `POOL_CENTER` | interior | no |
| `0xC2` | `POOL_RIGHT` | right edge | no |
| `0xD0` | `POOL_BOTTOM_LEFT` | bottom-left corner | no |
| `0xD1` | `POOL_BOTTOM` | bottom edge | no |
| `0xD2` | `POOL_BOTTOM_RIGHT` | bottom-right corner | no |

A small cave, as runtime IDs. `..` is void.

```text
.. .. 83 90 95 94 82 ..
83 9B 92 A0 A2 A2 91 82
83 A0 A1 A2 A1 AC A2 82
83 A2 AA A0 A2 A0 84 88
89 85 A1 A1 A2 84 88 ..
.. 89 81 8D 81 88 .. ..
```

It has a ladder up at its west end, ore in the north wall, a boulder, a stalagmite and
an exit in its south side.

Rules for caves:

- **Irregular outlines.** Step the outline in or out by one or two cells every two to
  four cells. Avoid straight runs longer than four cells and avoid rectangles.
- **Wall faces follow the outline.** Every non-room cell above a room cell is a cave wall
  row piece. Where the outline steps, the face next to a room cell is `CAVE_WALL_END_LEFT`,
  `CAVE_WALL_END_RIGHT` or `CAVE_WALL_PILLAR`. The decorated cave faces have no end pieces,
  so they cannot stand next to a room cell.
- **Mix the plain faces.** Paint about one `CAVE_WALL` in three as `CAVE_WALL_ALT`.
- **Chambers and passages.** Make a cave from a few chambers joined by passages one or two
  cells wide. Use two cells where an NPC walks.
- **Rails join by their sides.** A straight rail joins the two sides it runs between. A
  curve joins the two sides in its name. Every rail end meets another rail, a wall, or an
  exit. `MINECART` stands on a west-to-east run, between `RAIL_HORIZONTAL` cells.
- **Mines.** A mine is a cave with rails, `CAVE_WALL_TIMBER` frames every few cells of
  wall and `CAVE_WALL_TORCH` for light.
- **Cave and house pieces meet cleanly.** Both kinds of band are three pixels thick at
  cell borders. A cellar or a dungeon can open into a cave by changing the piece style at
  any cell border.
- **Exits are doorways.** `CAVE_TUNNEL`, `CAVE_LADDER_UP`, `LADDER_DOWN` and the edge exits
  follow [Doors and stairs](#doors-and-stairs). `CAVE_EDGE_LEFT_RIGHT_EXIT` joins two
  chambers of the same map and needs no handler.
- A cave reached through `CLIFF_CAVE` outdoors has a `CAVE_EDGE_TOP_EXIT` that leads back.
  A cave reached through `HOLE_LADDER` has a `CAVE_LADDER_UP`.

## Dungeons

A dungeon is built like a house: straight `EDGE_*` cells, the `WALL_STONE` faces and the
stone floor cells. These pieces add to it.

Dungeon wall row pieces. Each replaces a `WALL_STONE` face and cannot stand next to a room
cell.

| Hex | Symbol | Passable | Meaning |
| --- | --- | --- | --- |
| `0xB8` | `TORCH` | no | Wall torch with a flame. |
| `0xCF` | `GRATE_WINDOW` | no | Small barred window high in the wall. |
| `0xBE` | `CHAINS` | no | Two hanging shackles. |
| `0xBF` | `WALL_STONE_CRACKED` | no | Stone wall with a deep crack. A weak wall that content code can open. |
| `0xB9` | `BARS` | no | Iron bars across the wall row. The front of a cell. |
| `0xBA` | `BARS_DOOR` | no | Closed cell door of iron bars. |
| `0xBB` | `BARS_DOOR_OPEN` | yes | Opened cell door. The open state of `BARS_DOOR`. A doorway. |
| `0xBC` | `DUNGEON_DOOR` | no | Closed heavy double door in an arch. |
| `0xBD` | `DUNGEON_DOOR_LOCKED` | no | Closed arched door with a lock plate. |
| `0xD3` | `DUNGEON_DOOR_OPEN` | yes | Open arch. The open state of both dungeon doors. A doorway. |

Dungeon floor pieces:

| Hex | Symbol | Passable | Meaning |
| --- | --- | --- | --- |
| `0xC7` | `FLOOR_STONE_CRACKED` | yes | Cracked flagstones. An accent: use it for about one stone cell in ten. |
| `0xC6` | `FLOOR_GRATE` | yes | Drain grate in the floor. Decoration. |
| `0xC3` | `PRESSURE_PLATE` | yes | Pressure plate. Its handler reacts to the player standing on it. |
| `0xC5` | `SPIKES_DOWN` | yes | Retracted spikes. The safe state of `SPIKES`. |
| `0xC4` | `SPIKES` | no | Raised spikes. Solid. |

Dungeon objects:

| Hex | Symbol | Passable | Meaning |
| --- | --- | --- | --- |
| `0xC8` | `COLUMN` | no | Stone column. |
| `0xC9` | `STATUE` | no | Statue on a pedestal. |
| `0xCA` | `BRAZIER` | no | Burning brazier. |
| `0xCB` | `ALTAR` | no | Stone altar with a candle. |
| `0xCC` | `TOMB_HEAD` | no | Head of a stone tomb. `TOMB_FOOT` goes directly below it. |
| `0xDC` | `TOMB_FOOT` | no | Foot of a stone tomb. |
| `0xCD` | `SWITCH_LEFT` | partly | Interactive lever pointing left. Solid on the bottom half, as outdoors. |
| `0xCE` | `SWITCH_RIGHT` | partly | Interactive lever pointing right. Solid on the bottom half, as outdoors. |

Rules for dungeons:

- **Rooms and corridors.** Dungeon rooms are rectangles joined by corridors one or two
  cells wide. `ARCHWAY` and `EDGE_LEFT_RIGHT_DOOR` join rooms through walls one cell thick.
- **Light the rooms.** Put a `TORCH` every three to five cells of wall, or a `BRAZIER` on
  the floor.
- **Columns in pairs.** Place `COLUMN` cells in a regular pattern, and leave two walkable
  cells between columns.
- **Cells.** A cell is a small room whose wall row holds `BARS` on both sides of a
  `BARS_DOOR`. The cell lies above the corridor.
- **State changes are content code.** `BARS_DOOR` opens to `BARS_DOOR_OPEN`. `DUNGEON_DOOR`
  and `DUNGEON_DOOR_LOCKED` open to `DUNGEON_DOOR_OPEN`. `SPIKES` lower to `SPIKES_DOWN`.
  A cracked wall opens to `ARCHWAY`, `DOOR_OPEN` or `CAVE_TUNNEL`. The handler swaps the
  tile with `set_tile<POS>()` and records the state in a world flag, because the cache is
  rebuilt from the map when it reloads. The map shows the initial state.
- **Levers and plates.** `SWITCH_LEFT` and `SWITCH_RIGHT` are interactive, and the handler
  uses `player_just_interacted<POS>()`. A `PRESSURE_PLATE` handler uses
  `game.player_is_on_tile<POS>()`.
- **Open doors are doorways.** `BARS_DOOR_OPEN` and `DUNGEON_DOOR_OPEN` are walkable. If
  the room behind is on another map, the handler teleports. If the room is directly above
  on the same map, use `ARCHWAY` as the open state so the wall top band is broken.
- **A tomb is two cells tall.** `TOMB_HEAD` goes above `TOMB_FOOT`.
- A dungeon reached through `CLIFF_MINE` outdoors has an `EDGE_TOP_DOOR` that leads back.

## Solidity

- Edge cells, wall faces, wall decorations, furniture, pools, pits and raised spikes are
  fully solid (`65535`).
- Floors, rugs, rails, floor decorations and every doorway are fully walkable (`0`). The
  closed doors `DOOR_CLOSED`, `BARS_DOOR`, `DUNGEON_DOOR` and `DUNGEON_DOOR_LOCKED` are solid.
- `POT_BROKEN`, `SWITCH_LEFT` and `SWITCH_RIGHT` are solid on the bottom half (`0xFF00`).
- NPCs treat any nonzero mask as solid. NPCs can step onto doorway cells, so keep routes
  and roaming areas away from them.
- The player's sprite overlaps the wall face above it and the side bands beside it by a
  few pixels. That is expected.

## Layout principles

- **Small rooms.** The view is 8x4 tiles. A floor of 6x2 fits one screen with its walls. A
  typical house room has a floor of 4x3 to 8x5. Larger rooms scroll.
- **Furniture along the walls.** Put beds, dressers, counters and stoves against the wall
  row or in corners. Keep the middle of the room open.
- **A clear path.** Every floor cell the player should reach connects to the doorway. Leave
  a path one cell wide, and two cells wide where an NPC walks.
- **Group by use.** A kitchen has a stove, a counter with a sink, a cupboard and hanging
  pans. A bedroom has a bed, a dresser and a wardrobe. A living room has a table with
  chairs, a rug and a fireplace.
- **Do not fill the room.** Leave at least half of the floor cells empty.
- **One light source or more.** Every room has a window, a sconce, a lit fireplace or a
  candelabra.
- **Rooms of one house share a map.** Join them with `ARCHWAY` or `EDGE_LEFT_RIGHT_DOOR`,
  with walls one cell thick between them.

## Before committing an interior map

- Every non-room cell above a room cell is a wall row piece.
- Every edge cell uses the piece that its bands and dots select, and void is unpainted.
- Each wall face next to a room cell is an end or pillar piece, and no decoration stands
  next to a room cell.
- One wall style and one floor family are used per room.
- Wood, stone and plain floors mix the cells of their family. A tile floor uses one tile
  cell throughout.
- No chair, stool, plant, vase or other piece that leaves its cell partly empty stands
  on `FLOOR_CHECKER`.
- Every walkable doorway has a handler point that teleports, except `ARCHWAY` and
  `EDGE_LEFT_RIGHT_DOOR`.
- The floor cell in front of every doorway is clear.
- Rugs and pools are at least 2x2. A one-cell rug is `RUG_SMALL` or `DOORMAT`.
- A cave uses cave pieces for every edge and wall face, and its outline has no long
  straight runs.
- Every rail end meets another rail, a wall or an exit.
- Tiles that change state show their initial state, and content code records the change.
- Beds, tombs, long tables and counters are complete, and chairs face their table.
- The spawn is on a floor cell, and every reachable floor cell connects to the doorway.
- No `EMPTY` (`0x00`) cell is painted.
