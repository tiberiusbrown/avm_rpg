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
    return tiles[m.data[y * m.width + x]]["collision"] == 0


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
    "update_wanderer",
}
# Tile semantics from assets/maps/AUTHORING.md. Runtime IDs are atlas cells.
assert tiles[0]["symbol"] == "EMPTY" and tiles[0]["collision"] == 0xFFFF
assert 0 not in m.data, "0x00 is EMPTY and must not be painted"
for index, symbol in (
    (0x06, "GRASS_06"),
    (0x07, "GRASS_07"),
    (0x16, "GRASS_16"),
    (0x17, "GRASS_17"),
    (0x26, "FLOWERS_26"),
    (0x27, "FLOWERS_27"),
):
    assert tiles[index]["symbol"] == symbol and tiles[index]["collision"] == 0
for index, symbol in (
    (0x32, "POT"),
    (0x42, "CHEST_CLOSED"),
    (0x43, "CHEST_OPEN"),
):
    assert tiles[index]["symbol"] == symbol and tiles[index]["collision"] == 0xFFFF
for index, symbol in (
    (0x02, "TREE_ROUND"),
    (0x11, "TREE_PLUS_CENTER"),
    (0x24, "TREE_SPLIT_VERTICAL"),
):
    assert tiles[index]["symbol"] == symbol and tiles[index]["collision"] == 0xFFFF
# TREE_ROUND is for isolated single trees. Groups use the tiling pieces.
round_trees = {
    (x, y) for y in range(m.height) for x in range(m.width) if m.data[y * m.width + x] == 0x02
}
for x, y in round_trees:
    for nx, ny in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
        assert (nx, ny) not in round_trees, f"TREE_ROUND at {(x, y)} touches another"
# Tree seams: each side of a tree piece is empty, H or V. Neighbors must match, and
# nothing may face outside the map. Mirrors assets/maps/AUTHORING.md.
TREE_SEAMS = {
    0x02: (None, None, None, None),
    0x23: ("H", "H", None, None),
    0x24: (None, None, "V", "V"),
    0x03: (None, "H", None, "V"),
    0x04: ("H", None, None, "V"),
    0x13: (None, "H", "V", None),
    0x14: ("H", None, "V", None),
    0x01: (None, None, None, "V"),
    0x10: (None, "H", None, None),
    0x11: ("H", "H", "V", "V"),
    0x12: ("H", None, None, None),
    0x21: (None, None, "V", None),
}
SIDES = {0: (-1, 0), 1: (1, 0), 2: (0, -1), 3: (0, 1)}
OPPOSITE = {0: 1, 1: 0, 2: 3, 3: 2}


def seam(value, side):
    return TREE_SEAMS[value][side] if value in TREE_SEAMS else None


for y in range(m.height):
    for x in range(m.width):
        value = m.data[y * m.width + x]
        for side, (dx, dy) in SIDES.items():
            nx, ny = x + dx, y + dy
            if not (0 <= nx < m.width and 0 <= ny < m.height):
                assert seam(value, side) is None, f"tree seam faces outside at {(x, y)}"
                continue
            other = m.data[ny * m.width + nx]
            assert seam(value, side) == seam(other, OPPOSITE[side]), (
                f"tree seam mismatch at {(x, y)} side {side}"
            )
# Shaped collision: quadrant bits are TL 0-3, TR 4-7, BL 8-11, BR 12-15.
for index, symbol, mask in (
    (0x01, "TREE_PLUS_NORTH", 0xFF00),
    (0x03, "TREE_CLUMP_TOP_LEFT", 0xFFF0),
    (0x13, "TREE_CLUMP_BOTTOM_LEFT", 0xF0FF),
    (0x14, "TREE_CLUMP_BOTTOM_RIGHT", 0x0FFF),
    (0x04, "TREE_CLUMP_TOP_RIGHT", 0xFF0F),
    (0x05, "ATLAS_05", 0xFFFF),
    (0x10, "TREE_PLUS_WEST", 0xF0F0),
    (0x12, "TREE_PLUS_EAST", 0x0F0F),
    (0x23, "TREE_SPLIT_HORIZONTAL", 0xFFFF),
    (0x25, "ATLAS_25", 0xFFFF),
    (0x21, "TREE_PLUS_SOUTH", 0x00FF),
    (0x30, "SWITCH_RIGHT", 0xFF00),
    (0x33, "POT_BROKEN", 0xFF00),
    (0x20, "TREE", 0xFFC6),
    (0x31, "SWITCH_LEFT", 0xFF00),
    (0x90, "ATLAS_90", 0xF660),
    (0x92, "ATLAS_92", 0xCF0C),
    (0x81, "ATLAS_81", 0xC600),
):
    assert tiles[index]["symbol"] == symbol and tiles[index]["collision"] == mask
# Fully walkable atlas cells: no collision at all.
for index, symbol in (
    (0x34, "ATLAS_34"),
    (0x35, "ATLAS_35"),
    (0x36, "ATLAS_36"),
    (0x44, "ATLAS_44"),
    (0x45, "ATLAS_45"),
    (0x46, "ATLAS_46"),
    (0x54, "ATLAS_54"),
    (0x55, "ATLAS_55"),
    (0x56, "ATLAS_56"),
    (0x65, "ATLAS_65"),
    (0x66, "ATLAS_66"),
    (0x75, "ATLAS_75"),
    (0x76, "ATLAS_76"),
):
    assert tiles[index]["symbol"] == symbol and tiles[index]["collision"] == 0
assert {0x06, 0x07, 0x16, 0x17} <= set(m.data), "outdoor floors vary across grass"
for name in ("VillagerHome", "WandererHome", "VillagerWest", "VillagerEast"):
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
