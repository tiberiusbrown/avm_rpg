"""Check the shipped score, including two complete decoded loop iterations."""

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
assert data[:2] == bytes([0xA2, 7]), "three tonal voices, no percussion"
assert len(data) < 65536

notes, controls = [0x80] * 3, [24] * 3
first_notes = None
for iteration in range(2):
    offset, ticks, attacks = 2, 0, [0] * 3
    peak_volumes = [0] * 3
    first = True
    while True:
        delta, mask = data[offset : offset + 2]
        offset += 2
        ticks += delta + 1
        if mask == 0:
            command = data[offset]
            offset += 1
            assert command in (0, 2), "peaceful music must loop without stopping"
            if command == 2:
                break
            continue
        assert not mask & 0x88, "noise voice must remain unused"
        for channel in range(3):
            if mask & (1 << channel):
                notes[channel] = data[offset]
                offset += 1
                assert notes[channel] == 0x80 or 24 <= notes[channel] <= 95
                attacks[channel] += notes[channel] != 0x80
        for channel in range(3):
            if mask & (16 << channel):
                controls[channel] = data[offset]
                offset += 1
                volume = controls[channel] & 31
                peak_volumes[channel] = max(peak_volumes[channel], volume)
                assert controls[channel] >> 5 == (1 if channel == 1 else 3)
        if first:
            assert mask == 0x77, "every loop must restore all notes and controls"
            assert controls == [100, 36, 100], "gentle sine/triangle/sine attacks"
            if first_notes is None:
                first_notes = notes.copy()
            assert notes == first_notes == [76, 64, 48]
            first = False
    assert offset == len(data), "loop command must terminate the bounded asset"
    assert ticks == 12000, "16 bars of 3/4 at 60 BPM must loop every 48 seconds"
    assert attacks[0] >= 32 and attacks[1:] == [96, 16]
    assert peak_volumes == [31, 12, 19], "full-scale melody with proportional accompaniment"
    assert notes == [0x80] * 3, "release all voices before returning to the opening"

print(f"PASS peaceful music: {len(data)} bytes, 48-second loop, two iterations")
