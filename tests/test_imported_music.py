"""Check imported notes/timing, source identity, all voices, and repeatable loops."""

import argparse
from fractions import Fraction
import hashlib
from pathlib import Path
import re
import runpy
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from generate_music import SONGS, song_source
from import_music import (GAINS, IMPORTED_SONGS, compile_import, ftm_source,
                          increment, midi_events, render_ftm)

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--compiler", type=Path, required=True)
encoder = runpy.run_path(str(parser.parse_args().compiler))
assert SONGS[-3:] == ("leap", "boss", "venus")
assert sum(GAINS) * 2 <= 127

# Running status and a tempo change must retain cumulative, absolute time.
track = bytes.fromhex("00 ff 51 03 07 a1 20 00 90 3c 64 60 3c 00 "
                      "00 ff 51 03 0f 42 40 60 90 40 64 60 80 40 00 00 ff 2f 00")
midi = b"MThd" + struct.pack(">IHHH", 6, 0, 1, 96) + b"MTrk" + struct.pack(">I", len(track)) + track
events, duration = midi_events(midi)
assert [event[0] for event in events] == [0, Fraction(1, 2), Fraction(3, 2), Fraction(5, 2)]
assert duration == Fraction(5, 2)
assert increment(69) == encoder["note_increment"](69)

header = (ROOT / "generated/music.h").read_text(encoding="utf-8")
expected_ticks = {"leap": 40258, "boss": 8500, "venus": 6400}
for song in IMPORTED_SONGS:
    source = song_source(song)
    data = compile_import(source, encoder)
    words = re.search(rf"{song.upper()}_MUSIC\[\] = \{{(.*?)\}};", header, re.DOTALL)[1]
    stored = b"".join(int(word, 16).to_bytes(8, "little")
                      for word in re.findall(r"0x([0-9a-f]{16})ULL", words))
    assert stored[:len(data)] == data and not any(stored[len(data):])
    assert data[:2] == bytes([0xA5, 15])
    notes, controls = [65535] * 4, [127] * 4
    previous = None
    for iteration in range(2):
        offset, ticks, attacks, frames, peak = 2, 0, [0] * 4, [], 0
        while True:
            delta, mask = data[offset:offset + 2]
            offset += 2
            ticks += delta + 1
            if not mask:
                command = data[offset]
                offset += 1
                assert command in (0, 2)
                if command == 2:
                    break
                continue
            if not frames:
                assert ticks == 1 and mask == 255, "reset dirty state immediately"
            for channel in range(4):
                if mask & (1 << channel):
                    notes[channel] = int.from_bytes(data[offset:offset + 2], "little")
                    attacks[channel] += notes[channel] != 0
                    offset += 2
            for channel in range(4):
                if mask & (16 << channel):
                    encoded_control = data[offset]
                    controls[channel] = encoded_control & 31
                    offset += 1
                    assert encoded_control & 0x60 == 0
                    if encoded_control & 128:
                        extension = data[offset]
                        offset += 1
                        assert channel < 3 or 1 <= extension <= 63
                    assert controls[channel] & 31 <= GAINS[channel]
            peak = max(peak, sum((control & 31) * 2 for note, control in zip(notes, controls) if note))
            frames.append((ticks, tuple(notes), tuple(controls)))
        assert ticks == expected_ticks[song], "preserve complete source song duration"
        assert offset == len(data) and all(attacks), "audible music and percussion on all four voices"
        assert notes == [0] * 4 and all(control & 31 == 0 for control in controls)
        assert 0 < peak <= 127
        if previous is not None:
            assert frames == previous, "repeat without stale state from the previous loop/song"
        previous = frames
    if source.suffix == ".ftm":
        raw = source.read_bytes()
        instruments, sequences, orders, patterns, speed, tempo, length = ftm_source(raw)
        assert (len(orders), speed, tempo, length) == ((9, 5, 200, 64) if song == "boss" else (4, 8, 200, 64))
        rendered = list(render_ftm(raw, song))
        if song == "boss":
            assert len(rendered) == 2040, "D00 skips half the introductory frame; B01 closes the song"
            assert rendered[0][1][0][0] == increment(50), "D-2 in FamiTracker is MIDI D3"
        else:
            assert len(rendered) == 1536
            assert rendered[0][1][0][0] == increment(72), "retain Venus's opening C-4 lead"
    else:
        _, length = midi_events(source.read_bytes())
        assert length == Fraction(2516124, 15625), "include all 104 MIDI bars"
    print(f"PASS {song}: {ticks * .004:.3f}s, {len(data)} bytes, "
          f"four voices, peak {peak}/127, two clean loops; source SHA256 "
          f"{hashlib.sha256(source.read_bytes()).hexdigest()}")
