"""Validate every original loop, its mixer headroom, and clean song handoffs."""

import argparse
from fractions import Fraction
from pathlib import Path
import re
import runpy
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from generate_music import ORIGINAL_SONGS, SONGS, generate, generate_catalog

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--compiler", type=Path, required=True)
args = parser.parse_args()
encoder = runpy.run_path(str(args.compiler))
header = (ROOT / "generated/music.h").read_text(encoding="utf-8")
assert header == generate(args.compiler), "regenerate music.h"
assert (ROOT / "generated/music_catalog.h").read_text(encoding="utf-8") == generate_catalog()
assert len(SONGS) == 9 and len(set(SONGS)) == 9
assert {path.stem for path in (ROOT / "assets/music").glob("*.avms")} == set(ORIGINAL_SONGS)
platform = (ROOT / "src/platform_avm.cpp").read_text(encoding="utf-8")
master = int(re.search(r"MUSIC_MASTER_VOLUME\s*=\s*(\d+)", platform)[1])

# Start with dirty state to exercise the full first-event reset used during
# song changes. Then carry the ending state through two loops of each song.
notes, controls = [2195, 2765, 3288, 16572], [31] * 4
themes = set()
for song in ORIGINAL_SONGS:
    source = ROOT / f"assets/music/{song}.avms"
    score = encoder["ScoreParser"](source.read_text(encoding="utf-8"), source).parse()
    parts, duration = encoder["score_events"](score)
    assert score.loop and set(parts) == {0, 1, 2, 3}
    bars = duration / (score.meter * 60000 / score.tempo)
    assert bars.denominator == 1 and bars >= 16, "complete musical form and measures"
    assert 80 <= score.tempo <= 150
    assert all(parts[channel] for channel in range(3)), "audible melody, harmony, and bass"
    # Intervals and relative onset spacing distinguish themes even after
    # transposing a melody or changing its tempo.
    theme = tuple((right["pitch"] - left["pitch"],
                   (right["start"] - left["start"]) * score.tempo / 60000)
                  for left, right in zip(parts[0][:12], parts[0][1:12]))
    assert theme not in themes, "each song needs its own melodic/rhythmic identity"
    themes.add(theme)

    words = re.search(rf"{song.upper()}_MUSIC\[\] = \{{(.*?)\}};", header, re.DOTALL)[1]
    data = b"".join(int(value, 16).to_bytes(8, "little")
                    for value in re.findall(r"0x([0-9a-f]{16})ULL", words))
    assert data[:2] == bytes([0xA5, 15]), "every song must own and reset all four voices"
    previous = None
    peak_mix = 0
    reference_peak_mix = 0
    for iteration in range(2):
        offset, ticks = 2, 0
        attacks = [[] for _ in range(4)]
        events = []
        first = True
        polyphony = 0
        while True:
            assert offset + 2 <= len(data), "missing loop terminator"
            delta, mask = data[offset:offset + 2]
            offset += 2
            ticks += delta + 1
            if mask == 0:
                command = data[offset]
                offset += 1
                assert command in (0, 2), "loop without End or unsupported commands"
                if command == 2:
                    break
                continue
            if first:
                assert ticks == 1 and mask == 0xFF, "immediately restore every note and control"
                first = False
            for channel in range(4):
                if mask & (1 << channel):
                    notes[channel] = int.from_bytes(data[offset:offset + 2], "little")
                    offset += 2
                    assert notes[channel] == 0 or 137 <= notes[channel] <= 8286
                    if notes[channel] != 0:
                        attacks[channel].append(notes[channel])
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
            active = [channel for channel in range(4)
                      if notes[channel] != 0 and controls[channel] & 31]
            polyphony = max(polyphony, len([channel for channel in active if channel < 3]))
            peak_mix = max(peak_mix, sum((controls[channel] & 31) * master // 32 for channel in active))
            # Preserve the authored headroom at 200% gain, independently of
            # the game's chosen (potentially clipping) playback boost.
            reference_peak_mix = max(reference_peak_mix,
                                     sum((controls[channel] & 31) * 2 for channel in active))
            events.append((ticks, tuple(notes), tuple(controls)))
        assert ticks == int(duration / 4 + Fraction(1, 2)), "exact written loop length"
        assert attacks == [[encoder["note_increment"](note["pitch"]) for note in parts[channel]] for channel in range(4)]
        assert polyphony == 3, "exercise melody, counterline, and bass together"
        assert notes == [0] * 4 and all(control & 31 == 0 for control in controls), (
            "all release tails must finish before the loop boundary"
        )
        assert len(data) - offset < 8 and not any(data[offset:]), "only word padding after Loop"
        assert 0 < reference_peak_mix <= 127, "preserve authored headroom at master 64"
        if previous is not None:
            assert events == previous, "loop playback must not inherit stale channel state"
        previous = events
    print(f"PASS {song}: {bars} bars, {ticks * 0.004:.3f}s, {offset} bytes, "
          f"{sum(map(len, attacks))} attacks, peak {peak_mix}/127; two loops")
