# AVM Wayfarer — Nocturne Grove

C++20 exploration on the 128x64 Arduboy AVM display, with 16x16 terrain and
alpha-masked characters. D-pad moves, A reads the sign immediately north of the
starting position and then starts a test battle, enters a house when the player faces
its door, and B opens the pause menu. Two sample villagers show
standing and wandering characters using different rows of the actor atlas.

Teleports, including entering and leaving buildings, use the same 320 ms dither fade through
black as battles. The destination's camera, tile cache and actors are prepared while the screen
is black. The shared fade adds no static RAM.

The pause menu slides in from the left, easing halfway to its target each frame while the
world fades to 50% black. Up/down selects **Resume**, **Save**, **Party**, **Journal**,
**Map**, **Audio**, or **Load**; A activates the selection, and B resumes with the
reverse animation. The compact black panel has a white border and black outer edge.
Map appears only in the overworld, and Load appears only when the save flag is set, including
immediately after saving. Party opens the traveler status screen. Journal and Map are placeholders. The world stays frozen while the menu is open; music continues.
Save writes the existing single save slot and immediately sets the save flag, without readback. Selecting Load eases the panel out
while fading the world completely to black, restores the saved world while hidden, then
fades it completely back in before play resumes.

Selecting **Audio** eases the pause panel out to the left, then immediately eases the
Audio panel in from the right. The world remains frozen at 50% fade throughout the
handoff. Up/down selects **Music**, **SFX**, or **Low-pass**; left/right adjusts the
slider, with repeat while held. Volume sliders run from Off to 200% in 31 positions, with a default of 100%.
The low-pass slider runs from bypass to maximum smoothing; the native filter also
smooths tonal SFX. B eases Audio out and returns directly to gameplay.

Audio settings take effect immediately and are stored alongside progress when **Save** is
selected. Startup initializes defaults (both stream gains 128, shown as 100%; filter off), then loads valid
saved settings. New Game, Continue and later Load actions preserve the current settings;
only the Audio menu changes them during play.

The sign's battle is a synchronous prototype. Its controls and rules are described in
[Battle prototype](#battle-prototype) below.

The build automatically discovers every `assets/music/*.avms` score and adds it
to the soundtrack. It uses `avm_music.py` through `tools/compile_music.py` to emit
C++ constexpr assets and generates the song IDs and playback list. Add or remove
a score and build normally; no CMake or C++ song-list edits are needed.
Track titles come from each score's `title` setting, with the filename as a fallback.
The FamiTracker reference files remain in `assets/music/imported/`.

| Song | Character | Key / meter | BPM | Loop |
|---|---|---|---:|---:|
| Glade and Hearth | Original woodland-village theme: breathy pulse flute, lute and rolled chords, saw bass, two bird calls | G major / 6/8 | 96 | 60.000 s |
| Steel at Dawn | Original fantasy battle theme | D minor / 4/4 | 152 | About 12.6 s |

[`peaceful.avms`](assets/music/peaceful.avms) is a 32-bar A-A'-B-A'' theme in 6/8,
two dotted-quarter beats per bar. A breathy flute states a four-note hook over a
lilting lute that plays two chord tones after each beat, with a saw bass walking
root and fifth stepwise (G D E B C G D A). The first A closes on the dominant and
the answer on a held G, with a bird calling above it. A' lifts the hook a fifth and climbs
to G6. B is a shaded glade: sighing long notes over rolled chords in three quarter-note
beats against the bass's two. A'' brings the hook back, sings the climax as two long
notes, and rises to D6 so the loop re-enters on the hook. Held notes use a slight
vibrato. The bird calls are short rising frequency slides on the harmony voice. The
score uses 16 ms automation, channels 0/1/2, and leaves noise free; channels 0 and 2
carry the tune if an effect borrows channel 1. The score needs no manual detuning: the
compiler tunes every note five cents flat and corrects each by at most six cents more (see
[`docs/audio.md`](../../docs/audio.md#note-tuning)), which keeps the tonic G5, G6 and C6 off the
pitches where a plain pulse beats at the 15.6 kHz synthesis rate. The lead never rests for a full beat,
because the synthesis interrupt costs more per frame with each sounding voice and the
pause animation's frame-timing check tolerates only small steps in that load. The generated catalog exposes it as `MUSIC_PEACEFUL` with the title
**Glade and Hearth**.

Battles have sound effects. Each `assets/sfx/*.avms` score is a one-shot effect, and the
build compiles them all with `tools/compile_sfx.py` into `generated/sfx_ids.h` and
`generated/sfx_data.h`; add a score and it becomes `SFX_<FILE>`, played with
`sfx_play` from `src/sfx.h`. Only one effect plays at a time, and it borrows the
music's harmony voice, its noise voice, or both, while the melody and bass continue.
Noise carries the impacts.

| Effect | Heard when |
|---|---|
| `battle_start`, `victory`, `defeat` | A battle opens, and its result appears |
| `cursor_move`, `cursor_edge` | The cursor moves a cell, or is pushed against the board's edge |
| `select`, `cancel`, `error` | A hero is selected, the selection is cancelled, or A changes nothing |
| `step`, `leap` | Each footfall of a walking unit, and a rogue's jump and landing |
| `strike`, `thrust`, `bite`, `smash` | A fighter's or rogue's, spearman's, wolf's, or brute's attack lands |
| `bow`, `arrow_hit` | An archer shoots and the arrow flies, then lands |
| `clash`, `price` | A move engages adjacent enemies, or costs only its blood price |
| `fall` | A unit is defeated, after the blow that defeated it |
| `enemy_turn` | The enemy's turn starts |

Playback starts with Glade and Hearth. Battles switch to Steel at Dawn and return to
Glade and Hearth when A dismisses the result screen. Music continues through dialogue and menus.

`music_play(MUSIC_TRACK)` queues a different song with `avm_audio_play_fade`.
The runtime starts it immediately when no song is active.
The interpreter lowers music attenuation by one every 4 ms, then restores the
gain captured by the initial fade request and starts the queued song. Further
requests replace only the queued song, preserving gain and fade progress. At the default
music gain of 128 the fade lasts 512 ms; the 200% slider setting uses gain 255 and fades
for 1.02 seconds. Other gains fade at the same slope.
Input, display animation and gameplay continue during the fade. Requesting the
current song leaves it playing. The current-song getter avoids extra static RAM
for selection tracking. Master gain and sound effects are unaffected.

Playback uses the 16 kHz synthesis setting (15625 Hz, without smoothing).

CMake invokes the SDK's
`avm_music.py` compiler for each editable AVMS file, without rewriting
the scores. `tools/generate_music_catalog.py` emits `generated/music_ids.h` and
`generated/music_tracks.h`, used by `src/music_catalog.h` and `src/music.h`.
`MUSIC_MASTER_VOLUME` in `src/platform_avm.cpp` is the shared gain, 255 (796.875%), the native maximum.
Individual music and SFX gains default to 128, approximately half their former native
values. Sliders map 0–200% to native 0–255: 100% is 128 and 200% is 255. Stored slider
percentages are unchanged by this expanded range.
Physical Arduboy piezo listening and balance checks remain necessary.

## Environment animation

The world is never still. Water ripples and glitters, flowers sway, chimneys smoke, torches,
campfires and the lights in the houses flicker, a flag waves, and gusts of wind roll through
the grass. Butterflies, bees, rabbits, falling leaves, jumping fish, perching birds, moths and
drifting dust appear on the tiles that suit them, and flocks of birds cross the sky.
Underground, torches and braziers throw sparks, pools ripple, crystals glint, water drips from
the roof, bats wheel through the caves and pale wisps drift over the dungeon's flagstones.

All of it is a pure function of the game tick and a world position (`src/ambient.h`), so it
adds no static RAM, nothing to the save, and stays frozen with the world behind menus and
dialogue. Animated tiles cost nothing per tile: each of the four phases has its own sprite
sheet. The tile and map rules are in `assets/maps/AUTHORING.md`.

The overworld is one 240x248-tile map with a village in its north-west corner, beside
a river with two bridges. Forest, cliffs, sea and rocks close the map border. Each of the
ten houses has its own interior: face a door and press A to enter, and walk out through
the exit at the bottom of the room to leave.

Two more places lie under the cliff north of the village. A cave mouth north of the green
leads into Hollow Mine, and a mine door at the end of the path from the north street leads
into the Old Vault. Walk into either to enter. Both are built around mechanisms: levers,
gates that draw down into the floor, pressure plates, spikes, sinking blocks, a bridge over a
chasm and a steam vent that blows by the clock. The mine's far ladder climbs out to a hole in
the west meadow. `assets/maps/AUTHORING_CAVE.md` and `AUTHORING_DUNGEON.md` describe both.

**This development build rejects saves from versions 1–7.** Save version 8 writes the live,
pointer-free `Game` struct directly from AVM's `.saved` prefix (268 bytes), with no separate
save buffer or checksum. Startup loads and validates that state to initialize the global
`save_exists` flag, then prepares a new game for the title screen. New Game preserves the
flag; Continue and Load restore the saved world. NPC casts, the tile cache and the animation
tick are rebuilt when a save is loaded. Total shipping static RAM is 479 bytes,
including the save flag and three saved audio options.

- `assets/maps/outdoor.tmx`: authoritative sample map, collision and native handler anchors.
- `assets/tilesets/world.tsx`: row-major tile IDs, symbols and solidity.
- `assets/images/tiles16.png`: opaque 256x256 atlas; IDs 0x00–0xFE are usable.
- `assets/images/tiles16_phase1.png` to `_phase3.png`, `tiles16_wind.png` and the matching
  `tiles16_house_phase*.png`: the animation frames of the tiles. `assets/images/ambient16.png`:
  the ambient creatures. See [Environment animation](#environment-animation).
- `assets/maps/interiors.tmx`: the inside of every house, entered through the house doors.
- `assets/maps/cave.tmx` and `dungeon.tmx`: Hollow Mine and the Old Vault.
- `assets/tilesets/house.tsx`, `cave.tsx` and `dungeon.tsx`, with `tiles16_house.png`,
  `tiles16_cave.png` and `tiles16_dungeon.png`: the interior tilesets. Each map uses one
  tileset, and the game draws each area with its own. See
  `assets/maps/AUTHORING_INTERIOR.md`, `AUTHORING_CAVE.md` and `AUTHORING_DUNGEON.md`.
- `assets/images/actors16.png`: 32 character rows (16 people, then 16 creatures and
  enemies), 12 frames per row, binary alpha.
- `assets/images/cursor22.png`, `battle_targets16.png` and `battle_arrows16.png`: the battle
  cursor, the destination markers, and the archer's arrow, one 16x16 frame per direction in
  north/east/south/west order.
- `assets/images/battle_damage16.png`: the damage indicators, nine 16x16 frames for losses of
  1 to 9 HP. Each is the same spiky burst with its own 3x5 digit in the black middle.
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
- `assets/sfx/*.avms`: the battle sound effects, one score per effect.
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

- Entering and leaving battle fades through black with a fixed 4x4 ordered dither. Each half
  takes eight frames at 50 Hz (160 ms), with the world and battle state paused. The cursor appears
  after the board is revealed. Leaving begins only when A dismisses the victory or defeat screen.
  Erase-mask sprites stay in flash; fading out reuses the framebuffer, and fading in redraws the
  incoming scene without a second framebuffer or additional static RAM.
- D-pad moves the cursor one cell per press. It is a 22x22 plus-mask sprite with black borders
  around a dashed outline. The dashes are two white pixels with two black pixels between them, and
  they turn clockwise around the outline. The cursor glides to each cell it moves to.
  A selects a hero, then A confirms a destination: an empty cell its movement reaches, or an enemy
  its attack reaches. Every legal destination on the board is marked, with a pulsing diamond for a
  move and corner brackets for an attack. A on another hero switches the selection to that hero, and
  A on the selected hero or B cancels a selection. The selected hero has a two-pixel white border.
  An invalid confirmation does nothing, and it does not use up the turn. After a hero acts, the
  cursor returns to that hero, so it can be selected again without moving the cursor.
- Hovering the cursor over a living actor shows its class and current HP in a small panel on
  the opposite side of the screen. The panel disappears over empty cells.
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
- During a player turn, B opens the shared pause menu when no unit is selected; otherwise it
  cancels the selected unit. The battle menu contains **Resume**, **Resign**, **Audio**, and
  **Pass**. Pass and Resign open centered confirmation panels: A accepts after a 240 ms guard
  and a fresh press; B cancels immediately. Confirmed Resign ends the battle as a defeat,
  with the normal result screen and defeat outcome for the caller.
  Exploration items, including Load, are hidden in battle.
  Resume or B returns to the same battle, and Audio uses the same sliders and transition as
  exploration, with the frozen board behind it. Confirmed Pass voluntarily consumes the party's
  turn. The AI considers Pass for either side at every search depth, alongside moves and attacks.
- Each activation is one move, one attack, or a pass. Every move costs its mover 1 HP, whatever its
  distance, but first deals 1 damage to every enemy beside its destination; an attack costs nothing
  and hits only its target. A unit with 1 HP may still move: it deals its damage and then falls,
  unless that damage defeats the last enemy, which wins the battle before the cost is paid. A side
  with no legal action passes automatically. Every pass deals 1 damage to every living member
  of the passing team, for both the player and AI, and can defeat the whole team. Every activation costs HP, so
  every battle ends.
- A straight move walks at the exploration walking speed and pose. A knight leap is a fast arcing
  jump: the rogue reaches its cell in eight frames, rising up to ten pixels above its path. An archer faces its target and looses an arrow, drawn in one of four direction
  sprites, which flies four pixels a frame from the archer to the target; the target flashes only
  once the arrow arrives. Any other attacker lunges four pixels toward its target, holds for about a
  quarter second, and returns. Once a move arrives, every unit that lost HP flashes: for about a
  second when an enemy was hit, and briefly when only the mover paid. A knocked-back unit flashes at
  its new cell. While a unit flashes, a spiky burst over its cell shows the HP it lost. The next
  turn waits for the flashing to end, and a defeated unit flashes before it disappears. After the
  enemy's action the cursor returns on the next frame.
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

The installed SDK must include `bin/avm/interp.fxc.hex`, independent music/SFX
attenuation, and the `avm_audio_get_music` / volume getters. Update an older SDK
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
