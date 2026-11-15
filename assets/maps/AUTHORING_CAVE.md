# Cave map and tile authoring guide

This guide tells agents how to build caves and mines with the cave tileset. A cave uses the
room model of [AUTHORING_INTERIOR.md](AUTHORING_INTERIOR.md): room cells, a wall row, edge
cells and void, with the shared edge pieces `0x01` to `0x0F`, the wall faces `0x10` to `0x13`
and the exits. Read that guide first, then [AUTHORING.md](AUTHORING.md) for collision masks,
objects and handlers. This guide covers what is particular to caves.

## Files

- `assets/tilesets/cave.tsx`: metadata for every atlas cell.
- `assets/images/tiles16_cave.png`: the 256x256 monochrome atlas.
- `assets/images/tiles16_cave_phase1.png` to `tiles16_cave_phase3.png`: the animation layers.
- `assets/images/tiles16_cave_collision.png`: the atlas with solid pixels tinted red. The
  generator writes it on every run.
- `assets/maps/cave.tmx`: Hollow Mine, the cave north of the village. `CAVE_preview.png` shows
  the whole map.
- The generator emits `CAVE_TILE_<symbol>` constants and `AREA_CAVE`.

## How a cave looks

A cave is seen from the front and slightly above, as every map is. Rock walls are rough white
bands, and the north wall shows its face one tile tall. The floor is black with pebbles.
Objects show their top as well as their front: a barrel shows its lid, a mine cart shows its
open box, and a boulder shows the ridge between its top and its front. The light comes from
the top left, so a rounded piece takes a second line inside its rim on its right and lower
side, and a few hatch strokes. Draw new pieces the same way. A piece that shows only its side
does not belong in this view, and no piece is filled with a checkerboard stipple, which
flickers when the view scrolls.

## Wall row pieces

Every non-room cell above a room cell is a wall row piece. The four wall faces are shared with
the other interior tilesets. These pieces are the cave's own. All are solid except the
doorways.

| Hex | Symbol | Passable | Meaning |
| --- | --- | --- | --- |
| `0x14` | `WALL_ALT` | no | Rock face with different cracks. Mix it with `WALL`. |
| `0x1E` | `WALL_MOSS` | no | Rock face with moss hanging from its top. Mix it in near water. |
| `0x15` | `WALL_ORE` | no | Rock face with ore nuggets that glint. |
| `0x16` | `WALL_CRYSTAL` | no | Rock face with crystals that glint. |
| `0x17` | `WALL_CRACKED` | no | Rock face with a deep crack. A weak wall that content code can open to `TUNNEL`. |
| `0x18` | `WALL_TIMBER` | no | Mine support frame of two posts and a beam. |
| `0x19` | `WALL_TORCH` | no | Rock face with a burning torch. |
| `0x1D` | `WALL_WATERFALL` | no | Water that runs down the rock face. Put a pool or a `PUDDLE` below it. |
| `0x1A` | `TUNNEL` | yes | Tunnel mouth in the rock face. A doorway. |
| `0x1F` | `TUNNEL_BARRED` | no | Tunnel mouth closed by bars. It opens to `TUNNEL`. |
| `0x1B` | `LADDER_UP` | yes | Ladder up through the rock. A doorway. It pairs with `HOLE_LADDER` outdoors. |

`WALL_ALT` and `WALL_MOSS` are plain faces: they have no end pieces, so they stand only where
`WALL` would, between two cells that are not room cells. The decorated pieces follow the same
rule.

## Floor pieces

All are fully walkable.

| Hex | Symbol | Passable | Meaning |
| --- | --- | --- | --- |
| `0x20` | `FLOOR_A` | yes | Bare ground with pebbles. Mix the three floor cells. |
| `0x21` | `FLOOR_B` | yes | Bare ground with pebbles. |
| `0x22` | `FLOOR_C` | yes | Bare ground with pebbles. |
| `0x2D` | `FLOOR_MOSS` | yes | Ground with patches of moss. Use it near water, about one floor cell in eight. |
| `0x29` | `PUDDLE` | yes | Shallow puddle that shimmers. Water drips into it. |
| `0x2A` | `BONES` | yes | Skull and bone. Decoration. |
| `0x2B` | `LADDER_DOWN` | yes | Hole with the top of a ladder. A doorway to a lower level. |
| `0x23` | `RAIL_HORIZONTAL` | yes | Mine rails that run west to east. |
| `0x24` | `RAIL_VERTICAL` | yes | Mine rails that run north to south. |
| `0x25` | `RAIL_WEST_SOUTH` | yes | Rail curve that joins the west and south sides. |
| `0x26` | `RAIL_EAST_SOUTH` | yes | Rail curve that joins the east and south sides. |
| `0x27` | `RAIL_WEST_NORTH` | yes | Rail curve that joins the west and north sides. |
| `0x28` | `RAIL_EAST_NORTH` | yes | Rail curve that joins the east and north sides. |

## Pools

A pool of two or more cells in each direction uses its 3x3 block. Choose each cell by its
place in the pool. Pools are solid.

Cave water is still, so a pool is quieter than water outdoors:

- **The rim wanders.** It steps in and out by a pixel and rounds the corners, as a shore
  does outdoors. It is never a straight line.
- **The surface lies a little below the rim.** Under the north rim, which faces the viewer,
  a dark row and a broken line show the water's edge.
- **The ripples barely move.** Every cell has two short ripples. For one phase of the four,
  one of them stands a pixel to the east, and for another phase the other stands a pixel to
  the west, so the two move in opposite directions.
- **The rim does not move.** Water does not lap at a pool's edge.

| Hex | Symbol | Passable | Meaning |
| --- | --- | --- | --- |
| `0x40` | `POOL_TOP_LEFT` | no | Top-left corner. |
| `0x41` | `POOL_TOP` | no | Top edge. |
| `0x42` | `POOL_TOP_RIGHT` | no | Top-right corner. |
| `0x50` | `POOL_LEFT` | no | Left edge. |
| `0x51` | `POOL_CENTER` | no | Interior. |
| `0x52` | `POOL_RIGHT` | no | Right edge. |
| `0x60` | `POOL_BOTTOM_LEFT` | no | Bottom-left corner. |
| `0x61` | `POOL_BOTTOM` | no | Bottom edge. |
| `0x62` | `POOL_BOTTOM_RIGHT` | no | Bottom-right corner. |

## Pits, chasms and bridges

| Hex | Symbol | Passable | Meaning |
| --- | --- | --- | --- |
| `0x2C` | `PIT` | no | One deep pit with a rim. Solid, so nothing walks into it. |
| `0x4D` | `CHASM_EDGE` | no | North row of a chasm: the far wall's face under a ragged lip. Its cracks thin out downward, so it fades into the dark. |
| `0x4E` | `CHASM` | no | Middle rows of a chasm: darkness. A black cell. |
| `0x4F` | `CHASM_RIM` | no | South row of a chasm: darkness, with the near lip as one ragged line along the bottom. |
| `0x5A` | `CHASM_LEFT` | no | West column of a chasm: the west lip, a rough edge with darkness beyond it. |
| `0x5B` | `CHASM_RIGHT` | no | East column of a chasm: the east lip, a rough edge with darkness beyond it. |
| `0x5C` | `CHASM_EDGE_LEFT` | no | North-west corner: the far wall's face and the west lip. |
| `0x5D` | `CHASM_EDGE_RIGHT` | no | North-east corner: the far wall's face and the east lip. |
| `0x5E` | `CHASM_RIM_LEFT` | no | South-west corner: the near lip and the west lip. |
| `0x5F` | `CHASM_RIM_RIGHT` | no | South-east corner: the near lip and the east lip. |
| `0x4B` | `BRIDGE_HORIZONTAL` | yes | Plank bridge that runs west to east. |
| `0x4C` | `BRIDGE_VERTICAL` | yes | Plank bridge that runs north to south. |

- **A chasm is darkness.** Only its lips and the fading face of its far wall are drawn. The
  far wall is the one wall that faces the viewer, so it alone shows a face. The side walls
  are not seen, so a side lip is a rough edge and nothing more. Do not texture the dark.
- **A chasm is a block of cells at least two rows deep and two columns wide.** Choose each
  cell by the sides on which floor lies, as for a pool:

  | Floor lies | Piece |
  | --- | --- |
  | north | `CHASM_EDGE` |
  | south | `CHASM_RIM` |
  | west | `CHASM_LEFT` |
  | east | `CHASM_RIGHT` |
  | north and west, north and east | `CHASM_EDGE_LEFT`, `CHASM_EDGE_RIGHT` |
  | south and west, south and east | `CHASM_RIM_LEFT`, `CHASM_RIM_RIGHT` |
  | on no side | `CHASM` |

- **A side with a wall beside it takes no lip.** A chasm that runs west to east from wall to
  wall is a row of `CHASM_EDGE` over a row of `CHASM_RIM`. A chasm that runs north to south
  from wall to wall is a column of `CHASM_LEFT` beside a column of `CHASM_RIGHT`, with `CHASM`
  between them when it is wider. A chasm in the middle of a floor uses the corners too.
- Chasm cells are room cells, so the walls beside them take their edge pieces as they do
  beside floor, and the cell above a chasm that touches the north wall is a wall face.
- **A bridge replaces chasm cells in a straight line**, from the floor on one side to the floor
  on the other. A north-to-south bridge is a column of `BRIDGE_VERTICAL`, and a west-to-east
  bridge is a row of `BRIDGE_HORIZONTAL` across a chasm that runs north to south.
- **A bridge can be a mechanism.** Paint the chasm, put a rectangle on the bridge's cells with
  the handler `show_state`, and the bridge appears while its flag is set. The rectangle may
  cover any chasm pieces. See
  [Mechanisms](#mechanisms).

## Mechanisms

These pieces come in pairs: the state that the map paints, and the state that content code
swaps in.

| Hex | Symbol | Passable | Meaning |
| --- | --- | --- | --- |
| `0x43` | `SWITCH_LEFT` | partly | Lever, not thrown. Interactive. |
| `0x44` | `SWITCH_RIGHT` | partly | Lever, thrown. The set state of `SWITCH_LEFT`. |
| `0x2E` | `PRESSURE_PLATE` | yes | Stone plate, up. It goes down under the player. |
| `0x2F` | `PRESSURE_PLATE_DOWN` | yes | Stone plate, down. The set state of `PRESSURE_PLATE`. |
| `0x45` | `GATE_FRONT` | no | Row of stakes across a passage that runs north to south. Seen from the front. |
| `0x46` | `GATE_FRONT_OPEN` | yes | The stakes drawn down into the floor. The set state of `GATE_FRONT`. |
| `0x47` | `GATE_SIDE` | partly | Row of stakes across a passage that runs west to east. Seen end on, on the left of its cell. |
| `0x48` | `GATE_SIDE_OPEN` | yes | The stakes drawn down into the floor. The set state of `GATE_SIDE`. |
| `0x49` | `VENT` | yes | Fissure in the floor, at rest. |
| `0x4A` | `VENT_ACTIVE` | no | The fissure blowing steam. Solid while it blows. |

- **A gate fills a passage one cell wide.** `GATE_FRONT` stands in a passage that runs north
  to south, with a cell that is not a room cell on its left and on its right. `GATE_SIDE`
  stands in a passage that runs west to east, with a wall face above it and an edge cell
  below it. `GATE_SIDE` blocks the left half of its cell, which closes the whole passage.
- **Paint the clear state.** The map shows every mechanism as it is before the player touches
  it: levers as `SWITCH_LEFT`, plates up, gates closed, bridges absent. The handler shows the
  other state while the mechanism's world flag is set.
- **Handlers.** Each mechanism cell takes a handler point, and each handler is listed with its
  flag and its set tile in `MECHANISMS` in `src/logic/handlers.h`. A cell that is missing from
  the table fails to compile.

  | Handler | On | What it does |
  | --- | --- | --- |
  | `use_lever` | `SWITCH_LEFT` | Facing it and pressing A toggles the flag. |
  | `press_plate` | `PRESSURE_PLATE` | Standing on it sets the flag for good. |
  | `show_state` | a gate, or a rectangle of chasm cells | Shows the set tile while the flag is set. |
  | `open_chest` | `CHEST_CLOSED` | Pressing A gives gold once and opens the chest. |
  | `steam_vent` | `VENT` | Blows for 96 ticks of every 256, on the game clock. |
  | `read_notice` | `SIGN` | Pressing A shows the notice's words, which `read_notice` holds. |
  | `take_passage` | an exit or a doorway | Walking onto it leaves the cave. See [Exits](#exits). |

- **One flag can drive several cells.** A lever and the gate it opens share a flag, and each
  has its own row in the table.
- **A vent needs no flag.** It reads the clock, so it keeps no state. A player who stands on
  it when it blows is moved off it. Put a vent in a passage one cell wide to make the player
  wait for it.

## Objects

Objects stand on room cells. Each takes the simple convex collision shape that roughly covers
its art: the whole cell, the diamond, or the cell without two of its corners. States and pieces
of one thing take the same shape. See [AUTHORING.md](AUTHORING.md#what-solidity-means).

| Hex | Symbol | Passable | Meaning |
| --- | --- | --- | --- |
| `0x30` | `BOULDER` | no | Boulder. |
| `0x31` | `RUBBLE` | partly | Pile of small rocks. |
| `0x32` | `STALAGMITE` | partly | One tall stalagmite. |
| `0x33` | `STALAGMITES` | partly | Two stalagmites. |
| `0x34` | `CRYSTALS` | partly | Crystal cluster that glints. |
| `0x3F` | `CRYSTAL_TALL` | partly | One tall crystal that glints. |
| `0x35` | `MUSHROOMS` | partly | Cave mushrooms that shed spores. |
| `0x36` | `CAMPFIRE` | partly | Burning campfire. |
| `0x55` | `LANTERN` | partly | Miner's lantern on the floor, lit. |
| `0x37` | `MINECART` | partly | Empty mine cart on west-to-east rails. |
| `0x57` | `MINECART_FULL` | partly | Mine cart loaded with ore. |
| `0x53` | `ORE_PILE` | partly | Pile of ore that glints. |
| `0x54` | `TOOLS` | partly | Pickaxe and shovel. |
| `0x56` | `BARRICADE` | partly | Plank barrier on trestles. |
| `0x58` | `SIGN` | no | Wooden notice. `read_notice` belongs on it. |
| `0x59` | `SKELETON` | partly | Remains of a miner. |
| `0x38` | `POT` | partly | Unbroken pot. Collectible. |
| `0x39` | `POT_BROKEN` | partly | Broken pot. The collected state of `POT`. |
| `0x3A` | `CHEST_CLOSED` | partly | Closed chest. Collectible. |
| `0x3B` | `CHEST_OPEN` | partly | Opened chest. The collected state of `CHEST_CLOSED`. |
| `0x3C` | `BARREL` | no | Barrel. |
| `0x3D` | `CRATE` | no | Crate. |
| `0x3E` | `SACK` | partly | Sack. |

## Animation

Something moves on every screen of a cave. Animated tiles cost nothing per tile, and the
ambient effects keep no state. [AUTHORING.md](AUTHORING.md#animation) explains both.

- **Animated tiles.** `WALL_TORCH`, `CAMPFIRE` and `LANTERN` flicker. The nine pool cells
  ripple very slightly, and `PUDDLE` shimmers. `WALL_WATERFALL` runs. `CRYSTALS`, `CRYSTAL_TALL`, `WALL_CRYSTAL`,
  `WALL_ORE` and `ORE_PILE` glint. `MUSHROOMS` shed spores, and `VENT_ACTIVE` blows.
- **Ambient classes.** The `ambient` property of a tile decides what may appear on it.

  | Class | Tiles | What appears |
  | --- | --- | --- |
  | `cave` | the three floor cells and `FLOOR_MOSS` | A bat that wheels about the chamber, or a drop of water that falls and rings |
  | `drip` | `PUDDLE` | A drop of water that falls and rings |
  | `pool` | the nine pool cells | A ripple ring or a glint |
  | `crystal` | the crystals and the ore | A glint on a facet |
  | `ember` | `WALL_TORCH`, `CAMPFIRE`, `LANTERN` | A spark that rises from the flame |
  | `steam` | `VENT_ACTIVE` | A puff of steam |

- **Keep an animated tile in sight.** From every floor cell, an animated tile lies within five
  columns and three rows. The content test checks this, and it checks that every 8x4 chunk
  with floor in it holds a tile with an ambient class. Rails, bridges and plates have no
  class, so do not fill a chunk with them.

## Exits

The edge exits and the doorways follow [AUTHORING_INTERIOR.md](AUTHORING_INTERIOR.md#exits).
A cave joins the outdoor map through passages.

- **A passage is a pair of markers**: an entrance outdoors and a doorway in the cave. Put a
  point with the handler `take_passage` on each, and add the pair to `PASSAGES` in
  `src/logic/handlers.h`. A marker that is missing from the table fails to compile.
- **A cave mouth pairs with an `EDGE_TOP_EXIT`.** The player walks north into `CLIFF_CAVE`
  outdoors and arrives on the floor cell above the exit, facing north. Walking south onto the
  exit returns the player to the cell below the cave mouth. The table entry's `inside_step`
  is `-1`.
- **A hole pairs with a `LADDER_UP`.** The player walks onto `HOLE_LADDER` outdoors and arrives
  on the floor cell below the ladder, facing south. Walking north onto the ladder returns the
  player to the cell below the hole. The table entry's `inside_step` is `1`.
- Keep the arrival cells fully walkable and clear of handlers.

## Rules for caves

- **Irregular outlines.** Step the outline in or out by one or two cells every two to four
  cells. Avoid straight runs longer than four cells and avoid rectangles.
- **Wall faces follow the outline.** Where the outline steps, the face next to a room cell is
  `WALL_END_LEFT`, `WALL_END_RIGHT` or `WALL_PILLAR`.
- **Mix the plain faces.** Paint about one `WALL` in three as `WALL_ALT` or `WALL_MOSS`.
- **Chambers and passages.** Make a cave from a few chambers joined by passages one or two
  cells wide. Use two cells where an NPC walks.
- **Rails join by their sides.** A straight rail joins the two sides it runs between. A curve
  joins the two sides in its name. Every rail end meets another rail, a wall or an exit. A
  mine cart stands on a west-to-east run, between `RAIL_HORIZONTAL` cells.
- **Mines.** A mine is a cave with rails, a `WALL_TIMBER` frame every few cells of wall and
  `WALL_TORCH` for light.
- **Leave a way through.** An object blocks most of its cell. Do not close a passage or box in
  a floor cell with objects. The content test fails when a walkable cell cannot be reached.

## Hollow Mine

`cave.tmx` is a worked example. Its chambers and its chain of mechanisms:

- The **entrance chamber** in the south holds a campfire and a notice. Its exit leads to the
  cave mouth north of the village green.
- The **gallery** runs west to east, with rails, two mine carts, timber frames and torches.
  `CaveLever` on its north wall opens `CaveGate`.
- The **vault** in the north-east lies behind `CaveGate`. It holds `CaveChest` and
  `CaveVaultLever`, which opens `CaveNookGate`.
- The **nook** west of the gallery lies behind `CaveNookGate`. `CavePlate` on its floor runs
  the bridge `CaveBridge` out over the chasm.
- The **grotto** in the north-west holds the pool under a waterfall.
- A passage one cell wide, with `CaveVent` in it, leads north from the gallery to the
  **chasm**. Across the bridge, `CaveLadder` climbs to `CaveHole` in the west meadow.

## Before committing a cave map

- The map uses `cave.tsx`, and no cell is unpainted. Void is `0x00`.
- Every non-room cell above a room cell is a wall row piece, and every edge cell uses the piece
  that its bands and dots select.
- No decorated wall row piece, `WALL_ALT` or `WALL_MOSS` stands next to a room cell.
- The outline has no long straight runs.
- Pools are at least 2x2. A chasm is at least two rows deep, with `CHASM_EDGE` on its north
  row and `CHASM_RIM` on its south row.
- Every rail end meets another rail, a wall or an exit.
- Every gate fills a passage one cell wide, and `GATE_SIDE` is used only across a passage that
  runs west to east.
- Mechanisms are painted in their clear state. Each has a handler point on its tile and a row
  in `MECHANISMS`, and each exit has a row in `PASSAGES`.
- Every walkable cell can be reached once the mechanisms are open.
- An animated tile is in sight of every floor cell.
- The spawn is on a walkable cell with no handler.
- The generator, `test_pipeline.py` and `test_content.py` all pass.
