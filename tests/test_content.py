"""Connected sample map, sign access and fixed-capacity guest checks."""

import re
import sys
import xml.etree.ElementTree as ET
from collections import deque
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from generate_assets import load_world, tileset

maps, tiles, names = load_world(ROOT)
assert len(maps) == 1 and len(tiles) == 255 and 255 not in tiles
m = maps[0]
start = names[m.spawn]["tx"], names[m.spawn]["ty"]


def clear(x, y):
    return tiles[m.data[y * m.width + x]]["collision"] == 0


def flood(passable):
    found, queue = {start}, deque([start])
    while queue:
        x, y = queue.popleft()
        for nx, ny in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
            if (
                0 <= nx < m.width
                and 0 <= ny < m.height
                and passable(nx, ny)
                and (nx, ny) not in found
            ):
                found.add((nx, ny))
                queue.append((nx, ny))
    return found


seen = flood(clear)
# Cliff pieces seal ground off on purpose: a raised area with no ramp is scenery. Ground
# behind an unbroken outline of cliff pieces is exempt; all other floor must be reachable.
cliff_outline = {
    index
    for index, tile in tiles.items()
    if tile["symbol"].startswith("CLIFF_") and tile["collision"]
}
unsealed = flood(lambda x, y: m.data[y * m.width + x] not in cliff_outline)
assert all(
    (x, y) in seen
    for y in range(m.height)
    for x in range(m.width)
    if clear(x, y) and (x, y) in unsealed
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
# Sparse grass, water, raised areas, fences and entrances. The four names are halves.
TOP, BOTTOM, LEFT, RIGHT = 0x00FF, 0xFF00, 0x0F0F, 0xF0F0
for index, symbol, mask in (
    (0x08, "GRASS_08", 0),
    (0x09, "GRASS_09", 0),
    (0x0A, "GRASS_0A", 0),
    (0x59, "GRASS_59", 0),
    (0x0B, "HOLE_LADDER", 0),
    (0x18, "WATER_TOP_LEFT", 0xFFFF),
    (0x19, "WATER_TOP", 0xFFFF),
    (0x1A, "WATER_TOP_RIGHT", 0xFFFF),
    (0x28, "WATER_LEFT", 0xFFFF),
    (0x29, "WATER", 0xFFFF),
    (0x2A, "WATER_RIGHT", 0xFFFF),
    (0x38, "WATER_BOTTOM_LEFT", 0xFFFF),
    (0x39, "WATER_BOTTOM", 0xFFFF),
    (0x3A, "WATER_BOTTOM_RIGHT", 0xFFFF),
    (0x1B, "WATER_INNER_BOTTOM_RIGHT", 0xFFFF),
    (0x1C, "WATER_INNER_BOTTOM_LEFT", 0xFFFF),
    (0x2B, "WATER_INNER_TOP_RIGHT", 0xFFFF),
    (0x2C, "WATER_INNER_TOP_LEFT", 0xFFFF),
    (0x1D, "WATER_TOP_INNER_BOTTOM_LEFT", 0xFFFF),
    (0x1E, "WATER_TOP_INNER_BOTTOM_RIGHT", 0xFFFF),
    (0x2D, "WATER_BOTTOM_INNER_TOP_LEFT", 0xFFFF),
    (0x2E, "WATER_BOTTOM_INNER_TOP_RIGHT", 0xFFFF),
    (0x3B, "WATER_LEFT_INNER_TOP_RIGHT", 0xFFFF),
    (0x3C, "WATER_LEFT_INNER_BOTTOM_RIGHT", 0xFFFF),
    (0x3D, "WATER_RIGHT_INNER_TOP_LEFT", 0xFFFF),
    (0x3E, "WATER_RIGHT_INNER_BOTTOM_LEFT", 0xFFFF),
    (0x1F, "WATER_INNER_TOP_RIGHT_BOTTOM_LEFT", 0xFFFF),
    (0x2F, "WATER_INNER_TOP_LEFT_BOTTOM_RIGHT", 0xFFFF),
    (0x3F, "WATER_CALM", 0xFFFF),
    (0x48, "CLIFF_TOP_LEFT", TOP | LEFT),
    (0x49, "CLIFF_TOP", TOP),
    (0x4A, "CLIFF_TOP_RIGHT", TOP | RIGHT),
    (0x58, "CLIFF_LEFT", LEFT),
    (0x5A, "CLIFF_RIGHT", RIGHT),
    (0x68, "CLIFF_WALL_LEFT", 0xFFFF),
    (0x69, "CLIFF_WALL", 0xFFFF),
    (0x6A, "CLIFF_WALL_RIGHT", 0xFFFF),
    (0x6B, "CLIFF_WALL_ALT", 0xFFFF),
    (0x4B, "CLIFF_INNER_TOP_LEFT", 0x000F),
    (0x4C, "CLIFF_INNER_TOP_RIGHT", 0x00F0),
    (0x4D, "CLIFF_LEFT_RIGHT", 0xFFFF),
    (0x5B, "CLIFF_LEFT_INNER_TOP_RIGHT", LEFT | 0x00F0),
    (0x5C, "CLIFF_RIGHT_INNER_TOP_LEFT", RIGHT | 0x000F),
    (0x78, "CLIFF_RAMP", 0),
    (0x79, "CLIFF_CAVE", 0),
    (0x7A, "CLIFF_MINE", 0),
    (0x88, "FENCE_HORIZONTAL", BOTTOM),
    (0x89, "FENCE_VERTICAL", LEFT),
    (0x8A, "FENCE_END_WEST", 0xF000),
    (0x8B, "FENCE_END_EAST", BOTTOM),
    (0x8C, "FENCE_END_NORTH", 0x0F00),
    (0x8D, "FENCE_END_SOUTH", LEFT),
    (0x98, "FENCE_TOP_LEFT", BOTTOM),
    (0x99, "FENCE_TOP_RIGHT", 0x0F00),
    (0xA8, "FENCE_BOTTOM_LEFT", BOTTOM | LEFT),
    (0xA9, "FENCE_BOTTOM_RIGHT", LEFT),
):
    assert tiles[index]["symbol"] == symbol and tiles[index]["collision"] == mask
    assert not tiles[index]["portal"]
assert {0x06, 0x07, 0x16, 0x17} <= set(m.data), "outdoor floors vary across grass"
# Interior tileset semantics from assets/maps/AUTHORING_INTERIOR.md. No map uses it yet.
interior = tileset(ROOT / "assets/tilesets/interior.tsx")
assert len(interior) == 255 and all(index == tile["runtime"] for index, tile in interior.items())
assert interior[0]["symbol"] == "EMPTY" and interior[0]["collision"] == 0xFFFF
INTERIOR_WALKABLE = {
    "EDGE_TOP_DOOR", "EDGE_LEFT_DOOR", "EDGE_RIGHT_DOOR", "EDGE_LEFT_RIGHT_DOOR",
    "DOOR_OPEN", "ARCHWAY", "STAIRS_UP", "LADDER_UP",
    "FLOOR_WOOD_A", "FLOOR_WOOD_B", "FLOOR_WOOD_C", "FLOOR_TILE_A",
    "FLOOR_TILE_B", "FLOOR_CHECKER", "FLOOR_STONE_A", "FLOOR_STONE_B",
    "FLOOR_PLAIN_A", "FLOOR_PLAIN_B", "STAIRS_DOWN", "TRAPDOOR",
    "DOORMAT", "RUG_SMALL", "RUG_TOP_LEFT", "RUG_TOP",
    "RUG_TOP_RIGHT", "RUG_CENTER_PLAIN", "RUG_LEFT", "RUG_CENTER",
    "RUG_RIGHT", "RUG_BOTTOM_LEFT", "RUG_BOTTOM", "RUG_BOTTOM_RIGHT",
}
INTERIOR_SOLID = {
    "EDGE_TOP", "EDGE_LEFT", "EDGE_RIGHT", "EDGE_TOP_LEFT",
    "EDGE_TOP_RIGHT", "EDGE_LEFT_RIGHT", "EDGE_TOP_LEFT_RIGHT", "EDGE_DOT_LEFT",
    "EDGE_DOT_RIGHT", "EDGE_DOT_BOTH", "EDGE_LEFT_DOT_RIGHT", "EDGE_RIGHT_DOT_LEFT",
    "WALL", "WALL_END_LEFT", "WALL_END_RIGHT", "WALL_PILLAR",
    "WALL_WOOD", "WALL_WOOD_END_LEFT", "WALL_WOOD_END_RIGHT", "WALL_WOOD_PILLAR",
    "WALL_STONE", "WALL_STONE_END_LEFT", "WALL_STONE_END_RIGHT", "WALL_STONE_PILLAR",
    "WINDOW", "WINDOW_CURTAINS", "PAINTING_LANDSCAPE", "PAINTING_PORTRAIT",
    "SCONCE", "MIRROR", "CLOCK", "BANNER",
    "SHELF_BOTTLES", "WEAPONS", "TROPHY", "HANGING_PANS",
    "DOOR_CLOSED", "FIREPLACE", "FIREPLACE_LIT", "BOOKCASE",
    "BOOKCASE_SPARSE", "CUPBOARD", "WARDROBE", "TABLE",
    "TABLE_LEFT", "TABLE_MIDDLE", "TABLE_RIGHT", "TABLE_SET",
    "TABLE_CANDLE", "CHAIR_SOUTH", "CHAIR_NORTH", "CHAIR_WEST",
    "CHAIR_EAST", "STOOL", "BENCH", "BED_HEAD",
    "DRESSER", "CABINET", "SHELF_LOW", "STOVE",
    "COUNTER_LEFT", "COUNTER_MIDDLE", "COUNTER_RIGHT", "COUNTER_SINK",
    "BARREL", "CRATE", "SACK", "BED_FOOT",
    "PLANT", "VASE", "CANDELABRA", "POT",
    "CHEST_CLOSED", "CHEST_OPEN", "BUCKET", "CAULDRON",
}
interior_masks = {tile["symbol"]: tile["collision"] for tile in interior.values()}
assert all(interior_masks[symbol] == 0 for symbol in INTERIOR_WALKABLE)
assert all(interior_masks[symbol] == 0xFFFF for symbol in INTERIOR_SOLID)
assert interior_masks["POT_BROKEN"] == 0xFF00
# Every other cell is unnamed and solid, and no interior tile is a portal.
assert all(
    mask == 0xFFFF or symbol in INTERIOR_WALKABLE | {"POT_BROKEN"}
    for symbol, mask in interior_masks.items()
)
assert not any(tile["portal"] for tile in interior.values())
assert len(INTERIOR_WALKABLE) == 32 and len(INTERIOR_SOLID) == 76
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
