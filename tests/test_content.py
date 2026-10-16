"""Offline reachability, portal, program-memory, and allocation checks."""

from collections import deque
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
for name, start in [("outdoor", (4, 5)), ("interior", (2, 7))]:
    rows = (ROOT / "assets_src" / f"{name}.txt").read_text().splitlines()
    seen = {start}
    queue = deque([start])
    while queue:
        x, y = queue.popleft()
        for nx, ny in [(x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)]:
            if (
                0 <= ny < len(rows)
                and 0 <= nx < len(rows[0])
                and rows[ny][nx] not in "#~"
                and (nx, ny) not in seen
            ):
                seen.add((nx, ny))
                queue.append((nx, ny))
    portals = [(x, y) for y, row in enumerate(rows) for x, c in enumerate(row) if c == "D"]
    assert len(portals) == 1 and portals[0] in seen
    assert all(
        (x, y) in seen for y, row in enumerate(rows) for x, c in enumerate(row) if c not in "#~"
    ), f"{name} contains unreachable floor"
for path in (ROOT / "src").glob("*.cpp"):
    text = path.read_text()
    assert not re.search(r"\b(malloc|calloc|realloc|free)\s*\(", text)
header = (ROOT / "src/rpg.h").read_text()
assert "#define PROGMEM" in header and "#define F(text)" in header
assert "Game*" not in header and "Game const*" not in header
print("PASS connected maps, paired accessible portals, no guest allocations, PROGMEM mapping")
