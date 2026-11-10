"""16x16 atlas/mask packing and finite Tiled authoring contracts."""

import io
import re
import shutil
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
# The largest logical map the exporter accepts.
MAX_WIDTH, MAX_HEIGHT = 240, 248
sys.path.insert(0, str(ROOT / "tools"))
from generate_assets import (
    COLLISION_OVERLAY,
    map_preview_path,
    OUTSIDE_PREVIEW_GRAY,
    collision_overlay,
    collision_pixel_solid,
    export_world,
    generate,
    load_world,
    map_previews,
)


class Pipeline(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(
            prefix="rpg16-test-", dir=ROOT / "build"
        )
        self.root = Path(self.temp.name)
        shutil.copytree(ROOT / "assets", self.root / "assets")
        self.path = self.root / "assets/maps/outdoor.tmx"
        self.tree = ET.parse(self.path)

    def tearDown(self):
        self.temp.cleanup()

    def write(self):
        self.tree.write(self.path, encoding="utf-8", xml_declaration=True)

    def sign(self):
        return self.object("GroveSign")

    def object(self, name):
        return self.tree.find(f".//object[@name='{name}']")

    def tile_of(self, name):
        obj = self.object(name)
        return int(float(obj.get("x"))) // 16, int(float(obj.get("y"))) // 16

    def move_to(self, obj, tx, ty):
        obj.set("x", str(16 * tx + 8))
        obj.set("y", str(16 * ty + 8))

    def fit_objects(self, width, height):
        """Move every named point into a small map: the spawn in the middle, the rest on
        distinct tiles, so that the map is still a valid authoring input."""
        spawn = (width // 2, height // 2)
        free = [(x, y) for y in range(height) for x in range(width) if (x, y) != spawn]
        for obj in self.tree.getroot().iter("object"):
            if not obj.get("name"):
                continue
            if obj.get("name") == "PlayerStart":
                self.move_to(obj, *spawn)
            else:
                self.move_to(obj, *free.pop(0))
        return spawn

    def fails(self, message):
        self.write()
        with self.assertRaisesRegex(ValueError, message):
            export_world(self.root)

    def dimensions(self, width, height):
        self.tree.getroot().set("width", str(width))
        self.tree.getroot().set("height", str(height))
        layer = self.tree.find("layer")
        layer.set("width", str(width))
        layer.set("height", str(height))
        # gid 7 is GRASS_06, the passable floor. gid 1 is EMPTY and is solid.
        layer.find("data").text = ",".join(["7"] * (width * height))

    def test_determinism_and_stable_atlas_indices(self):
        first = generate(self.root)
        self.assertEqual(first, generate(self.root))
        _, tiles, _ = load_world(self.root)
        self.assertEqual(list(tiles), list(range(255)))
        self.assertEqual(tiles[0x0F]["frame"], 15)
        self.assertEqual(tiles[0x10]["frame"], 16)
        self.assertEqual(tiles[0x41]["symbol"], "SIGN")
        self.assertIn("TILE_COUNT = 255", first["tile_ids.h"])
        self.assertNotIn("TILE_ATLAS_FF", first["tile_ids.h"])

    def test_all_bitmap_and_alpha_mask_pixels(self):
        source = generate(self.root)["sprites.cpp"]
        for name, file, frames, masked in [
            ("TILE_SPRITES", "tiles16.png", 255, False),
            ("ACTOR_SPRITES", "actors16.png", 192, True),
        ]:
            body = source.split(name + "[] = {", 1)[1].split("};", 1)[0]
            body = re.sub(r"//[^\n]*", "", body)
            data = [int(v, 0) for v in re.findall(r"0x[0-9a-f]+|\b\d+\b", body)]
            self.assertEqual(data[:2], [16, 16])
            stride = 64 if masked else 32
            self.assertEqual(len(data), 2 + frames * stride)
            image = Image.open(self.root / "assets" / "images" / file).convert("RGBA")
            columns = image.width // 16
            opaque_black, transparent = False, False
            for frame in range(frames):
                for y in range(16):
                    for x in range(16):
                        r, g, b, a = image.getpixel(
                            (frame % columns * 16 + x, frame // columns * 16 + y)
                        )
                        offset = (y // 8 * 16 + x) * (2 if masked else 1)
                        byte = data[2 + frame * stride + offset]
                        self.assertEqual((byte >> (y % 8)) & 1, bool(r and a))
                        if masked:
                            mask = data[2 + frame * stride + offset + 1]
                            self.assertEqual((mask >> (y % 8)) & 1, bool(a))
                            opaque_black |= a == 255 and r == 0
                            transparent |= a == 0
            if masked:
                self.assertTrue(opaque_black and transparent)
            else:
                self.assertEqual(data[2 + 0x0F * 32 : 2 + 0x10 * 32], [0] * 32)

    def test_reserved_gid_is_not_a_tile(self):
        payload = self.tree.find("layer/data")
        payload.text = "256" + payload.text[payload.text.index(",") :]
        self.fails("unsupported tile reference")

    def test_reserved_cell_metadata(self):
        path = self.root / "assets/tilesets/world.tsx"
        tree = ET.parse(path)
        tree.find("tile[@id='255']/properties/property").set("value", "false")
        tree.write(path)
        with self.assertRaisesRegex(ValueError, "reserved"):
            export_world(self.root)

    def test_nonbinary_alpha_and_opaque_tiles_validation(self):
        path = self.root / "assets/images/actors16.png"
        image = Image.open(path).convert("RGBA")
        image.putpixel((0, 0), (0, 0, 0, 127))
        image.save(path)
        with self.assertRaisesRegex(ValueError, "binary monochrome and alpha"):
            generate(self.root)
        path = self.root / "assets/images/tiles16.png"
        image = Image.open(path).convert("RGBA")
        image.putpixel((0, 0), (0, 0, 0, 0))
        image.save(path)
        with self.assertRaisesRegex(ValueError, "opaque monochrome"):
            export_world(self.root)

    def test_sign_relocation_crosses_chunk(self):
        tx, ty = self.tile_of("GroveSign")
        before = export_world(self.root)
        nx = (tx // 8 + 1) * 8  # The first tile of the next chunk column.
        self.move_to(self.sign(), nx, ty)
        self.write()
        after = export_world(self.root)
        self.assertIn(f"GroveSign{{1, {tx}, {ty}}}", before["map_positions.h"])
        self.assertIn(f"GroveSign{{1, {nx}, {ty}}}", after["map_positions.h"])
        body = after["chunk_dispatch.cpp"].split(
            f"static void run_chunk_1_{nx // 8}_{ty // 4}()", 1
        )[1]
        self.assertIn("Handler<positions::GroveSign, STAGE>{}.read_sign()", body)

    def test_maximum_map_and_high_chunk_index(self):
        width, height = MAX_WIDTH, MAX_HEIGHT
        self.dimensions(width, height)
        self.move_to(self.sign(), width - 1, height - 1)
        self.write()
        result = export_world(self.root)
        columns, rows = -(-width // 8), -(-height // 4)
        self.assertIn(f"GroveSign{{1, {width - 1}, {height - 1}}}", result["map_positions.h"])
        self.assertIn(f"OUTDOOR_CHUNK_COLUMNS = {columns}", result["world_assets.h"])
        self.assertIn(f"OUTDOOR_CHUNK_ROWS = {rows}", result["world_assets.h"])
        last = (rows - 1) * columns + columns - 1
        self.assertIn(
            f"case {last}: run_chunk_1_{columns - 1}_{rows - 1}<STAGE>()", result["chunk_dispatch.cpp"]
        )
        data = (
            result["map_assets.cpp"]
            .split("OUTDOOR_TILES[] = {", 1)[1]
            .split("};", 1)[0]
        )
        self.assertEqual(len(re.findall(r"\d+", data)), width * height)

    def test_row_major_map_without_chunk_padding(self):
        self.dimensions(19, 13)
        spawn = self.fit_objects(19, 13)
        gids = [1 + (x + y * 19) % 255 for y in range(13) for x in range(19)]
        # Keep the spawn passable while giving every other tile a distinct index, including rows
        # and columns across handler chunks.
        gids[spawn[1] * 19 + spawn[0]] = 7
        self.tree.find("layer/data").text = ",".join(map(str, gids))
        self.write()
        result = export_world(self.root)
        body = (
            result["map_assets.cpp"]
            .split("OUTDOOR_TILES[] = {", 1)[1]
            .split("};", 1)[0]
        )
        data = list(map(int, re.findall(r"\d+", body)))
        self.assertEqual(len(data), 19 * 13)
        self.assertEqual(data, [gid - 1 for gid in gids])

    def test_rectangle_expansion_at_four_chunks(self):
        # A two-by-two rectangle whose top-left tile sits just before a chunk corner.
        tx, ty = self.tile_of("GroveSign")
        bx, by = (tx // 8 + 1) * 8, (ty // 4 + 1) * 4
        rx, ry = bx - 1, by - 1
        sign = self.sign()
        sign.set("x", str(16 * rx))
        sign.set("y", str(16 * ry))
        sign.set("width", "32")
        sign.set("height", "32")
        self.write()
        result = export_world(self.root)
        self.assertIn(f"GroveSignRegion{{{{1, {rx}, {ry}}}, 2, 2}}", result["map_positions.h"])
        for i, (dx, dy) in enumerate(((0, 0), (1, 0), (0, 1), (1, 1))):
            body = (
                result["chunk_dispatch.cpp"]
                .split(f"static void run_chunk_1_{rx // 8 + dx}_{ry // 4 + dy}()", 1)[1]
                .split("\ntemplate <HandlerStage STAGE>", 1)[0]
            )
            self.assertIn(f"Handler<positions::GroveSign_{i}, STAGE>", body)

    def test_fractional_rectangle_edges(self):
        tx, ty = self.tile_of("GroveSign")
        bx, by = (tx // 8 + 1) * 8, (ty // 4 + 1) * 4
        rx, ry = bx - 1, by - 1
        self.sign().set("x", str(16 * rx + 15.9))
        self.sign().set("y", str(16 * ry + 15.9))
        self.sign().set("width", "0.2")
        self.sign().set("height", "0.2")
        self.write()
        self.assertIn(
            f"GroveSignRegion{{{{1, {rx}, {ry}}}, 2, 2}}",
            export_world(self.root)["map_positions.h"],
        )

    def test_safe_spawn_and_duplicate_names(self):
        self.tree.find("properties/property[@name='spawn']").set("value", "GroveSign")
        self.fails("tiles without collision")
        self.tree.find("properties/property[@name='spawn']").set("value", "PlayerStart")
        self.sign().set("name", "PlayerStart")
        self.fails("duplicate world location")

    def test_reject_map_dimensions_and_8_pixel_maps(self):
        for width, height in ((0, 12), (241, 12), (18, 249), (256, 256)):
            self.dimensions(width, height)
            self.fails("240x248")
        self.dimensions(18, 12)
        self.tree.getroot().set("tilewidth", "8")
        self.fails("16x16")

    def test_flips_compression_and_object_contract(self):
        payload = self.tree.find("layer/data")
        original = payload.text
        payload.text = str(0x80000001) + original[original.index(",") :]
        self.fails("flipped/rotated")
        payload.text = original
        payload.set("compression", "zlib")
        self.fails("uncompressed finite CSV")
        del payload.attrib["compression"]
        self.sign().set("type", "Sign")
        self.fails("object types")

    def test_default_handlers_and_private_points(self):
        group = self.tree.find("objectgroup")
        for name in ("Pending", None):
            attributes = dict(x="216", y="152")
            if name:
                attributes["name"] = name
            obj = ET.SubElement(group, "object", **attributes)
            ET.SubElement(
                ET.SubElement(obj, "properties"),
                "property",
                name="handler",
                value="pending_action",
            )
        self.write()
        result = export_world(self.root)
        self.assertEqual(
            result["handler_defaults.h"].count(
                "void pending_action(Tag<POS, STAGE>) {}"
            ),
            1,
        )
        self.assertIn(
            "Handler<positions::Pending, STAGE>", result["chunk_dispatch.cpp"]
        )
        self.assertIn("GENERATED_TILE_1_", result["handler_defaults.h"])
        self.assertNotIn("GENERATED_TILE", result["map_positions.h"])
        for name in ("Handler", "POS", "STAGE", "Tag", "Dialogue"):
            self.sign().find("properties/property").set("value", name)
            self.fails("conflicts with the generated Handler")

    def test_route_markers_and_ids(self):
        sx, sy = self.tile_of("PlayerStart")
        group = ET.SubElement(self.tree.getroot(), "objectgroup", name="Routes")
        route = ET.SubElement(group, "object", name="TestRoute", x=str(16 * sx + 8), y=str(16 * sy + 8))
        line = ET.SubElement(route, "polyline", points="0,0 0,16")
        self.write()
        result = export_world(self.root)
        self.assertIn("TestRoute = 1", result["npc_routes.h"])
        self.assertIn(f"{{{sx}, {sy}}}", result["npc_routes.cpp"])
        self.assertIn(f"{{{sx}, {sy + 1}}}", result["npc_routes.cpp"])
        route.set("x", str(16 * sx + 8.5))
        self.write()
        self.assertEqual(result, export_world(self.root))
        route.set("x", str(16 * sx))
        self.fails("inside tile boundaries")
        route.set("x", str(16 * sx + 8))
        line.set("points", "0,0")
        self.fails("2..255")

def write_handler_fixture(output):
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(
        prefix="handler-fixture-", dir=output.parent
    ) as temp:
        root = Path(temp)
        shutil.copytree(ROOT / "assets", root / "assets")
        path = root / "assets/maps/outdoor.tmx"
        tree = ET.parse(path)
        group = tree.find("objectgroup")
        field = tree.find(".//object[@name='TestField']")
        fx, fy = int(float(field.get("x"))) // 16, int(float(field.get("y"))) // 16
        for name in ("Pending0", "Pending1", None):
            obj = ET.SubElement(
                group,
                "object",
                x=str(16 * fx + 8),
                y=str(16 * fy + 8),
                **({"name": name} if name else {}),
            )
            ET.SubElement(
                ET.SubElement(obj, "properties"),
                "property",
                name="handler",
                value="pending_action",
            )
        tree.write(path, encoding="utf-8", xml_declaration=True)
        for name, content in export_world(root).items():
            (output / name).write_text(content, encoding="utf-8", newline="\n")


class CollisionOverlay(unittest.TestCase):
    def test_pixel_rule_counts_and_quadrants(self):
        # The top half covers 128 pixels. Each quadrant nibble covers 64 pixels with
        # diagonal centers shared by two triangles, so a single full quadrant is 64.
        self.assertEqual(sum(collision_pixel_solid(0x00FF, x, y) for x in range(16) for y in range(16)), 128)
        self.assertEqual(sum(collision_pixel_solid(0xF000, x, y) for x in range(16) for y in range(16)), 64)
        self.assertEqual(sum(collision_pixel_solid(0xFFFF, x, y) for x in range(16) for y in range(16)), 256)
        self.assertTrue(collision_pixel_solid(0x0001, 4, 0) and not collision_pixel_solid(0x0001, 0, 4))

    def test_ramps_are_clean_diagonals(self):
        # 0xF660 is solid where x + y >= 15, and 0xCF0C is solid where y >= x.
        for y in range(16):
            for x in range(16):
                self.assertEqual(collision_pixel_solid(0xF660, x, y), x + y >= 15, (x, y))
                self.assertEqual(collision_pixel_solid(0xCF0C, x, y), y >= x, (x, y))
                # 0xC600 is the triangle below the center and the two bottom corners.
                self.assertEqual(collision_pixel_solid(0xC600, x, y), 2 * y - 15 >= abs(2 * x - 15), (x, y))

    def test_overlay_is_current_png(self):
        image_path = ROOT / COLLISION_OVERLAY
        self.assertTrue(image_path.exists())
        with image_path.open("rb") as handle:
            self.assertEqual(handle.read(8), b"\x89PNG\r\n\x1a\n")
        self.assertEqual(image_path.read_bytes(), collision_overlay(ROOT))


class MapPreviews(unittest.TestCase):
    def test_previews_are_current_pngs(self):
        for name, data in map_previews(ROOT).items():
            self.assertEqual(data[:8], b"\x89PNG\r\n\x1a\n", name)
            self.assertEqual((ROOT / name).read_bytes(), data, name)

    def test_each_tile_is_its_atlas_cell_at_native_size(self):
        maps, _, _ = load_world(ROOT)
        atlas = Image.open(ROOT / "assets/images/tiles16.png").convert("L")
        previews = map_previews(ROOT)
        for m in maps:
            name = map_preview_path(m.symbol)
            image = Image.open(io.BytesIO(previews[name])).convert("L")
            self.assertEqual(image.size, (m.width * 16, m.height * 16), name)
            for index, runtime in enumerate(m.data):
                x, y = index % m.width * 16, index // m.width * 16
                cell = image.crop((x, y, x + 16, y + 16)).tobytes()
                if runtime == 255:
                    self.assertEqual(set(cell), {OUTSIDE_PREVIEW_GRAY}, (name, x, y))
                    continue
                left, top = runtime % 16 * 16, runtime // 16 * 16
                expected = atlas.crop((left, top, left + 16, top + 16)).tobytes()
                self.assertEqual(cell, expected, (name, x, y, runtime))


if __name__ == "__main__":
    if len(sys.argv) == 3 and sys.argv[1] == "--write-handler-fixture":
        write_handler_fixture(Path(sys.argv[2]))
    else:
        unittest.main()
