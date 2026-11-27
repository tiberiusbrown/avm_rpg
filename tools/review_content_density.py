"""Advisory 8×4 views from reachable outdoor tiles at game zoom; the village by default."""

import argparse
from collections import deque
from pathlib import Path

from generate_assets import load_world


def neighbors(x, y):
    return ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bounds", nargs=4, type=int, default=(31, 3, 56, 29),
                        metavar=("LEFT", "TOP", "RIGHT", "BOTTOM"))
    args = parser.parse_args()
    maps, _, names = load_world(Path(__file__).resolve().parents[1])
    area = next(area for area in maps if area.symbol == "OUTDOOR")
    left, top, right, bottom = args.bounds
    if not (0 <= left <= right < area.width and 0 <= top <= bottom < area.height):
        parser.error("bounds must be ordered and inside the outdoor map")

    def tile(x, y):
        return area.tiles[area.data[y * area.width + x]]

    portals = {(o["tx"], o["ty"]) for o in names.values()
               if o["area"] == area.area and o["handler"] in {"take_passage", "maintenance_hatch"}}
    start = names[area.spawn]
    origin = (start["tx"], start["ty"])
    reached, queue = {origin}, deque([origin])
    while queue:
        for x, y in neighbors(*queue.popleft()):
            step = (x, y)
            if (0 <= x < area.width and 0 <= y < area.height and step not in reached
                    and step not in portals and tile(x, y)["collision"] == 0):
                reached.add(step)
                queue.append(step)

    # Roads count visually at junctions and turnoffs. Continuous straight paving and ordinary
    # grass can be useful space, but a viewport containing only these merits a look.
    def landmark(x, y):
        name = tile(x, y)["symbol"]
        return not (name.startswith("GRASS_") or name in ("ROAD_NS", "ROAD_EW"))

    points = {(o["tx"], o["ty"]) for name, o in names.items()
              if o["area"] == area.area and name != area.spawn}
    points.update((x, y)
                  for x in range(max(0, left - 4), min(area.width, right + 5))
                  for y in range(max(0, top - 2), min(area.height, bottom + 3))
                  if landmark(x, y))
    stands = {p for p in reached if left <= p[0] <= right and top <= p[1] <= bottom}
    sparse = {(x, y) for x, y in stands
              if all((vx, vy) not in points for vx in range(x - 3, x + 5)
                     for vy in range(y - 1, y + 3))}
    print(f"Outdoor {args.bounds}: {len(stands)} reachable tile centers, "
          f"{len(sparse)} with a sparse 8×4 view.")
    while sparse:
        first = min(sparse, key=lambda p: (p[1], p[0]))
        sparse.remove(first)
        group, queue = [first], deque([first])
        while queue:
            for step in neighbors(*queue.popleft()):
                if step in sparse:
                    sparse.remove(step)
                    group.append(step)
                    queue.append(step)
        print(f"  Review near {first} ({len(group)} adjacent sparse views).")
    print("Advisory only. Review paths and camera compositions in-game.")


if __name__ == "__main__":
    main()
