"""Connected sample map, sign access and fixed-capacity guest checks."""

import re
import sys
import xml.etree.ElementTree as ET
from collections import deque
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from generate_assets import load_world

maps, tiles, names = load_world(ROOT)
assert len(maps) == 1 and len(tiles) == 255 and 255 not in tiles
m = maps[0]
start = names[m.spawn]["tx"], names[m.spawn]["ty"]


def clear(x, y):
    return not tiles[m.data[y * m.width + x]]["solid"]


seen, queue = {start}, deque([start])
while queue:
    x, y = queue.popleft()
    for nx, ny in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
        if (
            0 <= nx < m.width
            and 0 <= ny < m.height
            and clear(nx, ny)
            and (nx, ny) not in seen
        ):
            seen.add((nx, ny))
            queue.append((nx, ny))
assert all(
    (x, y) in seen for y in range(m.height) for x in range(m.width) if clear(x, y)
)
sign = names["GroveSign"]
assert m.data[sign["ty"] * m.width + sign["tx"]] == 0x41
assert (sign["tx"], sign["ty"] + 1) == start
assert {o["handler"] for o in m.objects if o["handler"]} == {
    "read_sign",
    "update_villager",
    "update_wanderer",
}
for name in ("VillagerHome", "WandererHome"):
    assert (names[name]["tx"], names[name]["ty"]) in seen
for path in (ROOT / "assets/maps").glob("*.tmx"):
    node = ET.parse(path).getroot()
    ids = [int(o.get("id")) for o in node.findall(".//object")]
    assert (
        len(ids) == len(set(ids))
        and min(ids) > 0
        and int(node.get("nextobjectid")) > max(ids)
    )
for path in (ROOT / "src").rglob("*.cpp"):
    assert not re.search(r"\b(malloc|calloc|realloc|free)\s*\(", path.read_text())
npc_core = (ROOT / "src/npc.cpp").read_text()
assert not re.search(
    r"\bFLAG_\w+|\bNPC_(?:VILLAGER|WANDERER)\b|\bpositions::|\broutes::", npc_core
)
assert "#define PROGMEM" in (ROOT / "src/rpg.h").read_text()
print(
    "PASS connected grove, accessible sign/NPCs, reserved atlas cell and no guest allocations"
)
