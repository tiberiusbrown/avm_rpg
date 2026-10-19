"""Read the supplied MIDI as an independent reference for transcription tests."""

from collections import defaultdict, deque
from pathlib import Path
import struct


def read_reference(path):
    data = Path(path).read_bytes()
    assert data[:4] == b"MThd"
    header_size = int.from_bytes(data[4:8], "big")
    format_id, count, division = struct.unpack_from(">HHH", data, 8)
    assert format_id == 1 and not division & 0x8000
    offset = 8 + header_size
    tracks, tempos = [], []
    for _ in range(count):
        assert data[offset : offset + 4] == b"MTrk"
        size = int.from_bytes(data[offset + 4 : offset + 8], "big")
        track = data[offset + 8 : offset + 8 + size]
        offset += 8 + size
        position, tick, running = 0, 0, None
        active, notes = defaultdict(deque), []
        name = ""

        def vlq():
            nonlocal position
            value = 0
            while True:
                byte = track[position]
                position += 1
                value = value * 128 + (byte & 127)
                if not byte & 128:
                    return value

        while position < size:
            tick += vlq()
            if track[position] & 128:
                status = track[position]
                position += 1
                if status < 240:
                    running = status
            else:
                assert running is not None
                status = running
            if status == 255:
                kind = track[position]
                position += 1
                length = vlq()
                payload = track[position : position + length]
                position += length
                if kind == 3:
                    name = payload.decode("latin1")
                elif kind == 81:
                    tempos.append((tick, int.from_bytes(payload, "big")))
            elif status in (240, 247):
                length = vlq()
                position += length
            else:
                kind, channel = status >> 4, status & 15
                length = 1 if kind in (12, 13) else 2
                payload = track[position : position + length]
                position += length
                if kind not in (8, 9):
                    continue
                pitch, velocity = payload
                if kind == 9 and velocity:
                    active[channel, pitch].append((tick, velocity))
                else:
                    assert active[channel, pitch], "unmatched MIDI note-off"
                    start, attack = active[channel, pitch].popleft()
                    notes.append(dict(start=start, end=tick, pitch=pitch, velocity=attack))
        assert not any(active.values()), "unterminated MIDI note"
        tracks.append(dict(name=name, notes=sorted(notes, key=lambda note: note["start"])))
    assert offset == len(data)
    assert len(tempos) == 1 and tempos[0][0] == 0, "reference has one fixed tempo"
    return division, tempos[0][1], tracks
