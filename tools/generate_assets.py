"""Deterministic finite TMX/TSX -> native C++17/AVM program assets."""

import argparse
import re
import sys
import xml.etree.ElementTree as ET
from dataclasses import dataclass
from decimal import Decimal, InvalidOperation, ROUND_CEILING
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]


def require(ok, where, message):
    if not ok:
        raise ValueError(f"{where}: {message}")


def identifier(value, where):
    keywords = set("alignas alignof and and_eq asm auto bitand bitor bool break case catch char char16_t char32_t class compl const constexpr const_cast continue decltype default delete do double dynamic_cast else enum explicit export extern false float for friend goto if inline int long mutable namespace new noexcept not not_eq nullptr operator or or_eq private protected public register reinterpret_cast return short signed sizeof static static_assert static_cast struct switch template this thread_local throw true try typedef typeid typename union unsigned using virtual void volatile wchar_t while xor xor_eq".split())
    require(isinstance(value, str) and bool(re.fullmatch(r"[A-Za-z][A-Za-z0-9_]*", value)) and value not in keywords,
            where, f"invalid C++ identifier {value!r}")
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
    return image.get_flattened_data() if hasattr(image, "get_flattened_data") else image.getdata()


def tileset(path):
    node = ET.parse(path).getroot()
    require(node.tag == "tileset" and node.get("tilewidth") == node.get("tileheight") == "8",
            path, "expected an external 8x8 tileset")
    require(not any(node.find(tag) is not None for tag in ("tileoffset", "transformations", "wangsets")),
            path, "tile offsets, transformations and wangsets are unsupported")
    count, columns = integer(node, "tilecount", path), integer(node, "columns", path)
    require(0 < count <= 255 and columns > 0, path, "tileset must have 1..255 tiles in an atlas")
    image = node.find("image")
    require(image is not None, path, "atlas image required")
    image_path = (path.parent / image.get("source")).resolve()
    with Image.open(image_path) as pixels:
        require(pixels.size == (columns * 8, ((count + columns - 1) // columns) * 8),
                path, "image dimensions do not match the 8x8 atlas")
        require(all(p in ((0, 0, 0, 255), (255, 255, 255, 255))
                    for p in pixel_values(pixels.convert("RGBA"))), path, "use opaque monochrome pixels")
    records = {}
    for tile in node.findall("tile"):
        local = integer(tile, "id", path)
        require(0 <= local < count and local not in records, path, "invalid/duplicate tile id")
        require(len(tile) == 1 and tile[0].tag == "properties", path,
                "tile animation, per-tile images and collision shapes are unsupported; use solid property")
        p = properties(tile, path)
        runtime = p.get("runtime_id")
        require(type(runtime) is int and 0 <= runtime < 255, path, "explicit runtime_id 0..254 required")
        require(type(p.get("solid")) is bool and type(p.get("portal")) is bool,
                path, "each tile needs bool solid and portal properties")
        records[local] = dict(runtime=runtime, symbol=identifier(p.get("symbol"), path),
                              solid=int(p["solid"]), portal=int(p["portal"]),
                              image=image_path, frame=local, columns=columns)
    require(len(records) == count, path, "declare metadata for every atlas tile")
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


def load_world(root):
    maps, metadata, names = [], {}, {}
    for path in sorted((root / "assets/maps").glob("*.tmx")):
        node = ET.parse(path).getroot()
        require(node.tag == "map" and node.get("orientation") == "orthogonal" and
                node.get("infinite", "0") == "0" and node.get("renderorder", "right-down") == "right-down",
                path, "only finite orthogonal right-down maps are supported")
        require(node.get("tilewidth") == node.get("tileheight") == "8", path, "tiles must be 8x8 pixels")
        w, h = integer(node, "width", path), integer(node, "height", path)
        require(1 <= w <= 240 and 1 <= h <= 248, path,
                f"logical size {w}x{h} exceeds the 240x248 contract (width 248/256 is invalid)")
        p = properties(node, path)
        area, symbol = p.get("area_id"), identifier(p.get("symbol"), path)
        require(type(area) is int and 1 <= area <= 255 and
                all(m.area != area and m.symbol != symbol for m in maps), path,
                "unique explicit area_id 1..255 and symbol required")
        gids = {}
        for reference in node.findall("tileset"):
            require(reference.get("source") is not None, path, "use external TSX tilesets")
            first = integer(reference, "firstgid", path)
            require(first > 0, path, "firstgid must be positive")
            for local, tile in tileset(path.parent / reference.get("source")).items():
                require(first + local not in gids, path, "overlapping tileset GID ranges")
                gids[first + local] = tile["runtime"]
                old = metadata.get(tile["runtime"])
                require(old is None or old == tile, path, "conflicting runtime tile IDs across tilesets")
                metadata[tile["runtime"]] = tile
        data, objects, layers = [255] * (w * h), [], 0
        for child in node:
            require(child.tag in ("properties", "tileset", "layer", "objectgroup"), path,
                    f"unsupported map element {child.tag}")
            if child.tag not in ("layer", "objectgroup"):
                continue
            require(all(float(child.get(k, "0")) == 0 for k in ("offsetx", "offsety", "x", "y")) and
                    child.get("visible", "1") == "1" and child.get("opacity", "1") == "1" and
                    all(float(child.get(k, "1")) == 1 for k in ("parallaxx", "parallaxy")),
                    path, "layer offsets, parallax, hidden/transparent layers are unsupported")
            if child.tag == "layer":
                require(integer(child, "width", path) == w and integer(child, "height", path) == h,
                        path, "tile layer dimensions must match the map")
                payload = child.find("data")
                require(payload is not None and payload.get("encoding") == "csv" and
                        payload.get("compression") is None and len(payload) == 0,
                        path, "use uncompressed finite CSV tile layers")
                try:
                    values = [int(s.strip()) for s in (payload.text or "").strip().strip(",").split(",")]
                except ValueError:
                    raise ValueError(f"{path}: {child.get('name')}: invalid integer in CSV tiles") from None
                require(len(values) == w * h, path, "CSV tile count does not match dimensions")
                for index, gid in enumerate(values):
                    require(0 <= gid < 0x10000000, path, "flipped/rotated tile references are unsupported")
                    require(gid == 0 or gid in gids, path, f"unsupported tile reference GID {gid}")
                    if gid:
                        data[index] = gids[gid]
                layers += 1
                continue
            require(child.get("name") in ("Logic", "Locations"), path, "name object layers Logic or Locations")
            for obj in child.findall("object"):
                name = obj.get("name") or None
                if name is not None:
                    identifier(name, path)
                where = f"{path}:{name or '<unnamed ' + str(len(objects)) + '>'}"
                require(name is None or name not in names, where, "duplicate world location name")
                require(not any(obj.find(t) is not None for t in ("polygon", "polyline", "ellipse", "text", "template"))
                        and obj.get("gid") is None and obj.get("template") is None and
                        float(obj.get("rotation", "0")) == 0, where, "only axis-aligned rectangles/points are supported")
                require(not obj.get("type") and not obj.get("class"), where,
                        "object types are unsupported; use named tile positions")
                try:
                    x, y, ow, oh = (Decimal(obj.get(k, "0")) for k in ("x", "y", "width", "height"))
                    require(all(v.is_finite() for v in (x, y, ow, oh)), where, "coordinates must be finite")
                except InvalidOperation:
                    raise ValueError(f"{where}: invalid object coordinates") from None
                require(0 <= x < w * 8 and 0 <= y < h * 8, where, "object anchor is out of bounds")
                require(ow >= 0 and oh >= 0 and ((ow == oh == 0) or (ow > 0 and oh > 0)),
                        where, "footprint must have two positive dimensions or be a point")
                require(x + ow <= w * 8 and y + oh <= h * 8,
                        where, "feature footprint extends outside the logical map")
                props = properties(obj, where)
                require(set(props) <= {"handler"}, where, "only the handler property is supported on map objects")
                handler = props.get("handler")
                if handler is not None:
                    identifier(handler, where)
                    require(handler not in {"Handler", "POS", "STAGE", "Tag", "Gameplay", "Tiles", "Dialogue"}, where,
                            f"handler name {handler!r} conflicts with the generated Handler declaration")
                tx, ty = int(x // 8), int(y // 8)
                # Half-open rectangles include every tile with a positive-area intersection.
                # Points select their containing tile, independent of alignment.
                right = int(((Decimal(x) + ow) / 8).to_integral_value(rounding=ROUND_CEILING)) if ow else tx + 1
                bottom = int(((Decimal(y) + oh) / 8).to_integral_value(rounding=ROUND_CEILING)) if oh else ty + 1
                rw, rh = right - tx, bottom - ty
                cells = [(cx, cy) for cy in range(ty, bottom) for cx in range(tx, right)]
                points = [dict(name=(name if len(cells) == 1 else f"{name}_{i}") if name else None,
                               symbol=(f"positions::{name}" if len(cells) == 1 else f"positions::{name}_{i}")
                                      if name else f"GENERATED_TILE_{area}_{len(objects)}_{i}", tx=cx, ty=cy)
                          for i, (cx, cy) in enumerate(cells)]
                record = dict(name=name, index=len(objects), tx=tx, ty=ty, rw=rw, rh=rh, rectangle=ow > 0,
                              points=points, area=area, handler=handler, where=where)
                if name is not None:
                    names[name] = record
                objects.append(record)
        require(layers > 0, path, "at least one tile layer required")
        maps.append(Map(area, symbol, w, h, (w + 15) // 16, (h + 7) // 8, data,
                        sorted(objects, key=lambda o: (o["name"] is None, o["name"] or "", o["index"])), p.get("spawn")))
    require(maps, root, "no maps found in assets/maps")
    require(set(metadata) == set(range(len(metadata))) and len({t["symbol"] for t in metadata.values()}) == len(metadata),
            root, "runtime tile IDs must be dense from zero; tile symbols must be unique")
    symbols = set()
    for o in names.values():
        emitted = [point['name'] for point in o['points']]
        if o['rectangle']:
            emitted.append(o['name'] + 'Region')
        for symbol in emitted:
            require(symbol not in symbols, o['where'], f"duplicate generated identifier {symbol}")
            symbols.add(symbol)
    for m in maps:
        tile_owners = {}
        for o in m.objects:
            if not o['handler']:
                continue
            for point in o['points']:
                cell = point['tx'], point['ty']
                tile_owners[cell] = o['name'] or f'unnamed {o["index"]}'
        spawn = names.get(m.spawn)
        require(spawn is not None and spawn['area'] == m.area and len(spawn['points']) == 1,
                m.symbol, "spawn property must reference a single named tile in this area")
        cell = spawn['tx'], spawn['ty']
        tile = m.data[cell[1] * m.width + cell[0]]
        require(tile in metadata and not metadata[tile]['solid'], m.symbol, "spawn must be on passable baseline tiles")
        require(cell not in tile_owners, m.symbol, "safe spawn must not overlap a native handler")
    return sorted(maps, key=lambda m: m.area), metadata, names


def read_tileset_frames(root):
    _, metadata, _ = load_world(root)
    frames = []
    for _, t in sorted(metadata.items()):
        with Image.open(t["image"]) as im:
            x, y = t["frame"] % t["columns"] * 8, t["frame"] // t["columns"] * 8
            pixels = im.convert("RGB")
            frames.append(["".join("#" if pixels.getpixel((x + dx, y + dy))[0] else "."
                                   for dx in range(8)) for dy in range(8)])
    return frames


def export_world(root=ROOT):
    maps, metadata, names = load_world(root)
    prologue = '/* Generated from TMX/TSX; do not edit. */\n'
    areas = [prologue, '#pragma once', '#include <stdint.h>']
    areas += [f'inline constexpr uint8_t AREA_{m.symbol} = {m.area};' for m in maps]
    positions = [prologue, '#pragma once', '#include "world_types.h"', 'namespace positions {']
    for name, o in sorted(names.items()):
        for point in o['points']:
            positions.append(f'inline constexpr TilePos {point["name"]}{{{o["area"]}, {point["tx"]}, {point["ty"]}}};')
        if o['rectangle']:
            positions.append(f'inline constexpr TileRegion {name}Region' + '{{' + f'{o["area"]}, {o["tx"]}, {o["ty"]}' + '}, ' + f'{o["rw"]}, {o["rh"]}' + '};')
    positions.append('}')
    ids = [prologue, '#pragma once', '#include <stdint.h>']
    ids += [f'inline constexpr uint8_t TILE_{t["symbol"]} = {i};' for i, t in sorted(metadata.items())]
    ids.append(f'inline constexpr uint8_t TILE_COUNT = {len(metadata)};')
    header = [prologue, '#pragma once', '#include "rpg.h"', '#include "tile_ids.h"',
              'extern uint8_t const PROGMEM TILE_SOLID[];', 'extern uint8_t const PROGMEM TILE_PORTALS[];']
    assets = [prologue, '#include "world_assets.h"', '#include "chunk.h"']
    for m in maps:
        prefix = m.symbol
        for field, value in (("WIDTH", m.width), ("HEIGHT", m.height), ("CHUNK_COLUMNS", m.columns), ("CHUNK_ROWS", m.rows)):
            header.append(f'inline constexpr uint8_t {prefix}_{field} = {value};')
        header += [f'extern uint8_t const PROGMEM {prefix}_TILES[];', f'extern Area const PROGMEM {prefix}_AREA;']
        data = []
        for cy in range(m.rows):
            for cx in range(m.columns):
                for ly in range(8):
                    for lx in range(16):
                        x, y = cx * 16 + lx, cy * 8 + ly
                        data.append(m.data[y * m.width + x] if x < m.width and y < m.height else 255)
        assets += array_lines(f'constexpr uint8_t PROGMEM {prefix}_TILES[]', data)
        spawn = names[m.spawn]
        assets.append(f'constexpr Area PROGMEM {prefix}_AREA' + '{' + f'{m.width}, {m.height}, {m.columns}, {m.rows}, {prefix}_TILES, ' + '{' + f'{spawn["tx"] * 8}, {spawn["ty"] * 8}' + '}};')
    for array, key in (("TILE_SOLID", "solid"), ("TILE_PORTALS", "portal")):
        assets += array_lines(f'constexpr uint8_t PROGMEM {array}[]', [t[key] for _, t in sorted(metadata.items())])
    for fn, key in (("tile_solid", "solid"), ("tile_portal", "portal")):
        assets += [f'bool {fn}(uint8_t tile) {{', '    switch(tile) {']
        for i, t in sorted(metadata.items()):
            if t[key]:
                assets.append(f'        case {i}: return true;')
        assets += [('        default: return tile >= TILE_COUNT;' if key == 'solid' else
                    '        default: return false;'), '    }', '}']
    assets += ['Area const PROGMEM* world_area(uint8_t id) {', '    switch(id) {']
    assets += [f'        case {m.area}: return &{m.symbol}_AREA;' for m in maps]
    assets += ['        default: return nullptr;', '    }', '}']
    def entries(m, cx, cy):
        return [(o, point) for o in m.objects if o['handler']
                for point in o['points'] if point['tx'] // 16 == cx and point['ty'] // 8 == cy]

    defaults = [prologue, '#pragma once', '#include "chunk.h"', '#include "map_positions.h"']
    private_points = [(o, point) for m in maps for o in m.objects if o['handler'] and not o['name']
                      for point in o['points']]
    if private_points:
        defaults.append('namespace {')
        for o, point in private_points:
            defaults.append(f'inline constexpr TilePos {point["symbol"]}{{{o["area"]}, {point["tx"]}, {point["ty"]}}};')
        defaults.append('}')
    handler_names = sorted({o['handler'] for m in maps for o in m.objects if o['handler']})
    defaults += ['namespace handler_impl {',
                 'template <TilePos const& POS, HandlerStage STAGE> struct Tag {};']
    for name, stage in [('Gameplay', 'STAGE_GAMEPLAY'), ('Tiles', 'STAGE_TILES'), ('Dialogue', 'STAGE_DIALOGUE')]:
        defaults += [f'template <TilePos const& POS> using {name} = Tag<POS, {stage}>;']
    for name in handler_names:
        defaults += ['template <TilePos const& POS, HandlerStage STAGE>', f'void {name}(Tag<POS, STAGE>) {{}}']
    for name in handler_names:
        defaults += ['template <TilePos const& POS, HandlerStage STAGE>', f'void dispatch_{name}() {{',
                     f'    {name}(Tag<POS, STAGE>{{}});', '}']
    defaults += ['}', 'template <TilePos const& POS, HandlerStage STAGE>', 'struct Handler {']
    for name in handler_names:
        defaults += [f'    void {name}() {{', f'        handler_impl::dispatch_{name}<POS, STAGE>();', '    }']
    defaults.append('};')
    dispatch = [prologue, '#include "logic/handlers.h"']
    for m in maps:
        for cy in range(m.rows):
            for cx in range(m.columns):
                points = entries(m, cx, cy)
                if not points:
                    continue
                fn = f'run_chunk_{m.area}_{cx}_{cy}'
                dispatch.append(f'template <HandlerStage STAGE> static void {fn}() {{')
                for o, point in points:
                    dispatch += ['#ifndef RPG_AVM', '    ++feature_trace_count[STAGE];', '#endif']
                    dispatch.append(f'    Handler<{point["symbol"]}, STAGE>{{}}.{o["handler"]}();')
                    dispatch += ['    if constexpr(STAGE != STAGE_TILES) {', '        if(teleported)',
                                 '            return;', '    }']
                dispatch.append('}')
    dispatch += ['template <HandlerStage STAGE> RPG_NOINLINE void run_chunk(ChunkCoord coord) {',
                 '#ifndef RPG_AVM', '    if(chunk_trace_count[STAGE] < 4)',
                 '        chunk_trace[STAGE][chunk_trace_count[STAGE]++] = '
                 '(uint16_t)coord.y * world_area(coord.area)->chunk_columns + coord.x;', '#endif',
                 '    switch(coord.area) {']
    for m in maps:
        if not any(o['handler'] for o in m.objects):
            continue
        dispatch += [f'        case {m.area}:', f'            switch((uint16_t)coord.y * {m.columns} + coord.x) {{']
        for cy in range(m.rows):
            for cx in range(m.columns):
                if entries(m, cx, cy):
                    dispatch.append(f'                case {cy * m.columns + cx}: run_chunk_{m.area}_{cx}_{cy}<STAGE>(); break;')
        dispatch += ['                default: break;', '            }', '            break;']
    dispatch += ['        default: break;', '    }', '}',
                 'template void run_chunk<STAGE_GAMEPLAY>(ChunkCoord);',
                 'template void run_chunk<STAGE_TILES>(ChunkCoord);',
                 'template void run_chunk<STAGE_DIALOGUE>(ChunkCoord);']
    return {name: '\n'.join(lines) + '\n' for name, lines in
            [('area_ids.h', areas), ('map_positions.h', positions), ('tile_ids.h', ids), ('world_assets.h', header),
             ('handler_defaults.h', defaults), ('map_assets.cpp', assets), ('chunk_dispatch.cpp', dispatch)]}


def read_sprite_sheet(name, columns, rows, root=ROOT):
    path = root / "assets_src" / name
    with Image.open(path) as image:
        if image.format != "PNG" or image.size != (columns * 8, rows * 8):
            raise ValueError(f"{path}: expected a {columns * 8}x{rows * 8} PNG")
        pixels = image.convert("RGBA")
        if any(pixel not in ((0, 0, 0, 255), (255, 255, 255, 255)) for pixel in pixel_values(pixels)):
            raise ValueError(f"{path}: use opaque black and white pixels only")

    return [
        [
            "".join(
                "#" if pixels.getpixel((frame_x * 8 + x, frame_y * 8 + y))[0] else "."
                for x in range(8)
            )
            for y in range(8)
        ]
        for frame_y in range(rows)
        for frame_x in range(columns)
    ]


def frame_bytes(rows):
    return [sum(1 << y for y in range(8) if rows[y][x] == "#") for x in range(8)]


def array_lines(declaration, values, row_width=16):
    lines = [f"{declaration} = {{"]
    for start in range(0, len(values), row_width):
        lines.append("    " + ", ".join(map(str, values[start : start + row_width])) + ",")
    lines.append("};")
    return lines


def sprite_lines(name, frames, mask=None):
    lines = [f"constexpr uint8_t PROGMEM {name}[] = {{", "    8, 8, // width, height"]
    for frame_index, rows in enumerate(frames):
        assert len(rows) == 8 and all(len(row) == 8 for row in rows)
        pixels = frame_bytes(rows)
        if mask is not None:
            pixels = [byte for pair in zip(pixels, mask) for byte in pair]
        lines.extend(["", f"    // Frame {frame_index}"])
        lines.extend(f"    // {row}" for row in rows)
        lines.append("    " + ", ".join(f"0x{byte:02x}" for byte in pixels) + ",")
    lines.append("};")
    return lines


def generate(root=ROOT):
    world = export_world(root)
    tile_frames = read_tileset_frames(root)
    actor_frames = read_sprite_sheet("actors.png", 2, 4, root)
    # Clear the silhouette behind each actor, including black facial pixels.
    actor_mask = frame_bytes(read_sprite_sheet("actor_mask.png", 1, 1, root)[0])
    art = [
        "/* Generated; eight actor frames, interleaved plus masks. */",
        '#include "rpg.h"',
        "",
        "// clang-format off: preserve frame artwork and byte groups.",
    ]
    art += sprite_lines("TILE_SPRITES", tile_frames)
    art.append("")
    art += sprite_lines("ACTOR_SPRITES", actor_frames, actor_mask)
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
    print("Asset generation/check passed (named locations, map shapes, sprite layouts).")


if __name__ == "__main__":
    main()
