# AVM Wayfarer — Nocturne Grove

C++20 exploration on the 128x64 Arduboy AVM display, with 16x16 terrain and
alpha-masked characters. D-pad moves, A reads the sign immediately north of the
starting position and then starts a test battle, enters a house when the player faces
its door, and B opens the status/save menu. Two sample villagers show
standing and wandering characters using different rows of the actor atlas.

The sign's battle is a synchronous prototype. Its controls and rules are described in
[Battle prototype](#battle-prototype) below.

The build automatically discovers every `assets/music/*.avms` score and adds it
to the soundtrack. It uses `avm_music.py` through `tools/compile_music.py` to emit
C++ constexpr assets and generates the song IDs and playback list. Add or remove
a score and build normally; no CMake or C++ song-list edits are needed.
Menu titles come from each score's `title` setting, with the filename as a fallback.
The FamiTracker reference files remain in `assets/music/imported/`.

| Song | Character | Key / meter | BPM | Loop |
|---|---|---|---:|---:|
| Peaceful | Original NES-inspired village theme: pulse melody, broken chords, triangle bass | C major / 4/4 | 100 | 57.600 s |
| Boss | FamiTracker APU/VRC6 melody, pulse guitar, saw bass, and percussion | Tracker speed 5 | 200 | 34.000 s |
| Venus | FamiTracker VRC6 melody, APU harmony, saw bass, and percussion | Tracker speed 8 | 200 | 25.600 s |
| Good Endings | Cleaned MIDI glockenspiel, lute/horn/viola accompaniment, and electric bass | 4/4 | 120 | 172.000 s |

[`peaceful.avms`](assets/music/peaceful.avms) is a 24-bar A-B-A' town theme with
an original melody inspired by the gentle three-voice texture of NES Final Fantasy.
The middle eight bars introduce a higher answering melody and sparser counterpoint;
the final phrase resolves to C, with a pickup into the next loop. It uses channels
0/1/2 and leaves noise free. The generated catalog exposes it as `MUSIC_PEACEFUL`
and displays **Peaceful** in the music menu.

Press **B**, choose **Switch music** (or press Up twice from Status), and press **A**
to advance through the scores in filename order, wrapping at the end. The selected
title appears in the menu footer. Playback starts with `test.avms` when present,
otherwise the first score; the selection lasts for the current session.
Music continues through dialogue and menus.

For the synthesis-rate experiment, press **B**, **Up**, **A** to toggle between
**Audio: 8 kHz** and **Audio: 16 kHz** while the song keeps playing. Repeated
presses of A compare the two rates; B returns to exploration. The selection
persists when the menu closes and resets to 16 kHz on restart. It is not saved.
The exact rates are 7812.5 and 15625 Hz, both without smoothing.

The FamiTracker conversions preserve complete song traversals, all four arranged
voices, and the tracker's quantized pitch, volume, and pulse-duty changes.
Millisecond durations retain the 4 ms event timing; constant frequency slides
preserve fractional pitches used by bends and vibrato. Every song resets all
four channels on entry and silences them before looping. Good Endings uses
the cleaned `arduboy_grayscale_rpg/assets/song_peaceful.mid`, which already
omits the synth pad. It reduces the parts to three pulse voices and keeps noise
silent. The loop ends at 172 seconds, trimming the MIDI's empty 12.1-second
tail; the final 8 ms allow the last notes to release before looping.
See [conversion details](assets/music/imported/README.md) for voice selection,
the NES/DPCM adaptations, and build commands. CMake invokes the SDK's
`avm_music.py` compiler for each editable AVMS file, without rewriting
the scores. `tools/generate_music_catalog.py` emits `generated/music_ids.h` and
`generated/music_tracks.h`, used by `src/music_catalog.h` and `src/music.h`.
`MUSIC_MASTER_VOLUME` in `src/platform_avm.cpp` is 96 (300%). Tests compare the
AVMS playback against recorded tracker and MIDI-arrangement fingerprints and check complete duration,
two repeatable loops, channel resets, and headroom at reference master 64.
Physical Arduboy piezo listening and balance checks remain necessary.

The overworld is one 240x248-tile map with a village in its north-west corner, beside
a river with two bridges. Forest, cliffs, sea and rocks close the map border. Each of the
ten houses has its own interior: face a door and press A to enter, and walk out through
the exit at the bottom of the room to leave. **This development build rejects saves from
versions 1–3.** Its save format is version 4; new saves still use 24 bytes.

- `assets/maps/outdoor.tmx`: authoritative sample map, collision and native handler anchors.
- `assets/tilesets/world.tsx`: row-major tile IDs, symbols and solidity.
- `assets/images/tiles16.png`: opaque 256x256 atlas; IDs 0x00–0xFE are usable.
- `assets/maps/interiors.tmx`: the inside of every house, entered through the house doors.
- `assets/tilesets/house.tsx`, `cave.tsx` and `dungeon.tsx`, with `tiles16_house.png`,
  `tiles16_cave.png` and `tiles16_dungeon.png`: the interior tilesets. Each map uses one
  tileset, and the game draws each area with its own. See
  `assets/maps/AUTHORING_INTERIOR.md`.
- `assets/images/actors16.png`: 32 character rows (16 people, then 16 creatures and
  enemies), 12 frames per row, binary alpha.
- `assets/images/cursor22.png`, `battle_targets16.png` and `battle_arrows16.png`: the battle
  cursor, the destination markers, and the archer's arrow, one 16x16 frame per direction in
  north/east/south/west order.
- `src/logic/`: sign dialogue and sample NPC definitions, separate from generic mechanics.
- `src/battle.h` and `src/battle.cpp`: the portable battle rules, the Grove battlefield,
  encounter and unit types, and the one legal-action enumeration that the UI and the AI share.
  `src/battle_ai.*` holds the enemy's budgeted negamax search, its static evaluation, and the greedy
  choice it falls back on. `src/battle_screen.cpp` is the AVM screen behind `battle_show()`.
  `tests/test_battle.cpp` covers the rules and checks the search against an unpruned reference
  in `tests/battle_reference.h`. `tests/battle_bench.cpp` reports search costs on the host, and
  `tests/story_stub.*` records story calls on the host.
- `assets/music/*.avms`: all editable soundtrack scores.
  CMake uses `tools/compile_music.py` and the SDK's `avm_music.py` to rebuild
  each `generated/*_music.h` constexpr header and automatically updates the song list.
- `assets/music/imported/`: FamiTracker reference files, retained for provenance.
- `generated/`: deterministic program-memory assets and compile-time handler dispatch.
- `rpg.arduboy`: Arduboy FX package, refreshed by the default build.
- `rpg.fxc.arduboy`: Arduboy FX-C package, built from the same game with FX-C
  interpreter firmware (flash chip select on HWB/PE2), also refreshed by default.
- `build/artifacts/rpg_qa.arduboy` and `rpg_qa.fxc.arduboy`: the same game with
  device-QA frame markers. Device tests and QA scripts read these; the root packages
  carry no markers.

## Battle prototype

Reading the Nocturne Grove sign shows its welcome line, runs `battle_show()` on the
Grove encounter, and then shows an outcome line. The battle blocks in the same way as
dialogue, so story code can alternate dialogue and battles in ordinary sequence. Its
8x4 battlefield is a PROGMEM tile array in the world tileset. It is separate from the
overworld cache and from the current area.

- D-pad moves the cursor one cell per press. It is a 22x22 plus-mask sprite with black borders
  around a dashed outline. The dashes are two white pixels with two black pixels between them, and
  they turn clockwise around the outline. The cursor glides to each cell it moves to.
  A selects a hero, then A confirms a destination: an empty cell its movement reaches, or an enemy
  its attack reaches. Every legal destination on the board is marked, with a pulsing diamond for a
  move and corner brackets for an attack. A on another hero switches the selection to that hero, and
  B cancels a selection. The selected hero has a two-pixel white border. An invalid confirmation does
  nothing, and it does not use up the turn. After a hero acts, the cursor returns to that hero, so it
  can be selected again without moving the cursor.
- Each unit is one of six archetypes, with separate movement and attack patterns:

  | Archetype | Movement | Attack | Damage | Starting HP |
  |---|---|---|---|---|
  | Fighter | one cell straight | adjacent | 3 | 10 |
  | Rogue | knight leap over anything | adjacent | 3 | 10 |
  | Archer | one cell straight | straight line, 2 to 4 cells | 3 | 9 |
  | Spearman | one cell straight | straight line, 1 or 2 cells | 3 | 10 |
  | Wolf | one or two cells straight | adjacent | 3 | 9 |
  | Brute | one cell straight | adjacent, then knockback | 2 | 12 |

  Moves need empty, open cells all the way, except the rogue's leap, which needs only an empty, open
  landing cell. Attacks need every cell between to be empty and open, whoever stands there. A brute
  pushes a surviving victim one cell straight away when that cell is on the board, empty, and open.
  Attackers never move. A battle holds up to four units a side. The Grove pits the party's rogue and
  archer against a wolf and a brute.
- Battlefield tiles with collision in the world tileset block. No unit stands on them or moves
  through them, and they stop arrows, spear thrusts, and knockback. A rogue's leap passes over them,
  as it passes over units, but cannot land on them. The Grove has a pine at (4,0), a round tree at
  (6,0), rocks at (1,3), and a boulder at (4,3), which leave an open lane through rows 1 and 2.
- Each activation is one move or one attack. Every move costs its mover 1 HP, whatever its
  distance, but first deals 1 damage to every enemy beside its destination; an attack costs nothing
  and hits only its target. A unit with 1 HP may still move: it deals its damage and then falls,
  unless that damage defeats the last enemy, which wins the battle before the cost is paid. A side
  with no legal action passes, and its first living unit loses 1 HP. Every activation costs HP, so
  every battle ends.
- A straight move walks at the exploration walking speed and pose. A knight leap is a fast arcing
  jump: the rogue reaches its cell in eight frames, rising up to ten pixels above its path. An archer faces its target and looses an arrow, drawn in one of four direction
  sprites, which flies four pixels a frame from the archer to the target; the target flashes only
  once the arrow arrives. Any other attacker lunges four pixels toward its target, holds for about a
  quarter second, and returns. Once a move arrives, every unit that lost HP flashes: for about a
  second when an enemy was hit, and briefly when only the mover paid. A knocked-back unit flashes at
  its new cell. The next turn waits for the flashing to end, and a defeated unit flashes before it
  disappears.
- Each side moves in turn. The enemy deepens a negamax search with alpha-beta pruning one activation at
  a time, up to four, within a budget of 480 positions (fewer with more than four living units), and
  uses the deepest completed search. It assumes the heroes reply with their best action. It scores a
  position by the damage it deals and the heroes it removes, which count for more than its own losses,
  plus terms for the attacks each side can make and for how far each enemy still is from an attack
  position. Moves that damage or defeat enemies are searched alongside attacks. The search runs
  synchronously and takes up to two seconds on the AVM: 0.3 to 1.9 seconds in the measured positions,
  including three and four units a side (`docs/performance.md`). While it runs, a small box at the
  center of the screen shows three dots that fill in about five times a second; each update redraws
  only that box and pushes the display without clearing it.
- A press of A confirms the VICTORY or DEFEAT overlay. The battle does not change HP,
  position, flags, or any other overworld state.

The battle is the only feature in this prototype. Other encounters and battlefields are not
implemented.

The bottom-right atlas cell (0xFF) is reserved for the impassable outside-map
sentinel. Black cells retain their indices. Characters use direction order
north/east/south/west, rest on frame 1, and walk through 1,2,1,3.
Map tiles are stored in contiguous rows without padding. Handler chunks cover
8x4 tiles; a 9x5 cache supports smooth scrolling and uses 108 fewer
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
it also checks FXC. CSV and JSON
distributions are written to `build/audio-timing`; matplotlib adds PNG/SVG plots.
Audio timing is a manual measurement tool and is excluded from CTest. See
[the timing investigation](docs/audio_timing.md) for measured results and definitions.

See [Tiled/asset pipeline](docs/tiled_pipeline.md), [world architecture](docs/chunk_world.md),
[NPC mechanics](docs/essential_npcs.md) and [current measurements](docs/tiles16_measurements.md).
The M0 proof is an independent historical API diagnostic and retains its tiny test art.
