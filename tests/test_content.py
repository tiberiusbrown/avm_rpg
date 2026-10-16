"""Offline reachability, portal, program-memory, and allocation checks."""

from collections import deque
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
import sys
sys.path.insert(0, str(ROOT / "tools"))
from generate_assets import load_world
maps, tiles, names = load_world(ROOT)
for area, start in [(1, (4, 5)), (2, (2, 7)), (3, (2, 3))]:
    m = next(m for m in maps if m.area == area)
    def clear(x, y):
        tile = m.data[y * m.width + x]
        return tile in tiles and not tiles[tile]["solid"]
    seen, queue = {start}, deque([start])
    while queue:
        x, y = queue.popleft()
        for nx, ny in [(x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)]:
            if 0 <= nx < m.width and 0 <= ny < m.height and clear(nx, ny) and (nx, ny) not in seen:
                seen.add((nx, ny))
                queue.append((nx, ny))
    portals = [o for o in m.objects if o["name"] == {1: "HouseDoor", 2: "InteriorExit", 3: "HallExit"}[area]]
    assert len(portals) == 1 and (portals[0]["tx"], portals[0]["ty"]) in seen
    assert all((x, y) in seen for y in range(m.height) for x in range(m.width) if clear(x, y))
# Valid Tiled editor identity metadata is independent of essential NPC identity.
import xml.etree.ElementTree as ET
for path in (ROOT / 'assets/maps').glob('*.tmx'):
    node = ET.parse(path).getroot()
    ids = [int(o.get('id')) for o in node.findall('.//object')]
    assert len(ids) == len(set(ids)) and min(ids) > 0
    assert int(node.get('nextobjectid')) > max(ids)
for path in (ROOT / "src").rglob("*.cpp"):
    text = path.read_text()
    assert not re.search(r"\b(malloc|calloc|realloc|free)\s*\(", text)
# NPC mechanics must not acquire character identities, quest flags or authored routes.
npc_core = (ROOT / "src/npc.cpp").read_text()
assert not re.search(r"\bFLAG_\w+|\bNPC_(?:GUIDE|ROWAN|GUARD|COURIER|HALL_\d+)\b|\bpositions::|\broutes::", npc_core)
header = (ROOT / "src/rpg.h").read_text()
assert "#define PROGMEM" in header and "#define F(text)" in header
assert "Game*" not in header and "Game const*" not in header
print("PASS connected maps, paired accessible portals, no guest allocations, PROGMEM mapping")
