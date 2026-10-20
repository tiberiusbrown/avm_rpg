"""Deterministic four-voice arrangements of the three supplied music files.

Only Python's standard library is needed. This is an arranger for these assets,
not a general MIDI synthesizer or a bit-exact NES emulator. See imported/README.md.
"""

from collections import defaultdict
from fractions import Fraction
import math
import struct


IMPORTED_SONGS = {
    "leap": ("leap.mid", "Leap"),
    "boss": ("Boss.ftm", "Boss"),
    "venus": ("Venus.ftm", "Venus"),
}
GAINS = (20, 15, 17, 9)  # Sum 61: preserve headroom at reference master 64.


class Reader:
    def __init__(self, data):
        self.data = data
        self.position = 0

    def take(self, size):
        end = self.position + size
        if end > len(self.data):
            raise ValueError("truncated music source")
        result = self.data[self.position:end]
        self.position = end
        return result

    def unpack(self, fmt):
        return struct.unpack(fmt, self.take(struct.calcsize(fmt)))

    def byte(self):
        return self.take(1)[0]

    def vlq(self):
        value = 0
        for _ in range(4):
            byte = self.byte()
            value = value * 128 + (byte & 127)
            if byte < 128:
                return value
        raise ValueError("invalid MIDI variable-length quantity")


def midi_events(data):
    reader = Reader(data)
    if reader.take(4) != b"MThd" or reader.unpack(">I")[0] != 6:
        raise ValueError("expected standard MIDI header")
    kind, count, division = reader.unpack(">HHH")
    if kind not in (0, 1) or not division or division & 0x8000:
        raise ValueError("expected synchronous MIDI with beat-based timing")
    events = []
    for track in range(count):
        if reader.take(4) != b"MTrk":
            raise ValueError("expected MIDI track")
        part = Reader(reader.take(reader.unpack(">I")[0]))
        tick, running = 0, None
        while part.position < len(part.data):
            tick += part.vlq()
            status = part.byte()
            if status < 128:
                if running is None:
                    raise ValueError("MIDI data without running status")
                part.position -= 1
                status = running
            if status == 0xFF:
                tag = part.byte()
                payload = part.take(part.vlq())
                events.append((tick, track, "meta", tag, payload))
            elif status in (0xF0, 0xF7):
                part.take(part.vlq())
                running = None
            elif 0x80 <= status < 0xF0:
                running = status
                payload = part.take(1 if status >> 4 in (12, 13) else 2)
                events.append((tick, track, status >> 4, status & 15, payload))
            else:
                raise ValueError(f"unsupported MIDI status {status:02x}")
    seconds, last, tempo = Fraction(0), 0, 500000
    result = []
    for tick, track, kind, channel, payload in sorted(events, key=lambda item: item[:2]):
        seconds += Fraction((tick - last) * tempo, division * 1000000)
        last = tick
        if kind == "meta" and channel == 0x51:
            tempo = int.from_bytes(payload, "big")
        elif kind != "meta":
            result.append((seconds, kind, channel, payload))
    return result, seconds


def increment(pitch):
    # AVM A5 stores uint16 phase increments, including fractional-semitone bends.
    return max(1, min(65535, round(440 * 2 ** ((pitch - 69) / 12) * 65536 / 15625)))


def midi_snapshots(data):
    events, duration = midi_events(data)
    active = defaultdict(dict)
    volumes, expression = [100] * 16, [127] * 16
    bends, bend_range = [0] * 16, [2] * 16
    rpn = [[127, 127] for _ in range(16)]
    event_index = 0
    # Melody can pass from guitar to keys/strings; keep bass on its own voice.
    groups = ((0, 14, 2, 3, 4), (2, 3, 4, 15, 8), (6,))
    for tick in range(math.ceil(duration * 250)):
        time = Fraction(tick, 250)
        while event_index < len(events) and events[event_index][0] <= time:
            at, kind, channel, payload = events[event_index]
            event_index += 1
            a = payload[0]
            b = payload[1] if len(payload) == 2 else 0
            if kind == 9 and b:
                active[channel][a] = (b, at)
            elif kind == 8 or (kind == 9 and not b):
                active[channel].pop(a, None)
            elif kind == 11:
                if a == 7:
                    volumes[channel] = b
                elif a == 11:
                    expression[channel] = b
                elif a in (100, 101):
                    rpn[channel][101 - a] = b
                elif a == 6 and rpn[channel] == [0, 0]:
                    bend_range[channel] = b
                elif a in (120, 123):
                    active[channel].clear()
            elif kind == 14:
                bends[channel] = (a + 128 * b - 8192) / 8192 * bend_range[channel]
        snapshot, selected = [], set()
        for voice, group in enumerate(groups):
            candidates = []
            for channel in group:
                candidates = [(pitch, velocity, start, channel)
                              for pitch, (velocity, start) in active[channel].items()
                              if (channel, pitch) not in selected]
                if candidates:
                    break
            if not candidates:
                snapshot.append((0, 0, "triangle"))
                continue
            pitch, velocity, start, channel = (min if voice == 2 else max)(candidates)
            selected.add((channel, pitch))
            age = float(time - start)
            envelope = min(1, (age + .004) / .008) * (0.8 + .2 * max(0, 1 - age / .08))
            level = round(GAINS[voice] * velocity / 127 * volumes[channel] / 127
                          * expression[channel] / 127 * envelope)
            snapshot.append((increment(pitch + bends[channel]), level,
                             "square" if voice == 1 else "triangle"))
        yield time, snapshot


def render_midi(data):
    events, _ = midi_events(data)
    drums = [(time, payload[0], payload[1]) for time, kind, channel, payload in events
             if kind == 9 and channel == 9 and payload[1]]
    index, hits = 0, []
    for time, snapshot in midi_snapshots(data):
        while index < len(drums) and drums[index][0] <= time:
            hits.append(drums[index])
            index += 1
        # MIDI drum note-offs are often extremely short; latch each onset.
        hits = [hit for hit in hits if time - hit[0] < Fraction(12, 100)]
        noise = []
        for at, pitch, velocity in hits:
            length = .045 if pitch in (35, 36, 42, 44) else .12
            level = max(0, 1 - float(time - at) / length)
            noise.append((round(GAINS[3] * velocity / 127 * level),
                          increment(45 if pitch in (35, 36) else 85)))
        level, note = max(noise, default=(0, 0))
        yield time, snapshot + [(note, level, "square")]


def ftm_source(data):
    reader = Reader(data)
    if reader.take(18) != b"FamiTracker Module" or reader.unpack("<I")[0] != 0x440:
        raise ValueError("expected FamiTracker 0.4.x module")
    blocks = {}
    while reader.data[reader.position:reader.position + 3] != b"END":
        name = reader.take(16).rstrip(b"\0").decode("ascii")
        version, size = reader.unpack("<II")
        blocks[name] = (version, reader.take(size))
    for name, version in (("PARAMS", 6), ("HEADER", 3), ("INSTRUMENTS", 6),
                          ("FRAMES", 3), ("PATTERNS", 5)):
        if blocks[name][0] != version:
            raise ValueError(f"unsupported {name} block version")
    params = blocks["PARAMS"][1]
    if params[0] != 1 or struct.unpack_from("<II", params, 1) != (8, 0):
        raise ValueError("arrangements require NTSC APU + VRC6, eight channels")
    if struct.unpack_from("<I", params, 9)[0] != 0:
        raise ValueError("custom FamiTracker engine rates are unsupported")
    header = Reader(blocks["HEADER"][1])
    if header.byte() != 0:
        raise ValueError("arrangements require a single song")
    while header.byte():
        pass
    columns = []
    for channel in range(8):
        if header.byte() != channel:
            raise ValueError("unexpected FamiTracker channel order")
        columns.append(header.byte() + 1)
    instruments = {}
    part = Reader(blocks["INSTRUMENTS"][1])
    for _ in range(part.unpack("<I")[0]):
        index, kind, count = part.unpack("<IBI")
        if kind not in (1, 2) or count != 5:
            raise ValueError("expected APU/VRC6 sequence instrument")
        sequences = [part.unpack("BB") for _ in range(count)]
        if kind == 1:
            part.take(96 * 3)  # DPCM assignment table; percussion is resynthesized.
        name = part.take(part.unpack("<I")[0]).decode("ascii")
        instruments[index] = (kind, sequences, name)
    sequences = {}
    for chip, tag in ((1, "SEQUENCES"), (2, "SEQUENCES_VRC6")):
        version, payload = blocks[tag]
        if version != 6:
            raise ValueError("expected version 6 sequences")
        part, entries = Reader(payload), []
        for _ in range(part.unpack("<I")[0]):
            index, kind, size, loop = part.unpack("<IIBi")
            entries.append((index, kind, loop, part.unpack(f"<{size}b")))
        for index, kind, loop, values in entries:
            release, setting = part.unpack("<iI")
            sequences[chip, index, kind] = (values, loop, release, setting)
    part = Reader(blocks["FRAMES"][1])
    count, speed, tempo, length = part.unpack("<4I")
    frames = [part.take(8) for _ in range(count)]
    patterns = {}
    part = Reader(blocks["PATTERNS"][1])
    while part.position < len(part.data):
        track, channel, pattern, count = part.unpack("<4I")
        if track != 0:
            raise ValueError("unexpected pattern song")
        rows = {}
        for _ in range(count):
            row, note, octave, instrument, volume = part.unpack("<I4B")
            effects = [part.unpack("BB") for _ in range(columns[channel])]
            if any(effect not in (0, 1, 2, 3, 4, 11, 12, 20, 21) for effect, _ in effects):
                raise ValueError("unsupported FamiTracker effect; update the arrangement")
            rows[row] = (note, octave, instrument, volume, effects)
        patterns[channel, pattern] = rows
    return instruments, sequences, frames, patterns, speed, tempo, length


class FtmVoice:
    def __init__(self, channel, instruments, sequences):
        self.channel, self.instruments, self.sequences = channel, instruments, sequences
        self.note, self.instrument, self.volume = None, None, 15
        self.age, self.released, self.offset = 0, False, 0
        self.positions, self.values = [0] * 5, [15, 0, 0, 0, 0]
        self.vibrato, self.tremolo, self.slide = 0, 0, 0
        self.vphase, self.tphase = 0, 0

    def row(self, row):
        note, octave, instrument, volume, effects = row
        if instrument in self.instruments:
            self.instrument = instrument
        if volume < 16:
            self.volume = volume
        if 1 <= note <= 12:
            # FamiTracker's C-0 is MIDI C1 (A-3 = 440 Hz).
            self.note = 24 + octave * 12 + note - 1
            self.age, self.released, self.offset = 0, False, 0
            self.positions, self.values = [0] * 5, [15, 0, 0, 0, 0]
            self.slide = 0
        elif note == 14:
            self.note = None
        elif note == 13:
            self.released = True
            for kind, sequence in self.instrument_sequences():
                if sequence[2] >= 0:
                    self.positions[kind] = sequence[2]
        for effect, parameter in effects:
            if effect == 11:
                self.vibrato = parameter
            elif effect == 12:
                self.tremolo = parameter
            elif effect in (20, 21):
                self.slide = (1 if effect == 20 else -1) * (parameter & 15)
                self.slide_speed = (parameter >> 4) * 2 + 1
                self.slide_progress = 0

    def instrument_sequences(self):
        chip, enabled, _ = self.instruments.get(self.instrument, (1, [], ""))
        return [(kind, self.sequences[chip, index, kind])
                for kind, (on, index) in enumerate(enabled) if on]

    def sample(self):
        if self.note is None:
            return (0, 0, "square")
        arp = 0
        for kind, (values, loop, release, setting) in self.instrument_sequences():
            position = self.positions[kind]
            if position < len(values):
                self.values[kind] = values[position]
                position += 1
                boundary = release + 1 if release >= 0 and not self.released else len(values)
                if position >= boundary:
                    if 0 <= loop < boundary and not self.released:
                        position = loop
                    elif release >= 0 and not self.released:
                        position = boundary - 1
                self.positions[kind] = position
                if kind in (2, 3):
                    self.offset += self.values[kind] * (16 if kind == 3 else 1)
            if kind == 1:
                if setting != 0:
                    raise ValueError("unsupported non-absolute sequence arpeggio")
                arp = self.values[kind]
        self.vphase = (self.vphase + (self.vibrato >> 4)) % 64
        self.tphase = (self.tphase + (self.tremolo >> 4)) % 64
        pitch = self.note + arp
        if self.slide:
            self.slide_progress = min(abs(self.slide), self.slide_progress + self.slide_speed / 16)
            pitch += math.copysign(self.slide_progress, self.slide)
        frequency = 440 * 2 ** ((pitch - 69) / 12)
        divisor = 14 if self.channel == 7 else 16
        period = 1789773 / (divisor * frequency) - 1
        depth = (self.vibrato & 15) ** 2 / 4
        period += self.offset - math.sin(self.vphase * math.tau / 64) * depth
        frequency = 1789773 / (divisor * max(8, period + 1))
        amplitude = self.volume * max(0, self.values[0]) / 225
        amplitude *= 1 - (self.tremolo & 15) / 30 * (1 + math.sin(self.tphase * math.tau / 64)) / 2
        # Map source timbres onto programmable pulse duty thresholds.
        wave = "sawtooth" if self.channel == 7 else "triangle" if self.channel == 2 else "square"
        result = (max(1, min(65535, round(frequency * 65536 / 15625))), amplitude, wave)
        self.age += 1
        return result


def render_ftm(data, song):
    instruments, sequences, frames, patterns, speed, tempo, length = ftm_source(data)
    voices = [FtmVoice(channel, instruments, sequences) for channel in range(8)]
    groups = ((0, 1), (5, 6), (7, 2)) if song == "boss" else ((5, 6), (0, 1), (7, 2))
    frame, row, sample, next_row = 0, 0, 0, Fraction(0)
    seen = set()
    while True:
        if sample >= next_row:
            if (frame, row) in seen:
                break  # One complete traversal, including the Boss introduction.
            seen.add((frame, row))
            jump, skip, halt = None, None, False
            for channel, voice in enumerate(voices):
                value = patterns.get((channel, frames[frame][channel]), {}).get(row)
                if value is None:
                    continue
                voice.row(value)
                for effect, parameter in value[4]:
                    if effect == 1:
                        if parameter < 32:
                            speed = parameter
                        else:
                            tempo = parameter
                    elif effect == 2:
                        jump = parameter
                    elif effect == 3:
                        skip = parameter
                    elif effect == 4:
                        halt = True
            if not speed or not tempo:
                raise ValueError("invalid FamiTracker speed/tempo")
            next_row += Fraction(speed * 150, tempo)
            row += 1
            if row >= length or skip is not None or jump is not None or halt:
                frame = jump if jump is not None else frame + 1
                row = skip if skip is not None else 0
                if halt or frame >= len(frames):
                    # Render the final row before ending the full-song loop.
                    frame = 0
                    row = 0
        rendered = [voice.sample() for voice in voices]
        snapshot = []
        for target, group in enumerate(groups):
            chosen = next((rendered[channel] for channel in group
                           if rendered[channel][1] > 0), (0, 0, "square"))
            pitch, level, wave = chosen
            snapshot.append((pitch, round(GAINS[target] * level), wave))
        pitch, level, wave = rendered[3]
        # DPCM samples become short noise transients on the same percussion voice.
        kick = max(0, 1 - voices[4].age / 3) if voices[4].note is not None else 0
        if kick > level:
            pitch, level = increment(45), kick
        snapshot.append((pitch, round(GAINS[3] * level), "square"))
        yield Fraction(sample, 60), snapshot
        sample += 1


def compile_import(source, encoder):
    data = source.read_bytes()
    song = source.stem.lower()
    if source.suffix.lower() == ".mid":
        snapshots = render_midi(data)
        _, duration = midi_events(data)
    else:
        snapshots = list(render_ftm(data, song))
        duration = Fraction(len(snapshots), 60)
    terminal = round(duration * 250)
    timeline = {}
    for time, snapshot in snapshots:
        position = round(time * 250) + 1
        if position < terminal - 1:
            timeline[position] = snapshot
    # Both sides of the loop boundary are silent, and entry owns every voice.
    timeline[terminal - 1] = [(0, 0, "square")] * 4
    previous_state, previous_tick, events = None, 0, []
    for tick, snapshot in sorted(timeline.items()):
        updates = {}
        for channel, (pitch, volume, wave) in enumerate(snapshot):
            current = (pitch if volume else 0, volume, dict(square=128, triangle=128, sawtooth=64, sine=64)[wave])
            update = {field: value for index, (field, value) in enumerate(zip(
                ("increment", "volume", "duty"), current))
                if previous_state is None or value != previous_state[channel][index]}
            if channel == 3:
                update.pop("duty", None)
            if update:
                updates[channel] = update
        if previous_state is None or updates:
            events.append((tick - previous_tick, updates))
            previous_tick = tick
        previous_state = [(pitch if volume else 0, volume, dict(square=128, triangle=128, sawtooth=64, sine=64)[wave]) for pitch, volume, wave in snapshot]
    return encoder["_encode_events"](15, events, terminal - previous_tick, True)
