# Venlor

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
immediately after saving. Party opens an eased sliding roster with four slots, showing each member's HP/MHP, movement class (M) and attack class (A). A or B slides the roster out and returns directly to gameplay.
A new game starts with the traveler (20/20 HP, fighter movement and attack). Rowan waits southeast
of the starting point; talking to him recruits him once (18/18 HP, fighter movement, archer attack).
Rowan disappears from the map upon joining and stays absent on revisits and after loading.
Party membership and statistics are saved. The version 10 save layout rejects older saves. Journal is a placeholder. The world stays frozen while the menu is open; music continues.

Map opens a scrollable monochrome exterior overview at one pixel per tile. Roads and
building silhouettes are solid white, woodland is dotted, and water uses a checkerboard
pattern. The cursor starts at the player; a separate blinking square marks the player
after the cursor moves away. The crosshair gently expands and contracts, with an opaque
black outline around its arms and an open center. D-pad taps adjust its position precisely;
holding accelerates over sixteen frames independently to one pixel per frame on each axis,
so diagonal movement is faster. Scrolling starts within twelve pixels of a screen edge.
B returns directly to gameplay. After the pause panel slides out, a 240 ms dither
through black reveals the map. Exiting uses a 320 ms dither through black to restore
the fully visible world. The map cursor and player marker appear when the entry
reveal finishes. Gameplay stays frozen throughout each transition and music continues.
The map reveals the whole exterior; A is reserved for future marker and travel actions.
Save writes the existing single save slot and immediately sets the save flag, without readback. Selecting Load eases the panel out
while fading the world completely to black, restores the saved world while hidden, then
fades it completely back in before play resumes.

Selecting **Audio** eases the pause panel out to the left, then immediately eases the
Audio panel in from the right. The world remains frozen at 50% fade throughout the
handoff. Up/down selects **Music** or **SFX**; left/right adjusts the
slider, with repeat while held. Volume sliders run from Off to 200% in 31 positions, with a default of 100%.
B eases Audio out and returns directly to gameplay.

Audio settings take effect immediately and are stored alongside progress when **Save** is
selected. Startup initializes defaults (both stream gains 128, shown as 100%), then loads valid
saved settings. Starting or continuing from the title and later Load actions preserve the current settings;
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
| Morning in the Meadow | Original pastoral theme: breathy pulse flute, rocking harp, slow saw roots, no drums; quotes the Venlor theme in two halves | G major / 4/4 | 76 | 75.789 s |
| Glade and Hearth | Original woodland-village theme: breathy pulse flute, lute and rolled chords, saw bass, two bird calls (`peaceful2.avms`) | G major / 6/8 | 96 | 60.000 s |
| Peaceful Village | Original NES-inspired town theme over a stepwise descending bass (`peaceful3.avms`) | D major / 4/4 | 96 | 60.000 s |
| Steel at Dawn | Original fantasy battle theme | D minor / 4/4 | 152 | About 12.6 s |
| Call of Venlor | Epic main title: a galloping march in which the singing Venlor theme returns between new tunes | D major / 4/4 | 160 | 63.000 s |
| Stone and Shadow | Dark, foreboding cave march: swelling drone, tolling bell, distant drums and a slow minor tune with a glimmer in the middle (`cave.avms`) | E minor / 4/4 | 68 | 91.765 s |
| Where the Light Ends | Super creepy special-purpose track, kept for a later scene and played by no area yet (`creepy.avms`) | D Phrygian / 4/4 | 56 | 85.714 s |

[`peaceful.avms`](assets/music/peaceful.avms) is *Morning in the Meadow*, the song of the outdoors, the interiors and
the dungeon: 24 bars at 76 BPM in G major, in one quiet style. A breathy flute sings its own eight-bar tune
over G Em C D | G Em C D, with a rocking harp (one chord a bar, in eighth notes) and slow saw roots, and no
drums. The [Venlor theme](docs/venlor_theme.md) is only quoted, in G, and its two halves are far apart. The question
(`D4 | G4 A4 | D5 C5 A4`) opens the second section over G D and is left unanswered by the song's own music,
and the answer (`C5 D5 G5 A5 | G5 D5 G4`) is held back until bars 21-22, where it is the song's final cadence
over C G; its last G swells, vibrates and rings into the coda. The rest is the song's own music: the tune,
a rising contrast over Em Am D | Am D | Em D, and the tune's first half again. Held flute notes swell with a
12 cent vibrato. The score uses 16 ms automation, channels 0/1/2, and leaves noise free; channels 0 and 2
carry the song if an effect borrows channel 1. The score needs no manual detuning: the
compiler tunes every note five cents flat and corrects each by at most six cents more (see
[`docs/audio.md`](../../docs/audio.md#note-tuning)). The lead and the harp never rest for a full beat,
because the synthesis interrupt costs more per frame with each sounding voice and the
pause animation's frame-timing check tolerates only small steps in that load. The generated catalog exposes it as
`MUSIC_PEACEFUL` with the title **Morning in the Meadow**.

[`title.avms`](assets/music/title.avms) is *Call of Venlor*, the main title: 42 bars at 160 BPM (63 seconds) in D major,
in one epic style from start to finish: a bright pulse horn, offbeat chord stabs, a galloping saw bass
and a drum kit. It plays the [Venlor theme](docs/venlor_theme.md), a four-bar, thirteen-note lyrical melody
meant to return throughout the soundtrack, three times, each time the same over D A G D, between three
pieces of new music. After a heartbeat of toms and a swelling roll the theme enters on a crash, its long notes
swelling with 16 to 22 cent vibrato, and is followed by a rising fanfare riff. A flowing second tune in
eighth notes follows, then a bridge of long falling and rising arpeggios that climbs to a held E6 under a
growing roll. The theme returns, then the riff, then the theme a last time, whose final D rings into a
fading heartbeat that leads back to the start. The noise voice plays kicks, snares, three toms, crashes and
snare rolls made with `noise_retrigger`. The theme takes 12 of the 42 bars. The score compiles to about 23 KB
and sends at most five events in any 20 ms frame. With the song playing, the animated title still holds
its 50 Hz pace (600 frames in 12 seconds, longest interval 332,136 cycles). The generated catalog exposes it as
`MUSIC_TITLE` with the title **Call of Venlor**.

[`cave.avms`](assets/music/cave.avms) is *Stone and Shadow*, the song of the Hollow Mine: 26 bars at 68 BPM
(91.8 seconds) in E minor. It is dark and foreboding but a real song, with a singable tune, honest chords and a
steady bass, and none of the spoiled consonances of the creepy track. After a two-bar intro (a drone on E swells,
a distant bell tolls E and B, and one drum answers) the tune enters on a 50% pulse "horn": two four-bar phrases
over Em C | Am B and Em C | D Em. The first climbs to F# and leaves D# hanging over the dominant; the second
answers it and walks down to a low E. A quiet turning chord outlines each bar on channel 1, the bass tolls roots
and fifths, and a deep drum marks beats one and three. The middle section is a glimmer in the cavern over
C G | Am Em | C G | B B: the lead climbs to B5 in a ringing voice, a plucked arpeggio drips beneath it, the
drums and bass begin to march, and the last two bars lean on the dominant under a growing roll. The tune then
returns in a brighter horn over a marching bass in quarter notes, and its last E fades into the drone that
opens the loop. Lead and bass carry the song if an effect borrows channel 1 or 3. The score compiles to about
25 KB, sends at most four events in any 20 ms frame, and renders as loudly as the title song without clipping.
It does not quote the Venlor theme. The generated catalog exposes it as `MUSIC_CAVE` with the title
**Stone and Shadow**.

[`creepy.avms`](assets/music/creepy.avms) is *Where the Light Ends*, the cave's earlier song, now a super creepy
special-purpose track held back for a later scene (20 bars at 56 BPM, D Phrygian and chromatic: a breathing
drone, drips a semitone or a tritone off the drone, a limping music box, a heartbeat and a scream). No area plays
it, and `tests/test_world.cpp` asserts so. Play it with `music_play(MUSIC_CREEPY)`.

Sound effects cover battles, menus, mechanisms, doors and the places themselves. Each
`assets/sfx/*.avms` score is a one-shot effect, and the build compiles them all with
`tools/compile_sfx.py` into `generated/sfx_ids.h` and `generated/sfx_data.h`; add a score and it
becomes `SFX_<FILE>`, played with `sfx_play` from `src/sfx.h`. Only one effect plays at a time, and it
borrows the music's harmony voice, its noise voice, or both, while the melody and bass continue.
Noise carries the impacts. Nothing plays while the SFX option is zero, because a muted effect would
still take the music's voices.

`sfx_play(effect, then)` can chain two effects: `then` starts as soon as `effect` ends, unless
something else starts first. A lever's click is followed by the grind of the gate it opens that way.
The platform starts the second from `input_wait_frame`, which every loop that shows frames waits in,
so the chain needs one byte of static RAM and works in menus, dialogue and fades alike.
`sfx_ambient` plays an effect only when none is playing or waiting, so the sounds of the
surroundings never cut off a sound that answers the player.

| Effect | Heard when |
|---|---|
| `battle_start`, `victory`, `defeat` | A battle opens, and its result appears |
| `cursor_move`, `cursor_edge` | The cursor moves a cell, or is pushed against the board's edge |
| `select`, `cancel`, `error` | A hero is selected, the selection is cancelled, or A changes nothing. Menus use `select` for A and `cancel` for B |
| `step`, `leap` | Each footfall of a walking unit, and a rogue's jump and landing |
| `strike`, `thrust`, `bite`, `smash` | A fighter's or rogue's, spearman's, wolf's, or brute's attack lands |
| `bow`, `arrow_hit` | An archer shoots and the arrow flies, then lands |
| `clash`, `price` | A move engages adjacent enemies, or costs only its blood price |
| `fall` | A unit is defeated, after the blow that defeated it |
| `enemy_turn` | The enemy's turn starts |
| `menu_tick_a` to `menu_tick_e` | The cursor moves through a menu, one of the five picked at random |
| `lever_on`, `lever_off` | A lever is thrown on, and back off |
| `plate_down` | A pressure plate sinks under the player |
| `door_unlock`, `orb_light`, `chest_open` | A barred door is forced, the orb wakes, a chest opens |
| `grind_open`, `grind_close` | A gate, a vault archway or a block of stone slides open or shut |
| `scrape_open`, `scrape_close` | Spikes sink into the floor, or thrust up out of it |
| `bridge_extend`, `bridge_retract` | A bridge runs out over a chasm, or folds away |
| `door_open`, `door_close` | The player enters a house, and leaves it |
| `bird_chirp`, `bird_coo`, `bird_trill`, `bird_tweet` | Outdoors, a bird sings from the trees |
| `bee_buzz`, `water_plip`, `water_splash`, `wind` | A bee works the flowers, a drop or a fish stirs the water, and a gust of wind reaches the player |
| `creak`, `fire_crackle` | A house floor creaks, and a hearth, stove, campfire or torch crackles |
| `drip_a`, `drip_b`, `bat_squeak`, `crystal_chime`, `steam_hiss`, `rumble` | In the cave, drops fall, bats wheel, crystals ring, a vent starts to blow and stone settles |
| `wisp_moan`, `chain_rattle` | In the dungeon, a wisp moans and a chain stirs in the dark |

The pause menu ticks as it opens and with every move through its list, with one of five small
sounds chosen at random, so a list never sounds mechanical. A plays `select` and B plays `cancel`.
The Audio screen ticks as a slider moves, at the new SFX level, and a slider at its end bumps once
per press. Every menu picks its tick from the millisecond clock at the press, which keeps no
state and leaves the game's random numbers alone.

**Mechanisms** have a kind (`MECHANISM_LEVER`, `MECHANISM_PLATE`, `MECHANISM_DOOR`, `MECHANISM_ORB`,
`MECHANISM_CHEST`, and `MECHANISM_GATE`, `MECHANISM_BLOCKS`, `MECHANISM_SPIKES`, `MECHANISM_BRIDGE` and
`MECHANISM_RUNE` for the cells that only follow a flag). The kind in a table row of
`src/logic/mechanisms.h` says how the player works the cell, whether a second operation undoes the
first, and what each change sounds like. Every operation goes through one function, which sets or
clears the flag and plays the cell's own sound and then the sound of what its flag moves, so the cave's
lever clicks and its gate grinds, and the dungeon's plate sinks and the east gate grinds. A flag that a
load or a test sets changes the cells without a sound, because only the player's operation is heard.

**Environment sounds** come from `src/soundscape.h`, which finds them the way `src/ambient.h` finds
animation: from the game tick, the area and a tile, with no state, nothing in the save, and nothing
heard while the tick is frozen. Each 256-tick epoch (5.12 seconds) holds at most one sound, and it
is there about two times in three. It looks at up to three tiles within five columns and three rows
of the player, and the ambient class of the first tile that has a voice decides what is heard: a bird
from the trees, perches and grass, a bee over flowers, a fish or a drop at the water, a creak on a house
floor, the crackle of a stove or a torch, a drip, a bat or a ringing crystal in the cave, and a moan,
a rattling chain or settling stone in the crypt. Outdoors a gust of wind is heard on the tick when
its line reaches the player, as the grass bends there, and a cave vent hisses on the tick that it
starts to blow, which warns that it is about to bar the way. Standing still, the player hears about
eight sounds a minute outdoors (a third of them wind), five in the cave, three in the dungeon and
two in a house. Ambient effects are quieter than the ones that answer the player.

Playback starts with Call of Venlor on the title screen. Starting or continuing a game fades it into the
area's song: Morning in the Meadow outdoors, in the interiors and in the dungeon, and Stone and Shadow in the
cave. Battles switch to Steel at Dawn and return to
Morning in the Meadow when A dismisses the result screen. Music continues through dialogue and menus.

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
sheet. The tile and map rules are in `assets/maps/AUTHORING.md`. The same tiles decide what the
player hears: see the environment sounds under the music section above.

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

**This development build rejects saves from versions 1–8.** Save version 9 writes the live,
pointer-free `Game` struct directly from AVM's `.saved` prefix (267 bytes), with no separate
save buffer or checksum. Startup loads and validates that state to initialize the global
`save_exists` flag, then prepares a new game for the title screen. The title shows an animated
scene under the game's name and no menu, with Call of Venlor playing: A or B continues the saved game when the flag is set
and starts a new game otherwise. A new game preserves the flag; continuing and Load restore the
saved world. NPC casts, the tile cache and the animation
tick are rebuilt when a save is loaded. Total shipping static RAM is 478 bytes,
including the save flag and two saved audio options.

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
- `src/logic/`: the handlers that the maps name, split by environment, and the sample NPC
  definitions, separate from generic mechanics. `handlers.h` only includes the others.
  `mechanisms.h` holds what the environments share: flag-driven cells (levers, plates, doors,
  chests), each with a mechanism kind that decides how it is worked and how it sounds, messages
  that only show words, and passages between areas. `area_village.h` has the
  Grove sign and the houses, `area_cave.h` and `area_dungeon.h` have the mechanism, message and
  passage tables of their own areas and their own handlers.
- `src/battle.h` and `src/battle.cpp`: the portable battle rules, the Grove battlefield,
  encounter and unit types, and the one legal-action enumeration that the UI and the AI share.
  `src/battle_ai.*` holds the enemy's budgeted negamax search, its static evaluation, and the greedy
  choice it falls back on. `src/battle_screen.cpp` is the AVM screen behind `battle_show()`.
  `tests/test_battle.cpp` covers the rules and checks the search against an unpruned reference
  in `tests/battle_reference.h`. `tests/battle_bench.cpp` reports search costs on the host, and
  `tests/story_stub.*` records story calls and sound requests on the host.
- `assets/music/*.avms`: all editable soundtrack scores.
  CMake uses `tools/compile_music.py` and the SDK's `avm_music.py` to rebuild
  each `generated/*_music.h` constexpr header and automatically updates the song list.
- `assets/sfx/*.avms`: the sound effects of battles, menus, mechanisms, doors and the environment,
  one score per effect. `src/soundscape.h` and `src/soundscape.cpp` choose the environment sounds.
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
- Hovering the cursor over a living actor shows its movement class, attack class and HP/MHP in a small panel on
  the opposite side of the screen. The panel disappears over empty cells.
- Party members store uint8 HP/MHP and independent movement/attack class IDs in saved RAM.
  An enemy stores only uint8 HP and an enemy ID in its live battle record; its MHP, movement,
  attack and artwork are read from `EnemyInfo` in program memory. Board placement is separate
  metadata. Both sides use the same movement and attack rules. The six enemy presets are:

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
  Attackers never move. A battle holds up to four units a side. The Grove deploys the living
  members of the current saved party against a wolf and a brute. Recruitment therefore changes
  the next battle's roster. HP supports the complete 0..255 range; zero-HP members do not deploy.
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
- At the result overlay, final ally HP is copied back into the saved party, including move/pass
  costs and defeated members at zero HP. MHP, classes and membership are preserved. Resigning
  preserves the party's remaining battle HP. A press of A confirms VICTORY or DEFEAT. Battles
  provide no automatic healing; entering with no living party members immediately results in defeat.

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
cmake --build build --config RelWithDebInfo --target rpg_integration_full
cmake --build build --config RelWithDebInfo --target rpg_audio_timing
python tools/collect_evidence.py --build build --write-docs
```

`rpg_integration` runs focused guest checks, short native profiles, and normal/FXC
device smoke checks (boot, input, drawing, pause, save and restart) and sound checks
(menu ticks, a gust heard as wind, the muted option, and a lever's chained effects, in the
device mode `sounds`). With current artifacts it targets a 10–20 second run. `rpg_integration_full` retains the exhaustive
guest collision matrix, full device scenarios and 8-second profiles; run it before
collecting performance evidence. Both targets print per-stage wall times and retain
logs/captures in a fresh `build/verification-*` directory. Compilation is additional.

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
[NPC mechanics](docs/essential_npcs.md), [the Venlor theme](docs/venlor_theme.md), the game's recurring musical theme, and [current measurements](docs/tiles16_measurements.md).
The M0 proof is an independent historical API diagnostic and retains its tiny test art.
