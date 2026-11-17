# The Venlor theme

The Venlor theme is the game's musical theme: a four-bar, thirteen-note melody that opens the
main title and is meant to return throughout the soundtrack, as the Force theme does in *Star
Wars*. It is emotional and lyrical, a slow singing line that an epic song can carry over a
driving beat. This document defines it exactly, says what may change when a song uses it, and
gives a recipe for each mood. Read it before writing any song that quotes the theme.

The one song that uses it so far is [Call of Venlor](../assets/music/title.avms), the title
theme.

## The one rule

**A song presents the theme in one style, and the theme is only part of the song.** The theme's
flexibility is between songs: the title plays it as an epic march, a village song as a soft
flute melody, a battle song as a hard-driven figure, a sad scene as a slow lament. It is not a
reason to show several of those styles inside one song. Within a song, every statement has the
same notes, the same rhythm, the same chords, the same tempo and the same instruments, as a
film score restates its main theme. And the song is not the theme repeated: it needs its own
other melodies. The theme is the landmark that the other music walks between. In a song built
around it, like the title, aim for it to fill roughly a quarter to a third of the bars,
returning two to four times. Most songs should use it far more lightly, as a
[subtle quotation](#subtle-quotation): present for a listener who knows it, but not what the
song is about.

## The theme

Played in D, the key of the title song:

```text
 pickup | bar 1             | bar 2                | bar 3                 | bar 4
 A4     | D5 ------  E5     | A5 ----  G5   E5     | G5    A5    D6    E6  | D6 --.  A5  D5 ----
 degree | 1          2      | 5        4    2      | 4     5     1'    2'  | 1'      5   1
 beats  | 3          1      | 2        1    1      | 1     1     1     1   | 1.5     .5  2
 chord  | D                 | A                    | G                     | D
```

- **Four bars and a quarter-note pickup.** Thirteen notes in two phrases of two bars. The first
  rises from D to A and falls back to E, so it asks a question. The second climbs through G
  and A to a peak on E6, then falls in two long steps, D6 to A5 to D5, and settles.
- **Long, singing notes.** The first note lasts three beats, the A5 two. The vibrato, the
  swell and the percussion belong in those long notes.
- **Interval signature.** In semitones the thirteen notes move
  **+5 +2 +5 −2 −3 +3 +2 +5 +2 −2 −5 −7**. A transposed or re-voiced theme keeps this
  signature. Anything that breaks it is a different tune.
- **Rhythm.** In beats of a quarter note: pickup 1, then 3 1, 2 1 1, 1 1 1 1, 1.5 .5 2. The
  bar of four quarter notes in bar 3 is the quickening before the peak.
- **Four pitches.** The melody uses only A, D, E and G: scale degrees 5, 1, 2 and 4 (in two
  octaves). There is no third, sixth or seventh, so *the chords decide the mood*. Over D, A, G
  and D major chords it is bright and heroic. Over D minor, A minor, G minor and D minor it is
  sorrowful. Both work without changing a note.

In AVMS (channel octave 5), as a phrase to copy:

```avms
// The pickup: a quarter A4 at the end of a bar of rests.
phrase theme_pickup { a,4 }
phrase theme {
    d2. e4 |
    a2 g4 e4 |
    g4 a4 d'4 e'4 |
    d'4. a8 d2 |
}
```

Write the pickup at the end of a bar of rests (`r2. a,4 |`); a bar check straight after a lone
pickup fails. `@instrument` on a note replaces the channel's sound for that note, so a song that
gives the held notes a swelling horn and the short ones a plain horn writes `d2.@rise e4@lead`,
as the title score does.

### Why these notes

- **Open leaps, then sighs.** The two fourths and a fifth at the start reach upward, and the
  small falling steps after them are the sigh. It is the shape of a long breath out.
- **A quickening and a peak.** Four rising quarter notes in bar 3 lift the line to its highest
  note, E6, held back until the second phrase. The descent that follows is long, two big steps
  that take a whole bar and a half.
- **Mode-neutral.** The melody leaves out the notes that distinguish major from minor, so the
  chords carry the mood.
- **One octave and a half, mostly in the piezo's best register.** In D the theme spans A4 to
  E6, and most of it sits in D5-A5, where the lead is clearest. See [other keys](#other-keys)
  before moving it.

## What must not change

A listener should recognise the theme from its first bar. Keep all of these in every
statement of it:

1. The interval signature (+5 +2 +5 −2 −3 +3 +2 +5 +2 −2 −5 −7), at any transposition and in
   any octave.
2. The rhythm, including the pickup. Doubling or halving every note value is allowed (see
   below); changing one is not.
3. The melody stays on the four pitch classes. Do not add a third, sixth or seventh to it, and
   do not decorate it with passing notes or runs. Put those in another voice.
4. A single voice carries the theme. It may be doubled, but not split between voices.

## What may change, per song

Choose these once for a song and keep them for every statement in it.

| Choice | Rule |
| --- | --- |
| Key | Any key; keep the register in mind ([below](#other-keys)). |
| Octave | Any octave. Another voice may double it at the octave. |
| Tempo | About 50 to 176 BPM. |
| Augmentation | Every note value doubled: eight bars and a half-note pickup. Use for grandeur or grief. |
| Diminution | Every note value halved: two bars and an eighth-note pickup. Use for speed and urgency. |
| Harmony | Any chords that contain the theme's pitches; this is the main mood control. |
| Quotation | The whole theme, or only its first half (pickup and bars 1-2, the question) or its second half (bars 3-4, the answer); see [subtle quotation](#subtle-quotation). |
| Timbre | Any waveform, duty, envelope, vibrato, tremolo or swell, except effects that move the pitches off the written notes. |
| Accompaniment | Anything that does not mask the theme. |

Vibrato, a swell and a short slide into a note are decoration and leave the theme intact.
Arpeggio and glissando on the lead would replace its pitches, so do not use them on the voice
that carries the theme. Dynamics may grow over a song, as any arrangement does, but they are
not a change of style.

## Subtle quotation

Not every song should be about the theme. A song can instead carry it quietly, so that a
listener who knows it hears it and one who does not just hears the song's own music. Quote it
exactly (the same notes, rhythm, and chords as everywhere else, in the song's one style) and use
these tools for a light touch:

- **Quote a half.** The first half (the pickup and bars 1-2) ends on E in D, the question; the
  second half (bars 3-4) starts on G and ends on the tonic, the answer. Each half keeps its part
  of the interval signature, so each is recognisable alone.
- **Separate the halves.** Put the question in the middle of the song and leave the song's own
  music to continue from it, and keep the answer for the end, where it becomes the closing
  cadence. The listener hears one question and, a minute later, its answer.
- **Keep it a small share.** A song that quotes both halves once spends about a sixth of its
  bars on the theme or less; the rest is the song's own tune.
- **Do not alter it to blend in.** The quoted notes stay exact. The song's own music should lead
  into them and away from them, and the chords under them are the theme's chords (here G D | C G).
- **Same voice and style as the song.** The quote is sung by the song's lead in its usual
  voice, not given a special instrument or a special level.

*Morning in the Meadow* is the model: its question opens the second section, and its answer is the
last cadence ([below](#morning-in-the-meadow)).

## The theme in each mood

Each row is a whole song's style. The table gives starting points. Every row uses the same
pitches and rhythm. The chords are given for the four bars of the theme: bar 1, bar 2, bar 3, bar 4.

| Mood | Tempo | Mode and chords | Lead voice | Rest of the band | Theme form |
| --- | --- | --- | --- | --- | --- |
| Epic, triumphant | 150-176 | D major: D, A, G, D | 25% pulse horn; the held notes swell with 16-22 cent vibrato | Galloping saw bass, offbeat chord stabs, toms and crashes | Full length, over a fast beat |
| Adventurous | 120-140 | D Mixolydian: D, C, G, D | 25% pulse horn; vibrato on the held notes only | Octave-bounce bass, offbeat stabs, light drums | Full length |
| Peaceful, soothing | 60-84 | D major: D, A, G, D | Soft 37.5% pulse flute, breathy attack, 10 cent vibrato | Harp-like arpeggios, slow roots, no drums | Full length |
| Battle | 150-176 | D minor: Dm, A, Bb, A (two beats each, with the theme halved) | Short 25% pulse notes | Gallop bass and power chords, hard snare | Diminished; the last chord never resolves |
| Sad, mournful | 48-60 | D minor: Dm, Am, Gm, Dm (two bars each, with the theme doubled) | 50% pulse with a falling envelope and 22 cent vibrato | A sparse harp arpeggio, a very quiet bass | Augmented, once, then silence |
| Mysterious | 56-80 | D with a ♭2 pedal (E♭ under the opening) | Music-box pulse, 12.5% duty, fast decay | Drips, breath, low drone | Full length, high and quiet |

The epic row is the title song itself, and the adventurous, peaceful, battle and mournful rows
are backed by the presentations below. The mysterious row is an untried sketch: write and check
it before relying on it.

Notes on the harmony:

- **Major (epic, peaceful).** The A5 of bar 2 sits on the A chord and the G5 of bar 3 on the G
  chord, a rising *I to V* and a falling *IV to I* across the phrase pair. The final D5 lands on
  the D chord and settles it.
- **Mixolydian (adventurous).** Replacing the A chord of bar 2 with C puts the held A5 on the
  sixth of a C chord. It sounds open and breezy where the A chord is final.
- **Minor (battle, mournful).** The same pitches stay and the chords turn minor. In battle the
  E6 over the B♭ chord is a tritone, a deliberate grind, and the last D over the A chord is a
  suspended fourth that wants to go somewhere, so the loop pushes on. A sad version lets the
  last D settle on D minor.
- **Do not add the third to the melody.** If the harmony needs the third, put it in the chord
  voices.

### Presentations

The four examples below show only the theme in one style, with an accompaniment that fits its
chords. They are not songs. A real song surrounds the theme with its own melodies, as the title
score does ([below](#call-of-venlor)). Each example compiles as written, and its lead plays the
identical theme. The `--stats` output of
[`tools/avm_music.py`](../../../tools/avm_music.py) is the quickest check.

**Peaceful** (D major, 72 BPM): a flute, a rocking harp and slow roots.

```avms
tempo 72
meter 4/4
volume max
automation 16ms

envelope breath { points 0ms:0%, 40ms:100%, 400ms:80% release 120ms:0% }
envelope pluck  { points 0ms:100%, 90ms:55%, 300ms:25% release 40ms:0% }

instrument flute {
    wave pulse duty 37.5% volume 12 envelope breath gate 94%
    lfo { target pitch depth 10cents period 220ms }
}
instrument harp { wave pulse duty 25% volume 7 envelope pluck gate 90% }
instrument bass { wave saw volume 7 envelope pluck gate 95% }

channel 0 flute octave 5 {
    r2. a,4 |
    d2. e4 | a2 g4 e4 | g4 a4 d'4 e'4 | d'4. a8 d2 |
}
// The chords are D A G D, one per bar; a harp rocks around each.
channel 1 harp octave 3 {
    r1 |
    d8 a8 d'8 a8 d8 a8 d'8 a8 |
    a8 e'8 a'8 e'8 a8 e'8 a'8 e'8 |
    g8 d'8 g'8 d'8 g8 d'8 g'8 d'8 |
    d8 a8 d'8 a8 d8 a8 d'8 a8 |
}
channel 2 bass octave 2 {
    r1 |
    d1 | a1 | g1 | d1 |
}
```

**Adventurous** (D Mixolydian, 132 BPM): a horn over a bouncing bass and light drums.

```avms
tempo 132
meter 4/4
volume max
automation 16ms

envelope horn_env  { points 0ms:78%, 28ms:100%, 170ms:90%, 560ms:80% release 60ms:0% }
envelope stab_env  { points 0ms:100%, 70ms:55%, 180ms:25% release 20ms:0% }
envelope bass_env  { points 0ms:100%, 60ms:82%, 190ms:62% release 24ms:0% }
envelope kick_env  { points 0ms:100%, 30ms:62%, 100ms:0% }
envelope snare_env { points 0ms:100%, 22ms:75%, 100ms:0% }
envelope hat_env   { points 0ms:100%, 26ms:0% }

instrument horn { wave pulse duty 25% volume 15 envelope horn_env gate 92% }
instrument stabM {
    wave pulse duty 37.5% volume 8 envelope stab_env gate 78%
    arpeggio { offsets 0,4,7 step 32ms }
}
instrument bass  { wave saw volume 11 envelope bass_env gate 86% }
instrument kick  { wave noise volume 9 envelope kick_env gate 85% }
instrument snare { wave noise volume 6 envelope snare_env gate 85% }
instrument hat   { wave noise volume 2 envelope hat_env gate 55% }

channel 0 horn octave 5 {
    r2. a,4 |
    d2. e4 | a2 g4 e4 | g4 a4 d'4 e'4 | d'4. a8 d2 |
}
// The chords are D C G D, one per bar.
channel 1 stabM octave 3 {
    r1 |
    r8 d8 r8 d8 r8 d8 r8 d8 | r8 c8 r8 c8 r8 c8 r8 c8 |
    r8 g8 r8 g8 r8 g8 r8 g8 | r8 d8 r8 d8 r8 d8 r8 d8 |
}
channel 2 bass octave 2 {
    r1 |
    d8 d'8 d8 d'8 d8 d'8 d8 d'8 | c'8 c''8 c'8 c''8 c'8 c''8 c'8 c''8 |
    g8 g'8 g8 g'8 g8 g'8 g8 g'8 | d8 d'8 d8 d'8 d8 d'8 d8 d'8 |
}
channel 3 octave 4 {
    r1 |
    repeat 4 { c8@kick c8@hat c8@snare c8@hat c8@kick c8@hat c8@snare c8@hat | }
}
```

**Battle** (D minor, 164 BPM): the theme at half the note values, twice, over a gallop. The
chords are Dm, A, B♭ and A, two beats each, and the last never resolves.

```avms
tempo 164
meter 4/4
volume max
automation 16ms

envelope snap_env  { points 0ms:100%, 80ms:72%, 200ms:58% release 30ms:0% }
envelope pow_env   { points 0ms:100%, 50ms:72%, 120ms:52% release 12ms:0% }
envelope bass_env  { points 0ms:100%, 60ms:82%, 190ms:62% release 24ms:0% }
envelope kick_env  { points 0ms:100%, 30ms:62%, 100ms:0% }
envelope snare_env { points 0ms:100%, 22ms:75%, 100ms:0% }
envelope hat_env   { points 0ms:100%, 26ms:0% }

instrument snap { wave pulse duty 25% volume 15 envelope snap_env gate 80% }
instrument pow  {
    wave pulse duty 37.5% volume 8 envelope pow_env gate 80%
    arpeggio { offsets 0,7,12 step 24ms }
}
instrument bass  { wave saw volume 11 envelope bass_env gate 86% }
instrument kick  { wave noise volume 9 envelope kick_env gate 85% }
instrument snare { wave noise volume 6 envelope snare_env gate 85% }
instrument hat   { wave noise volume 2 envelope hat_env gate 55% }

// The theme at half the note values: two bars, with an eighth-note pickup. The chords are
// Dm A | Bb A, two beats each; the last never resolves, and the E over the Bb grinds.
phrase theme_a {
    d4. e8 a4 g8 e8 |
    g8 a8 d'8 e'8 d'8. a16 d8 a,8 |
}
phrase theme_z {
    d4. e8 a4 g8 e8 |
    g8 a8 d'8 e'8 d'8. a16 d4 |
}
phrase pg_dm { d8 d16 d16 d8 d16 d16 a8 a16 a16 a8 a16 a16 | }
phrase pg_bb { bb8 bb16 bb16 bb8 bb16 bb16 a8 a16 a16 a8 a16 a16 | }

channel 0 snap octave 5 {
    r2. r8 a,8 |
    theme_a theme_z
}
// Power chords in step with the bass.
channel 1 pow octave 3 {
    r1 |
    pg_dm pg_bb pg_dm pg_bb
}
channel 2 bass octave 2 {
    r1 |
    pg_dm pg_bb pg_dm pg_bb
}
channel 3 octave 4 {
    r1 |
    repeat 4 {
        c16@kick r16 c16@hat r16  c16@snare r16 c16@hat c16@hat
        c16@kick r16 c16@hat r16  c16@snare r16 c16@hat c16@hat |
    }
}
```

**Mournful** (D minor, 54 BPM): the theme at twice the note values, one slow harp, no drums.
The chords are Dm, Am, Gm and Dm, two bars each.

```avms
tempo 54
meter 4/4
volume max
automation 16ms

envelope sigh  { points 0ms:0%, 120ms:100%, 1200ms:62%, 3000ms:22% release 250ms:0% }
envelope pluck { points 0ms:100%, 120ms:55%, 400ms:25% release 60ms:0% }

instrument sigh {
    wave pulse duty 50% volume 12 envelope sigh gate 98%
    lfo { target pitch depth 22cents period 260ms }
}
instrument harp {
    wave pulse duty 25% volume 6 envelope pluck gate 90%
    arpeggio { offsets 0,3,7 step 160ms }
}
instrument low { wave saw volume 6 envelope pluck gate 95% }

// The theme at twice the note values: eight bars, with a half-note pickup.
channel 0 sigh octave 5 {
    r2 a,2 |
    d1. e2 |
    a1 g2 e2 |
    g2 a2 d'2 e'2 |
    d'2. a4 d1 |
    r1 |
}
channel 1 harp octave 3 {
    r1 |
    d1 d1 | a1 a1 | g1 g1 | d1 d1 |
    r1 |
}
channel 2 low octave 2 {
    r1 |
    d1 d1 | a1 a1 | g1 g1 | d1 d1 |
    r1 |
}
```

## Other keys

The theme is the same thirteen scale positions in every key, from the pickup (degree 5 below the
tonic) to the peak (degree 2 in the next octave). Written for a channel at the octave shown,
with the pickup at the end of a bar of rests, each key is spelled below. The pickup is the
lowest note and the peak is the tenth. The peaks of E and F (F#6 and G6) lie above the lead's
comfortable range, which ends near E6, so prefer C, D, and the keys written at octave 4.

```avms
// C   octave 5 (G4 to D6)   g,4 | c2. d4 | g2 f4 d4 | f4 g4 c'4 d'4 | c'4. g8 c2 |
// D   octave 5 (A4 to E6)   a,4 | d2. e4 | a2 g4 e4 | g4 a4 d'4 e'4 | d'4. a8 d2 |
// E   octave 5 (B4 to F#6)  b,4 | e2. f#4 | b2 a4 f#4 | a4 b4 e'4 f#'4 | e'4. b8 e2 |
// F   octave 5 (C5 to G6)   c4 | f2. g4 | c'2 bb4 g4 | bb4 c'4 f'4 g'4 | f'4. c'8 f2 |
// G   octave 4 (D4 to A5)   d4 | g2. a4 | d'2 c'4 a4 | c'4 d'4 g'4 a'4 | g'4. d'8 g2 |
// A   octave 4 (E4 to B5)   e4 | a2. b4 | e'2 d'4 b4 | d'4 e'4 a'4 b'4 | a'4. e'8 a2 |
// Bb  octave 4 (F4 to C6)   f4 | bb2. c'4 | f'2 eb'4 c'4 | eb'4 f'4 bb'4 c''4 | bb'4. f'8 bb2 |
```

Each line was compiled and found to carry the same interval signature. G is the key of
*Morning in the Meadow*, which quotes it there.

## Using the theme in a song

- **One style per song.** Pick a row of the mood table and keep it. Do not show the theme in
  several styles in one song; that is what the other songs are for.
- **Everything else is new music.** Write the song's other tunes, riffs, bridges and
  interludes first, and place the theme between them. Do not fill the song with the theme or
  its fragments. The title song spends about 29 percent of its bars on the theme.
- **Introduce it early.** State it within the first eight bars after any introduction. Return to
  it two to four times, and make the returns a real landmark: after contrasting music, and
  with the dynamics at least as large as the first time.
- **Restate it exactly.** Every statement has the same notes, rhythm, chords, key and voices.
  A last statement may be louder and a note may ring longer at the end.
- **Keep it on the lead.** Put the theme on channel 0 and the bass on channel 2 so that a
  sound effect borrowing channel 1 or 3 never silences it.
- **Give it room.** Its long notes are where swells, vibrato and crashes belong. A held D5 or
  A5 under a galloping bass is the sound of the title song.
- **Check the signature.** Compile the score, then count the theme with the snippet below. The
  interval signature must appear whenever a song claims to quote it.

```python
# Count the Venlor theme in a compiled score (needs numpy and scipy for the renderer module).
import math
import sys

sys.path.insert(0, "tools")  # the repository's tools/ directory
from avm_audio_render import decode

QUESTION = [5, 2, 5, -2, -3]  # the pickup and bars 1-2
ANSWER = [2, 5, 2, -2, -5, -7]  # bars 3-4
THEME = [5, 2, 5, -2, -3, 3, 2, 5, 2, -2, -5, -7]


def lead_semitones(path):
    """The lead's sounding notes as rounded MIDI numbers, repeats collapsed."""
    increment, volume, notes = 0, 0, []
    for event in decode(open(path, "rb").read()):
        for channel, pitch in event.pitches:
            if channel == 0:
                increment = pitch
        for channel, control, _ in event.controls:
            if channel == 0:
                volume = control & 31
        if increment and volume:
            note = round(69 + 12 * math.log2(increment * 15625 / 65536 / 440))
            if not notes or notes[-1] != note:
                notes.append(note)
    return notes


def count(notes, signature):
    steps = [b - a for a, b in zip(notes, notes[1:])]
    return sum(steps[i:i + len(signature)] == signature for i in range(len(steps)))


notes = lead_semitones(sys.argv[1])
print("question", count(notes, QUESTION), "answer", count(notes, ANSWER),
      "whole theme", count(notes, THEME))
```

Run it from the repository root with `python3 count_theme.py score.bin`, where `score.bin`
comes from `python3 tools/avm_music.py score.avms score.bin`.

## Where the theme appears

| Song | File | Style | Key |
| --- | --- | --- | --- |
| Call of Venlor | [`title.avms`](../assets/music/title.avms) | Epic march | D major |
| Morning in the Meadow | [`peaceful.avms`](../assets/music/peaceful.avms) | Pastoral, quiet | G major |

Add a row, and a bar map if the song is long, whenever a score quotes the theme.

### Call of Venlor

The title song runs 42 bars at 160 BPM (63 seconds), in a single style: a bright pulse horn,
offbeat chord stabs, a galloping saw bass and a drum kit. It plays the theme three times, each
time in full, in D with the chords D, A, G, D, and surrounds it with three other tunes.

| Bars | Part | Music |
| --- | --- | --- |
| 1-2 | Intro | A heartbeat of toms and a swelling roll under a D pedal; the A4 pickup |
| 3-6 | A | **The theme.** A crash on the downbeat; the first note swells and vibrates |
| 7-10 | A | A rising fanfare riff over G, A, G, A: `G G B D B`, `E C# A` (new material) |
| 11-18 | B | A flowing second tune in eighth notes over Bm G D A, Bm G A D (new material) |
| 19-26 | C | The bridge over Em Em G A, Bm G A A: long falling arpeggios, then rising ones, then a held E6 under a growing roll (new material) |
| 27-30 | A' | **The theme** again, exactly as before |
| 31-34 | A' | The riff again, handing over the pickup |
| 35-38 | Finale | **The theme** a last time; its last D swells, vibrates and rings into the coda |
| 39-42 | Coda | The chord rings and fades to a heartbeat of toms that leads back to bar 1 |

The theme takes twelve of the 42 bars, and the riff, the flowing tune and the bridge take the
other twenty-four. None of the three uses the theme's rhythm or melodic shape: the riff is
staccato fanfare figures, the flowing tune moves by step in eighth notes, and the bridge is long
arpeggios, so the theme stays the one long, singing line. The riff closes on D the first time
and hands the pickup to the finale the second.

### Morning in the Meadow

The song of the outdoors, the interiors and the dungeon runs 24 bars at 76 BPM (75.8 seconds)
in G major and one quiet style: a breathy flute, a rocking harp and slow saw roots, no drums. It
quotes the theme in G, once per half, and the rest is its own music. The count by the snippet
above is one question, one answer and no whole theme.

| Bars | Part | Music |
| --- | --- | --- |
| 1-8 | A | The song's own tune over G Em C D, twice; its last note, D4, is the quotation's pickup |
| 9-10 | B | **The question** (`D4 | G4 A4 | D5 C5 A4`) over G D |
| 11-16 | B | New music over Em Am D, Am D, Em D that rises and returns |
| 17-20 | A' | The first half of the song's tune again over G Em C D |
| 21-22 | End | **The answer** (`C5 D5 G5 A5 | G5 D5 G4`) over C G, the song's closing cadence |
| 23-24 | End | The last G rings and fades |

The theme takes four of the 24 bars. The song's own tune moves by thirds and scale steps in
quarter and half notes, and the quote enters on the same flute with no special treatment, so it
sounds like part of the tune. The score compiles to about 14.2 KB and sends at most four events in any
20 ms frame; its lead and harp never rest for a whole beat, which the pause animation's frame-timing
check needs.

## Playback notes (title song)

- The game enables 16 kHz audio and music looping before the title screen starts, then calls
  `music_play(MUSIC_TITLE)`. When A or B ends the title, `music_play_area()` fades the title
  song out and the area's song in through the normal song-change fade.
- The compiled score is about 22.9 KB and sends at most five events in any 20 ms frame, in line
  with the other songs. Most of its bars reach the 31-unit channel sum that fixes the
  `volume max` scale at about 77%; the intro, the finale's ring and the coda are quieter by
  design.
- The animated title is frame-paced. Measured in the emulator with the song playing, 600
  frames took 12 seconds (50 Hz); the mean interval was 319,652 cycles and the longest
  332,136, inside the 336,000-cycle tolerance the other frame-budget checks use. A voice or
  effect added to the song or to the title scene should be measured again.
- Listening on the physical piezo remains **Unverified**. The levels, the swell and vibrato
  depths, and the balance of the drums are starting points.
