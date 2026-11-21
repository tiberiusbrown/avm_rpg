# Interior map and tile authoring guide

This guide tells agents how to build rooms, and houses in particular. It sits beside
[AUTHORING.md](AUTHORING.md), which covers the world tileset and outdoor maps. The rules
there for collision masks, objects, handlers and routes apply here too. Read it first.

There are three interior tilesets. They share one room model and the same wall edge cells.
This guide covers the room model and the house tileset. Caves and dungeons have their own
guides, which build on this one.

| Tileset | Atlas | For | Used by | Guide |
| --- | --- | --- | --- | --- |
| `house.tsx` | `tiles16_house.png` | Houses, shops and cellars | `interiors.tmx` | this guide |
| `cave.tsx` | `tiles16_cave.png` | Caves and mines | `cave.tmx` | [AUTHORING_CAVE.md](AUTHORING_CAVE.md) |
| `dungeon.tsx` | `tiles16_dungeon.png` | Dungeons, crypts and halls | `dungeon.tmx` | [AUTHORING_DUNGEON.md](AUTHORING_DUNGEON.md) |

## Files

- `assets/tilesets/house.tsx`, `cave.tsx` and `dungeon.tsx`: metadata for every atlas cell.
- `assets/images/tiles16_house.png`, `tiles16_cave.png` and `tiles16_dungeon.png`: the
  256x256 monochrome atlases.
- `assets/images/tiles16_<name>_collision.png`: each atlas with solid collision pixels
  tinted red. The generator writes them on every run.
- `assets/maps/interiors.tmx`: the inside of every house, in one map. `cave.tmx` and
  `dungeon.tmx` are one cave and one dungeon.
- **Each map uses exactly one tileset.** The game draws and collides an area with the
  tileset of its map. A house map uses `house.tsx`, a cave map uses `cave.tsx`, and a
  dungeon map uses `dungeon.tsx`. Pieces from two tilesets cannot share a map.
- The generator emits tile constants per tileset: `HOUSE_TILE_<symbol>`,
  `CAVE_TILE_<symbol>` and `DUNGEON_TILE_<symbol>`.

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

**Everything that stands on the floor shows its top as well as its front.** A top that faces
up is lit: it is filled white. A front is dark inside a white outline. A table is a white top
over an apron and legs, a barrel is a white lid over a banded body, and a stove is a white
hob over its oven door. The light comes from the top left, so a round dark surface takes a
second line inside its rim, or a few hatch strokes, on its right and lower side. No piece is
filled with a checkerboard stipple, which flickers when the view scrolls.
Pieces on the north wall face the viewer, so they are drawn from the front. No piece shows
only its side.

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

Do not box a floor cell in. Furniture blocks most of its cell, so a floor cell with furniture
or walls on all four sides cannot be reached.

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
| `0x10` | `WALL` | neither | Plain wall face. In a house it is plaster over a panelled dado. |
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
  `src/logic/area_village.h`.
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
| `0x14` | `WALL_WOOD` | neither | Wall of upright boards. |
| `0x15` | `WALL_WOOD_END_LEFT` | left | Wood-panelled wall with its left end exposed. |
| `0x16` | `WALL_WOOD_END_RIGHT` | right | Wood-panelled wall with its right end exposed. |
| `0x17` | `WALL_WOOD_PILLAR` | both | Wood-panelled wall one cell wide. |
| `0x18` | `WALL_STONE` | neither | Wall of stone blocks in courses, for cellars. |
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
| `0x40` | `FLOOR_WOOD_A` | yes | Floorboards eight pixels wide: a stretch of each seam and one board end. Mix the four wood cells. |
| `0x41` | `FLOOR_WOOD_B` | yes | Floorboards, with the seams and the board end elsewhere. |
| `0x42` | `FLOOR_WOOD_C` | yes | Floorboards, with the seams and the board end elsewhere. |
| `0x4E` | `FLOOR_WOOD_D` | yes | Floorboards, with the seams and the board end elsewhere. |
| `0x43` | `FLOOR_TILE_A` | yes | Floor tiles marked by small crosses where tiles meet. It tiles with itself. |
| `0x44` | `FLOOR_TILE_B` | yes | Small floor tiles marked by a grid of dots. It tiles with itself. |
| `0x45` | `FLOOR_CHECKER` | yes | Checkered tiles eight pixels square: whole lit squares and dark squares. Dense. It tiles with itself. |
| `0x46` | `FLOOR_STONE_A` | yes | Flagstones in a running bond, shown by a mark where three stones meet. Mix the two stone cells. |
| `0x47` | `FLOOR_STONE_B` | yes | Flagstones with two specks. |
| `0x48` | `FLOOR_PLAIN_A` | yes | Bare floor with a few specks. Mix the two plain cells. |
| `0x49` | `FLOOR_PLAIN_B` | yes | Bare floor. |
| `0x4C` | `DOORMAT` | yes | Striped doormat. Put it on the floor cell above an `EDGE_TOP_EXIT`. |
| `0x4D` | `RUG_SMALL` | yes | Oval rug with a border, one cell. |

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

Tables and seats. Furniture stands on room cells. See [Solidity](#solidity) for how it
blocks.

| Hex | Symbol | Passable | Meaning |
| --- | --- | --- | --- |
| `0x54` | `TABLE` | partly | Table, one cell. |
| `0x55` | `TABLE_LEFT` | partly | Left end of a long table. |
| `0x56` | `TABLE_MIDDLE` | partly | Middle of a long table. Repeat it for longer tables. |
| `0x57` | `TABLE_RIGHT` | partly | Right end of a long table. |
| `0x58` | `TABLE_SET` | partly | One-cell table set with a plate and a mug. |
| `0x59` | `TABLE_CANDLE` | partly | One-cell table with a candle. |
| `0x5A` | `CHAIR_SOUTH` | partly | Chair that faces south, toward the viewer. It goes north of a table. |
| `0x5B` | `CHAIR_NORTH` | partly | Chair that faces north, seen from behind. It goes south of a table. |
| `0x5C` | `CHAIR_WEST` | partly | Chair that faces west. It goes east of a table. |
| `0x5D` | `CHAIR_EAST` | partly | Chair that faces east. It goes west of a table. |
| `0x5E` | `STOOL` | partly | Round stool. |
| `0x5F` | `BENCH` | partly | Short bench. |

Bedroom and storage:

| Hex | Symbol | Passable | Meaning |
| --- | --- | --- | --- |
| `0x64` | `BED_HEAD` | no | Head of a bed, with the pillow. `BED_FOOT` goes directly below it. |
| `0x74` | `BED_FOOT` | no | Foot of a bed. |
| `0x65` | `DRESSER` | partly | Chest of drawers. |
| `0x66` | `CABINET` | partly | Low cabinet with two doors. |
| `0x67` | `SHELF_LOW` | partly | Low bookshelf that stands on the floor. |

Kitchen and stores:

| Hex | Symbol | Passable | Meaning |
| --- | --- | --- | --- |
| `0x68` | `STOVE` | partly | Cooking stove with a pot. |
| `0x69` | `COUNTER_LEFT` | partly | Left end of a counter. |
| `0x6A` | `COUNTER_MIDDLE` | partly | Middle of a counter. Repeat it for longer counters. |
| `0x6B` | `COUNTER_RIGHT` | partly | Right end of a counter. |
| `0x6C` | `COUNTER_SINK` | partly | Counter middle with a basin and a tap. |
| `0x6D` | `BARREL` | partly | Barrel. |
| `0x6E` | `CRATE` | partly | Crate. |
| `0x6F` | `SACK` | partly | Sack. |
| `0x7C` | `BUCKET` | partly | Open bucket with a handle. |
| `0x7D` | `CAULDRON` | partly | Open cauldron on legs. Its brew bubbles. |

Other objects:

| Hex | Symbol | Passable | Meaning |
| --- | --- | --- | --- |
| `0x75` | `PLANT` | partly | Potted plant. |
| `0x76` | `VASE` | partly | Vase. |
| `0x77` | `CANDELABRA` | partly | Standing candelabra. |
| `0x78` | `POT` | partly | Unbroken pot. Collectible, as outdoors. |
| `0x79` | `POT_BROKEN` | partly | Broken pot. The collected state of `POT`. |
| `0x7A` | `CHEST_CLOSED` | partly | Closed chest with a rounded lid and a lock. Collectible, as outdoors. |
| `0x7B` | `CHEST_OPEN` | partly | Opened chest with its lid raised. The collected state of `CHEST_CLOSED`. |

Rules for houses:

- **Floors are quiet on purpose.** Wood, tile, stone and plain floors are a few marks on
  black. Furniture cells are opaque and carry their own black background, so they sit on
  these floors without a visible edge. A new floor keeps to about twenty lit pixels.
- **Use one floor family per room:** wood for living rooms and bedrooms, tile for kitchens
  and shops, stone for cellars, plain for sheds and storerooms.
- **Mix the cells of the wood, stone and plain families,** because a run of one cell
  repeats visibly. Floorboards run left to right, and the marks are stretches of the seams
  between boards. The two stone cells share one grid of stones, so they join without a seam.
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
- **Shop.** `ShopCounter` is the `open_shop` point on the middle of the counter.
  Keep the floor south of it clear. The dialogue-stage handler requests the shop
  modal after world dispatch returns. `ShopExit` remains the walk-out doorway.
- **Inn.** `InnRest` is the `rest_at_inn` point on the right end of the counter.
  Keep the floor south of it clear so the player can face the counter and use A.
  `InnExit` remains the walk-out doorway.
- **Interactive tiles block.** A pot or chest takes its handler point on its own
  cell, and the player interacts from the next cell while facing it. Content code records
  the collected state, as outdoors.

## Cave and dungeon tilesets

[AUTHORING_CAVE.md](AUTHORING_CAVE.md) and [AUTHORING_DUNGEON.md](AUTHORING_DUNGEON.md)
describe the cave and dungeon tilesets: their wall row pieces, floors, objects, animation and
the mechanisms for puzzles, such as levers, gates, pressure plates, spikes and bridges. Both
use the room model, the edge pieces, the wall faces and the exits of this guide.

## Animation

Interiors animate as the world does. See [AUTHORING.md](AUTHORING.md#animation) for the
phase layer atlases and the ambient classes. The house tileset's layers are
`tiles16_house_phase1.png` to `tiles16_house_phase3.png`.

- Animated house tiles: `SCONCE`, `FIREPLACE_LIT`, `TABLE_CANDLE`, `CANDELABRA` (flames),
  `CLOCK` (pendulum), `CAULDRON` (bubbles), `STOVE` (steam) and `WINDOW_CURTAINS` (curtains).
- The floor cells of the wood, tile, checker, stone and plain families have the ambient class
  `floor`: a moth or a drifting speck of dust appears over them. Rugs and the doormat have no
  class.
- `STOVE` and `CAULDRON` have the class `steam`: a puff rises from them.
- **Every house has an animated tile and some bare floor.** The view is four rows high, so
  the wall row is often out of sight. The floor effects keep the room alive then. The
  content test checks both.
- The cave and dungeon tilesets have their own layers and ambient classes. Their guides list
  them.

## Solidity

- **The room's structure blocks its whole cell.** `EMPTY`, edge cells, wall faces and every
  wall row piece are fully solid (`65535`), except the doorways.
- **Floors, rugs and every exit are fully walkable (`0`).** Closed doors are solid.
- **Everything that stands on the floor takes a simple convex shape** that roughly covers
  its art: the whole cell for a piece that fills it, the cell without its top corners for a
  piece with a rounded top, the cell without its bottom corners for a piece that narrows to
  its feet, or the diamond for a small round piece.
  [AUTHORING.md](AUTHORING.md#what-solidity-means) lists the shapes. The shaped masks of the
  house tileset: `CHAIR_SOUTH` (`0x93FF`), `CHAIR_NORTH` (`0x93FF`), `CHAIR_WEST` (`0x93FF`), `CHAIR_EAST` (`0x93FF`), `STOOL` (`0x93FF`), `BENCH` (`0xFFC6`), `CABINET` (`0xFFC6`), `SACK` (`0xFFC6`), `PLANT` (`0x93C6`), `VASE` (`0xFFC6`), `CANDELABRA` (`0x93FF`), `POT` (`0xFFC6`), `POT_BROKEN` (`0xFF00`), `CHEST_CLOSED` (`0xFFC6`), `CHEST_OPEN` (`0xFFC6`), `BUCKET` (`0xFFC6`), `CAULDRON` (`0xFFC6`).
- **Pieces and states of one thing take one mask.** Every piece of a long table or a counter,
  the four chairs, and a chest shut or open block alike. Never use a concave mask for an
  object.
- **A piece always blocks somewhere.** No object has the mask `0`. A piece that leaves much
  of its cell bare, such as a stool or a plant, is not fully solid. Check
  `tiles16_house_collision.png` after you set a mask.
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
- Rugs are at least 2x2. Beds, long tables and counters are complete, and chairs face their
  table.
- No floor cell is boxed in by furniture and walls.
- A cave or a dungeon also passes the checklist of its own guide.
- Tiles that change state show their initial state, and content code records the change.
- The spawn is on a floor cell with no handler.
