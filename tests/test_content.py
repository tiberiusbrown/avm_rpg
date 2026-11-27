"""Content checks: the village and forest clearing, the house interiors, the boulder gate and
the tile semantics that assets/maps/AUTHORING.md and AUTHORING_INTERIOR.md describe."""

import re
import sys
import xml.etree.ElementTree as ET
from collections import deque
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from generate_assets import (CHUNK_HEIGHT, CHUNK_WIDTH, collision_pixel_solid, load_world,
                             static_npc_frames, tileset)

maps, tiles, names = load_world(ROOT)
by_symbol = {m.symbol: m for m in maps}
assert set(by_symbol) == {"OUTDOOR", "INTERIORS"}
m, interiors = by_symbol["OUTDOOR"], by_symbol["INTERIORS"]
assert m.tileset == "world" and interiors.tileset == "house"
assert len(tiles) == 255 and 255 not in tiles and m.tiles is tiles
start = names[m.spawn]["tx"], names[m.spawn]["ty"]
symbol = {tile["symbol"]: index for index, tile in tiles.items()}
symbol_index = dict(symbol)


def at(x, y):
    return m.data[y * m.width + x]


def position(name):
    return names[name]["tx"], names[name]["ty"]


def cells_of(name):
    return {(point["tx"], point["ty"]) for point in names[name]["points"]}


def adjacent(cell):
    x, y = cell
    return ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1))


class Ground:
    """Pixel-level walking over one map with the player's 8x8 foot box at (4, 8)."""

    def __init__(self, area, blocked=(), bodies=()):
        self.width, self.height = area.width * 16, area.height * 16
        self.solid = bytearray(self.width * self.height)
        shapes = {}
        for ty in range(area.height):
            for tx in range(area.width):
                value = area.data[ty * area.width + tx]
                mask = 0xFFFF if (tx, ty) in blocked or value == 255 else area.tiles[value]["collision"]
                if not mask:
                    continue
                if mask not in shapes:
                    shapes[mask] = [
                        bytes(1 if mask == 0xFFFF or collision_pixel_solid(mask, x, y) else 0 for x in range(16))
                        for y in range(16)
                    ]
                for y, row in enumerate(shapes[mask]):
                    offset = (ty * 16 + y) * self.width + tx * 16
                    self.solid[offset:offset + 16] = row
        # Other actors, such as Bram, block with their own foot boxes at their sprite origins.
        self.bodies = [(x * 16, y * 16) for x, y in bodies]

    def clear(self, x, y):
        if not (0 <= x <= self.width - 16 and 0 <= y <= self.height - 16):
            return False
        for row in range(y + 8, y + 16):
            if 1 in self.solid[row * self.width + x + 4:row * self.width + x + 12]:
                return False
        return all(not (x < bx + 8 and bx < x + 8 and y < by + 8 and by < y + 8) for bx, by in self.bodies)

    def walk(self, cell):
        """Every sprite position that the player can reach from standing on a tile."""
        first = (cell[0] * 16, cell[1] * 16)
        assert self.clear(*first), cell
        seen, queue = {first}, deque([first])
        while queue:
            x, y = queue.popleft()
            for step in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                if step not in seen and self.clear(*step):
                    seen.add(step)
                    queue.append(step)
        return seen


def standing_tiles(positions):
    """The tiles that hold the player's foot center at some reachable position."""
    return {((x + 8) >> 4, (y + 12) >> 4) for x, y in positions}


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
    (0x2F, "WATER_INNER_TOP_LEFT_BOTTOM_RIGHT", 0xFFFF), (0x30, "SWITCH_RIGHT", 0xFFC6),
    (0x31, "SWITCH_LEFT", 0xFFC6), (0x32, "POT", 0xFFC6),
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
assert sum(not t["symbol"].startswith("ATLAS_") for t in tiles.values()) == 136
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
assert 0 not in m.data, "0x00 is EMPTY and must not be painted outdoors"
assert 255 not in m.data, "every outdoor cell is painted"
assert {o["handler"] for o in m.objects if o["handler"]} == {"enter_house", "show_message"}
assert {o["handler"] for o in interiors.objects if o["handler"]} == {"leave_house"}

# The player cannot reach the edge of the map, and every marker and walkable cell is reachable.
outdoors = Ground(m)
walked = outdoors.walk(start)
assert not any(
    x <= 0 or y <= 0 or x >= outdoors.width - 16 or y >= outdoors.height - 16 for x, y in walked
), "the player can reach the edge of the map"
seen = standing_tiles(walked)
for name, o in names.items():
    if o["area"] == m.area and len(o["points"]) == 1:
        cell = position(name)
        assert cell in seen or any(step in seen for step in adjacent(cell)), f"{name} cannot be reached"
# Walkable ground that the player cannot reach is either a plateau sealed behind cliff pieces
# or a small gap inside a forest.
cliff_outline = {
    index for index, tile in tiles.items() if tile["symbol"].startswith("CLIFF_") and tile["collision"]
}
unreached = {
    (x, y) for y in range(m.height) for x in range(m.width)
    if tiles[at(x, y)]["collision"] == 0 and (x, y) not in seen
}
while unreached:
    first = unreached.pop()
    pocket, queue = {first}, deque([first])
    while queue:
        for step in adjacent(queue.popleft()):
            if step in unreached:
                unreached.remove(step)
                pocket.add(step)
                queue.append(step)
    if len(pocket) <= 2:
        continue
    outline, queue = set(pocket), deque(pocket)
    while queue:
        for x, y in adjacent(queue.popleft()):
            if 0 <= x < m.width and 0 <= y < m.height and (x, y) not in outline and at(x, y) not in cliff_outline:
                assert (x, y) not in seen, f"walled-off ground near {first}"
                outline.add((x, y))
                queue.append((x, y))
# The border uses several kinds of barrier.
ring = {at(x, y) for x in range(m.width) for y in (0, m.height - 1)} | {
    at(x, y) for y in range(m.height) for x in (0, m.width - 1)
}
ring_symbols = {tiles[index]["symbol"] for index in ring}
for prefix in ("WATER", "CLIFF_", "TREE", "ROCK"):
    assert any(name.startswith(prefix) for name in ring_symbols), f"no {prefix} barrier on the border"
# Forests are broken up: no 4x4 block holds only tree pieces.
trees = {index for index, tile in tiles.items() if tile["symbol"].startswith("TREE")}
for top in range(m.height - 3):
    for left in range(m.width - 3):
        assert not all(at(left + x, top + y) in trees for x in range(4) for y in range(4)), (
            f"solid block of trees at {(left, top)}"
        )

# The boulder gate. The boulders are the only thing between the clearing and the village, and no
# order of pushes can ever trap the player in the clearing.
gate = cells_of("BoulderGate")
boulders = frozenset(position(f"Boulder{letter}") for letter in "ABCD")
assert boulders <= gate
spawn_side, village_side = position("GateMouth"), position("ClearingExit")
assert not (gate & {spawn_side, village_side})
# A boulder stays in the chunk of its site, and a chunk's boulders unload together. The whole
# gate lies in one chunk, so walking away restores the opening arrangement, never a part of it.
assert len({(x // CHUNK_WIDTH, y // CHUNK_HEIGHT) for x, y in gate}) == 1
for x, y in gate:
    for nx, ny in ((x + dx, y + dy) for dx in (-1, 0, 1) for dy in (-1, 0, 1)):
        # Whole or empty cells make the gate behave exactly as a grid puzzle.
        assert tiles[at(nx, ny)]["collision"] in (0, 0xFFFF), f"partial tile beside the gate at {(nx, ny)}"
assert all(tiles[at(*cell)]["collision"] == 0 for cell in boulders)
sealed = standing_tiles(Ground(m, blocked=gate).walk(start))
assert village_side not in sealed and not any(
    cell in sealed for cell in cells_of("PathEast")
), "the clearing has a way out around the boulders"
assert village_side not in standing_tiles(Ground(m, blocked=boulders).walk(start)), "the boulders do not block the way"

floor = {(x, y) for y in range(m.height) for x in range(m.width) if tiles[at(x, y)]["collision"] == 0}


def region_from(cell, stones):
    seen, queue = {cell}, deque([cell])
    while queue:
        for step in adjacent(queue.popleft()):
            if step in floor and step not in stones and step not in seen:
                seen.add(step)
                queue.append(step)
    return seen


def push_states(cell, stones):
    """Every arrangement of boulders and player region reachable by pushing, as a graph."""
    first = (stones, min(region_from(cell, stones)))
    index, states, edges, depth = {first: 0}, [first], [], [0]
    i = 0
    while i < len(states):
        stones, anchor = states[i]
        reach = region_from(anchor, stones)
        out = []
        for stone in stones:
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                behind = (stone[0] - dx, stone[1] - dy)
                ahead = (stone[0] + dx, stone[1] + dy)
                if behind not in reach or ahead not in gate or ahead not in floor or ahead in stones:
                    continue
                moved = (stones - {stone}) | {ahead}
                key = (moved, min(region_from(stone, moved)))
                if key not in index:
                    index[key] = len(states)
                    states.append(key)
                    depth.append(depth[i] + 1)
                out.append(index[key])
        edges.append(out)
        i += 1
    return states, edges, depth


def every_state_escapes(cell):
    """Fewest pushes to the village side, asserting that no push can remove that way out."""
    states, edges, depth = push_states(cell, boulders)
    goal = [village_side in region_from(anchor, stones) for stones, anchor in states]
    back = [[] for _ in states]
    for i, out in enumerate(edges):
        for j in out:
            back[j].append(i)
    good, queue = list(goal), deque(i for i, found in enumerate(goal) if found)
    while queue:
        j = queue.popleft()
        for i in back[j]:
            if not good[i]:
                good[i] = True
                queue.append(i)
    assert all(good), f"the boulders can trap the player who starts at {cell}"
    return min(depth[i] for i, found in enumerate(goal) if found), len(states)


pushes, states = every_state_escapes(start)
assert 4 <= pushes <= 8 and states > 50, ("the puzzle should be simple but not trivial", pushes, states)
# The boulders restart with the player outside the gate on either side: after a load, which moves
# a player in the gate to a mouth, or after walking away. From the village side too, the player
# can push a way into the clearing and back out.
every_state_escapes(spawn_side)
every_state_escapes(village_side)
returning, _, _ = push_states(village_side, boulders)
assert any(spawn_side in region_from(anchor, stones) for stones, anchor in returning), (
    "the clearing cannot be entered from the village side"
)

# Bram stands in the middle of the bridge, the only way south out of the village.
bram = position("BramPost")
assert tiles[at(*bram)]["symbol"] == "BRIDGE_VERTICAL"
assert all(tiles[at(bram[0] + dx, bram[1])]["symbol"].startswith("WATER") for dx in (-1, 1))
open_gate = Ground(m, bodies=[bram])
beyond = standing_tiles(open_gate.walk(start))
assert not any(y > bram[1] and abs(x - bram[0]) <= 4 for x, y in beyond), "Bram can be passed"
assert (bram[0], bram[1] + 1) in standing_tiles(Ground(m).walk(start))

# Every building has a sign beside its door.
buildings = ("Alden", "Eliza", "Bram", "Shop", "Inn", "NorthHouse", "SouthHouse", "Barn")
for name in buildings:
    door, sign = position(f"{name}Door"), position(f"{name}Sign")
    assert at(*door) in (0x84, 0x85), f"{name}Door is not on a door tile"
    assert at(*sign) == symbol["SIGN"] and names[f"{name}Sign"]["handler"] == "show_message"
    assert abs(door[0] - sign[0]) <= 1 and sign[1] - door[1] == 1, f"{name}Sign is not by the door"
    assert (door[0], door[1] + 1) in seen, f"the ground in front of {name}Door is not reachable"

# Houses. Each enterable door pairs with an interior exit of the same name. The player enters
# by activating the door and leaves by walking onto the exit. Other doors only answer.
house = interiors.tiles
house_symbol = {tile["symbol"]: index for index, tile in house.items()}


def inside(x, y):
    return interiors.data[y * interiors.width + x]


def roof_at(x, y):
    return at(x, y) if 0 <= x < m.width and 0 <= y < m.height else None


def check_gable(name, x, y):
    # Use any surviving front edge or ridge junction to recognize a gable.
    front = ((0xB0,), (0xB1, 0x80), (0xB2,))
    if roof_at(x, y - 2) != 0xA1 and not any(
        roof_at(x + dx, y - 1) in variants for dx, variants in enumerate(front, -1)
    ):
        return

    left_slope, right_slope = (0xA0, 0x82), (0xA2, 0x83)
    deep = (
        roof_at(x - 1, y - 2) in left_slope
        or roof_at(x + 1, y - 2) in right_slope
        or roof_at(x, y - 3) == 0x91
    )
    peak = ((), (0x81,), ())
    if deep:
        rows = (front, (left_slope, (0xA1,), right_slope), ((0x90,), (0x91,), (0x92, 0x4F)), peak)
    else:
        rows = (front, ((0x90,), (0xA1,), (0x92, 0x4F)), peak)

    for dy, row in enumerate(rows, 1):
        for dx, variants in enumerate(row, -1):
            if variants:
                cell = (x + dx, y - dy)
                expected = "/".join(f"0x{tile:02X}" for tile in variants)
                assert roof_at(*cell) in variants, f"{name} gable at {cell} needs {expected}"


# Flat roof pieces have an open seam on each side that continues over the next tile.
# Horizontal joins share a row profile, and vertical joins share a column profile.
flat_seams = {
    0xA4 + 16 * row + col: (
        row if col > 0 else None,
        row if col < 2 else None,
        col if row > 0 else None,
        col if row < 2 else None,
    )
    for row in range(3)
    for col in range(3)
}
flat_seams[0x4E] = flat_seams[0xB5]
for index, tile in enumerate(m.data):
    if tile not in flat_seams:
        continue
    y, x = divmod(index, m.width)
    for side, profile in enumerate(flat_seams[tile]):
        if profile is None:
            continue
        dx, dy = SIDES[side]
        neighbor = x + dx, y + dy
        other = flat_seams.get(roof_at(*neighbor))
        assert other is not None and other[OPPOSITE[side]] == profile, (
            f"Flat roof at {(x, y)} has a broken join toward {neighbor}"
        )


assert 255 not in interiors.data, "interior void is painted with EMPTY (0x00), never left unpainted"
houses = sorted(name for name in buildings if names[f"{name}Door"]["handler"] == "enter_house")
assert houses == ["Alden", "Eliza", "Inn", "Shop"]
exits = sorted(name[:-4] for name, o in names.items() if name.endswith("Exit") and o["area"] == interiors.area)
assert houses == exits, (houses, exits)
logic = (ROOT / "src/logic/area_village.h").read_text()
for name in buildings:
    check_gable(name, *position(f"{name}Door"))
    if name not in houses:
        assert names[f"{name}Door"]["handler"] == "show_message"
        assert f"positions::{name}Door" in logic
room_cells = {}
for name in houses:
    way_out = names[name + "Exit"]
    assert way_out["handler"] == "leave_house"
    x, y = way_out["tx"], way_out["ty"]
    assert inside(x, y) == house_symbol["EDGE_TOP_EXIT"]
    assert house[inside(x, y - 1)]["collision"] == 0
    assert f"positions::{name}Door, positions::{name}Exit, positions::{name}RoomsRegion" in logic, (
        f"{name} is missing from HOUSE_LINKS"
    )
    # The room: every cell the player's feet can stand on from the exit, and the floor they
    # must reach. Furniture never walls off floor.
    room = standing_tiles(Ground(interiors).walk((x, y - 1)))
    room_cells[name] = room
    region = cells_of(f"{name}Rooms")
    for cell in region:
        if house[inside(*cell)]["collision"] == 0:
            assert cell in room, f"{name}: the floor at {cell} is walled off"
    assert room - {(x, y)} <= region, f"{name}Rooms does not cover the house"
assert len({frozenset(room) for room in room_cells.values()}) == len(houses), "two houses share a room"
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
# Eliza's home is a kitchen and living room, with a bedroom for the rat.
bedroom = cells_of("ElizaBedroom")
assert bedroom <= cells_of("ElizaRooms") and position("RatNest") in bedroom & room_cells["Eliza"]
for person, room in (("ElizaAtAlden", "Alden"), ("RatNest", "Eliza")):
    assert position(person) in room_cells[room] and house[inside(*position(person))]["collision"] == 0
spawn = names[interiors.spawn]
assert house[inside(spawn["tx"], spawn["ty"])]["collision"] == 0
garden = position("ElizaGarden")
assert garden in seen and tiles[at(*garden)]["collision"] == 0


def check_rugs(area):
    """Every rug or carpet of several cells is a complete rectangle of at least 2x2 pieces,
    each in its place. Only furniture inside its outline may hide a piece."""
    sheet = area.tiles
    places = {"TOP_LEFT": (0, 0), "TOP": (1, 0), "TOP_RIGHT": (2, 0), "LEFT": (0, 1),
              "CENTER": (1, 1), "CENTER_PLAIN": (1, 1), "RIGHT": (2, 1), "BOTTOM_LEFT": (0, 2),
              "BOTTOM": (1, 2), "BOTTOM_RIGHT": (2, 2)}
    pieces = {}
    for index, tile in sheet.items():
        for prefix in ("RUG_", "CARPET_"):
            name = tile["symbol"]
            if name.startswith(prefix) and name[len(prefix):] in places:
                pieces[index] = places[name[len(prefix):]]
    cells = {
        (x, y): pieces[area.data[y * area.width + x]]
        for y in range(area.height) for x in range(area.width)
        if area.data[y * area.width + x] in pieces
    }
    remaining = set(cells)
    while remaining:
        queue = deque([min(remaining)])
        remaining.remove(queue[0])
        rug = set(queue)
        while queue:
            for step in adjacent(queue.popleft()):
                if step in remaining:
                    remaining.remove(step)
                    rug.add(step)
                    queue.append(step)
        left, right = min(x for x, _ in rug), max(x for x, _ in rug)
        top, bottom = min(y for _, y in rug), max(y for _, y in rug)
        where = f"{area.symbol}: rug at {(left, top)}"
        assert right > left and bottom > top, f"{where} is narrower than 2x2"
        for y in range(top, bottom + 1):
            for x in range(left, right + 1):
                want = (0 if x == left else 2 if x == right else 1,
                        0 if y == top else 2 if y == bottom else 1)
                if (x, y) in rug:
                    assert cells[x, y] == want, f"{where}: the piece at {(x, y)} is out of place"
                else:
                    value = area.data[y * area.width + x]
                    assert want == (1, 1) and sheet[value]["collision"] != 0, f"{where}: gap at {(x, y)}"


# The room model's walls, by tileset: the cells of the north wall row, which show a wall face
# from the front, and the edge pieces around a room.
WALL_ROWS = {
    "house": set(range(0x10, 0x1C)) | set(range(0x20, 0x2C)) | set(range(0x30, 0x3B)),
    "cave": set(range(0x10, 0x20)),
    "dungeon": (set(range(0x10, 0x24)) | set(range(0x40, 0x4A))) - {0x1C, 0x44},
}
EDGE_CELLS = set(range(0x01, 0x10)) | {0x1C}


def check_north_walls(area):
    """In the 3/4 view the north side of every floor shows a wall. A floor or furniture cell has
    another one above it or a wall row cell, never an edge or the void. This holds for the side
    passages too; only the exit in a south wall is an edge that the player walks onto."""
    walls = WALL_ROWS[area.tileset]

    def cell(x, y):
        return area.data[y * area.width + x] if y >= 0 else 0

    def room(value):
        return value != 0 and value not in walls and value not in EDGE_CELLS

    for y in range(area.height):
        for x in range(area.width):
            value = cell(x, y)
            walked_edge = value in EDGE_CELLS and area.tiles[value]["collision"] == 0
            if not room(value) and not (walked_edge and area.tiles[value]["symbol"] != "EDGE_TOP_EXIT"):
                continue
            above = cell(x, y - 1)
            assert room(above) or above in walls, (
                f"{area.symbol}: {area.tiles[value]['symbol']} at {(x, y)} has no wall to its north"
            )


for area in maps:
    if area.tileset in WALL_ROWS:
        check_north_walls(area)
    check_rugs(area)
assert any(tile["symbol"].startswith("RUG_") for tile in (house[v] for v in interiors.data))

# Animation. Something moves wherever the player goes: every house has an animated light and
# a floor for moths, and outdoors every grass, flowers, water and tree cell takes an ambient effect.
def world_symbol_of(symbol):
    return next(index for index, tile in m.tiles.items() if tile["symbol"] == symbol)


animated_house = {index for index, tile in house.items() if any(rows is not None for rows in tile["phases"])}
for name in houses:
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
        "named": 112,
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
    ("TABLE", "TABLE_SET", "TABLE_CANDLE", "TABLE_LEFT", "TABLE_MIDDLE", "TABLE_RIGHT",
     "TABLE_WORKSHOP_RECORD", "TABLE_SURVEY_NOTES"),
    ("VILLAGE_LANTERN_DARK", "VILLAGE_LANTERN_MODERN", "VILLAGE_LANTERN_ANCIENT"),
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

# Static NPC images block with convex shapes, and the boulders fill their cells.
symbols, collisions, _, _ = static_npc_frames(ROOT)
assert {"BOULDER", "BOULDER_SPLIT"} <= set(symbols)
assert all(mask in CONVEX for mask in collisions)
assert all(collisions[symbols.index(name)] == 0xFFFF for name in ("BOULDER", "BOULDER_SPLIT"))

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

# Every sound effect is a one-shot score that the game plays: a score that nothing asks for is dead
# weight in flash, and a score without the `sfx` setting would compile as music.
code = "".join(
    path.read_text() for path in (ROOT / "src").rglob("*") if path.suffix in {".h", ".cpp"}
)
scores = sorted((ROOT / "assets/sfx").glob("*.avms"))
assert len(scores) > 50
for path in scores:
    assert re.search(r"^sfx$", path.read_text(), re.MULTILINE), f"{path.name} lacks the sfx setting"
    symbol = "SFX_" + re.sub(r"[^A-Za-z0-9_]", "_", path.stem).upper()
    assert re.search(rf"\b{symbol}\b", code), f"{symbol} is never played"
print(
    "PASS village and clearing, trap-free boulder gate, sealed border, linked houses, roof and rug "
    "joins, tile semantics, no guest allocations and only played sound effects"
)
