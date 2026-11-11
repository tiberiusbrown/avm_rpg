# Interior map and tile authoring guide

This guide tells agents how to build houses, caves and dungeons. It sits beside
[AUTHORING.md](AUTHORING.md), which covers the world tileset and outdoor maps. The rules
there for collision masks, objects, handlers and routes apply here too. Read it first.

There are three interior tilesets. They share one room model and the same wall edge cells,
so most of this guide applies to all three.

| Tileset | Atlas | For | Used by |
| --- | --- | --- | --- |
| `house.tsx` | `tiles16_house.png` | Houses, shops and cellars | `interiors.tmx` |
| `cave.tsx` | `tiles16_cave.png` | Caves and mines | no map yet |
| `dungeon.tsx` | `tiles16_dungeon.png` | Dungeons, crypts and halls | no map yet |

## Files

- `assets/tilesets/house.tsx`, `cave.tsx` and `dungeon.tsx`: metadata for every atlas cell.
- `assets/images/tiles16_house.png`, `tiles16_cave.png` and `tiles16_dungeon.png`: the
  256x256 monochrome atlases.
- `assets/images/tiles16_<name>_collision.png`: each atlas with solid collision pixels
  tinted red. The generator writes them on every run.
- `assets/maps/interiors.tmx`: the inside of every house, in one map.
- **Each map uses exactly one tileset.** The game draws and collides an area with the
  tileset of its map. A house map uses `house.tsx`, a cave map uses `cave.tsx`, and a
  dungeon map uses `dungeon.tsx`. Pieces from two tilesets cannot share a map.
- The generator emits tile constants per tileset: `HOUSE_TILE_<symbol>`,
  `CAVE_TILE_<symbol>` and `DUNGEON_TILE_<symbol>`, once a map uses that tileset.

## Tile IDs

- A tile's runtime ID is its atlas cell index, `row * 16 + column`, as outdoors. IDs
  belong to a tileset: `0x40` is a wood floor in the house tileset and a pool corner in
  the cave tileset.
- **`0x00` is `EMPTY`, the void outside a room. Paint it.** It is black and solid in all
  three tilesets. Every cell of an interior map that is not part of a room takes `0x00`.
  This is what lets a room have any outline, not only a rectangle.
- **Do not leave cells unpainted.** An unpainted cell is not drawn at all, so it shows
  whatever was on the screen before. Use `0x00` for all empty space.
- `0xFF` is reserved. Unnamed cells are `ATLAS_XX` and are solid.
- Some symbols repeat across tilesets, such as `POT` and `CHEST_CLOSED`. They are
  separate cells in separate atlases.

## How a room is drawn

A room is seen from the front and slightly above. The floor is black with a few marks.
Walls are white bands three pixels thick around it. The north wall also shows its face,
one tile tall, which carries windows, pictures and tall furniture. Everything outside is
void.

Every cell of an interior map is one of four kinds:

- **Room cell.** A floor cell, a rug cell, or anything that stands on the floor. Furniture
  is opaque, so it hides the floor under it.
- **Wall row cell.** A cell that is not a room cell and has a room cell directly below it.
  It shows the north wall from the front.
- **Edge cell.** Any other cell that touches the room. It is void with a band on each side
  that faces the room.
- **Void.** Everything else. Paint it `0x00`.

Build a room in this order:

1. Paint the room cells with floor. The room can have any outline.
2. Paint the wall row. Each wall row cell takes a wall face from
   [Wall faces](#wall-faces), or a wall row piece of the map's tileset.
3. Paint the edge cells with the pieces that [Edges](#edges) selects.
4. Place rugs, furniture and objects on the floor.
5. Add exits, then the handler points that make them work.
6. Paint every remaining cell `0x00`.

The cottage from `interiors.tmx`, as house tileset IDs. `00` is void.

```text
03 14 20 37 14 24 02
03 64 65 40 41 75 02
03 74 40 4D 5D 58 02
03 42 4E 4C 40 42 02
09 01 01 0D 01 01 08
```

Its wall row holds a window, a bookcase and a sconce on wood panelling. A bed, a dresser,
a plant, a round rug, a chair and a set table stand on a wood floor. A doormat lies above
the exit in the south wall.

## Edges

Edge cells are solid. They have the same names and IDs in all three tilesets. The house
and dungeon tilesets draw straight bands, and the cave tileset draws rough rock. Work out
the bands of each edge cell:

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

A wall face has the wall top band on its top three rows, the face below it and a foot
line on its bottom row. Wall faces are solid. These four have the same names and IDs in
all three tilesets: plaster in the house tileset, rock in the cave tileset and stone
blocks in the dungeon tileset.

| Hex | Symbol | Room cell on its left or right | Meaning |
| --- | --- | --- | --- |
| `0x10` | `WALL` | neither | Plain wall face. |
| `0x11` | `WALL_END_LEFT` | left | Wall face with its left end exposed. |
| `0x12` | `WALL_END_RIGHT` | right | Wall face with its right end exposed. |
| `0x13` | `WALL_PILLAR` | both | Wall face one cell wide. |

The end and pillar pieces close the side of a wall that stands next to a room cell. That
happens where a room is L-shaped, or where a wall stub juts into a room. Decorated wall row
pieces have no end pieces, so they cannot stand next to a room cell.

## Exits

An exit replaces an edge cell and is fully walkable. These have the same names and IDs in
all three tilesets.

| Hex | Symbol | Replaces | Meaning |
| --- | --- | --- | --- |
| `0x0D` | `EDGE_TOP_EXIT` | `EDGE_TOP` | Way out through the south wall. The usual exit of a room. |
| `0x0E` | `EDGE_LEFT_EXIT` | `EDGE_LEFT` | Way out through the east wall of the room on its left. |
| `0x0F` | `EDGE_RIGHT_EXIT` | `EDGE_RIGHT` | Way out through the west wall of the room on its right. |
| `0x1C` | `EDGE_LEFT_RIGHT_EXIT` | `EDGE_LEFT_RIGHT` | Open passage through a wall one cell thick, between two rooms side by side. |

Each tileset adds doorways of its own in the wall row and the floor. These rules apply to
all of them:

- **An exit is content code.** Put a handler point on the cell. The handler calls
  `player_is_walking_onto<POS>()` and then `teleport_player<TARGET>()`. No tile sets
  `portal`. See the entrance rules in [AUTHORING.md](AUTHORING.md#entrances).
- **Give every walkable exit a handler that teleports.** The cells have no collision.
  Without a handler the player walks onto the cell and stops in the wall.
- **`EDGE_LEFT_RIGHT_EXIT` and `ARCHWAY` need no handler.** They join two rooms of the same
  map, and the player walks through.
- **The player leaves by walking.** Walking onto an exit leaves the room with no button
  press. Entering from outdoors is the opposite: the player activates a door.
- Keep the floor cell in front of every exit clear of furniture.

## The house map

All house interiors live in `interiors.tmx`, one room per house. One map costs one area
ID and one tileset, and it keeps every house of a region in one place.

- **Keep rooms apart.** No painted cell of another room may be on screen from anywhere
  the player can stand. Leave at least five void cells between two rooms in every
  direction. The content test checks five columns and three rows around every walkable
  cell.
- **Link each room to its door.** Put a point named `<House>Exit` on the room's
  `EDGE_TOP_EXIT`, with the handler `leave_house`. The outdoor door carries a point named
  `<House>Door` with the handler `enter_house`. Add the pair to `HOUSE_LINKS` in
  `src/logic/handlers.h`.
- **Entering and leaving.** `enter_house` runs when the player faces the door and presses
  A. It moves the player to the floor cell above the exit, facing north. `leave_house`
  runs when the player walks onto the exit. It moves the player to the cell below the
  door, facing south.
- **One exit per house, at the bottom.** The floor cell above the exit takes a `DOORMAT`
  and stays clear.
- The map's `spawn` names a floor cell with no handler. The game does not start there.
- Give each house its own floor family, wall style and furniture, so the houses of a
  village differ inside as they do outside.

## House tileset

Two more wall styles, with their end pieces. Use one style for all the walls of a room.

| Hex | Symbol | Room cell on its left or right | Meaning |
| --- | --- | --- | --- |
| `0x14` | `WALL_WOOD` | neither | Wood-panelled wall. |
| `0x15` | `WALL_WOOD_END_LEFT` | left | Wood-panelled wall with its left end exposed. |
| `0x16` | `WALL_WOOD_END_RIGHT` | right | Wood-panelled wall with its right end exposed. |
| `0x17` | `WALL_WOOD_PILLAR` | both | Wood-panelled wall one cell wide. |
| `0x18` | `WALL_STONE` | neither | Stone block wall, for cellars. |
| `0x19` | `WALL_STONE_END_LEFT` | left | Stone wall with its left end exposed. |
| `0x1A` | `WALL_STONE_END_RIGHT` | right | Stone wall with its right end exposed. |
| `0x1B` | `WALL_STONE_PILLAR` | both | Stone wall one cell wide. |

Wall decorations. Each replaces a plain wall face. All are solid, and they are drawn
without a wall pattern, so they suit all three wall styles.

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

Doors and stairs. All but `DOOR_CLOSED` are fully walkable exits.

| Hex | Symbol | Passable | Meaning |
| --- | --- | --- | --- |
| `0x30` | `DOOR_CLOSED` | no | Closed door in the north wall. A locked or decorative door, or one opened by interaction. |
| `0x31` | `DOOR_OPEN` | yes | Open doorway in the north wall that leads somewhere else. |
| `0x32` | `ARCHWAY` | yes | Open passage through the north wall to the room directly above. It breaks the wall top band. |
| `0x33` | `STAIRS_UP` | yes | Stairs that climb into the north wall. |
| `0x34` | `LADDER_UP` | yes | Ladder up through a hatch in the ceiling. |
| `0x4A` | `STAIRS_DOWN` | yes | Stairwell down through the floor. |
| `0x4B` | `TRAPDOOR` | yes | Trapdoor with a ring pull. |

Floors. All are fully walkable.

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
| `0x4C` | `DOORMAT` | yes | Doormat. Put it on the floor cell above an `EDGE_TOP_EXIT`. |
| `0x4D` | `RUG_SMALL` | yes | Round rug, one cell. |

A rug of two or more cells in each direction uses the 3x3 block. Choose each cell by its
place in the rug, as for flat roofs outdoors.

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

Tables and seats. Furniture is solid and stands on room cells.

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
| `0x7A` | `CHEST_CLOSED` | no | Closed chest with a rounded lid and a lock. Collectible, as outdoors. |
| `0x7B` | `CHEST_OPEN` | no | Opened chest with its lid raised. The collected state of `CHEST_CLOSED`. |

Rules for houses:

- **Floors are quiet on purpose.** Wood, tile, stone and plain floors are a few marks on
  black. Furniture cells are opaque and carry their own black background, so they sit on
  these floors without a visible edge.
- **Use one floor family per room:** wood for living rooms and bedrooms, tile for kitchens
  and shops, stone for cellars, plain for sheds and storerooms.
- **Mix the cells of the wood, stone and plain families,** because a run of one cell
  repeats visibly. Wood planks run left to right, and the marks are pieces of the seams
  between boards.
- **Do not mix the tile cells.** `FLOOR_TILE_A` and `FLOOR_TILE_B` are two patterns. Fill a
  room with one of them.
- **`FLOOR_CHECKER` is the one dense floor.** A piece that does not fill its cell, such
  as a chair, a stool, a plant or a vase, shows a black square around it on a checkered
  floor. Use the checker in kitchens and similar rooms, where the pieces on it are
  counters, stoves, barrels and crates that fill their cells.
- **Tall furniture belongs in the wall row.** `BOOKCASE`, `CUPBOARD`, `WARDROBE` and the
  fireplaces stand against the north wall and take no floor space.
- **Leave plain wall between decorations.** Two or three decorated cells in a wall of six
  is enough. Put a `SCONCE`, a window or a lit fireplace in every room.
- **Long tables and counters run left to right.** A table is `TABLE_LEFT`, any number of
  `TABLE_MIDDLE`, then `TABLE_RIGHT`. A counter is `COUNTER_LEFT`, any number of
  `COUNTER_MIDDLE` or `COUNTER_SINK`, then `COUNTER_RIGHT`.
- **A bed is two cells tall.** `BED_HEAD` goes above `BED_FOOT`. Put the head against the
  wall row.
- **Chairs face their table.** Choose the chair by the side of the table it stands on.
- **A rug is at least 2x2.** For one cell, use `RUG_SMALL` or `DOORMAT`. Furniture hides
  the rug cell under it, so keep a rug's outline cells free.
- **Interactive tiles are solid.** A pot or chest takes its handler point on its own
  cell, and the player interacts from the next cell while facing it. Content code records
  the collected state, as outdoors.

## Cave tileset

A cave uses the room model above. Its edge cells and wall faces show rough rock.

Cave wall row pieces. All are solid except the two doorways.

| Hex | Symbol | Passable | Meaning |
| --- | --- | --- | --- |
| `0x14` | `WALL_ALT` | no | Rock face with different cracks. Mix it with `WALL`. |
| `0x15` | `WALL_ORE` | no | Rock face with ore nuggets. |
| `0x16` | `WALL_CRYSTAL` | no | Rock face with crystals. |
| `0x17` | `WALL_CRACKED` | no | Rock face with a deep crack. A weak wall that content code can open. |
| `0x18` | `WALL_TIMBER` | no | Mine support frame of two posts and a beam. |
| `0x19` | `WALL_TORCH` | no | Rock face with a lit torch. |
| `0x1A` | `TUNNEL` | yes | Tunnel mouth in the rock face. A doorway. |
| `0x1B` | `LADDER_UP` | yes | Ladder up through the rock. A doorway. It pairs with `HOLE_LADDER` outdoors. |

Cave floor pieces. All are fully walkable.

| Hex | Symbol | Passable | Meaning |
| --- | --- | --- | --- |
| `0x20` | `FLOOR_A` | yes | Bare ground with pebbles. Mix the three floor cells. |
| `0x21` | `FLOOR_B` | yes | Bare ground with pebbles. |
| `0x22` | `FLOOR_C` | yes | Bare ground with pebbles. |
| `0x29` | `PUDDLE` | yes | Shallow puddle. Decoration. |
| `0x2A` | `BONES` | yes | Skull and bone. Decoration. |
| `0x2B` | `LADDER_DOWN` | yes | Hole with the top of a ladder. A doorway to a lower level. |
| `0x23` | `RAIL_HORIZONTAL` | yes | Mine rails that run west to east. |
| `0x24` | `RAIL_VERTICAL` | yes | Mine rails that run north to south. |
| `0x25` | `RAIL_WEST_SOUTH` | yes | Rail curve that joins the west and south sides. |
| `0x26` | `RAIL_EAST_SOUTH` | yes | Rail curve that joins the east and south sides. |
| `0x27` | `RAIL_WEST_NORTH` | yes | Rail curve that joins the west and north sides. |
| `0x28` | `RAIL_EAST_NORTH` | yes | Rail curve that joins the east and north sides. |

Cave objects. All are solid, except `POT_BROKEN`.

| Hex | Symbol | Passable | Meaning |
| --- | --- | --- | --- |
| `0x30` | `BOULDER` | no | Boulder. |
| `0x31` | `RUBBLE` | no | Pile of small rocks. |
| `0x32` | `STALAGMITE` | no | One tall stalagmite. |
| `0x33` | `STALAGMITES` | no | Cluster of stalagmites. |
| `0x34` | `CRYSTALS` | no | Crystal cluster. |
| `0x35` | `MUSHROOMS` | no | Cave mushrooms. |
| `0x36` | `CAMPFIRE` | no | Burning campfire. |
| `0x37` | `MINECART` | no | Mine cart on west-to-east rails. |
| `0x2C` | `PIT` | no | Deep pit. Solid, so nothing walks into it. |
| `0x38` | `POT` | no | Unbroken pot. Collectible. |
| `0x39` | `POT_BROKEN` | partly | Broken pot. Solid on the bottom half. |
| `0x3A` | `CHEST_CLOSED` | no | Closed chest. Collectible. |
| `0x3B` | `CHEST_OPEN` | no | Opened chest. |
| `0x3C` | `BARREL` | no | Barrel. |
| `0x3D` | `CRATE` | no | Crate. |
| `0x3E` | `SACK` | no | Sack. |

A pool of two or more cells in each direction uses its 3x3 block, as a rug does. Pools are
solid.

| Hex | Symbol | Place in the pool | Passable |
| --- | --- | --- | --- |
| `0x40` | `POOL_TOP_LEFT` | top-left corner | no |
| `0x41` | `POOL_TOP` | top edge | no |
| `0x42` | `POOL_TOP_RIGHT` | top-right corner | no |
| `0x50` | `POOL_LEFT` | left edge | no |
| `0x51` | `POOL_CENTER` | interior | no |
| `0x52` | `POOL_RIGHT` | right edge | no |
| `0x60` | `POOL_BOTTOM_LEFT` | bottom-left corner | no |
| `0x61` | `POOL_BOTTOM` | bottom edge | no |
| `0x62` | `POOL_BOTTOM_RIGHT` | bottom-right corner | no |

A small cave, as cave tileset IDs. `00` is void.

```text
00 00 03 10 15 14 02 00
03 1B 12 21 21 21 11 02
03 22 22 22 22 32 22 02
03 20 30 20 20 20 04 08
09 05 21 21 21 04 08 00
00 09 01 0D 01 08 00 00
```

It has a ladder up at its west end, ore in the north wall, a boulder, a stalagmite and
an exit in its south side.

Rules for caves:

- **Irregular outlines.** Step the outline in or out by one or two cells every two to
  four cells. Avoid straight runs longer than four cells and avoid rectangles.
- **Wall faces follow the outline.** Every non-room cell above a room cell is a wall row
  piece. Where the outline steps, the face next to a room cell is `WALL_END_LEFT`,
  `WALL_END_RIGHT` or `WALL_PILLAR`.
- **Mix the plain faces.** Paint about one `WALL` in three as `WALL_ALT`.
- **Chambers and passages.** Make a cave from a few chambers joined by passages one or two
  cells wide. Use two cells where an NPC walks.
- **Rails join by their sides.** A straight rail joins the two sides it runs between. A
  curve joins the two sides in its name. Every rail end meets another rail, a wall, or an
  exit. `MINECART` stands on a west-to-east run, between `RAIL_HORIZONTAL` cells.
- **Mines.** A mine is a cave with rails, `WALL_TIMBER` frames every few cells of wall and
  `WALL_TORCH` for light.
- **Exits.** `TUNNEL`, `LADDER_UP`, `LADDER_DOWN` and the edge exits follow
  [Exits](#exits). A cave reached through `CLIFF_CAVE` outdoors has an `EDGE_TOP_EXIT` that
  leads back. A cave reached through `HOLE_LADDER` has a `LADDER_UP`.

## Dungeon tileset

A dungeon is built like a house, with straight edges and stone block walls.

Dungeon wall row pieces. Each replaces a `WALL` face and cannot stand next to a room cell.

| Hex | Symbol | Passable | Meaning |
| --- | --- | --- | --- |
| `0x14` | `WALL_CRACKED` | no | Stone wall with a deep crack. A weak wall that content code can open. |
| `0x15` | `TORCH` | no | Wall torch with a flame. |
| `0x16` | `GRATE_WINDOW` | no | Small barred window high in the wall. |
| `0x17` | `CHAINS` | no | Two hanging shackles. |
| `0x18` | `BANNER` | no | Hanging banner. |
| `0x19` | `BARS` | no | Iron bars across the wall row. The front of a cell. |
| `0x1A` | `BARS_DOOR` | no | Closed cell door of iron bars. |
| `0x1B` | `BARS_DOOR_OPEN` | yes | Opened cell door. The open state of `BARS_DOOR`. A doorway. |
| `0x1D` | `DUNGEON_DOOR` | no | Closed heavy double door in an arch. |
| `0x1E` | `DUNGEON_DOOR_LOCKED` | no | Closed arched door with a lock plate. |
| `0x1F` | `DUNGEON_DOOR_OPEN` | yes | Open arch. The open state of both dungeon doors. A doorway. |
| `0x20` | `ARCHWAY` | yes | Open passage through the north wall to the room directly above. It breaks the wall top band. |
| `0x21` | `STAIRS_UP` | yes | Stairs that climb into the north wall. A doorway. |
| `0x22` | `LADDER_UP` | yes | Ladder up through a hatch in the ceiling. A doorway. |

Dungeon floor pieces:

| Hex | Symbol | Passable | Meaning |
| --- | --- | --- | --- |
| `0x24` | `FLOOR_A` | yes | Flagstones, with one joint. Mix the two floor cells. |
| `0x25` | `FLOOR_B` | yes | Flagstones, with one joint. |
| `0x26` | `FLOOR_CRACKED` | yes | Cracked flagstones. An accent: use it for about one floor cell in ten. |
| `0x27` | `FLOOR_GRATE` | yes | Drain grate in the floor. Decoration. |
| `0x28` | `PRESSURE_PLATE` | yes | Pressure plate. Its handler reacts to the player standing on it. |
| `0x2A` | `SPIKES_DOWN` | yes | Retracted spikes. The safe state of `SPIKES`. |
| `0x29` | `SPIKES` | no | Raised spikes. Solid. |
| `0x2B` | `STAIRS_DOWN` | yes | Stairwell down through the floor. A doorway. |
| `0x2C` | `TRAPDOOR` | yes | Trapdoor with a ring pull. A doorway. |
| `0x2D` | `PIT` | no | Deep pit. Solid, so nothing walks into it. |
| `0x2E` | `PUDDLE` | yes | Shallow puddle. Decoration. |
| `0x2F` | `BONES` | yes | Skull and bone. Decoration. |

Dungeon objects:

| Hex | Symbol | Passable | Meaning |
| --- | --- | --- | --- |
| `0x30` | `COLUMN` | no | Stone column. |
| `0x31` | `STATUE` | no | Statue on a pedestal. |
| `0x32` | `BRAZIER` | no | Burning brazier. |
| `0x33` | `ALTAR` | no | Stone altar with a candle. |
| `0x34` | `TOMB_HEAD` | no | Head of a stone tomb. `TOMB_FOOT` goes directly below it. |
| `0x44` | `TOMB_FOOT` | no | Foot of a stone tomb. |
| `0x35` | `SWITCH_LEFT` | partly | Interactive lever pointing left. Solid on the bottom half, as outdoors. |
| `0x36` | `SWITCH_RIGHT` | partly | Interactive lever pointing right. Solid on the bottom half, as outdoors. |
| `0x37` | `RUBBLE` | no | Pile of small rocks. |
| `0x38` | `POT` | no | Unbroken pot. Collectible. |
| `0x39` | `POT_BROKEN` | partly | Broken pot. Solid on the bottom half. |
| `0x3A` | `CHEST_CLOSED` | no | Closed chest. Collectible. |
| `0x3B` | `CHEST_OPEN` | no | Opened chest. |
| `0x3C` | `BARREL` | no | Barrel. |
| `0x3D` | `CRATE` | no | Crate. |
| `0x3E` | `CANDELABRA` | no | Standing candelabra. |

Rules for dungeons:

- **Rooms and corridors.** Dungeon rooms are rectangles joined by corridors one or two
  cells wide. `ARCHWAY` and `EDGE_LEFT_RIGHT_EXIT` join rooms through walls one cell thick.
- **Light the rooms.** Put a `TORCH` every three to five cells of wall, or a `BRAZIER` on
  the floor.
- **Columns in pairs.** Place `COLUMN` cells in a regular pattern, and leave two walkable
  cells between columns.
- **Cells.** A cell is a small room whose wall row holds `BARS` on both sides of a
  `BARS_DOOR`. The cell lies above the corridor.
- **State changes are content code.** `BARS_DOOR` opens to `BARS_DOOR_OPEN`. `DUNGEON_DOOR`
  and `DUNGEON_DOOR_LOCKED` open to `DUNGEON_DOOR_OPEN`. `SPIKES` lower to `SPIKES_DOWN`.
  A cracked wall opens to `ARCHWAY`. The handler swaps the tile with `set_tile<POS>()` and
  records the state in a world flag, because the cache is rebuilt from the map when it
  reloads. The map shows the initial state.
- **Levers and plates.** `SWITCH_LEFT` and `SWITCH_RIGHT` are interactive, and the handler
  uses `player_just_interacted<POS>()`. A `PRESSURE_PLATE` handler uses
  `game.player_is_on_tile<POS>()`.
- **Open doors are exits.** `BARS_DOOR_OPEN` and `DUNGEON_DOOR_OPEN` are walkable. If the
  room behind is on another map, the handler teleports. If the room is directly above on
  the same map, use `ARCHWAY` as the open state so the wall top band is broken.
- **A tomb is two cells tall.** `TOMB_HEAD` goes above `TOMB_FOOT`.
- A dungeon reached through `CLIFF_MINE` outdoors has an `EDGE_TOP_EXIT` that leads back.

## Solidity

- `EMPTY`, edge cells, wall faces, wall decorations, furniture, pools, pits and raised
  spikes are fully solid (`65535`).
- Floors, rugs, rails, floor decorations and every exit are fully walkable (`0`). Closed
  doors are solid.
- `POT_BROKEN`, `SWITCH_LEFT` and `SWITCH_RIGHT` are solid on the bottom half (`0xFF00`).
- NPCs treat any nonzero mask as solid. NPCs can step onto exit cells, so keep routes and
  roaming areas away from them.
- The player's sprite overlaps the wall face above it and the side bands beside it by a
  few pixels. That is expected.

## Layout principles

- **Small rooms.** The view is 8x4 tiles. A floor of 6x2 fits one screen with its walls. A
  typical house room has a floor of 4x3 to 8x5. Larger rooms scroll.
- **Any outline.** A room need not be a rectangle. Give larger houses an alcove or an
  L-shaped room, and fill the space outside with `0x00`.
- **Furniture along the walls.** Put beds, dressers, counters and stoves against the wall
  row or in corners. Keep the middle of the room open.
- **A clear path.** Every floor cell the player should reach connects to the exit. Leave
  a path one cell wide, and two cells wide where an NPC walks.
- **Group by use.** A kitchen has a stove, a counter with a sink, a cupboard and hanging
  pans. A bedroom has a bed, a dresser and a wardrobe. A living room has a table with
  chairs, a rug and a fireplace.
- **Do not fill the room.** Leave at least half of the floor cells empty.
- **One light source or more.** Every room has a window, a sconce, a torch, a lit
  fireplace, a brazier or a candelabra.

## Before committing an interior map

- The map uses one tileset, and no cell is unpainted. Void is `0x00`.
- Every non-room cell above a room cell is a wall row piece.
- Every edge cell uses the piece that its bands and dots select.
- Each wall face next to a room cell is an end or pillar piece, and no decorated wall row
  piece stands next to a room cell.
- One wall style and one floor family are used per room. Wood, stone and plain floors mix
  the cells of their family. A tile floor uses one tile cell throughout.
- No chair, stool, plant, vase or other piece that leaves its cell partly empty stands
  on `FLOOR_CHECKER`.
- Every walkable exit has a handler point that teleports, except `ARCHWAY` and
  `EDGE_LEFT_RIGHT_EXIT`. The floor cell in front of every exit is clear.
- Every house room has a `<House>Exit` point with `leave_house`, a matching `<House>Door`
  outdoors, and an entry in `HOUSE_LINKS`.
- No other room is on screen from any cell the player can stand on.
- Rugs and pools are at least 2x2. Beds, tombs, long tables and counters are complete, and
  chairs face their table.
- A cave has no long straight runs in its outline, and every rail end meets another rail,
  a wall or an exit.
- Tiles that change state show their initial state, and content code records the change.
- The spawn is on a floor cell with no handler.
