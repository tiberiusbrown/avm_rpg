# AVM Wayfarer — M1.6

Native C++20 pixel exploration for the 128x64 AVM display. Tiled maps generate
chunk-major program assets and per-tile native template calls. Map objects have
optional names and handlers, with no types. Named rectangles retain query metadata;
unnamed handlers use private compile-time positions. Every exploration
frame runs gameplay across all live chunks, rebuilds a 17x9 tile cache, completes
all tile handlers, then runs dialogue handlers. Ordinary tag overloads customize
each stage; generated empty defaults let new handler names compile before behavior
is written. Essential characters use a fixed eight-actor pool with native quest placement, pixel movement,
tile-based Tiled routes with automatic IDs and bounded corridor pass-through. See [essential NPCs](docs/essential_npcs.md).

- `assets/maps/*.tmx`, `assets/tilesets/*.tsx`: authoritative world content.
- `assets_src/*.png`: sprite atlases referenced by the tileset/exporter.
- `src/logic/`: ordinary C++ tile, interaction and quest functions.
- `generated/`: deterministic assets, named locations and direct-call dispatch.
- `tests/`, `tools/`: shared-rule, exporter, guest/device QA and measurements.
- `rpg.arduboy`: current playable package; open in Ardens or an FX-compatible player.

See [Tiled authoring/build commands](docs/tiled_pipeline.md),
[frame/flag/save architecture](docs/chunk_world.md), and
[performance evidence](docs/performance.md). D-pad moves, A talks, B opens the
travel menu. Face the north gate or bridge anchor tile and press A to toggle it, or face
the lantern in the house for the remembered choice and five-gold reward.

Face Mara north of the house to assign her to the northeast interior post. Enter the
eight-NPC corridor at outdoor tile (26,10); sustained movement always gets through.
Tiled route points select containing tiles; route IDs are assigned automatically.
NPCs hold their ground when the player can walk around, and otherwise step aside
smoothly after a brief sustained push. Current
[AVM measurements](docs/m1_6_measurements.md) include eight-NPC scenes.
