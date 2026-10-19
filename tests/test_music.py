"""Check two decoded loops for the bright, contrapuntal three-voice arrangement."""

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from generate_music import generate

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--compiler", type=Path, required=True)
args = parser.parse_args()
header = (ROOT / "generated/peaceful_music.h").read_text(encoding="utf-8")
assert header == generate(args.compiler), "regenerate peaceful_music.h"
data = bytes(int(value, 16) for value in re.findall(r"0x([0-9a-f]{2})", header))
assert data[:2] == bytes([0xA2, 7]), "three tonal voices, including pitched percussion"
assert len(data) < 65536

notes, controls = [0x80] * 3, [24] * 3
for iteration in range(2):
    offset, ticks, attacks = 2, 0, [0] * 3
    peak_volumes, started = [0] * 3, [None] * 3
    gates, pitches = [[], [], []], [[], [], []]
    beat_onsets = []
    upper_onsets, duet_ticks = [[], []], 0
    overlap, longest_overlap, peak_mix = 0, 0, 0
    first = True
    while True:
        delta, mask = data[offset : offset + 2]
        offset += 2
        delta += 1
        active = [ch for ch in range(3) if notes[ch] != 0x80 and controls[ch] & 31]
        if 0 in active and 1 in active:
            duet_ticks += delta
        assert len(active) <= 2, "the two melodic lines and beat accents stay sparse"
        peak_mix = max(peak_mix, sum(controls[ch] & 31 for ch in active))
        overlap = overlap + delta if len(active) == 2 else 0
        longest_overlap = max(longest_overlap, overlap)
        ticks += delta
        if mask == 0:
            command = data[offset]
            offset += 1
            assert command in (0, 2), "the score must loop without stopping"
            if command == 2:
                break
            continue
        assert not mask & 0x88, "percussion uses pitched notes, not the noise voice"
        for channel in range(3):
            if mask & (1 << channel):
                previous = notes[channel]
                note = data[offset]
                offset += 1
                assert note == 0x80 or 24 <= note <= 95
                notes[channel] = note
                if note == 0x80:
                    if started[channel] is not None:
                        gates[channel].append(ticks - started[channel])
                        started[channel] = None
                else:
                    attacks[channel] += 1
                    pitches[channel].append(note)
                    if previous == 0x80:
                        started[channel] = ticks
                        if channel < 2:
                            upper_onsets[channel].append(ticks)
                        if channel == 2:
                            beat_onsets.append(ticks)
                    else:
                        assert channel == 2, "melody and replies must release before retriggering"
                        assert note == previous - 12, "tom heads must drop one octave"
        for channel in range(3):
            if mask & (16 << channel):
                controls[channel] = data[offset]
                offset += 1
                peak_volumes[channel] = max(peak_volumes[channel], controls[channel] & 31)
                assert controls[channel] >> 5 == (3 if channel == 0 else 1)
        if first:
            assert mask == 0x77, "every loop must restore all notes and controls"
            assert notes == [84, 0x80, 60], "opening C6 and short C4-to-C3 beat"
            assert controls == [102, 32, 46]
            first = False
    assert offset == len(data), "loop command must terminate the bounded asset"
    assert ticks == 7680, "16 bars of 4/4 at 125 BPM must loop every 30.72 seconds"
    assert attacks[0] >= 72 and attacks[1] == 64, "active melody with an independent counterline"
    assert min(pitches[0]) >= 76 and max(pitches[0]) == 88, "lead stays in the high E5..E6 register"
    assert sum(note >= 79 for note in pitches[0]) >= len(pitches[0]) * 0.9
    assert min(pitches[1]) >= 72, "replies stay above middle-register accompaniment"
    assert max(gates[0]) <= 44 and max(gates[1]) <= 64, "short articulated melodic notes, no held chord bed"
    assert len(gates[2]) == 64 and max(gates[2]) <= 9, "64 tiny percussive beats"
    assert beat_onsets == [1 + beat * 120 for beat in range(64)], "steady quarter-note pulse"
    assert duet_ticks > 0, "the independent melodic lines should overlap"
    assert set(upper_onsets[1]) - set(upper_onsets[0]), "the counterline needs its own rhythm"
    assert longest_overlap <= 44, "overlaps belong to passing melodic notes"
    assert peak_volumes == [26, 11, 14]
    assert peak_mix * 2 <= 127, "preserve mixer headroom at music master 64"
    assert notes == [0x80] * 3, "release all voices before the loop boundary"
    assert started == [None] * 3

print(f"PASS Dewlight Skip: {len(data)} bytes, 30.72-second loop, "
      f"{attacks[0]} lead attacks, {attacks[1]} counterline notes, 64 beats; "
      f"maximum overlap {longest_overlap * 4} ms; two iterations")
