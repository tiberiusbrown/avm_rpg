# AVM Wayfarer — Nocturne Grove

C++20 exploration on the 128x64 Arduboy AVM display, with 16x16 terrain and
alpha-masked characters. D-pad moves, A reads the sign immediately north of the
starting position, and B opens the status/save menu. Two sample villagers show
standing and wandering characters using different rows of the actor atlas.

Starting New Game or Continue plays **The Legend of Zelda — Overworld**,
transcribed from the supplied `assets/music/z1overw.mid` (MIDI arrangement credited
to Jeffrey M Colletti). The complete 24-bar arrangement loops in 38.92 seconds,
in B-flat major and 4/4 at approximately 148 BPM. Two pulse voices carry melody
and harmony, with triangle bass and noise snare. All 455 pitched notes retain
their original pitches and onsets, including triplets and the bass's high
arpeggios; all 137 snare hits preserve the MIDI rhythm.
Gate fractions preserve MIDI articulation at the engine's 4 ms resolution;
five same-pitch harmony overlaps retrigger, and the final bass release is one
tick early to finish before Loop. Automatic envelopes approximate NES timbres.
MIDI drum ID 38 selects the snare; it maps to a high noise rate rather than D2.
Music continues through dialogue and menus; the title screen stays silent.
Channel levels peak at 11 for melody, 8 for harmony, 10 for bass, and 2 for snare.
`MUSIC_MASTER_VOLUME` in `src/platform_avm.cpp` controls overall music gain
and is currently 128 (400%); the four-voice mix stays within digital headroom.
The score is `assets/music/peaceful.avms`; repeated bars use named phrases,
and `8*2/3` denotes a triplet eighth. The MIDI is retained as a transcription
reference, and the score test compares every encoded pitch, onset, and release
against it over two loops.

The old outdoor/interior/hall maps and their quests have been replaced by one
18x12-tile forest clearing. **This development build rejects saves from versions
1–3.** Its save format is version 4; new saves still use 24 bytes.

- `assets/maps/outdoor.tmx`: authoritative sample map, collision and native handler anchors.
- `assets/tilesets/world.tsx`: row-major tile IDs, symbols and solidity.
- `assets_src/tiles16.png`: opaque 256x256 atlas; IDs 0x00–0xFE are usable.
- `assets_src/actors16.png`: 16 character rows, 12 frames per row, binary alpha.
- `src/logic/`: sign dialogue and sample NPC definitions, separate from generic mechanics.
- `assets/music/peaceful.avms`: editable note score with reusable instrument envelopes; the SDK's `avm_music.py` encoder
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
cmake --build build --config RelWithDebInfo --target rpg_audio_timing
python tools/collect_evidence.py --build build --write-docs
```

Audio uses Timer1 for 15,625 Hz output and 7,812.5 Hz synthesis, with the
separate 1 kHz Timer0 clock. The optional full-device build also provides
`rpg_audio_timing`. It measures
Timer1 entry lateness, PWM write intervals, synthesis cadence, ISR execution
time, and foreground interrupt-disabled spans in the packaged game. The full
run covers a complete song plus dialogue, menus, saving, walking, and collision;
it also checks FXC and calibrates against known NOP/CLI workloads. CSV and JSON
distributions are written to `build/audio-timing`; matplotlib adds PNG/SVG plots.
CTest's `audio_timing` runs shorter two-second gameplay windows. See
[the timing investigation](docs/audio_timing.md) for measured results and definitions.

See [Tiled/asset pipeline](docs/tiled_pipeline.md), [world architecture](docs/chunk_world.md),
[NPC mechanics](docs/essential_npcs.md) and [current measurements](docs/tiles16_measurements.md).
The M0 proof is an independent historical API diagnostic and retains its tiny test art.
