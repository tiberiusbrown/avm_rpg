"""Content checks: the outdoor map, the house interiors and the tile semantics that
assets/maps/AUTHORING.md and AUTHORING_INTERIOR.md describe."""

import re
import sys
import xml.etree.ElementTree as ET
from collections import deque
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from generate_assets import load_world, tileset

maps, tiles, names = load_world(ROOT)
by_symbol = {m.symbol: m for m in maps}
assert set(by_symbol) == {"OUTDOOR", "INTERIORS"}
m, interiors = by_symbol["OUTDOOR"], by_symbol["INTERIORS"]
assert m.tileset == "world" and interiors.tileset == "house"
assert len(tiles) == 255 and 255 not in tiles and m.tiles is tiles
start = names[m.spawn]["tx"], names[m.spawn]["ty"]
symbol = {tile["symbol"]: index for index, tile in tiles.items()}


def at(x, y):
    return m.data[y * m.width + x]


def clear(x, y):
    return tiles[at(x, y)]["collision"] == 0


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
unsealed = flood(lambda x, y: at(x, y) not in cliff_outline)
assert all(
    (x, y) in seen
    for y in range(m.height)
    for x in range(m.width)
    if clear(x, y) and (x, y) in unsealed
)
# The border keeps the player off the map edge. Checked per collision quadrant: no
# quadrant on the outermost ring of the map can be reached from the spawn.
def quadrant_open(qx, qy):
    if not (0 <= qx < 2 * m.width and 0 <= qy < 2 * m.height):
        return False
    mask = tiles[at(qx // 2, qy // 2)]["collision"]
    return (mask >> (4 * ((qy % 2) * 2 + qx % 2))) & 0xF != 0xF


quadrants, queue = {(2 * start[0], 2 * start[1] + 1)}, deque([(2 * start[0], 2 * start[1] + 1)])
while queue:
    qx, qy = queue.popleft()
    for nq in ((qx - 1, qy), (qx + 1, qy), (qx, qy - 1), (qx, qy + 1)):
        if nq not in quadrants and quadrant_open(*nq):
            quadrants.add(nq)
            queue.append(nq)
assert not any(
    qx in (0, 2 * m.width - 1) or qy in (0, 2 * m.height - 1) for qx, qy in quadrants
), "the player can reach the edge of the map"
# The border uses several kinds of barrier.
ring = {at(x, y) for x in range(m.width) for y in (0, m.height - 1)} | {
    at(x, y) for y in range(m.height) for x in (0, m.width - 1)
}
ring_symbols = {tiles[index]["symbol"] for index in ring}
for prefix in ("WATER", "CLIFF_", "TREE_", "BOULDER"):
    assert any(name.startswith(prefix) for name in ring_symbols), f"no {prefix} barrier on the border"

sign = names["GroveSign"]
assert at(sign["tx"], sign["ty"]) == 0x41
assert (sign["tx"], sign["ty"] + 1) == start
assert {o["handler"] for o in m.objects if o["handler"]} == {
    "read_sign",
    "update_wanderer",
    "enter_house",
}
assert {o["handler"] for o in interiors.objects if o["handler"]} == {"leave_house"}
# Tile semantics from assets/maps/AUTHORING.md. Runtime IDs are atlas cells.
assert tiles[0]["symbol"] == "EMPTY" and tiles[0]["collision"] == 0xFFFF
assert 0 not in m.data, "0x00 is EMPTY and must not be painted outdoors"
assert 255 not in m.data, "every outdoor cell is painted"
# Every named world tile and its collision mask. Quadrant bits are TL 0-3, TR 4-7,
# BL 8-11, BR 12-15.
for index, name, mask in (
    (0x00, "EMPTY", 0xFFFF), (0x01, "TREE_PLUS_NORTH", 0xFF00),
    (0x02, "TREE_ROUND", 0xFFFF), (0x03, "TREE_CLUMP_TOP_LEFT", 0xFFF0),
    (0x04, "TREE_CLUMP_TOP_RIGHT", 0xFF0F), (0x06, "GRASS_06", 0x0000),
    (0x07, "GRASS_07", 0x0000), (0x08, "GRASS_08", 0x0000),
    (0x09, "GRASS_09", 0x0000), (0x0A, "GRASS_0A", 0x0000),
    (0x0B, "HOLE_LADDER", 0x0000), (0x0C, "ROCK_FLAT", 0xFF00),
    (0x0D, "BOULDER_TALL", 0xFFFF), (0x0E, "ROCK_PILE", 0xFFFF),
    (0x10, "TREE_PLUS_WEST", 0xF0F0), (0x11, "TREE_PLUS_CENTER", 0xFFFF),
    (0x12, "TREE_PLUS_EAST", 0x0F0F), (0x13, "TREE_CLUMP_BOTTOM_LEFT", 0xF0FF),
    (0x14, "TREE_CLUMP_BOTTOM_RIGHT", 0x0FFF), (0x16, "GRASS_16", 0x0000),
    (0x17, "GRASS_17", 0x0000), (0x18, "WATER_TOP_LEFT", 0xFFFF),
    (0x19, "WATER_TOP", 0xFFFF), (0x1A, "WATER_TOP_RIGHT", 0xFFFF),
    (0x1B, "WATER_INNER_BOTTOM_RIGHT", 0xFFFF), (0x1C, "WATER_INNER_BOTTOM_LEFT", 0xFFFF),
    (0x1D, "WATER_TOP_INNER_BOTTOM_LEFT", 0xFFFF), (0x1E, "WATER_TOP_INNER_BOTTOM_RIGHT", 0xFFFF),
    (0x1F, "WATER_INNER_TOP_RIGHT_BOTTOM_LEFT", 0xFFFF), (0x20, "TREE", 0xFFC6),
    (0x21, "TREE_PLUS_SOUTH", 0x00FF), (0x23, "TREE_SPLIT_HORIZONTAL", 0xFFFF),
    (0x24, "TREE_SPLIT_VERTICAL", 0xFFFF), (0x26, "FLOWERS_26", 0x0000),
    (0x27, "FLOWERS_27", 0x0000), (0x28, "WATER_LEFT", 0xFFFF),
    (0x29, "WATER", 0xFFFF), (0x2A, "WATER_RIGHT", 0xFFFF),
    (0x2B, "WATER_INNER_TOP_RIGHT", 0xFFFF), (0x2C, "WATER_INNER_TOP_LEFT", 0xFFFF),
    (0x2D, "WATER_BOTTOM_INNER_TOP_LEFT", 0xFFFF), (0x2E, "WATER_BOTTOM_INNER_TOP_RIGHT", 0xFFFF),
    (0x2F, "WATER_INNER_TOP_LEFT_BOTTOM_RIGHT", 0xFFFF), (0x30, "SWITCH_RIGHT", 0xFF00),
    (0x31, "SWITCH_LEFT", 0xFF00), (0x32, "POT", 0xFFFF),
    (0x33, "POT_BROKEN", 0xFF00), (0x34, "ROCK_SMALL", 0xFF00),
    (0x35, "ROCKS", 0xFFFF), (0x36, "BOULDER", 0xFFFF),
    (0x37, "BOULDER_CRACKED", 0xFFFF), (0x38, "WATER_BOTTOM_LEFT", 0xFFFF),
    (0x39, "WATER_BOTTOM", 0xFFFF), (0x3A, "WATER_BOTTOM_RIGHT", 0xFFFF),
    (0x3B, "WATER_LEFT_INNER_TOP_RIGHT", 0xFFFF), (0x3C, "WATER_LEFT_INNER_BOTTOM_RIGHT", 0xFFFF),
    (0x3D, "WATER_RIGHT_INNER_TOP_LEFT", 0xFFFF), (0x3E, "WATER_RIGHT_INNER_BOTTOM_LEFT", 0xFFFF),
    (0x3F, "WATER_CALM", 0xFFFF), (0x40, "ROAD_NESW_ALT", 0x0000),
    (0x41, "SIGN", 0xFFFF), (0x42, "CHEST_CLOSED", 0xFFFF),
    (0x43, "CHEST_OPEN", 0xFFFF), (0x44, "ROAD_PATCH", 0x0000),
    (0x45, "ROAD_N", 0x0000), (0x46, "ROAD_E", 0x0000),
    (0x47, "ROAD_NE", 0x0000), (0x48, "CLIFF_TOP_LEFT", 0x0FFF),
    (0x49, "CLIFF_TOP", 0x00FF), (0x4A, "CLIFF_TOP_RIGHT", 0xF0FF),
    (0x4B, "CLIFF_INNER_TOP_LEFT", 0x000F), (0x4C, "CLIFF_INNER_TOP_RIGHT", 0x00F0),
    (0x4D, "CLIFF_LEFT_RIGHT", 0xFFFF), (0x50, "ROAD_INNER_NW", 0x0000),
    (0x51, "ROAD_INNER_NE", 0x0000), (0x52, "ROAD_INNER_SW", 0x0000),
    (0x53, "ROAD_INNER_SE", 0x0000), (0x54, "ROAD_S", 0x0000),
    (0x55, "ROAD_NS", 0x0000), (0x56, "ROAD_ES", 0x0000),
    (0x57, "ROAD_NES", 0x0000), (0x58, "CLIFF_LEFT", 0x0F0F),
    (0x59, "GRASS_59", 0x0000), (0x5A, "CLIFF_RIGHT", 0xF0F0),
    (0x5B, "CLIFF_LEFT_INNER_TOP_RIGHT", 0x0FFF), (0x5C, "CLIFF_RIGHT_INNER_TOP_LEFT", 0xF0FF),
    (0x5E, "BOULDER_WIDE_LEFT", 0xFFFF), (0x5F, "BOULDER_WIDE_RIGHT", 0xFFFF),
    (0x60, "BRIDGE_WEST", 0x0000), (0x61, "BRIDGE_HORIZONTAL", 0x0000),
    (0x62, "BRIDGE_EAST", 0x0000), (0x64, "ROAD_W", 0x0000),
    (0x65, "ROAD_NW", 0x0000), (0x66, "ROAD_EW", 0x0000),
    (0x67, "ROAD_NEW", 0x0000), (0x68, "CLIFF_WALL_LEFT", 0xFFFF),
    (0x69, "CLIFF_WALL", 0xFFFF), (0x6A, "CLIFF_WALL_RIGHT", 0xFFFF),
    (0x6B, "CLIFF_WALL_ALT", 0xFFFF), (0x6E, "BOULDER_BIG_TOP_LEFT", 0xFFFF),
    (0x6F, "BOULDER_BIG_TOP_RIGHT", 0xFFFF), (0x70, "BRIDGE_NORTH", 0x0000),
    (0x71, "BRIDGE_VERTICAL", 0x0000), (0x72, "BRIDGE_SOUTH", 0x0000),
    (0x74, "ROAD_SW", 0x0000), (0x75, "ROAD_NSW", 0x0000),
    (0x76, "ROAD_ESW", 0x0000), (0x77, "ROAD_NESW", 0x0000),
    (0x78, "CLIFF_RAMP", 0x0000), (0x79, "CLIFF_CAVE", 0x0000),
    (0x7A, "CLIFF_MINE", 0x0000), (0x7E, "BOULDER_BIG_BOTTOM_LEFT", 0xFFFF),
    (0x7F, "BOULDER_BIG_BOTTOM_RIGHT", 0xFFFF), (0x88, "FENCE_HORIZONTAL", 0xFF00),
    (0x89, "FENCE_VERTICAL", 0x0F0F), (0x8A, "FENCE_END_WEST", 0xF000),
    (0x8B, "FENCE_END_EAST", 0xFF00), (0x8C, "FENCE_END_NORTH", 0x0F00),
    (0x8D, "FENCE_END_SOUTH", 0x0F0F), (0x98, "FENCE_TOP_LEFT", 0xFF00),
    (0x99, "FENCE_TOP_RIGHT", 0x0F00), (0xA8, "FENCE_BOTTOM_LEFT", 0xFF0F),
    (0xA9, "FENCE_BOTTOM_RIGHT", 0x0F0F),
):
    assert tiles[index]["symbol"] == name and tiles[index]["collision"] == mask, name
    assert not tiles[index]["portal"]
assert sum(not t["symbol"].startswith("ATLAS_") for t in tiles.values()) == 123
# Unnamed cells are solid, except the shaped roof pieces.
SHAPED_UNNAMED = {0x81: 0xC600, 0x90: 0xF660, 0x92: 0xCF0C}
for index, tile in tiles.items():
    if tile["symbol"].startswith("ATLAS_"):
        assert tile["collision"] == SHAPED_UNNAMED.get(index, 0xFFFF), tile["symbol"]
# Road pieces: one per set of connected sides, at 0x44 + the side bits (N 1, E 2, S 4, W 8).
for bits in range(16):
    sides = "".join(c for c, bit in zip("NESW", (1, 2, 4, 8)) if bits & bit) or "PATCH"
    assert symbol["ROAD_" + sides] == 0x44 + (bits // 4) * 16 + bits % 4
ROADS = {index for name, index in symbol.items() if name.startswith("ROAD_")}
BRIDGES = {index for name, index in symbol.items() if name.startswith("BRIDGE_")}
WATER = {index for name, index in symbol.items() if name.startswith("WATER")}


def road_like(x, y):
    return 0 <= x < m.width and 0 <= y < m.height and at(x, y) in ROADS | BRIDGES


def wet(x, y):
    # Bridges stand in water, and water continues beyond the map edge.
    return not (0 <= x < m.width and 0 <= y < m.height) or at(x, y) in WATER | BRIDGES


for y in range(m.height):
    for x in range(m.width):
        value = at(x, y)
        if value in ROADS and 0x44 <= value <= 0x77 and value % 16 in (4, 5, 6, 7):
            bits = (value - 0x44) // 16 * 4 + value % 16 - 4
            want = (
                road_like(x, y - 1)
                | road_like(x + 1, y) << 1
                | road_like(x, y + 1) << 2
                | road_like(x - 1, y) << 3
            )
            assert bits == want, f"road piece at {(x, y)} does not match its neighbors"
        if value in WATER | BRIDGES:
            assert any(
                all(wet(x + ox + i, y + oy + j) for i in (0, 1) for j in (0, 1))
                for ox in (-1, 0)
                for oy in (-1, 0)
            ), f"water at {(x, y)} is one tile wide"
        if value in BRIDGES:
            name = tiles[value]["symbol"]
            if name in ("BRIDGE_WEST", "BRIDGE_HORIZONTAL", "BRIDGE_EAST"):
                assert wet(x, y - 1) and wet(x, y + 1), f"bridge at {(x, y)} is not over water"
            else:
                assert wet(x - 1, y) and wet(x + 1, y), f"bridge at {(x, y)} is not over water"
assert BRIDGES & set(m.data) and ROADS & set(m.data) and WATER & set(m.data)
# TREE_ROUND is for isolated single trees. Groups use the tiling pieces.
round_trees = {
    (x, y) for y in range(m.height) for x in range(m.width) if at(x, y) == 0x02
}
for x, y in round_trees:
    for nx, ny in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
        assert (nx, ny) not in round_trees, f"TREE_ROUND at {(x, y)} touches another"
# Tree seams: each side of a tree piece is empty, H or V. Neighbors must match. The map
# edge matches anything, because the forest continues beyond it. Mirrors AUTHORING.md.
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
        value = at(x, y)
        for side, (dx, dy) in SIDES.items():
            nx, ny = x + dx, y + dy
            if not (0 <= nx < m.width and 0 <= ny < m.height):
                continue
            assert seam(value, side) == seam(at(nx, ny), OPPOSITE[side]), (
                f"tree seam mismatch at {(x, y)} side {side}"
            )
assert {0x06, 0x07, 0x16, 0x17} <= set(m.data), "outdoor floors vary across grass"
assert {0x08, 0x09, 0x0A, 0x59} <= set(m.data), "outdoor floors use sparse grass too"
for name in ("VillagerHome", "WandererHome", "VillagerWest", "VillagerEast", "TestField"):
    assert (names[name]["tx"], names[name]["ty"]) in seen

# Houses. Each outdoor door pairs with an interior exit of the same name. The player enters
# by activating the door and leaves by walking onto the exit.
house = interiors.tiles
house_symbol = {tile["symbol"]: index for index, tile in house.items()}


def inside(x, y):
    return interiors.data[y * interiors.width + x]


assert 255 not in interiors.data, "interior void is painted with EMPTY (0x00), never left unpainted"
doors = sorted(name[:-4] for name, o in names.items() if name.endswith("Door") and o["area"] == m.area)
exits = sorted(name[:-4] for name, o in names.items() if name.endswith("Exit") and o["area"] == interiors.area)
assert doors == exits and len(doors) >= 8, (doors, exits)
logic = (ROOT / "src/logic/handlers.h").read_text()
room_cells = {}
for name in doors:
    door, way_out = names[name + "Door"], names[name + "Exit"]
    assert door["handler"] == "enter_house" and way_out["handler"] == "leave_house"
    assert at(door["tx"], door["ty"]) in (0x84, 0x85), f"{name}Door is not on a door tile"
    assert (door["tx"], door["ty"] + 1) in seen, f"the ground in front of {name}Door is not reachable"
    assert inside(way_out["tx"], way_out["ty"]) == house_symbol["EDGE_TOP_EXIT"]
    assert house[inside(way_out["tx"], way_out["ty"] - 1)]["collision"] == 0
    assert f"positions::{name}Door, positions::{name}Exit" in logic, f"{name} is missing from HOUSE_LINKS"
    # The room: every walkable cell joined to the exit.
    room, queue = set(), deque([(way_out["tx"], way_out["ty"])])
    while queue:
        x, y = queue.popleft()
        if (x, y) in room or not (0 <= x < interiors.width and 0 <= y < interiors.height):
            continue
        if house[inside(x, y)]["collision"] == 0xFFFF:
            continue
        room.add((x, y))
        queue.extend(((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)))
    room_cells[name] = room
assert len({frozenset(room) for room in room_cells.values()}) == len(doors), "two houses share a room"
# No other interior is on screen from anywhere the player can stand. The view is 8x4
# tiles around the player, and up to 9x5 when the camera is between tiles.
painted = {
    (x, y)
    for y in range(interiors.height)
    for x in range(interiors.width)
    if inside(x, y) != 0
}
owner = {}
for name, room in room_cells.items():
    queue = deque(room)
    while queue:
        x, y = queue.popleft()
        if (x, y) in owner or (x, y) not in painted:
            continue
        owner[(x, y)] = name
        queue.extend((x + dx, y + dy) for dx in (-1, 0, 1) for dy in (-1, 0, 1))
assert set(owner) == painted, "an interior cell belongs to no house"
for name, room in room_cells.items():
    for x, y in room:
        for dx in range(-5, 6):
            for dy in range(-3, 4):
                assert owner.get((x + dx, y + dy), name) == name, f"{name} can see another interior"
spawn = names[interiors.spawn]
assert house[inside(spawn["tx"], spawn["ty"])]["collision"] == 0

# Interior tileset semantics from assets/maps/AUTHORING_INTERIOR.md. Only the house tileset
# is used by a map so far. The three tilesets share their room pieces, cell for cell.
INTERIOR_TILESETS = {
    "house": {
        "named": 110,
        "bottom_half": {"POT_BROKEN"},
        "walkable": {
            "ARCHWAY", "DOORMAT", "DOOR_OPEN", "EDGE_LEFT_EXIT",
            "EDGE_LEFT_RIGHT_EXIT", "EDGE_RIGHT_EXIT", "EDGE_TOP_EXIT", "FLOOR_CHECKER",
            "FLOOR_PLAIN_A", "FLOOR_PLAIN_B", "FLOOR_STONE_A", "FLOOR_STONE_B",
            "FLOOR_TILE_A", "FLOOR_TILE_B", "FLOOR_WOOD_A", "FLOOR_WOOD_B",
            "FLOOR_WOOD_C", "FLOOR_WOOD_D", "LADDER_UP", "RUG_BOTTOM",
            "RUG_BOTTOM_LEFT", "RUG_BOTTOM_RIGHT", "RUG_CENTER", "RUG_CENTER_PLAIN",
            "RUG_LEFT", "RUG_RIGHT", "RUG_SMALL", "RUG_TOP",
            "RUG_TOP_LEFT", "RUG_TOP_RIGHT", "STAIRS_DOWN", "STAIRS_UP",
            "TRAPDOOR",
        },
    },
    "cave": {
        "named": 65,
        "bottom_half": {"POT_BROKEN"},
        "walkable": {
            "BONES", "EDGE_LEFT_EXIT", "EDGE_LEFT_RIGHT_EXIT", "EDGE_RIGHT_EXIT",
            "EDGE_TOP_EXIT", "FLOOR_A", "FLOOR_B", "FLOOR_C",
            "LADDER_DOWN", "LADDER_UP", "PUDDLE", "RAIL_EAST_NORTH",
            "RAIL_EAST_SOUTH", "RAIL_HORIZONTAL", "RAIL_VERTICAL", "RAIL_WEST_NORTH",
            "RAIL_WEST_SOUTH", "TUNNEL",
        },
    },
    "dungeon": {
        "named": 62,
        "bottom_half": {"POT_BROKEN", "SWITCH_LEFT", "SWITCH_RIGHT"},
        "walkable": {
            "ARCHWAY", "BARS_DOOR_OPEN", "BONES", "DUNGEON_DOOR_OPEN",
            "EDGE_LEFT_EXIT", "EDGE_LEFT_RIGHT_EXIT", "EDGE_RIGHT_EXIT", "EDGE_TOP_EXIT",
            "FLOOR_A", "FLOOR_B", "FLOOR_CRACKED", "FLOOR_GRATE",
            "LADDER_UP", "PRESSURE_PLATE", "PUDDLE", "SPIKES_DOWN",
            "STAIRS_DOWN", "STAIRS_UP", "TRAPDOOR",
        },
    },
}
SHARED = [
    "EDGE_TOP", "EDGE_LEFT", "EDGE_RIGHT", "EDGE_TOP_LEFT", "EDGE_TOP_RIGHT", "EDGE_LEFT_RIGHT",
    "EDGE_TOP_LEFT_RIGHT", "EDGE_DOT_LEFT", "EDGE_DOT_RIGHT", "EDGE_DOT_BOTH", "EDGE_LEFT_DOT_RIGHT",
    "EDGE_RIGHT_DOT_LEFT", "EDGE_TOP_EXIT", "EDGE_LEFT_EXIT", "EDGE_RIGHT_EXIT",
    "WALL", "WALL_END_LEFT", "WALL_END_RIGHT", "WALL_PILLAR",
]
for name, expected in INTERIOR_TILESETS.items():
    records = tileset(ROOT / f"assets/tilesets/{name}.tsx")
    assert len(records) == 255 and all(index == tile["runtime"] for index, tile in records.items())
    assert records[0]["symbol"] == "EMPTY" and records[0]["collision"] == 0xFFFF
    masks = {tile["symbol"]: tile["collision"] for tile in records.values()}
    index_of = {tile["symbol"]: index for index, tile in records.items()}
    assert [index_of[piece] for piece in SHARED] == list(range(0x01, 0x14)), name
    assert index_of["EDGE_LEFT_RIGHT_EXIT"] == 0x1C
    assert {s for s, mask in masks.items() if mask == 0} == expected["walkable"], name
    assert {s for s, mask in masks.items() if mask == 0xFF00} == expected["bottom_half"], name
    assert all(mask in (0, 0xFF00, 0xFFFF) for mask in masks.values())
    assert sum(not s.startswith("ATLAS_") and s != "EMPTY" for s in masks) == expected["named"], name
    assert not any(tile["portal"] for tile in records.values())
    for chest in ("CHEST_CLOSED", "CHEST_OPEN", "POT"):
        assert masks[chest] == 0xFFFF
assert tileset(ROOT / "assets/tilesets/house.tsx") == {t["frame"]: t for t in house.values()}

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
    "PASS connected village, sealed border, linked houses, tile semantics and no guest allocations"
)
