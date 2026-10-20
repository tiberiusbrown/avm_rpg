# Imported rotation tracks

These are the unmodified Boss and Venus FamiTracker reference files supplied
from Downloads. Their four-voice arrangements are stored in the editable
`../boss.avms` and `../venus.avms` scores. CMake invokes the SDK's `avm_music.py`
compiler directly to generate `boss_music.h` and `venus_music.h` for the game.
Rebuilding needs only Python's standard library and the SDK's `avm_music.py`.
Source timbre hints map to pulse duty: 50% for square/triangle arrangements,
25% for sine/saw arrangements. The noise voice receives volume and pitch only.

| Source | Menu title | Complete loop |
|---|---|---:|
| `Boss.ftm` | Boss | 34.000 s |
| `Venus.ftm` | Venus | 25.600 s |

Source of these files: https://opengameart.org/content/nes-shooter-music-5-tracks-3-jingles
Author is SketchyLogic.
License is CC0.

The AVMS scores retain each arranged pitch, gain, and duty change at its
original quantized 4 ms timestamp. Millisecond note/rest durations preserve
timing; constant frequency slides preserve fractional pitches for tracker
bends and vibrato. Splitting a note at a control change preserves oscillator
phase. All three tonal voices remain pulse waves.

Both FamiTracker modules use NTSC APU + VRC6. Boss prioritizes APU pulse 1 for
melody, VRC6 pulse 1 for accompaniment, and VRC6 saw for bass. Venus prioritizes
VRC6 pulse 1 for melody, APU pulse 1 for harmony, and VRC6 saw for bass. Each slot
can fall back to its partner pulse or triangle channel when silent. Volume and
arpeggio sequences, pitch sequences, row volume changes, and approximate
vibrato/tremolo/note slides are evaluated at 60 Hz. Fixed-duty AVM square waves
replace NES pulse-width sequences; sampled DPCM drums become noise transients.
These are four-voice arrangements, not exact NES/VRC6 reproductions.

Boss's D00 skips the last 32 introductory rows; B01 ends the first traversal
after all nine order entries. AVM restarts streams from their beginning, so the
introduction returns on subsequent loops. Venus plays all four order entries.
Both release all voices before restarting and restore every voice on entry.
The reference modules' file layout is documented in
the [Dn-FamiTracker format reference](https://github.com/Dn-Programming-Core-Management/Dn-FamiTracker/wiki/Dn-FT-module-format).

The imported voice gains total at most 61, keeping the mix below 127 at the
reference master volume of 64. The game's existing master boost is unchanged.

From the repository root, compile and validate with:

```powershell
cmake --build projects/avm_rpg/build --config RelWithDebInfo --target rpg_music
ctest --test-dir projects/avm_rpg/build -C RelWithDebInfo -R imported_music --output-on-failure
```

The build does not overwrite the AVMS sources. The test compares the compiled
playback with fingerprints captured from the tracker arrangements, including
the exact timing of every audible pitch, gain, and duty change.
