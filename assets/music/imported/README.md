# Imported rotation tracks

This folder contains the unmodified Boss and Venus FamiTracker reference files supplied
from Downloads. Their four-voice arrangements are stored in the editable
`../boss.avms` and `../venus.avms` scores. CMake invokes the SDK's `avm_music.py`
compiler through `tools/compile_music.py` to generate constexpr headers for the game.
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

## Good Endings MIDI arrangement

`../good_endings.avms` was converted from the cleaned
`arduboy_grayscale_rpg/assets/song_peaceful.mid`
(SHA-256 `88a29f0eaf4ac191406aad1175e25eb02f29315c529e9a3aa6d7294038233e25`).
The original MIDI remains outside the repository. It already omits the held
synth pad; all six note-bearing tracks are considered by the arranger.

The SDK MIDI arranger's balanced preset uses zero-based track 1 (Glockenspiel)
for the lead, track 3 (Electric Bass) for bass, and the remaining glockenspiel, lute,
horn, and viola parts for one accompaniment voice. Their polyphony is reduced
with source-derived arpeggios; pitches outside the playable register are folded
by octaves. The bass retains its original register with a MIDI range of 24–59.
Maximum lead/accompaniment/bass gains are 10/12/8. The MIDI has no percussion;
the score explicitly owns and silences noise so switching from Boss or Venus
cannot leave percussion playing.

The loop is 172.000 seconds at 120 BPM in 4/4, ending at the final musical
note boundary. The MIDI's empty tail to its 184.098-second end-of-track marker
is trimmed. Absolute note boundaries are rounded to the AVM 4 ms grid. The
last notes end 8 ms early so all four voices are released before the loop
command. The accepted arrangement retains all 70 selected glockenspiel attacks
and all 432 bass attacks; 188 accompaniment
segments represent the remaining melodic material. Ordinary builds compile the
editable AVMS and need no MIDI dependency. Reimporting uses `tools/midi_to_avms.py`
as a Python library with melody track 1, bass track 3, and an end time of 172 seconds.

## Validation

From the repository root, compile and validate with:

```powershell
cmake --build projects/avm_rpg/build --config RelWithDebInfo --target rpg_music
ctest --test-dir projects/avm_rpg/build -C RelWithDebInfo -R imported_music --output-on-failure
```

The build does not overwrite the AVMS sources. The test compares the compiled
playback with fingerprints captured from the tracker and accepted MIDI arrangements, including
the exact timing of every audible pitch, gain, and duty change.
