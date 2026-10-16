"""Minimal guest symbol reader, also used to avoid fixed RAM addresses in QA."""

import struct
from pathlib import Path


def symbols(path):
    data = Path(path).read_bytes()
    offset = struct.unpack_from("<I", data, 32)[0]
    stride, count, _ = struct.unpack_from("<HHH", data, 46)
    sections = [struct.unpack_from("<10I", data, offset + i * stride) for i in range(count)]
    result = {}
    for section in sections:
        if section[1] != 2:
            continue
        strings = sections[section[6]]
        names = data[strings[4] : strings[4] + strings[5]]
        for i in range(0, section[5], section[9]):
            name, value, size, info, other, index = struct.unpack_from(
                "<IIIBBH", data, section[4] + i
            )
            text = names[name:].split(b"\0", 1)[0].decode()
            result[text] = {"address": value, "bytes": size, "section": index}
    return result


def symbol_bytes(path, name):
    data = Path(path).read_bytes()
    symbol = symbols(path)[name]
    offset = struct.unpack_from("<I", data, 32)[0]
    stride = struct.unpack_from("<H", data, 46)[0]
    section = struct.unpack_from("<10I", data, offset + symbol["section"] * stride)
    start = section[4] + symbol["address"] - section[3]
    return data[start : start + symbol["bytes"]]
