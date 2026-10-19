"""Compare two A2 loops against every note and onset in the supplied Zelda MIDI."""

import argparse
from fractions import Fraction
import re
import sys
from pathlib import Path

from midi_reference import read_reference

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from generate_music import generate

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--compiler", type=Path, required=True)
args = parser.parse_args()
header = (ROOT / "generated/peaceful_music.h").read_text(encoding="utf-8")
assert header == generate(args.compiler), "regenerate peaceful_music.h"
data = b"".join(int(value, 16).to_bytes(8, "little")
                for value in re.findall(r"0x([0-9a-f]{16})ULL", header))
assert data[:2] == bytes([0xA2, 15]), "two pulse voices, triangle bass, and noise"
platform = (ROOT / "src/platform_avm.cpp").read_text(encoding="utf-8")
master = int(re.search(r"MUSIC_MASTER_VOLUME\s*=\s*(\d+)", platform).group(1))
division, tempo_us, tracks = read_reference(ROOT / "assets/music/z1overw.mid")
parts = {track["name"]: track["notes"] for track in tracks if track["notes"]}
assert set(parts) == {"Melody", "Harmony", "Bass Line", "Percussion"}
loop_pulses = 24 * 4 * division


def tick(pulse):
    return int(Fraction(pulse * tempo_us, division * 4000) + Fraction(1, 2))


expected, overlaps, early_releases = [], 0, 0
for name in ("Melody", "Harmony", "Bass Line", "Percussion"):
    part = parts[name]
    reference = []
    for index, note in enumerate(part):
        end = note["end"]
        if index + 1 < len(part) and end > part[index + 1]["start"]:
            # The MIDI overlaps five same-pitch retriggers. One hardware voice
            # retains every attack by ending the old note at the new onset.
            assert note["pitch"] == part[index + 1]["pitch"]
            end = part[index + 1]["start"]
            overlaps += 1
        end_tick = tick(end) + 1
        if end_tick >= tick(loop_pulses):
            # Leave one tick for Loop after the final bass release.
            assert index == len(part) - 1 and name == "Bass Line"
            end_tick -= 1
            early_releases += 1
        # Channel-10 MIDI note 38 selects an acoustic snare, not a D2 pitch.
        # Use the engine's highest noise rate for a brief, crisp snare burst.
        if name == "Percussion":
            assert note["pitch"] == 38
        pitch = 95 if name == "Percussion" else note["pitch"]
        reference.append((tick(note["start"]) + 1, end_tick, pitch))
    expected.append(reference)
assert overlaps == 5
assert early_releases == 1

notes, controls = [0x80] * 4, [24] * 4
previous_loop = None
for iteration in range(2):
    offset, ticks = 2, 0
    started = [None] * 4
    decoded = [[] for _ in range(4)]
    peak_mix, polyphony = 0, 0
    events = []
    first = True
    while True:
        delta, mask = data[offset : offset + 2]
        offset += 2
        delta += 1
        active = [ch for ch in range(4) if notes[ch] != 0x80 and controls[ch] & 31]
        polyphony = max(polyphony, len(active))
        peak_mix = max(peak_mix, sum((controls[ch] & 31) * master // 32 for ch in active))
        ticks += delta
        if mask == 0:
            command = data[offset]
            offset += 1
            assert command in (0, 2), "the score must loop without stopping"
            if command == 2:
                break
            continue
        for channel in range(4):
            if mask & (1 << channel):
                note = data[offset]
                offset += 1
                assert note == 0x80 or 24 <= note <= 95
                if started[channel] is not None:
                    decoded[channel].append((started[channel], ticks, notes[channel]))
                    started[channel] = None
                notes[channel] = note
                if note != 0x80:
                    started[channel] = ticks
        for channel in range(4):
            if mask & (16 << channel):
                controls[channel] = data[offset]
                offset += 1
                assert controls[channel] >> 5 == (0, 0, 1, 0)[channel]
        if first:
            assert mask == 0xFF, "every loop must restore all notes and controls"
            first = False
        events.append((ticks, tuple(notes), tuple(controls)))
    assert len(data) - offset < 8 and not any(data[offset:]), "only word padding may follow Loop"
    assert ticks == tick(loop_pulses), "24 bars at the MIDI's exact tempo"
    for channel in range(4):
        assert decoded[channel] == expected[channel], f"MIDI pitch/timing mismatch on channel {channel}"
    assert polyphony == 4, "exercise the full arrangement together"
    assert peak_mix <= 127, f"preserve mixer headroom at music master {master}: {peak_mix}"
    assert notes == [0x80] * 4, "release all voices before the loop boundary"
    assert started == [None] * 4
    if previous_loop is not None:
        assert events == previous_loop, "loop playback must not inherit stale channel state"
    previous_loop = events

print(f"PASS Zelda Overworld: {offset} bytes, {ticks * 0.004:.3f}-second loop, "
      f"all {sum(map(len, expected[:3]))} pitched notes and {len(expected[3])} drum hits "
      f"match the MIDI reference at 4 ms resolution; "
      f"maximum mixer amplitude {peak_mix}/127 at master {master}; two iterations")
