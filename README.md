# AVM Wayfarer — Nocturne Grove

C++20 exploration on the 128x64 Arduboy AVM display, with 16x16 terrain and
alpha-masked characters. D-pad moves, A reads the sign immediately north of the
starting position, and B opens the status/save menu. Two sample villagers show
standing and wandering characters using different rows of the actor atlas.

Starting New Game or Continue plays **Dewlight Skip**, an original 30.72-second
loop in C major, in 4/4 at 125 BPM. A high, detached sine melody overlaps an
independent triangle countermelody. Tiny descending triangle notes mark each beat;
there are no block chords or sustained bass notes. Music continues through
dialogue and menus; the title screen stays silent.
The melody peaks at channel level 26, the counterline at 11 and beat accents at 14.
`MUSIC_MASTER_VOLUME` in `src/platform_avm.cpp` controls overall music gain
and is currently 64 (200%); short overlaps preserve digital mixer headroom.

The old outdoor/interior/hall maps and their quests have been replaced by one
18x12-tile forest clearing. **This development build rejects saves from versions
1–3.** Its save format is version 4; new saves still use 24 bytes.

- `assets/maps/outdoor.tmx`: authoritative sample map, collision and native handler anchors.
- `assets/tilesets/world.tsx`: row-major tile IDs, symbols and solidity.
- `assets_src/tiles16.png`: opaque 256x256 atlas; IDs 0x00–0xFE are usable.
- `assets_src/actors16.png`: 16 character rows, 12 frames per row, binary alpha.
- `src/logic/`: sign dialogue and sample NPC definitions, separate from generic mechanics.
- `assets/music/peaceful.json`: editable A2 score; the SDK's `avm_music.py` encoder
  rebuilds `generated/peaceful_music.h` during guest builds.
- `generated/`: deterministic program-memory assets and compile-time handler dispatch.
- `rpg.arduboy`: Arduboy FX package, refreshed by the default build.
- `rpg.fxc.arduboy`: Arduboy FX-C package, built from the same game with FX-C
  interpreter firmware (flash chip select on HWB/PE2), also refreshed by default.

The bottom-right atlas cell (0xFF) is reserved for the impassable outside-map
sentinel. Black cells retain their indices. Characters use direction order
north/east/south/west, rest on frame 1, and walk through 1,2,1,3.
Chunks cover 8x4 tiles; a 9x5 cache supports smooth scrolling and uses 108 fewer
RAM bytes than the previous 17x9 cache.

The installed SDK must include `bin/avm/interp.fxc.hex`. Update an older SDK
with the AVM checkout's `avm_sdk_install` CMake target before building.

From this repository root:

```powershell
./tools/build.ps1 -SdkRoot ../../build/avm-sdk-install
cmake --build build --config RelWithDebInfo --target rpg_integration
python tools/collect_evidence.py --build build --write-docs
```

See [Tiled/asset pipeline](docs/tiled_pipeline.md), [world architecture](docs/chunk_world.md),
[NPC mechanics](docs/essential_npcs.md) and [current measurements](docs/tiles16_measurements.md).
The M0 proof is an independent historical API diagnostic and retains its tiny test art.
