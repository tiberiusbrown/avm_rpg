"""Deterministic finite TMX/TSX -> native C++20/AVM program assets."""

import argparse
import re
import xml.etree.ElementTree as ET
from dataclasses import dataclass
from decimal import ROUND_CEILING, Decimal, InvalidOperation
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
TILE_SIZE = 16
CHUNK_WIDTH, CHUNK_HEIGHT = 8, 4


def require(ok, where, message):
    if not ok:
        raise ValueError(f"{where}: {message}")


def identifier(value, where):
    keywords = set(
        "alignas alignof and and_eq asm auto bitand bitor bool break case catch char char16_t char32_t class compl const constexpr const_cast continue decltype default delete do double dynamic_cast else enum explicit export extern false float for friend goto if inline int long mutable namespace new noexcept not not_eq nullptr operator or or_eq private protected public register reinterpret_cast return short signed sizeof static static_assert static_cast struct switch template this thread_local throw true try typedef typeid typename union unsigned using virtual void volatile wchar_t while xor xor_eq".split()
    )
    require(
        isinstance(value, str)
        and bool(re.fullmatch(r"[A-Za-z][A-Za-z0-9_]*", value))
        and value not in keywords,
        where,
        f"invalid C++ identifier {value!r}",
    )
    return value


def properties(node, where):
    result = {}
    for prop in node.findall("properties/property"):
        name, value = prop.get("name"), prop.get("value", prop.text or "")
        require(name not in result, where, f"duplicate property {name}")
        kind = prop.get("type", "string")
        if kind in ("int", "object"):
            value = int(value)
        elif kind == "bool":
            require(value in ("true", "false"), where, f"invalid boolean {name}")
            value = value == "true"
        else:
            require(kind == "string", where, f"unsupported property type {kind}")
        result[name] = value
    return result


def integer(node, key, where, default=None):
    text = node.get(key, default)
    try:
        value = float(text)
        require(value.is_integer(), where, f"{key} must be integral")
        return int(value)
    except (TypeError, ValueError):
        raise ValueError(f"{where}: {key} must be an integer, got {text!r}") from None


def pixel_values(image):
    # Pillow 10 and 14 expose the same pixels through different iterator names.
    return (
        image.get_flattened_data()
        if hasattr(image, "get_flattened_data")
        else image.getdata()
    )


def tileset(path):
    node = ET.parse(path).getroot()
    require(
        node.tag == "tileset"
        and node.get("tilewidth") == node.get("tileheight") == "16",
        path,
        "expected an external 16x16 tileset",
    )
    require(
        not any(
            node.find(tag) is not None
            for tag in ("tileoffset", "transformations", "wangsets")
        ),
        path,
        "tile offsets, transformations and wangsets are unsupported",
    )
    count, columns = integer(node, "tilecount", path), integer(node, "columns", path)
    require(
        0 < count <= 256 and columns > 0,
        path,
        "tileset must have 1..256 cells; cell 255 is reserved",
    )
    image = node.find("image")
    require(image is not None, path, "atlas image required")
    image_path = (path.parent / image.get("source")).resolve()
    with Image.open(image_path) as pixels:
        require(
            pixels.size
            == (columns * TILE_SIZE, ((count + columns - 1) // columns) * TILE_SIZE),
            path,
            "image dimensions do not match the 16x16 atlas",
        )
        require(
            all(
                p in ((0, 0, 0, 255), (255, 255, 255, 255))
                for p in pixel_values(pixels.convert("RGBA"))
            ),
            path,
            "use opaque monochrome pixels",
        )
    records, local_ids = {}, set()
    for tile in node.findall("tile"):
        local = integer(tile, "id", path)
        require(
            0 <= local < count and local not in local_ids,
            path,
            "invalid/duplicate tile id",
        )
        local_ids.add(local)
        require(
            len(tile) == 1 and tile[0].tag == "properties",
            path,
            "tile animation, per-tile images and collision shapes are unsupported; use solid property",
        )
        p = properties(tile, path)
        if local == 255:
            require(
                count == 256 and p == {"reserved": True},
                path,
                "cell 255 must be reserved",
            )
            continue
        runtime = p.get("runtime_id")
        require(
            type(runtime) is int and 0 <= runtime < 255,
            path,
            "explicit runtime_id 0..254 required",
        )
        require(
            type(p.get("solid")) is bool and type(p.get("portal")) is bool,
            path,
            "each tile needs bool solid and portal properties",
        )
        records[local] = dict(
            runtime=runtime,
            symbol=identifier(p.get("symbol"), path),
            solid=int(p["solid"]),
            portal=int(p["portal"]),
            image=image_path,
            frame=local,
            columns=columns,
        )
    require(
        len(local_ids) == count and len(records) == min(count, 255),
        path,
        "declare metadata for every usable atlas tile and the reserved cell",
    )
    return records


@dataclass
class Map:
    area: int
    symbol: str
    width: int
    height: int
    columns: int
    rows: int
    data: list
    objects: list
    spawn: str
    routes: list


def load_world(root):
    maps, metadata, names, route_names = [], {}, {}, set()
    for path in sorted((root / "assets/maps").glob("*.tmx")):
        node = ET.parse(path).getroot()
        require(
            node.tag == "map"
            and node.get("orientation") == "orthogonal"
            and node.get("infinite", "0") == "0"
            and node.get("renderorder", "right-down") == "right-down",
            path,
            "only finite orthogonal right-down maps are supported",
        )
        require(
            node.get("tilewidth") == node.get("tileheight") == "16",
            path,
            "tiles must be 16x16 pixels",
        )
        w, h = integer(node, "width", path), integer(node, "height", path)
        require(
            1 <= w <= 240 and 1 <= h <= 248,
            path,
            f"logical size {w}x{h} exceeds the 240x248 contract (width 248/256 is invalid)",
        )
        p = properties(node, path)
        area, symbol = p.get("area_id"), identifier(p.get("symbol"), path)
        require(
            type(area) is int
            and 1 <= area <= 255
            and all(m.area != area and m.symbol != symbol for m in maps),
            path,
            "unique explicit area_id 1..255 and symbol required",
        )
        gids = {}
        for reference in node.findall("tileset"):
            require(
                reference.get("source") is not None, path, "use external TSX tilesets"
            )
            first = integer(reference, "firstgid", path)
            require(first > 0, path, "firstgid must be positive")
            for local, tile in tileset(path.parent / reference.get("source")).items():
                require(
                    first + local not in gids, path, "overlapping tileset GID ranges"
                )
                gids[first + local] = tile["runtime"]
                old = metadata.get(tile["runtime"])
                require(
                    old is None or old == tile,
                    path,
                    "conflicting runtime tile IDs across tilesets",
                )
                metadata[tile["runtime"]] = tile
        data, objects, routes, layers = [255] * (w * h), [], [], 0
        for child in node:
            require(
                child.tag in ("properties", "tileset", "layer", "objectgroup"),
                path,
                f"unsupported map element {child.tag}",
            )
            if child.tag not in ("layer", "objectgroup"):
                continue
            require(
                all(
                    float(child.get(k, "0")) == 0
                    for k in ("offsetx", "offsety", "x", "y")
                )
                and child.get("visible", "1") == "1"
                and child.get("opacity", "1") == "1"
                and all(
                    float(child.get(k, "1")) == 1 for k in ("parallaxx", "parallaxy")
                ),
                path,
                "layer offsets, parallax, hidden/transparent layers are unsupported",
            )
            if child.tag == "layer":
                require(
                    integer(child, "width", path) == w
                    and integer(child, "height", path) == h,
                    path,
                    "tile layer dimensions must match the map",
                )
                payload = child.find("data")
                require(
                    payload is not None
                    and payload.get("encoding") == "csv"
                    and payload.get("compression") is None
                    and len(payload) == 0,
                    path,
                    "use uncompressed finite CSV tile layers",
                )
                try:
                    values = [
                        int(s.strip())
                        for s in (payload.text or "").strip().strip(",").split(",")
                    ]
                except ValueError:
                    raise ValueError(
                        f"{path}: {child.get('name')}: invalid integer in CSV tiles"
                    ) from None
                require(
                    len(values) == w * h,
                    path,
                    "CSV tile count does not match dimensions",
                )
                for index, gid in enumerate(values):
                    require(
                        0 <= gid < 0x10000000,
                        path,
                        "flipped/rotated tile references are unsupported",
                    )
                    require(
                        gid == 0 or gid in gids,
                        path,
                        f"unsupported tile reference GID {gid}",
                    )
                    if gid:
                        data[index] = gids[gid]
                layers += 1
                continue
            require(
                child.get("name") in ("Logic", "Locations", "Routes"),
                path,
                "name object layers Logic, Locations or Routes",
            )
            for obj in child.findall("object"):
                if child.get("name") == "Routes":
                    where = f"{path}:{obj.get('name')}"
                    name = identifier(obj.get("name"), where)
                    props = properties(obj, where)
                    require(
                        set(props) <= {"mode", "pause"},
                        where,
                        "route properties: mode, pause; route_id is exporter-assigned",
                    )
                    mode, pause = props.get("mode", "once"), props.get("pause", 8)
                    require(name not in route_names, where, "duplicate route name")
                    require(
                        mode in ("once", "loop", "reverse")
                        and type(pause) is int
                        and 0 <= pause <= 255,
                        where,
                        "invalid route mode or pause",
                    )
                    line = obj.find("polyline")
                    require(
                        line is not None
                        and not obj.get("type")
                        and not obj.get("class")
                        and not obj.get("gid")
                        and not obj.get("template")
                        and float(obj.get("rotation", "0")) == 0
                        and all(c.tag in ("properties", "polyline") for c in obj),
                        where,
                        "routes must be unrotated polylines",
                    )
                    try:
                        ox, oy = Decimal(obj.get("x", "0")), Decimal(obj.get("y", "0"))
                    except InvalidOperation:
                        raise ValueError(f"{where}: invalid route origin") from None
                    require(
                        ox.is_finite() and oy.is_finite(),
                        where,
                        "route origin must be finite",
                    )
                    points = []
                    for pair in line.get("points", "").split():
                        try:
                            dx, dy = map(Decimal, pair.split(","))
                        except (ValueError, InvalidOperation):
                            raise ValueError(f"{where}: invalid route point") from None
                        require(
                            dx.is_finite() and dy.is_finite(),
                            where,
                            "route points must be finite",
                        )
                        x, y = ox + dx, oy + dy
                        require(
                            0 <= x < w * TILE_SIZE and 0 <= y < h * TILE_SIZE,
                            where,
                            "route point out of map bounds",
                        )
                        require(
                            x % TILE_SIZE != 0 and y % TILE_SIZE != 0,
                            where,
                            "route markers must be inside tile boundaries, not on corners/edges",
                        )
                        # Like location objects, each marker selects its containing tile.
                        points.append((int(x // TILE_SIZE), int(y // TILE_SIZE)))
                    require(
                        2 <= len(points) <= 255, where, "routes need 2..255 waypoints"
                    )
                    routes.append(
                        dict(name=name, mode=mode, pause=pause, points=points)
                    )
                    route_names.add(name)
                    continue
                name = obj.get("name") or None
                if name is not None:
                    identifier(name, path)
                where = f"{path}:{name or '<unnamed ' + str(len(objects)) + '>'}"
                require(
                    name is None or name not in names,
                    where,
                    "duplicate world location name",
                )
                require(
                    not any(
                        obj.find(t) is not None
                        for t in ("polygon", "polyline", "ellipse", "text", "template")
                    )
                    and obj.get("gid") is None
                    and obj.get("template") is None
                    and float(obj.get("rotation", "0")) == 0,
                    where,
                    "only axis-aligned rectangles/points are supported",
                )
                require(
                    not obj.get("type") and not obj.get("class"),
                    where,
                    "object types are unsupported; use named tile positions",
                )
                try:
                    x, y, ow, oh = (
                        Decimal(obj.get(k, "0")) for k in ("x", "y", "width", "height")
                    )
                    require(
                        all(v.is_finite() for v in (x, y, ow, oh)),
                        where,
                        "coordinates must be finite",
                    )
                except InvalidOperation:
                    raise ValueError(f"{where}: invalid object coordinates") from None
                require(
                    0 <= x < w * TILE_SIZE and 0 <= y < h * TILE_SIZE,
                    where,
                    "object anchor is out of bounds",
                )
                require(
                    ow >= 0 and oh >= 0 and ((ow == oh == 0) or (ow > 0 and oh > 0)),
                    where,
                    "footprint must have two positive dimensions or be a point",
                )
                require(
                    x + ow <= w * TILE_SIZE and y + oh <= h * TILE_SIZE,
                    where,
                    "feature footprint extends outside the logical map",
                )
                props = properties(obj, where)
                require(
                    set(props) <= {"handler"},
                    where,
                    "only the handler property is supported on map objects",
                )
                handler = props.get("handler")
                if handler is not None:
                    identifier(handler, where)
                    require(
                        handler
                        not in {
                            "Handler",
                            "POS",
                            "STAGE",
                            "Tag",
                            "Gameplay",
                            "Tiles",
                            "Dialogue",
                        },
                        where,
                        f"handler name {handler!r} conflicts with the generated Handler declaration",
                    )
                tx, ty = int(x // TILE_SIZE), int(y // TILE_SIZE)
                # Half-open rectangles include every tile with a positive-area intersection.
                # Points select their containing tile, independent of alignment.
                right = (
                    int(
                        ((Decimal(x) + ow) / TILE_SIZE).to_integral_value(
                            rounding=ROUND_CEILING
                        )
                    )
                    if ow
                    else tx + 1
                )
                bottom = (
                    int(
                        ((Decimal(y) + oh) / TILE_SIZE).to_integral_value(
                            rounding=ROUND_CEILING
                        )
                    )
                    if oh
                    else ty + 1
                )
                rw, rh = right - tx, bottom - ty
                cells = [
                    (cx, cy) for cy in range(ty, bottom) for cx in range(tx, right)
                ]
                points = [
                    dict(
                        name=(name if len(cells) == 1 else f"{name}_{i}")
                        if name
                        else None,
                        symbol=(
                            f"positions::{name}"
                            if len(cells) == 1
                            else f"positions::{name}_{i}"
                        )
                        if name
                        else f"GENERATED_TILE_{area}_{len(objects)}_{i}",
                        tx=cx,
                        ty=cy,
                    )
                    for i, (cx, cy) in enumerate(cells)
                ]
                record = dict(
                    name=name,
                    index=len(objects),
                    tx=tx,
                    ty=ty,
                    rw=rw,
                    rh=rh,
                    rectangle=ow > 0,
                    points=points,
                    area=area,
                    handler=handler,
                    where=where,
                )
                if name is not None:
                    names[name] = record
                objects.append(record)
        require(layers > 0, path, "at least one tile layer required")
        maps.append(
            Map(
                area,
                symbol,
                w,
                h,
                (w + CHUNK_WIDTH - 1) // CHUNK_WIDTH,
                (h + CHUNK_HEIGHT - 1) // CHUNK_HEIGHT,
                data,
                sorted(
                    objects,
                    key=lambda o: (o["name"] is None, o["name"] or "", o["index"]),
                ),
                p.get("spawn"),
                sorted(routes, key=lambda r: r["name"]),
            )
        )
    require(maps, root, "no maps found in assets/maps")
    all_routes = sorted(
        (route for m in maps for route in m.routes), key=lambda route: route["name"]
    )
    require(len(all_routes) <= 255, root, "at most 255 routes are supported")
    for route_id, route in enumerate(all_routes, 1):
        route["id"] = route_id
    require(
        set(metadata) == set(range(len(metadata)))
        and len({t["symbol"] for t in metadata.values()}) == len(metadata),
        root,
        "runtime tile IDs must be dense from zero; tile symbols must be unique",
    )
    symbols = set()
    for o in names.values():
        emitted = [point["name"] for point in o["points"]]
        if o["rectangle"]:
            emitted.append(o["name"] + "Region")
        for symbol in emitted:
            require(
                symbol not in symbols,
                o["where"],
                f"duplicate generated identifier {symbol}",
            )
            symbols.add(symbol)
    for m in maps:
        tile_owners = {}
        for o in m.objects:
            if not o["handler"]:
                continue
            for point in o["points"]:
                cell = point["tx"], point["ty"]
                tile_owners[cell] = o["name"] or f"unnamed {o['index']}"
        spawn = names.get(m.spawn)
        require(
            spawn is not None and spawn["area"] == m.area and len(spawn["points"]) == 1,
            m.symbol,
            "spawn property must reference a single named tile in this area",
        )
        cell = spawn["tx"], spawn["ty"]
        tile = m.data[cell[1] * m.width + cell[0]]
        require(
            tile in metadata and not metadata[tile]["solid"],
            m.symbol,
            "spawn must be on passable baseline tiles",
        )
        require(
            cell not in tile_owners,
            m.symbol,
            "safe spawn must not overlap a native handler",
        )
    return sorted(maps, key=lambda m: m.area), metadata, names


def read_tileset_frames(root):
    _, metadata, _ = load_world(root)
    frames = []
    for _, t in sorted(metadata.items()):
        with Image.open(t["image"]) as im:
            x, y = (
                t["frame"] % t["columns"] * TILE_SIZE,
                t["frame"] // t["columns"] * TILE_SIZE,
            )
            pixels = im.convert("RGB")
            frames.append(
                [
                    "".join(
                        "#" if pixels.getpixel((x + dx, y + dy))[0] else "."
                        for dx in range(TILE_SIZE)
                    )
                    for dy in range(TILE_SIZE)
                ]
            )
    return frames


def export_world(root=ROOT):
    maps, metadata, names = load_world(root)
    prologue = "/* Generated from TMX/TSX; do not edit. */\n"
    areas = [prologue, "#pragma once", "#include <stdint.h>"]
    areas += [f"inline constexpr uint8_t AREA_{m.symbol} = {m.area};" for m in maps]
    positions = [
        prologue,
        "#pragma once",
        '#include "world_types.h"',
        "namespace positions {",
    ]
    for name, o in sorted(names.items()):
        for point in o["points"]:
            positions.append(
                f"inline constexpr TilePos {point['name']}{{{o['area']}, {point['tx']}, {point['ty']}}};"
            )
        if o["rectangle"]:
            positions.append(
                f"inline constexpr TileRegion {name}Region"
                + "{{"
                + f"{o['area']}, {o['tx']}, {o['ty']}"
                + "}, "
                + f"{o['rw']}, {o['rh']}"
                + "};"
            )
    positions.append("}")
    route_header = [prologue, "#pragma once", '#include "npc.h"', "namespace routes {"]
    route_source = [prologue, '#include "npc_routes.h"']
    for m in maps:
        for route in m.routes:
            name = route["name"]
            route_header.append(f"inline constexpr uint8_t {name} = {route['id']};")
            route_source.append(
                f"static constexpr RoutePoint PROGMEM {name}_POINTS[] = {{"
            )
            route_source += [f"    {{{x}, {y}}}," for x, y in route["points"]]
            route_source += [
                "};",
                f"static constexpr NpcRoute PROGMEM {name}_ROUTE"
                + "{"
                + f"{name}_POINTS, {len(route['points'])}, ROUTE_{route['mode'].upper()}, {route['pause']}, {m.area}"
                + "};",
            ]
    route_header.append("}")
    route_source += ["NpcRoute const PROGMEM* npc_route(uint8_t id) {"]
    if any(m.routes for m in maps):
        route_source.append("    switch(id) {")
    for m in maps:
        route_source += [
            f"        case {r['id']}: return &{r['name']}_ROUTE;" for r in m.routes
        ]
    if any(m.routes for m in maps):
        route_source += ["        default: return nullptr;", "    }", "}"]
    else:
        route_source += ["    (void)id;", "    return nullptr;", "}"]
    ids = [prologue, "#pragma once", "#include <stdint.h>"]
    ids += [
        f"inline constexpr uint8_t TILE_{t['symbol']} = {i};"
        for i, t in sorted(metadata.items())
    ]
    ids.append(f"inline constexpr uint8_t TILE_COUNT = {len(metadata)};")
    header = [
        prologue,
        "#pragma once",
        '#include "rpg.h"',
        '#include "tile_ids.h"',
        "extern uint8_t const PROGMEM TILE_SOLID[];",
        "extern uint8_t const PROGMEM TILE_PORTALS[];",
    ]
    assets = [prologue, '#include "world_assets.h"', '#include "chunk.h"']
    for m in maps:
        prefix = m.symbol
        for field, value in (
            ("WIDTH", m.width),
            ("HEIGHT", m.height),
            ("CHUNK_COLUMNS", m.columns),
            ("CHUNK_ROWS", m.rows),
        ):
            header.append(f"inline constexpr uint8_t {prefix}_{field} = {value};")
        header += [
            f"extern uint8_t const PROGMEM {prefix}_TILES[];",
            f"extern Area const PROGMEM {prefix}_AREA;",
        ]
        data = []
        for cy in range(m.rows):
            for cx in range(m.columns):
                for ly in range(CHUNK_HEIGHT):
                    for lx in range(CHUNK_WIDTH):
                        x, y = cx * CHUNK_WIDTH + lx, cy * CHUNK_HEIGHT + ly
                        data.append(
                            m.data[y * m.width + x]
                            if x < m.width and y < m.height
                            else 255
                        )
        assets += array_lines(f"constexpr uint8_t PROGMEM {prefix}_TILES[]", data)
        spawn = names[m.spawn]
        assets.append(
            f"constexpr Area PROGMEM {prefix}_AREA"
            + "{"
            + f"{m.width}, {m.height}, {m.columns}, {m.rows}, {prefix}_TILES, "
            + "{"
            + f"{spawn['tx'] * TILE_SIZE}, {spawn['ty'] * TILE_SIZE}"
            + "}};"
        )
    for array, key in (("TILE_SOLID", "solid"), ("TILE_PORTALS", "portal")):
        assets += array_lines(
            f"constexpr uint8_t PROGMEM {array}[]",
            [t[key] for _, t in sorted(metadata.items())],
        )
    for fn, key in (("tile_solid", "solid"), ("tile_portal", "portal")):
        if not any(t[key] for t in metadata.values()):
            result = "tile >= TILE_COUNT" if key == "solid" else "false"
            assets += [
                f"bool {fn}(uint8_t tile) {{",
                "    (void)tile;",
                f"    return {result};",
                "}",
            ]
            continue
        assets += [f"bool {fn}(uint8_t tile) {{", "    switch(tile) {"]
        for i, t in sorted(metadata.items()):
            if t[key]:
                assets.append(f"        case {i}: return true;")
        assets += [
            (
                "        default: return tile >= TILE_COUNT;"
                if key == "solid"
                else "        default: return false;"
            ),
            "    }",
            "}",
        ]
    assets += ["Area const PROGMEM* world_area(uint8_t id) {", "    switch(id) {"]
    assets += [f"        case {m.area}: return &{m.symbol}_AREA;" for m in maps]
    assets += ["        default: return nullptr;", "    }", "}"]

    def entries(m, cx, cy):
        return [
            (o, point)
            for o in m.objects
            if o["handler"]
            for point in o["points"]
            if point["tx"] // CHUNK_WIDTH == cx and point["ty"] // CHUNK_HEIGHT == cy
        ]

    defaults = [
        prologue,
        "#pragma once",
        '#include "chunk.h"',
        '#include "map_positions.h"',
    ]
    private_points = [
        (o, point)
        for m in maps
        for o in m.objects
        if o["handler"] and not o["name"]
        for point in o["points"]
    ]
    if private_points:
        defaults.append("namespace {")
        for o, point in private_points:
            defaults.append(
                f"inline constexpr TilePos {point['symbol']}{{{o['area']}, {point['tx']}, {point['ty']}}};"
            )
        defaults.append("}")
    handler_names = sorted(
        {o["handler"] for m in maps for o in m.objects if o["handler"]}
    )
    defaults += [
        "namespace handler_impl {",
        "template <TilePos POS, HandlerStage STAGE> struct Tag {};",
    ]
    for name, stage in [
        ("Gameplay", "STAGE_GAMEPLAY"),
        ("Tiles", "STAGE_TILES"),
        ("Dialogue", "STAGE_DIALOGUE"),
    ]:
        defaults += [f"template <TilePos POS> using {name} = Tag<POS, {stage}>;"]
    for name in handler_names:
        defaults += [
            "template <TilePos POS, HandlerStage STAGE>",
            f"void {name}(Tag<POS, STAGE>) {{}}",
        ]
    for name in handler_names:
        defaults += [
            "template <TilePos POS, HandlerStage STAGE>",
            f"void dispatch_{name}() {{",
            f"    {name}(Tag<POS, STAGE>{{}});",
            "}",
        ]
    defaults += ["}", "template <TilePos POS, HandlerStage STAGE>", "struct Handler {"]
    for name in handler_names:
        defaults += [
            f"    void {name}() {{",
            f"        handler_impl::dispatch_{name}<POS, STAGE>();",
            "    }",
        ]
    defaults.append("};")
    dispatch = [prologue, '#include "logic/handlers.h"']
    for m in maps:
        for cy in range(m.rows):
            for cx in range(m.columns):
                points = entries(m, cx, cy)
                if not points:
                    continue
                fn = f"run_chunk_{m.area}_{cx}_{cy}"
                dispatch.append(f"template <HandlerStage STAGE> static void {fn}() {{")
                for o, point in points:
                    dispatch += [
                        "#ifndef RPG_AVM",
                        "    ++feature_trace_count[STAGE];",
                        "#endif",
                    ]
                    dispatch.append(
                        f"    Handler<{point['symbol']}, STAGE>{{}}.{o['handler']}();"
                    )
                    dispatch += [
                        "    if constexpr(STAGE != STAGE_TILES) {",
                        "        if(teleported)",
                        "            return;",
                        "    }",
                    ]
                dispatch.append("}")
    dispatch += [
        "template <HandlerStage STAGE> RPG_NOINLINE void run_chunk(ChunkCoord coord) {",
        "#ifndef RPG_AVM",
        "    if(chunk_trace_count[STAGE] < 4)",
        "        chunk_trace[STAGE][chunk_trace_count[STAGE]++] = "
        "(uint16_t)coord.y * world_area(coord.area)->chunk_columns + coord.x;",
        "#endif",
        "    switch(coord.area) {",
    ]
    for m in maps:
        if not any(o["handler"] for o in m.objects):
            continue
        dispatch += [
            f"        case {m.area}:",
            f"            switch((uint16_t)coord.y * {m.columns} + coord.x) {{",
        ]
        for cy in range(m.rows):
            for cx in range(m.columns):
                if entries(m, cx, cy):
                    dispatch.append(
                        f"                case {cy * m.columns + cx}: run_chunk_{m.area}_{cx}_{cy}<STAGE>(); break;"
                    )
        dispatch += [
            "                default: break;",
            "            }",
            "            break;",
        ]
    dispatch += [
        "        default: break;",
        "    }",
        "}",
        "template void run_chunk<STAGE_GAMEPLAY>(ChunkCoord);",
        "template void run_chunk<STAGE_TILES>(ChunkCoord);",
        "template void run_chunk<STAGE_DIALOGUE>(ChunkCoord);",
    ]
    return {
        name: "\n".join(lines) + "\n"
        for name, lines in [
            ("area_ids.h", areas),
            ("map_positions.h", positions),
            ("tile_ids.h", ids),
            ("world_assets.h", header),
            ("handler_defaults.h", defaults),
            ("map_assets.cpp", assets),
            ("chunk_dispatch.cpp", dispatch),
            ("npc_routes.h", route_header),
            ("npc_routes.cpp", route_source),
        ]
    }


def read_sprite_sheet(name, columns, rows, root=ROOT):
    path = root / "assets_src" / name
    with Image.open(path) as image:
        require(
            image.format == "PNG"
            and image.size == (columns * TILE_SIZE, rows * TILE_SIZE),
            path,
            f"expected a {columns * TILE_SIZE}x{rows * TILE_SIZE} PNG",
        )
        pixels = image.convert("RGBA")
        require(
            all(
                (r, g, b) in ((0, 0, 0), (255, 255, 255)) and a in (0, 255)
                for r, g, b, a in pixel_values(pixels)
            ),
            path,
            "use binary monochrome and alpha pixels",
        )
    frames, masks = [], []
    for frame_y in range(rows):
        for frame_x in range(columns):
            bitmap, mask = [], []
            for y in range(TILE_SIZE):
                colors, opacity = "", ""
                for x in range(TILE_SIZE):
                    r, _, _, a = pixels.getpixel(
                        (frame_x * TILE_SIZE + x, frame_y * TILE_SIZE + y)
                    )
                    colors += "#" if r and a else "."
                    opacity += "#" if a else "."
                bitmap.append(colors)
                mask.append(opacity)
            frames.append(bitmap)
            masks.append(mask)
    return frames, masks


def frame_bytes(rows):
    return [
        sum(1 << bit for bit in range(8) if rows[page + bit][x] == "#")
        for page in range(0, TILE_SIZE, 8)
        for x in range(TILE_SIZE)
    ]


def array_lines(declaration, values, row_width=16):
    lines = [f"{declaration} = {{"]
    for start in range(0, len(values), row_width):
        lines.append(
            "    " + ", ".join(map(str, values[start : start + row_width])) + ","
        )
    lines.append("};")
    return lines


def sprite_lines(name, frames, masks=None):
    lines = [f"constexpr uint8_t PROGMEM {name}[] = {{", "    16, 16, // width, height"]
    for frame_index, rows in enumerate(frames):
        assert len(rows) == TILE_SIZE and all(len(row) == TILE_SIZE for row in rows)
        pixels = frame_bytes(rows)
        if masks is not None:
            pixels = [
                byte
                for pair in zip(pixels, frame_bytes(masks[frame_index]))
                for byte in pair
            ]
        lines.extend(["", f"    // Frame {frame_index}"])
        for start in range(0, len(pixels), 16):
            lines.append(
                "    "
                + ", ".join(f"0x{byte:02x}" for byte in pixels[start : start + 16])
                + ","
            )
    lines.append("};")
    return lines


def generate(root=ROOT):
    world = export_world(root)
    tile_frames = read_tileset_frames(root)
    actor_frames, actor_masks = read_sprite_sheet("actors16.png", 12, 16, root)
    art = [
        "/* Generated; 16x16 tiles and alpha-derived actor plus masks. */",
        '#include "rpg.h"',
        "",
        "// clang-format off: preserve frame byte groups.",
    ]
    art += sprite_lines("TILE_SPRITES", tile_frames)
    art.append("")
    art += sprite_lines("ACTOR_SPRITES", actor_frames, actor_masks)
    art.append("// clang-format on")
    return {**world, "sprites.cpp": "\n".join(art) + "\n"}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    target = ROOT / "generated"
    target.mkdir(exist_ok=True)
    try:
        outputs = generate()
    except (ValueError, OSError, ET.ParseError) as error:
        parser.error(str(error))
    for name, text in outputs.items():
        path = target / name
        if args.check:
            if not path.exists() or path.read_bytes() != text.encode("utf-8"):
                parser.error(f"{path} is stale; run python tools/generate_assets.py")
        else:
            if not path.exists() or path.read_bytes() != text.encode("utf-8"):
                path.write_text(text, encoding="utf-8", newline="\n")
    print(
        "Asset generation/check passed (named locations, map shapes, sprite layouts)."
    )


if __name__ == "__main__":
    main()
