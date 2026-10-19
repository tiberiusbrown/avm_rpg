# AVM Wayfarer — Nocturne Grove

C++20 exploration on the 128x64 Arduboy AVM display, with 16x16 terrain and
alpha-masked characters. D-pad moves, A reads the sign immediately north of the
starting position, and B opens the status/save menu. Two sample villagers show
standing and wandering characters using different rows of the actor atlas.

The soundtrack contains six original, peaceful looping songs. The town MIDI
informed their unhurried pacing and sustained, singing phrases; its melody
was not transcribed. Each new tune has a two-bar refrain that returns at bars
5 and 13, a contrasting middle section, and a final tonic cadence. Lead notes
are held across beats, with quiet plucks, answering phrases, and a gentle bass
pulse. The upper-register melodies sit above their accompaniment.
The previous songs and all project MIDI files have been removed.

| Song | Character | Key / meter | BPM | Loop |
|---|---|---|---:|---:|
| Dewlight Grove | Falling woodland refrain with a gentle rocking bass; the default | A-flat major, 4/4 | 104 | 36.924 s |
| Lantern Square | Tender dotted call, offbeat plucks, and faint brushed taps | C major, 4/4 | 108 | 35.556 s |
| Sunward Road | Soft rising call over a light walking pulse | G major, 4/4 | 112 | 34.284 s |
| Clockwork Hollow | Wistful minor melody, short answers, and a quiet clock tick | A minor, 4/4 | 100 | 38.400 s |
| Moonlit Quay | Spacious night song with late-bar answering notes | D minor, 4/4 | 90 | 42.668 s |
| Homeward Stars | Consoling upward fourth, gentle counterline, and a descent home | E-flat major, 4/4 | 100 | 38.400 s |

Press **B**, choose **Switch music** (or press Up twice from Status), and press **A**
to advance through this order. The selected title appears in the menu footer;
after Homeward Stars, selection wraps to Dewlight Grove. New Game and Continue
start with Dewlight Grove; the selection lasts for the current session.
Music continues through dialogue and menus; the title screen stays silent.

For the synthesis-rate experiment, press **B**, **Up**, **A** to toggle between
**Audio: 8 kHz** and **Audio: 16 kHz** while the song keeps playing. Repeated
presses of A compare the two rates; B returns to exploration. The selection
persists when the menu closes and resets to 8 kHz on restart. It is not saved.
The exact rates are 7812.5 and 15625 Hz, both without smoothing.

Five songs use a sustained triangle lead; Moonlit Quay uses a sine lead.
Sunward Road has a softer attack, a lower held-note level, and harmony that
supports its sustained melody notes. Sine plucks and short counterlines stay
below the lead in register and volume. Bass notes pulse or alternate with fifths;
Moonlit Quay retains more sustained bass for its quieter character. Lantern
Square, Sunward Road, and Clockwork Hollow add very brief noise taps at volume 1.
The other songs silence the noise voice.

Authored attack/release envelopes soften note transitions. Every song restores
all four channels on entry and finishes its release tails before Loop.
`MUSIC_MASTER_VOLUME` in `src/platform_avm.cpp` is 180 (562.5%), which can clip
the mix. The score test checks two complete loops of every song, all note
attacks, musical duration, state resets, and authored mixer headroom at the
reference master of 64.

The old outdoor/interior/hall maps and their quests have been replaced by one
18x12-tile forest clearing. **This development build rejects saves from versions
1–3.** Its save format is version 4; new saves still use 24 bytes.

- `assets/maps/outdoor.tmx`: authoritative sample map, collision and native handler anchors.
- `assets/tilesets/world.tsx`: row-major tile IDs, symbols and solidity.
- `assets_src/tiles16.png`: opaque 256x256 atlas; IDs 0x00–0xFE are usable.
- `assets_src/actors16.png`: 16 character rows, 12 frames per row, binary alpha.
- `src/logic/`: sign dialogue and sample NPC definitions, separate from generic mechanics.
- `assets/music/*.avms`: six editable original scores with named phrases and
  reusable instrument envelopes. `tools/generate_music.py` uses the SDK's
  `avm_music.py` encoder to rebuild `generated/music.h` and `music_catalog.h`.
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

Audio uses Timer1 for output and synthesis at the selected 7,812.5 or 15,625 Hz,
with the separate 1 kHz Timer0 clock. The optional full-device build also provides
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
