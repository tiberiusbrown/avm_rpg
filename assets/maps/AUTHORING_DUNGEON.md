# Dungeon map and tile authoring guide

This guide tells agents how to build dungeons, crypts and halls with the dungeon tileset. A
dungeon uses the room model of [AUTHORING_INTERIOR.md](AUTHORING_INTERIOR.md): room cells, a
wall row, edge cells and void, with the shared edge pieces `0x01` to `0x0F`, the wall faces
`0x10` to `0x13` and the exits. Read that guide first, then [AUTHORING.md](AUTHORING.md) for
collision masks, objects and handlers. This guide covers what is particular to dungeons.

## Files

- `assets/tilesets/dungeon.tsx`: metadata for every atlas cell.
- `assets/images/tiles16_dungeon.png`: the 256x256 monochrome atlas.
- `assets/images/tiles16_dungeon_phase1.png` to `tiles16_dungeon_phase3.png`: the animation
  layers.
- `assets/images/tiles16_dungeon_collision.png`: the atlas with solid pixels tinted red. The
  generator writes it on every run.
- `assets/maps/dungeon.tmx`: the Old Vault, behind the mine door north-east of the village.
  `DUNGEON_preview.png` shows the whole map.
- The generator emits `DUNGEON_TILE_<symbol>` constants and `AREA_DUNGEON`.

## How a dungeon looks

A dungeon is seen from the front and slightly above, as every map is. Its walls are straight
white bands, and the north wall shows a face of stone blocks one tile tall. The floor is
flagstones, shown only by a small mark where three stones meet. Objects show their top as well
as their front: a top that faces up is lit white, and a front is dark inside its outline. A
column shows its capital and its plinth, a brazier shows the rim of its bowl, and a raised
block shows its whole top face. The light comes from the top left, so a rounded piece takes a
second line inside its rim, or a few hatch strokes, on its right and lower side. Draw new
pieces the same way. No piece is filled with a checkerboard stipple, which flickers when the
view scrolls.

## Wall row pieces

Each replaces a `WALL` face. A decorated piece cannot stand next to a room cell.

| Hex | Symbol | Passable | Meaning |
| --- | --- | --- | --- |
| `0x23` | `WALL_ALT` | no | Stone wall with a chipped block. Mix it with `WALL`. |
| `0x40` | `WALL_MOSS` | no | Stone wall with moss under its top. Mix it in near water. |
| `0x14` | `WALL_CRACKED` | no | Stone wall with a deep crack. A weak wall that content code can open to `ARCHWAY`. |
| `0x15` | `TORCH` | no | Wall torch with a flame. |
| `0x16` | `GRATE_WINDOW` | no | Small barred window high in the wall. |
| `0x17` | `CHAINS` | no | Two hanging shackles that sway. |
| `0x18` | `BANNER` | no | Hanging banner that stirs. |
| `0x41` | `WALL_FACE` | no | Carved stone face. Its eyes close and open. |
| `0x42` | `WALL_FOUNTAIN` | no | Carved head that pours water into a basin. |
| `0x43` | `WALL_NICHE` | no | Niche with a skull. |
| `0x45` | `WALL_RUNES` | no | Two lines of carved runes that glint. |
| `0x49` | `PLAQUE` | no | Inscribed tablet. `show_message` belongs on it. |
| `0x47` | `BOOKCASE` | no | Tall bookcase against the wall. |
| `0x48` | `WEAPON_RACK` | no | Shield over crossed swords. |
| `0x19` | `BARS` | no | Iron bars across the wall row. The front of a cell. |
| `0x1A` | `BARS_DOOR` | no | Closed cell door of iron bars. |
| `0x1B` | `BARS_DOOR_OPEN` | yes | Opened cell door. The set state of `BARS_DOOR`. A doorway. |
| `0x1D` | `DUNGEON_DOOR` | no | Closed heavy double door in an arch. |
| `0x1E` | `DUNGEON_DOOR_LOCKED` | no | Closed arched door with a lock plate. |
| `0x1F` | `DUNGEON_DOOR_OPEN` | yes | Open arch. The set state of both dungeon doors. A doorway. |
| `0x46` | `PORTCULLIS` | no | Arch closed by a portcullis. It opens to `ARCHWAY` or `DUNGEON_DOOR_OPEN`. |
| `0x20` | `ARCHWAY` | yes | Open passage through the north wall to the room directly above. It breaks the wall top band. |
| `0x21` | `STAIRS_UP` | yes | Stairs that climb into the north wall. A doorway. |
| `0x22` | `LADDER_UP` | yes | Ladder up through a hatch in the ceiling. A doorway. |

## Floor pieces

| Hex | Symbol | Passable | Meaning |
| --- | --- | --- | --- |
| `0x24` | `FLOOR_A` | yes | Flagstones. Mix the two floor cells. |
| `0x25` | `FLOOR_B` | yes | Flagstones with two specks. |
| `0x26` | `FLOOR_CRACKED` | yes | Flagstones with cracks. An accent: about one floor cell in sixteen. |
| `0x27` | `FLOOR_GRATE` | yes | Drain grate in the floor. Water drips onto it. |
| `0x2E` | `PUDDLE` | yes | Shallow puddle that shimmers. Water drips into it. |
| `0x2F` | `BONES` | yes | Skull and bone. Decoration. |
| `0x2B` | `STAIRS_DOWN` | yes | Stairwell down through the floor. A doorway. |
| `0x2C` | `TRAPDOOR` | yes | Trapdoor with a ring pull. A doorway. |
| `0x2D` | `PIT` | no | Deep pit. Solid, so nothing walks into it. |
| `0x56` | `CHANNEL_HORIZONTAL` | no | Stone channel of running water, west to east. |
| `0x57` | `CHANNEL_VERTICAL` | no | Stone channel of running water, north to south. |

- **The floor tiles in every direction.** The three flagstone cells share one grid of stones
  eight pixels square in a running bond, so any mix of them joins without a seam. Objects are
  opaque and hide the grid under them.
- **A channel runs straight from wall to wall.** There are no corner pieces. Channels are
  solid. Put a `WALL_FOUNTAIN` in the wall row at the head of a north-to-south channel.

A carpet of two or more cells in each direction uses its 3x3 block. Choose each cell by its
place in the carpet. Carpets are fully walkable.

| Hex | Symbol | Passable | Meaning |
| --- | --- | --- | --- |
| `0x50` | `CARPET_TOP_LEFT` | yes | Top-left corner. |
| `0x51` | `CARPET_TOP` | yes | Top edge. |
| `0x52` | `CARPET_TOP_RIGHT` | yes | Top-right corner. |
| `0x60` | `CARPET_LEFT` | yes | Left edge. |
| `0x61` | `CARPET_CENTER` | yes | Interior, with a diamond. |
| `0x62` | `CARPET_RIGHT` | yes | Right edge. |
| `0x70` | `CARPET_BOTTOM_LEFT` | yes | Bottom-left corner. |
| `0x71` | `CARPET_BOTTOM` | yes | Bottom edge. |
| `0x72` | `CARPET_BOTTOM_RIGHT` | yes | Bottom-right corner. |

## Mechanisms

These pieces come in pairs: the state that the map paints, and the state that content code
swaps in.

| Hex | Symbol | Passable | Meaning |
| --- | --- | --- | --- |
| `0x35` | `SWITCH_LEFT` | partly | Lever, not thrown. Interactive. |
| `0x36` | `SWITCH_RIGHT` | partly | Lever, thrown. The set state of `SWITCH_LEFT`. |
| `0x28` | `PRESSURE_PLATE` | yes | Pressure plate, up. It goes down under the player. |
| `0x4A` | `PRESSURE_PLATE_DOWN` | yes | Pressure plate, down. The set state of `PRESSURE_PLATE`. |
| `0x4B` | `GATE_FRONT` | no | Bars across a passage that runs north to south. Seen from the front. |
| `0x4C` | `GATE_FRONT_OPEN` | yes | The bars drawn down into the floor. The set state of `GATE_FRONT`. |
| `0x4D` | `GATE_SIDE` | partly | Bars across a passage that runs west to east. Seen end on, on the left of its cell. |
| `0x4E` | `GATE_SIDE_OPEN` | yes | The bars drawn down into the floor. The set state of `GATE_SIDE`. |
| `0x29` | `SPIKES` | no | Raised spikes. Solid. |
| `0x2A` | `SPIKES_DOWN` | yes | Spikes drawn down into the floor. The set state of `SPIKES`. |
| `0x4F` | `BLOCK_RAISED` | no | Stone block that stands in the way. Its top face is lit. |
| `0x53` | `BLOCK_LOWERED` | yes | The block sunk level with the floor. The set state of `BLOCK_RAISED`. |
| `0x54` | `RUNE` | yes | Rune circle, dormant. |
| `0x55` | `RUNE_LIT` | yes | Rune circle, lit and turning. The set state of `RUNE`. |
| `0x5A` | `ORB_DARK` | partly | Orb on a pedestal, dark. Interactive. |
| `0x5B` | `ORB_LIT` | partly | The orb lit, with rays. The set state of `ORB_DARK`. |

- **A barrier fills a passage.** `GATE_FRONT`, `SPIKES` and `BLOCK_RAISED` stand in a passage
  that runs north to south, across its whole width. `GATE_SIDE` stands in a passage one cell
  wide that runs west to east, with a wall face above it and an edge cell below it. It blocks
  the left half of its cell, which closes the whole passage.
- **A wall one cell thick takes a barrier in its wall row.** Two rooms, one above the other,
  share a row of wall faces. Paint `BLOCK_RAISED` or `GATE_FRONT` in that row as room cells, and
  the faces beside them become `WALL_END_LEFT` and `WALL_END_RIGHT`. A `PORTCULLIS` or a
  `BARS_DOOR` is a wall row piece itself, and it opens to `ARCHWAY` or `BARS_DOOR_OPEN`.
- **Paint the clear state.** The map shows every mechanism as it is before the player touches
  it: levers as `SWITCH_LEFT`, plates up, barriers closed, runes and orbs dark. The handler
  shows the other state while the mechanism's world flag is set.
- **Handlers.** Each mechanism cell takes a handler point, and each is listed with its flag and
  its set tile in `AreaMechanisms<AREA_DUNGEON>` in `src/logic/area_dungeon.h`. A cell that is
  missing from the table fails to compile.

  | Handler | On | What it does |
  | --- | --- | --- |
  | `use_lever` | `SWITCH_LEFT` | Facing it and pressing A toggles the flag. |
  | `press_plate` | `PRESSURE_PLATE` | Standing on it sets the flag for good. |
  | `open_door` | `BARS_DOOR` | Facing it and pressing A sets the flag for good. |
  | `light_orb` | `ORB_DARK` | Pressing A sets the flag for good and says what happened. |
  | `show_state` | a gate, spikes, blocks, a rune, a portcullis | Shows the set tile while the flag is set. |
  | `open_chest` | `CHEST_CLOSED` | Pressing A gives 25 gold, an Elixir and a Revive once if both stacks have room. |
  | `show_message` | `PLAQUE` | Pressing A shows the plaque's words, listed in `AreaMessages<AREA_DUNGEON>`. |
  | `take_passage` | an exit or a doorway | Walking onto it leaves the dungeon. See [Exits](#exits). |

- **One flag can drive several cells.** The orb, both runes and the vault's portcullis share
  one flag, and each has its own row in the table. A rectangle covers a barrier of several
  cells with one object.
- **Every row has a kind.** The kind is the first field of a row in the table, and it says what
  the cell is: `MECHANISM_LEVER`, `MECHANISM_PLATE`, `MECHANISM_DOOR`, `MECHANISM_ORB` or
  `MECHANISM_CHEST` for a cell that the player works, and `MECHANISM_GATE`, `MECHANISM_BLOCKS`,
  `MECHANISM_SPIKES`, `MECHANISM_BRIDGE` or `MECHANISM_RUNE` for a cell that only follows its
  flag. `mechanism_traits` in `src/logic/mechanisms.h` gives each kind its input (face and press
  A, stand on it, or none), whether a second operation undoes the first, and its sounds. A
  handler that does not suit its row's kind fails to compile, so `use_lever` on a `MECHANISM_PLATE`
  row is an error. Every flag needs a row that the player works.
- **Sounds.** The player's operation is heard: the worked cell makes its own sound, and then the
  first cell of the same flag that moves and makes a noise is heard after it. A lever that opens
  a gate clicks and then the gate grinds. A rune only lights, so it has no sound, and the
  cell that moves next to it is what is heard. A kind that never switches back has no sound for
  switching back. A flag that a load sets changes the cells silently.

- **A lever toggles. A plate, a door and an orb latch.** Use a lever where the player may need
  to close the way again, and the others where a step should stay solved.

## Objects

Objects stand on room cells. Each takes the simple convex collision shape that roughly covers
its art: the whole cell, the diamond, or the cell without two of its corners. States and pieces
of one thing take the same shape. See [AUTHORING.md](AUTHORING.md#what-solidity-means).

| Hex | Symbol | Passable | Meaning |
| --- | --- | --- | --- |
| `0x30` | `COLUMN` | no | Stone column. |
| `0x31` | `STATUE` | partly | Statue on a pedestal. |
| `0x5E` | `STATUE_BROKEN` | partly | Pedestal with the stump of a statue. |
| `0x32` | `BRAZIER` | partly | Burning brazier. |
| `0x33` | `ALTAR` | no | Stone altar with a cross and a candle. |
| `0x34` | `TOMB_HEAD` | no | Head of a stone tomb. `TOMB_FOOT` goes directly below it. |
| `0x44` | `TOMB_FOOT` | no | Foot of a stone tomb. |
| `0x58` | `THRONE` | no | Stone throne. |
| `0x59` | `PEDESTAL` | partly | Empty pedestal. |
| `0x5C` | `CAGE` | partly | Iron cage. |
| `0x3F` | `URN` | partly | Urn with a lid. |
| `0x3E` | `CANDELABRA` | partly | Standing candelabra. |
| `0x37` | `RUBBLE` | partly | Pile of small rocks. |
| `0x38` | `POT` | partly | Unbroken pot. Collectible. |
| `0x39` | `POT_BROKEN` | partly | Broken pot. The collected state of `POT`. |
| `0x3A` | `CHEST_CLOSED` | partly | Closed chest. Collectible. |
| `0x3B` | `CHEST_OPEN` | partly | Opened chest. The collected state of `CHEST_CLOSED`. |
| `0x3C` | `BARREL` | no | Barrel. |
| `0x3D` | `CRATE` | no | Crate. |

## Animation

Something moves on every screen of a dungeon. Animated tiles cost nothing per tile, and the
ambient effects keep no state. [AUTHORING.md](AUTHORING.md#animation) explains both.

- **Animated tiles.** `TORCH`, `BRAZIER`, `CANDELABRA` and the candle on `ALTAR` flicker.
  `CHAINS` and `BANNER` sway. `WALL_FACE` blinks, `WALL_FOUNTAIN` pours and `WALL_RUNES` glint.
  `PUDDLE` shimmers, and the two channels flow. `RUNE_LIT` turns and `ORB_LIT` shines.
- **Ambient classes.** The `ambient` property of a tile decides what may appear on it.

  | Class | Tiles | What appears |
  | --- | --- | --- |
  | `crypt` | the three flagstone cells | A pale wisp that drifts in slow loops, or grit that sifts down from the vault |
  | `drip` | `PUDDLE`, `FLOOR_GRATE` | A drop of water that falls and rings |
  | `pool` | the two channels | A ripple ring or a glint |
  | `crystal` | `WALL_RUNES`, `RUNE_LIT`, `ORB_LIT` | A glint |
  | `ember` | `TORCH`, `BRAZIER`, `CANDELABRA` | A spark that rises from the flame |

- **Keep an animated tile in sight.** From every flagstone cell, an animated tile lies within
  five columns and three rows. The content test checks this, and it checks that every 8x4 chunk
  with floor in it holds a tile with an ambient class. Carpets have no class, so leave
  flagstones around them.

## Exits

The edge exits and the doorways follow [AUTHORING_INTERIOR.md](AUTHORING_INTERIOR.md#exits).
A dungeon joins the outdoor map through passages.

- **A passage is a pair of markers**: an entrance outdoors and a doorway in the dungeon. Put a
  point with the handler `take_passage` on each, and add the pair to
  `AreaPassages<AREA_DUNGEON>` in `src/logic/area_dungeon.h`. A marker that is missing from the tables
  fails to compile.
- **A mine door pairs with an `EDGE_TOP_EXIT`.** The player walks north into `CLIFF_MINE`
  outdoors and arrives on the floor cell above the exit, facing north. Walking south onto the
  exit returns the player to the cell below the door. The table entry's `inside_step` is `-1`.
- **A hole pairs with a `LADDER_UP`**, with an `inside_step` of `1`, as in a cave.
- Keep the arrival cells fully walkable and clear of handlers.

## Rules for dungeons

- **Rooms and corridors.** Dungeon rooms are rectangles joined by corridors one or two cells
  wide. `ARCHWAY` and `EDGE_LEFT_RIGHT_EXIT` join rooms through walls one cell thick.
- **Mix the plain faces.** Paint about one `WALL` in four as `WALL_ALT` or `WALL_MOSS`.
- **Light the rooms.** Put a `TORCH` every three to five cells of wall, or a `BRAZIER` on the
  floor.
- **Columns in pairs.** Place `COLUMN` cells in a regular pattern, and leave two walkable cells
  between columns.
- **Cells.** A cell is a small room whose wall row holds `BARS` on both sides of a `BARS_DOOR`.
  The cell lies above the corridor. A cell whose door has no handler is scenery.
- **A tomb is two cells tall.** `TOMB_HEAD` goes above `TOMB_FOOT`.
- **Leave a way through.** An object blocks most of its cell. Do not close a corridor or box in
  a floor cell with objects. The content test fails when a walkable cell cannot be reached.

## The Old Vault

`dungeon.tmx` is a worked example. A plaque in the entrance hall names its four seals: spike,
bar, stone and light.

- The **entrance hall** in the south holds `DungeonLever`, which lowers `DungeonSpikes` in the
  corridor that leads north through an `ARCHWAY`.
- The **crossroads** has a corridor to each side and a wall of `DungeonBlocks` to the north.
- The **cell block** in the west has two cells. `DungeonCellDoor` opens the first, and
  `DungeonPlate` inside it opens `DungeonGate` in the east corridor.
- The **east wing** holds a channel fed by a fountain, and `DungeonBlockLever`, which sinks
  `DungeonBlocks`.
- The **shrine** north of the blocks holds `DungeonOrb`. Lighting it lights `DungeonRuneWest`
  and `DungeonRuneEast` and opens `DungeonVaultDoor`.
- The **vault** behind the shrine holds `DungeonChest`.

## Before committing a dungeon map

- The map uses `dungeon.tsx`, and no cell is unpainted. Void is `0x00`.
- Every non-room cell above a room cell is a wall row piece, and every edge cell uses the piece
  that its bands and dots select.
- No decorated wall row piece, `WALL_ALT` or `WALL_MOSS` stands next to a room cell.
- Carpets are at least 2x2, tombs are complete, and channels run straight from wall to wall.
- Every barrier fills its passage, and `GATE_SIDE` is used only across a passage one cell wide
  that runs west to east.
- Mechanisms are painted in their clear state. Each has a handler point on its tile and a row,
  with its kind, in `AreaMechanisms<AREA_DUNGEON>`, and each exit has a row in
  `AreaPassages<AREA_DUNGEON>`.
- Every walkable cell can be reached once the mechanisms are open, except inside a cell whose
  door has no handler.
- An animated tile is in sight of every flagstone cell, and every room has a light.
- The spawn is on a walkable cell with no handler.
- The generator, `test_pipeline.py` and `test_content.py` all pass.
