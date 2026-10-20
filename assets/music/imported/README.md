# Imported rotation tracks

These are the unmodified files supplied from Downloads. `tools/import_music.py`
arranges them for AVM's three pitched voices and one noise voice, and
`tools/generate_music.py` embeds their A4 streams after the six original songs.
Rebuilding needs only Python's standard library and the SDK's `avm_music.py`.

| Source | Menu title | Complete loop |
|---|---|---:|
| `leap.mid` | Leap | 161.032 s |
| `Boss.ftm` | Boss | 34.000 s |
| `Venus.ftm` | Venus | 25.600 s |

Leap keeps the lead guitar, hands the melody to keyboards/strings during their
sections, reduces the backing chords to one voice, and preserves the bass line.
MIDI tempo, note lengths, velocity, channel volume/expression, and pitch bends
(including the source's RPN bend range) control the arrangement. Drum onsets
become brief noise envelopes; stereo doubling and General MIDI instrument
timbres are reduced to AVM triangle/square waves.

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
The FTM reader intentionally checks these modules' format/chips/effects rather
than silently accepting unrelated formats. The file layout is documented in
the [Dn-FamiTracker format reference](https://github.com/Dn-Programming-Core-Management/Dn-FamiTracker/wiki/Dn-FT-module-format).

The imported voice gains total at most 61, keeping the mix below 127 at the
reference master volume of 64. The game's existing master boost is unchanged.

From the repository root, regenerate and validate with:

```powershell
python projects/avm_rpg/tools/generate_music.py --compiler tools/avm_music.py
python projects/avm_rpg/tests/test_imported_music.py --compiler tools/avm_music.py
```
