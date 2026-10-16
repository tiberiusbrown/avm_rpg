"""Read AVM ELF32 sections without dependencies; fail closed on resource drift."""

import json
import re
import struct
from pathlib import Path


def inspect(elf, link_text):
    data = Path(elf).read_bytes()
    assert data[:6] == b"\x7fELF\x01\x01"
    assert struct.unpack_from("<H", data, 18)[0] == 0x4156, "not EM_AVM"
    offset = struct.unpack_from("<I", data, 32)[0]
    entry, count, strings = struct.unpack_from("<HHH", data, 46)
    sections = [struct.unpack_from("<10I", data, offset + i * entry) for i in range(count)]
    names = sections[strings]
    name_data = data[names[4] : names[4] + names[5]]
    sizes = {name_data[s[0] :].split(b"\0", 1)[0].decode(): s[5] for s in sections}
    stack = re.search(r"(\d+) bytes.*complete bound: (yes|no)", link_text)
    if not stack:
        # Current LLD prints the peak and completeness on separate lines.
        peak = re.search(r"(?:peak|usage)[^\n]*?(\d+) bytes", link_text, re.I)
        bound = re.search(r"complete bound:\s*(yes|no)", link_text)
        assert peak and bound, f"cannot parse stack report:\n{link_text}"
        stack_bytes = int(peak[1])
        complete = bound[1] == "yes"
    else:
        stack_bytes = int(stack[1])
        complete = stack[2] == "yes"
    saved = sizes.get(".saved", 0)
    ordinary = sizes.get(".data", 0)
    assert saved + ordinary <= 1024, "static RAM overflow"
    assert stack_bytes <= 256 and complete, "stack bound exceeds budget or is incomplete"
    assert ".bss" not in sizes or sizes[".bss"] == 0
    result = {
        "saved_bytes": saved,
        "ordinary_data_bytes": ordinary,
        "static_bytes": saved + ordinary,
        "text_bytes": sizes.get(".text", 0),
        "rodata_bytes": sizes.get(".rodata", 0),
        "stack_bytes": stack_bytes,
        "stack_bound_complete": complete,
        "sections": sizes,
    }
    return result


if __name__ == "__main__":
    import sys

    print(json.dumps(inspect(sys.argv[1], Path(sys.argv[2]).read_text()), indent=2))
