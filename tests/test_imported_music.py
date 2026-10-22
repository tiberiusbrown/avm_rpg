"""Check imported playback, generated data, voice ownership, and repeatable loops."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import runpy

ROOT = Path(__file__).resolve().parents[1]
SONGS = ("boss", "venus", "good_endings")
GAINS = {"boss": (20, 15, 17, 9), "venus": (20, 15, 17, 9), "good_endings": (10, 12, 8, 0)}
AUDIBLE_VOICES = {"boss": (True,) * 4, "venus": (True,) * 4,
                  "good_endings": (True, True, True, False)}
# Fingerprints of the original tracker arrangements, captured before AVMS
# conversion, plus the cleaned song_peaceful.mid arrangement (no synth pad).
# Include every audible pitch, gain, duty, and exact event tick.
PLAYBACK_SHA256 = {
    "boss": "a8d64dc50c743c324021f89f9baf460639ad4bd0ed6645e7af4f25b1832165cf",
    "venus": "a23057724135cc6f600e2a0e7fbd09cb38f4b74c2ec337c465606d5f57982cd2",
    "good_endings": "6a5d1f1d6a1bcf3c3b65db5d1e29b2fa6f6eee956dd3c459575a3e89a507d7fd",
}

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--compiler", type=Path, required=True)
encoder = runpy.run_path(str(parser.parse_args().compiler))
assert not list((ROOT / "assets/music/imported").glob("*.mid"))
assert all(sum(gains) * 2 <= 127 for gains in GAINS.values())

expected_ticks = {"boss": 8500, "venus": 6400, "good_endings": 43000}


def audible_events(data):
    """Compare playback, ignoring redundant phase-preserving pitch writes."""
    notes, volumes, duties = [0] * 4, [0] * 4, [128] * 4
    offset, ticks, events = 1, 0, []
    while True:
        delta, mask = data[offset:offset + 2]
        offset += 2
        ticks += delta + 1
        if not mask:
            command = data[offset]
            offset += 1
            if command == 1:
                return ticks, events
            assert command == 0
            continue
        for channel in range(4):
            if mask & (1 << channel):
                notes[channel] = int.from_bytes(data[offset:offset + 2], "little")
                offset += 2
        for channel in range(4):
            if mask & (16 << channel):
                control = data[offset]
                volumes[channel] = control & 31
                offset += 1
                if control & 128:
                    assert channel < 3, "imported arrangements must not retrigger noise"
                    duties[channel] = data[offset]
                    offset += 1
        state = tuple((notes[channel], volumes[channel], duties[channel] if channel < 3 else 0)
                      if notes[channel] and volumes[channel] else (0, 0, 0)
                      for channel in range(4))
        if not events or state != events[-1][1]:
            events.append((ticks, state))


for song in SONGS:
    score = ROOT / f"assets/music/{song}.avms"
    data = encoder["compile_source"](score.read_text(encoding="utf-8"), score)
    playback = json.dumps(audible_events(data), separators=(",", ":")).encode()
    assert hashlib.sha256(playback).hexdigest() == PLAYBACK_SHA256[song], (
        f"{song}: AVMS must preserve every audible arranged pitch, volume, duty, and event tick"
    )
    header = (ROOT / f"generated/{song}_music.h").read_text(encoding="utf-8")
    words = re.search(rf"MUSIC_{song.upper()}_DATA\[\] = \{{(.*?)\}};", header, re.DOTALL)[1]
    stored = b"".join(int(word, 16).to_bytes(8, "little")
                      for word in re.findall(r"0x([0-9a-f]{16})ULL", words))
    assert stored[:len(data)] == data and not any(stored[len(data):])
    assert data[0] == 15
    notes, controls = [65535] * 4, [127] * 4
    previous = None
    for iteration in range(2):
        offset, ticks, attacks, frames, peak = 1, 0, [0] * 4, [], 0
        while True:
            delta, mask = data[offset:offset + 2]
            offset += 2
            ticks += delta + 1
            if not mask:
                command = data[offset]
                offset += 1
                assert command in (0, 1)
                if command == 1:
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
                    assert controls[channel] & 31 <= GAINS[song][channel]
            peak = max(peak, sum((control & 31) * 2 for note, control in zip(notes, controls) if note))
            frames.append((ticks, tuple(notes), tuple(controls)))
        assert ticks == expected_ticks[song], "preserve complete source song duration"
        assert offset == len(data) and tuple(bool(count) for count in attacks) == AUDIBLE_VOICES[song]
        assert notes == [0] * 4 and all(control & 31 == 0 for control in controls)
        assert 0 < peak <= 127
        if previous is not None:
            assert frames == previous, "repeat without stale state from the previous loop/song"
        previous = frames
    print(f"PASS {song}: {ticks * .004:.3f}s, {len(data)} bytes, "
          f"{sum(AUDIBLE_VOICES[song])} voices, peak {peak}/127, two clean loops; playback preserved")
