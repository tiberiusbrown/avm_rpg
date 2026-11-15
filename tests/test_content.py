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
assert set(by_symbol) == {"OUTDOOR", "INTERIORS", "CAVE", "DUNGEON"}
m, interiors = by_symbol["OUTDOOR"], by_symbol["INTERIORS"]
assert m.tileset == "world" and interiors.tileset == "house"
assert by_symbol["CAVE"].tileset == "cave" and by_symbol["DUNGEON"].tileset == "dungeon"
assert len(tiles) == 255 and 255 not in tiles and m.tiles is tiles
start = names[m.spawn]["tx"], names[m.spawn]["ty"]
symbol = {tile["symbol"]: index for index, tile in tiles.items()}
symbol_index = dict(symbol)


def at(x, y):
    return m.data[y * m.width + x]


# Walking onto an entrance teleports, so nothing walks through a cave mouth or a mine door onto
# the ground behind it. The floods below treat those cells as closed.
passages = {
    (point["tx"], point["ty"])
    for o in m.objects
    if o["handler"] == "take_passage"
    for point in o["points"]
}
assert len(passages) == 3


def clear(x, y):
    return tiles[at(x, y)]["collision"] == 0 and (x, y) not in passages


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
unsealed = flood(lambda x, y: at(x, y) not in cliff_outline and (x, y) not in passages)
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
    if (qx // 2, qy // 2) in passages:
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
    "take_passage",
    "read_notice",
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
    (0x02, "TREE_ROUND", 0x93FF), (0x03, "TREE_CLUMP_TOP_LEFT", 0xFFF0),
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
    (0x2F, "WATER_INNER_TOP_LEFT_BOTTOM_RIGHT", 0xFFFF), (0x30, "SWITCH_RIGHT", 0x93C6),
    (0x31, "SWITCH_LEFT", 0x93C6), (0x32, "POT", 0xFFC6),
    (0x33, "POT_BROKEN", 0xFF00), (0x34, "ROCK_SMALL", 0xFF00),
    (0x35, "ROCKS", 0xFFFF), (0x36, "BOULDER", 0xFFFF),
    (0x37, "BOULDER_CRACKED", 0xFFFF), (0x38, "WATER_BOTTOM_LEFT", 0xFFFF),
    (0x39, "WATER_BOTTOM", 0xFFFF), (0x3A, "WATER_BOTTOM_RIGHT", 0xFFFF),
    (0x3B, "WATER_LEFT_INNER_TOP_RIGHT", 0xFFFF), (0x3C, "WATER_LEFT_INNER_BOTTOM_RIGHT", 0xFFFF),
    (0x3D, "WATER_RIGHT_INNER_TOP_LEFT", 0xFFFF), (0x3E, "WATER_RIGHT_INNER_BOTTOM_LEFT", 0xFFFF),
    (0x3F, "WATER_CALM", 0xFFFF), (0x40, "ROAD_NESW_ALT", 0x0000),
    (0x41, "SIGN", 0xFFFF), (0x42, "CHEST_CLOSED", 0xFFC6),
    (0x43, "CHEST_OPEN", 0xFFC6), (0x44, "ROAD_PATCH", 0x0000),
    (0x45, "ROAD_N", 0x0000), (0x46, "ROAD_E", 0x0000),
    (0x47, "ROAD_NE", 0x0000), (0x48, "CLIFF_TOP_LEFT", 0x0FFF),
    (0x49, "CLIFF_TOP", 0x00FF), (0x4A, "CLIFF_TOP_RIGHT", 0xF0FF),
    (0x4B, "CLIFF_INNER_TOP_LEFT", 0x000F), (0x4C, "CLIFF_INNER_TOP_RIGHT", 0x00F0),
    (0x4D, "CLIFF_LEFT_RIGHT", 0xFFFF),
    (0x4E, "ROOF_FLAT_CHIMNEY", 0xFFFF), (0x4F, "ROOF_SLOPE_CHIMNEY", 0xCF0C),
    (0x50, "ROAD_INNER_NW", 0x0000),
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
assert sum(not t["symbol"].startswith("ATLAS_") for t in tiles.values()) == 133
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
# The piles of an east-to-west bridge stand in the water cell below each of its deck cells, and
# nowhere else. They are water: solid, and part of the river for the pieces around them.
PILES = {"BRIDGE_WEST": "WATER_PILES_WEST", "BRIDGE_HORIZONTAL": "WATER_PILES", "BRIDGE_EAST": "WATER_PILES_EAST"}
pile_tiles = {symbol[name] for name in PILES.values()}
assert all(tiles[index]["collision"] == 0xFFFF for index in pile_tiles)
for y in range(m.height):
    for x in range(m.width):
        deck = tiles[at(x, y)]["symbol"]
        if deck in PILES:
            assert at(x, y + 1) == symbol[PILES[deck]], f"no piles under the bridge at {(x, y)}"
        if at(x, y) in pile_tiles:
            above = tiles[at(x, y - 1)]["symbol"]
            assert PILES.get(above) == tiles[at(x, y)]["symbol"], f"piles at {(x, y)} stand under no bridge"
assert pile_tiles & set(m.data)
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

# Animation. Something moves wherever the player goes: every house has an animated light and
# a floor for moths, and outdoors every grass, flowers, water and tree cell takes an ambient effect.
def world_symbol_of(symbol):
    return next(index for index, tile in m.tiles.items() if tile["symbol"] == symbol)


animated_house = {index for index, tile in house.items() if any(rows is not None for rows in tile["phases"])}
for name in doors:
    cells = {cell for cell, owned in owner.items() if owned == name}
    assert any(inside(x, y) in animated_house for x, y in cells), f"{name} has no animated tile"
    assert any(house[inside(x, y)]["ambient"] == "floor" for x, y in room_cells[name]), f"{name} has no bare floor"
ambient_of = {index: tile["ambient"] for index, tile in m.tiles.items()}
# A bird lands on tall solid scenery whose top is near the top of its cell.
PERCHES = {
    "SIGN", "BOULDER_TALL", "ROCK_PILE", "ROCKS", "BOULDER", "BOULDER_CRACKED",
    "BOULDER_WIDE_LEFT", "BOULDER_WIDE_RIGHT",
}
for index, tile in m.tiles.items():
    symbol = tile["symbol"]
    expected = (
        "water" if symbol.startswith("WATER")
        else "grass" if symbol.startswith("GRASS_")
        else "flowers" if symbol.startswith("FLOWERS_")
        else "tree" if symbol.startswith("TREE")
        else "road" if symbol.startswith("ROAD_")
        else "perch" if symbol in PERCHES
        else "steam" if symbol in ("ROOF_FLAT_CHIMNEY", "ROOF_SLOPE_CHIMNEY", "CAMPFIRE")
        else "none"
    )
    assert ambient_of[index] == expected, f"{symbol} has ambient class {ambient_of[index]}"
    if expected == "water":
        assert any(rows is not None for rows in tile["phases"]), f"{symbol} does not animate"
    if expected in ("perch", "tree"):
        assert tile["collision"] != 0, f"{symbol} is walkable, so an actor could stand under its bird"
# The animated scenery tiles. Each is on the map, animates, and blocks as its art suggests.
for symbol, solid in (("TORCH_POST", True), ("CAMPFIRE", True), ("FLAG", True), ("SCARECROW", True), ("REEDS", False)):
    index = world_symbol_of(symbol)
    tile = m.tiles[index]
    assert (tile["collision"] != 0) == solid, symbol
    assert any(rows is not None for rows in tile["phases"]), f"{symbol} does not animate"
    assert index in m.data, f"the map has no {symbol}"
# Reeds grow on the bank: every one touches water.
REEDS = world_symbol_of("REEDS")
for y in range(m.height):
    for x in range(m.width):
        if at(x, y) == REEDS:
            assert any(wet(x + dx, y + dy) for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))), (x, y)
house_ambient = {tile["symbol"]: tile["ambient"] for tile in house.values()}
assert house_ambient["STOVE"] == house_ambient["CAULDRON"] == "steam"
# A chimney replaces a roof piece with the same collision, and every chimney smokes.
world_symbol = {tile["symbol"]: index for index, tile in m.tiles.items()}
for chimney, roof in (("ROOF_FLAT_CHIMNEY", 0xB5), ("ROOF_SLOPE_CHIMNEY", 0x92)):
    tile = m.tiles[world_symbol[chimney]]
    assert tile["collision"] == m.tiles[roof]["collision"]
    assert all(rows is not None for rows in tile["phases"])
    assert world_symbol[chimney] in m.data, f"no house uses {chimney}"

# Interior tileset semantics from assets/maps/AUTHORING_INTERIOR.md, AUTHORING_CAVE.md and
# AUTHORING_DUNGEON.md. The three tilesets share their room pieces, cell for cell.
INTERIOR_TILESETS = {
    "house": {
        "named": 110,
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
        "named": 98,
        "walkable": {
            "BONES", "BRIDGE_HORIZONTAL", "BRIDGE_VERTICAL", "EDGE_LEFT_EXIT",
            "EDGE_LEFT_RIGHT_EXIT", "EDGE_RIGHT_EXIT", "EDGE_TOP_EXIT", "FLOOR_A",
            "FLOOR_B", "FLOOR_C", "FLOOR_MOSS", "GATE_FRONT_OPEN",
            "GATE_SIDE_OPEN", "LADDER_DOWN", "LADDER_UP", "PRESSURE_PLATE",
            "PRESSURE_PLATE_DOWN", "PUDDLE", "RAIL_EAST_NORTH", "RAIL_EAST_SOUTH",
            "RAIL_HORIZONTAL", "RAIL_VERTICAL", "RAIL_WEST_NORTH", "RAIL_WEST_SOUTH",
            "TUNNEL", "VENT",
        },
    },
    "dungeon": {
        "named": 99,
        "walkable": {
            "ARCHWAY", "BARS_DOOR_OPEN", "BLOCK_LOWERED", "BONES",
            "CARPET_BOTTOM", "CARPET_BOTTOM_LEFT", "CARPET_BOTTOM_RIGHT", "CARPET_CENTER",
            "CARPET_LEFT", "CARPET_RIGHT", "CARPET_TOP", "CARPET_TOP_LEFT",
            "CARPET_TOP_RIGHT", "DUNGEON_DOOR_OPEN", "EDGE_LEFT_EXIT", "EDGE_LEFT_RIGHT_EXIT",
            "EDGE_RIGHT_EXIT", "EDGE_TOP_EXIT", "FLOOR_A", "FLOOR_B",
            "FLOOR_CRACKED", "FLOOR_GRATE", "GATE_FRONT_OPEN", "GATE_SIDE_OPEN",
            "LADDER_UP", "PRESSURE_PLATE", "PRESSURE_PLATE_DOWN", "PUDDLE",
            "RUNE", "RUNE_LIT", "SPIKES_DOWN", "STAIRS_DOWN",
            "STAIRS_UP", "TRAPDOOR",
        },
    },
}
SHARED = [
    "EDGE_TOP", "EDGE_LEFT", "EDGE_RIGHT", "EDGE_TOP_LEFT", "EDGE_TOP_RIGHT", "EDGE_LEFT_RIGHT",
    "EDGE_TOP_LEFT_RIGHT", "EDGE_DOT_LEFT", "EDGE_DOT_RIGHT", "EDGE_DOT_BOTH", "EDGE_LEFT_DOT_RIGHT",
    "EDGE_RIGHT_DOT_LEFT", "EDGE_TOP_EXIT", "EDGE_LEFT_EXIT", "EDGE_RIGHT_EXIT",
    "WALL", "WALL_END_LEFT", "WALL_END_RIGHT", "WALL_PILLAR",
]
EXITS = {"EDGE_TOP_EXIT", "EDGE_LEFT_EXIT", "EDGE_RIGHT_EXIT", "EDGE_LEFT_RIGHT_EXIT"}
interior_records = {}
for name, expected in INTERIOR_TILESETS.items():
    records = tileset(ROOT / f"assets/tilesets/{name}.tsx")
    interior_records[name] = records
    assert len(records) == 255 and all(index == tile["runtime"] for index, tile in records.items())
    assert records[0]["symbol"] == "EMPTY" and records[0]["collision"] == 0xFFFF
    masks = {tile["symbol"]: tile["collision"] for tile in records.values()}
    index_of = {tile["symbol"]: index for index, tile in records.items()}
    assert [index_of[piece] for piece in SHARED] == list(range(0x01, 0x14)), name
    assert index_of["EDGE_LEFT_RIGHT_EXIT"] == 0x1C
    assert {s for s, mask in masks.items() if mask == 0} == expected["walkable"], name
    # The room's structure blocks its whole cell. Everything that stands on the floor blocks
    # where its art is, and it always blocks somewhere.
    for piece in SHARED:
        assert masks[piece] == (0 if piece in EXITS else 0xFFFF), (name, piece)
    assert all(s.startswith("ATLAS_") is False or mask == 0xFFFF for s, mask in masks.items())
    assert sum(not s.startswith("ATLAS_") and s != "EMPTY" for s in masks) == expected["named"], name
    assert not any(tile["portal"] for tile in records.values())
    for solid in ("CHEST_CLOSED", "CHEST_OPEN", "POT", "POT_BROKEN", "BARREL", "CRATE"):
        assert masks[solid] != 0, (name, solid)
    # A gate that crosses a passage blocks the whole way across while it is closed.
    if name != "house":
        assert masks["GATE_FRONT"] == 0xFFFF and masks["GATE_SIDE"] == 0x0F0F
        assert masks["GATE_FRONT_OPEN"] == masks["GATE_SIDE_OPEN"] == 0
        assert masks["PRESSURE_PLATE"] == masks["PRESSURE_PLATE_DOWN"] == 0
assert tileset(ROOT / "assets/tilesets/house.tsx") == {t["frame"]: t for t in house.values()}


# Collision shapes. A mask follows its art roughly, as one of a few simple convex shapes: the
# whole cell, a half, a quadrant, the diamond, the cell with two corners cut, or a half diamond.
CONVEX = {
    0xFFFF, 0x00FF, 0xFF00, 0x0F0F, 0xF0F0, 0x000F, 0x00F0, 0x0F00, 0xF000, 0x93C6,
    0xFFC6, 0x93FF, 0xF3F6, 0x9FCF, 0xC600, 0x0093, 0x090C, 0x3060,
}
# Terrain that crosses cell corners is the exception: three quadrants of a tree clump, a cliff
# corner or a fence corner, and the two roof slopes.
TERRAIN_SHAPES = {0xFFF0, 0xFF0F, 0xF0FF, 0x0FFF, 0xF660, 0xCF0C}
world_records = tileset(ROOT / "assets/tilesets/world.tsx")
for name, records in {**interior_records, "world": world_records}.items():
    for tile in records.values():
        allowed = CONVEX | {0} | (TERRAIN_SHAPES if name == "world" else set())
        assert tile["collision"] in allowed, (name, tile["symbol"], hex(tile["collision"]))
# States and pieces of one thing block alike, in every tileset that has them.
ALIKE = (
    ("SWITCH_LEFT", "SWITCH_RIGHT"), ("CHEST_CLOSED", "CHEST_OPEN"), ("BARREL", "CRATE"),
    ("MINECART", "MINECART_FULL"), ("PEDESTAL", "ORB_DARK", "ORB_LIT"), ("STATUE", "STATUE_BROKEN"),
    ("TABLE", "TABLE_SET", "TABLE_CANDLE", "TABLE_LEFT", "TABLE_MIDDLE", "TABLE_RIGHT"),
    ("COUNTER_LEFT", "COUNTER_MIDDLE", "COUNTER_RIGHT", "COUNTER_SINK"),
    ("CHAIR_SOUTH", "CHAIR_NORTH", "CHAIR_EAST", "CHAIR_WEST"),
    ("ROCK_SMALL", "ROCK_FLAT"), ("TORCH_POST", "FLAG"),
    ("PRESSURE_PLATE", "PRESSURE_PLATE_DOWN"),
)
shared_masks = {}
for name, records in {**interior_records, "world": world_records}.items():
    masks = {tile["symbol"]: tile["collision"] for tile in records.values()}
    for group in ALIKE:
        found = {masks[member] for member in group if member in masks}
        assert len(found) <= 1, (name, group)
    for member in ("POT", "POT_BROKEN", "CHEST_CLOSED", "BARREL", "CRATE", "SWITCH_LEFT", "CANDELABRA", "RUBBLE"):
        if member in masks:
            assert shared_masks.setdefault(member, masks[member]) == masks[member], (name, member)
# An obstacle is not fully solid when its art leaves much of the cell bare.
for name, records in interior_records.items():
    masks = {tile["symbol"]: tile["collision"] for tile in records.values()}
    for member in ("POT", "POT_BROKEN", "CHEST_CLOSED", "SWITCH_LEFT", "STOOL", "PLANT", "LANTERN", "BRAZIER", "URN"):
        assert masks.get(member, 1) != 0xFFFF, (name, member)

# No tile holds a block of checkerboard stipple, in any frame: it flickers when the view scrolls
# one pixel at a time. Shade is a second line inside a rim or a few hatch strokes.
from PIL import Image

for path in sorted((ROOT / "assets/images").glob("*.png")):
    if not path.name.startswith(("tiles16", "ambient16")) or "collision" in path.name:
        continue
    pixels = Image.open(path).convert("RGBA")
    data = pixels.load()
    for top in range(0, pixels.height, 16):
        for left in range(0, pixels.width, 16):
            for y in range(top, top + 13):
                for x in range(left, left + 13):
                    first = data[x, y]
                    assert not all(
                        data[x + i, y + j][3] and (data[x + i, y + j][0] > 127) == ((first[0] > 127) ^ ((i + j) & 1 == 1))
                        for j in range(4)
                        for i in range(4)
                    ), f"{path.name}: checkerboard stipple in cell {(left // 16, top // 16)}"

# The cave and the dungeon. Each is one map with its own tileset, built on the room model.
logic = (ROOT / "src/logic/handlers.h").read_text()
WALL_ROWS = {
    "cave": set(range(0x10, 0x20)),
    "dungeon": (set(range(0x10, 0x24)) | set(range(0x40, 0x4A))) - {0x1C, 0x44},
}
EDGE_CELLS = set(range(0x01, 0x10)) | {0x1C}
EDGE_OF = {
    (True, False, False, False, False): "EDGE_TOP",
    (False, True, False, False, False): "EDGE_LEFT",
    (False, False, True, False, False): "EDGE_RIGHT",
    (True, True, False, False, False): "EDGE_TOP_LEFT",
    (True, False, True, False, False): "EDGE_TOP_RIGHT",
    (False, True, True, False, False): "EDGE_LEFT_RIGHT",
    (True, True, True, False, False): "EDGE_TOP_LEFT_RIGHT",
    (False, False, False, True, False): "EDGE_DOT_LEFT",
    (False, False, False, False, True): "EDGE_DOT_RIGHT",
    (False, False, False, True, True): "EDGE_DOT_BOTH",
    (False, True, False, False, True): "EDGE_LEFT_DOT_RIGHT",
    (False, False, True, True, False): "EDGE_RIGHT_DOT_LEFT",
}
# The tile that each handler belongs on, and the handlers whose cells open during play.
HANDLER_TILES = {
    "take_passage": {"EDGE_TOP_EXIT", "LADDER_UP"},
    "read_notice": {"SIGN", "PLAQUE"},
    "use_lever": {"SWITCH_LEFT"},
    "press_plate": {"PRESSURE_PLATE"},
    "open_chest": {"CHEST_CLOSED"},
    "open_door": {"BARS_DOOR"},
    "light_orb": {"ORB_DARK"},
    "steam_vent": {"VENT"},
    "show_state": {
        "GATE_FRONT", "GATE_SIDE", "SPIKES", "BLOCK_RAISED", "RUNE", "PORTCULLIS",
        "CHASM", "CHASM_EDGE", "CHASM_RIM", "CHASM_LEFT", "CHASM_RIGHT",
        "CHASM_EDGE_LEFT", "CHASM_EDGE_RIGHT", "CHASM_RIM_LEFT", "CHASM_RIM_RIGHT",
    },
}
OPENS = {"show_state", "open_door", "steam_vent"}
for map_symbol in ("CAVE", "DUNGEON"):
    area = by_symbol[map_symbol]
    kind = area.tileset
    sheet = area.tiles
    index_of = {tile["symbol"]: index for index, tile in sheet.items()}
    assert 255 not in area.data, f"{map_symbol}: void is painted with EMPTY (0x00)"

    def cell(x, y):
        return area.data[y * area.width + x] if 0 <= x < area.width and 0 <= y < area.height else 0

    def is_room(x, y):
        value = cell(x, y)
        return value != 0 and value not in EDGE_CELLS and value not in WALL_ROWS[kind]

    def wallish(x, y):
        return is_room(x, y) or is_room(x, y + 1)

    faces = {index_of[n] for n in ("WALL", "WALL_END_LEFT", "WALL_END_RIGHT", "WALL_PILLAR")}
    plain = faces | {index_of[n] for n in ("WALL_ALT", "WALL_MOSS")}
    for y in range(area.height):
        for x in range(area.width):
            value, where = cell(x, y), (map_symbol, x, y)
            if is_room(x, y):
                continue
            left, right = is_room(x - 1, y), is_room(x + 1, y)
            if is_room(x, y + 1):
                # The wall row: a face, or a decorated piece that stands clear of room cells.
                assert value in WALL_ROWS[kind], where
                if value in plain:
                    want = "WALL_PILLAR" if left and right else "WALL_END_LEFT" if left else "WALL_END_RIGHT" if right else None
                    assert (value == index_of[want]) if want else (value not in faces or value == index_of["WALL"]), where
                else:
                    assert not left and not right, where
                continue
            top = is_room(x, y - 1)
            band_left, band_right = wallish(x - 1, y), wallish(x + 1, y)
            key = (top, band_left, band_right,
                   not top and not band_left and is_room(x - 1, y - 1),
                   not top and not band_right and is_room(x + 1, y - 1))
            want = EDGE_OF.get(key, "EMPTY")
            if sheet[value]["symbol"] == "EDGE_TOP_EXIT":
                assert want == "EDGE_TOP", where
            else:
                assert value == index_of[want], (where, sheet[value]["symbol"], want)
    # A chasm cell takes the piece that the floor around it selects: a lip on each side where
    # floor lies, and none against a wall or more chasm.
    if kind == "cave":
        chasm_of = {index_of[n]: n for n in index_of if n.startswith("CHASM")}
        floor_at = lambda x, y: is_room(x, y) and cell(x, y) not in chasm_of
        for y in range(area.height):
            for x in range(area.width):
                if cell(x, y) not in chasm_of:
                    continue
                row = "EDGE" if floor_at(x, y - 1) else "RIM" if floor_at(x, y + 1) else ""
                side = "LEFT" if floor_at(x - 1, y) else "RIGHT" if floor_at(x + 1, y) else ""
                want = "_".join(part for part in ("CHASM", row, side) if part)
                assert chasm_of[cell(x, y)] == want, (map_symbol, x, y, want)
        assert all(sheet[index]["collision"] == 0xFFFF for index in chasm_of)
        assert len(chasm_of) == 9
    # Handlers sit on the tiles that show what they do, and every mechanism is in the tables.
    opening = set()
    for o in area.objects:
        if not o["handler"]:
            continue
        assert o["handler"] in HANDLER_TILES, o["handler"]
        for point in o["points"]:
            symbol_here = sheet[cell(point["tx"], point["ty"])]["symbol"]
            assert symbol_here in HANDLER_TILES[o["handler"]], (o["name"], symbol_here)
            if o["handler"] in OPENS:
                opening.add((point["tx"], point["ty"]))
        marker = o["name"] + ("Region" if o["rectangle"] else "")
        if o["handler"] not in ("read_notice", "steam_vent"):
            assert f"positions::{marker}" in logic, f"{o['name']} is missing from the tables in handlers.h"
    # Every walkable cell can be reached once the mechanisms are open. A barred cell that no
    # handler opens is scenery, and the flood passes its door.
    spawn_point = names[area.spawn]
    first = (spawn_point["tx"], spawn_point["ty"])
    assert sheet[cell(*first)]["collision"] == 0
    doors = {index_of[n] for n in ("BARS_DOOR",) if n in index_of}
    reached, queue = {first}, deque([first])
    while queue:
        x, y = queue.popleft()
        for step in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
            value = cell(*step)
            if step not in reached and (sheet[value]["collision"] == 0 or step in opening or value in doors):
                reached.add(step)
                queue.append(step)
    for y in range(area.height):
        for x in range(area.width):
            if sheet[cell(x, y)]["collision"] == 0:
                assert (x, y) in reached, f"{map_symbol}: walkable cell {(x, y)} cannot be reached"
    for o in area.objects:
        for point in o["points"]:
            near = [(point["tx"] + dx, point["ty"] + dy) for dx, dy in ((0, 0), (1, 0), (-1, 0), (0, 1), (0, -1))]
            assert any(step in reached for step in near), f"{o['name']} cannot be reached"
    # Something moves wherever the player stands: every chunk with floor in it holds a tile that
    # takes an ambient effect, and the map uses many animated tiles.
    animated = {index for index, tile in sheet.items() if any(rows is not None for rows in tile["phases"])}
    assert len(animated & set(area.data)) >= 6, f"{map_symbol} uses few animated tiles"
    classes = {sheet[value]["ambient"] for value in area.data}
    assert {"drip", "ember", "crystal", "pool"} <= classes, (map_symbol, classes)
    for cy in range(0, area.height, 4):
        for cx in range(0, area.width, 8):
            chunk = [cell(x, y) for y in range(cy, cy + 4) for x in range(cx, cx + 8)]
            if any(sheet[value]["collision"] == 0 for value in chunk):
                assert any(sheet[value]["ambient"] != "none" for value in chunk), (map_symbol, cx, cy)
    # A light burns in sight of every floor cell: the view is 8x4 tiles around the player.
    lights = {index for index, tile in sheet.items() if tile["ambient"] == "ember"}
    for x, y in reached:
        if sheet[cell(x, y)]["ambient"] in ("cave", "crypt"):
            assert any(cell(x + dx, y + dy) in animated for dx in range(-5, 6) for dy in range(-3, 4)), (
                f"{map_symbol}: no animated tile near {(x, y)}"
            )
    assert lights & set(area.data)
# Both ends of every passage are in the table, and the outdoor ends are the right entrances.
for name, tile_name in (("CaveMouth", "CLIFF_CAVE"), ("MineEntrance", "CLIFF_MINE"), ("CaveHole", "HOLE_LADDER")):
    o = names[name]
    assert o["handler"] == "take_passage" and at(o["tx"], o["ty"]) == symbol_index[tile_name]
    assert (o["tx"], o["ty"] + 1) in seen, f"the ground below {name} is not reachable"
    assert f"positions::{name}" in logic
assert "positions::CaveExit" in logic and "positions::CaveLadder" in logic and "positions::DungeonExit" in logic

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
