"""16x16 atlas/mask packing and finite Tiled authoring contracts."""

import re
import shutil
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from generate_assets import export_world, generate, load_world


class Pipeline(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(
            prefix="rpg16-test-", dir=ROOT / "build"
        )
        self.root = Path(self.temp.name)
        for name in ("assets", "assets_src"):
            shutil.copytree(ROOT / name, self.root / name)
        self.path = self.root / "assets/maps/outdoor.tmx"
        self.tree = ET.parse(self.path)

    def tearDown(self):
        self.temp.cleanup()

    def write(self):
        self.tree.write(self.path, encoding="utf-8", xml_declaration=True)

    def sign(self):
        return self.tree.find(".//object[@name='GroveSign']")

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
        layer.find("data").text = ",".join(["1"] * (width * height))

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
            image = Image.open(self.root / "assets_src" / file).convert("RGBA")
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
        path = self.root / "assets_src/actors16.png"
        image = Image.open(path).convert("RGBA")
        image.putpixel((0, 0), (0, 0, 0, 127))
        image.save(path)
        with self.assertRaisesRegex(ValueError, "binary monochrome and alpha"):
            generate(self.root)
        path = self.root / "assets_src/tiles16.png"
        image = Image.open(path).convert("RGBA")
        image.putpixel((0, 0), (0, 0, 0, 0))
        image.save(path)
        with self.assertRaisesRegex(ValueError, "opaque monochrome"):
            export_world(self.root)

    def test_sign_relocation_crosses_chunk(self):
        before = export_world(self.root)
        self.sign().set("x", "136")
        self.write()
        after = export_world(self.root)
        self.assertIn("GroveSign{1, 7, 4}", before["map_positions.h"])
        self.assertIn("GroveSign{1, 8, 4}", after["map_positions.h"])
        body = after["chunk_dispatch.cpp"].split("static void run_chunk_1_1_1()", 1)[1]
        self.assertIn("Handler<positions::GroveSign, STAGE>{}.read_sign()", body)

    def test_maximum_map_and_high_chunk_index(self):
        self.dimensions(240, 248)
        self.sign().set("x", "3832")
        self.sign().set("y", "3960")
        self.write()
        result = export_world(self.root)
        self.assertIn("GroveSign{1, 239, 247}", result["map_positions.h"])
        self.assertIn("OUTDOOR_CHUNK_COLUMNS = 30", result["world_assets.h"])
        self.assertIn("OUTDOOR_CHUNK_ROWS = 62", result["world_assets.h"])
        self.assertIn(
            "case 1859: run_chunk_1_29_61<STAGE>()", result["chunk_dispatch.cpp"]
        )
        data = (
            result["map_assets.cpp"]
            .split("OUTDOOR_TILES[] = {", 1)[1]
            .split("};", 1)[0]
        )
        self.assertEqual(len(re.findall(r"\d+", data)), 59520)

    def test_padding_and_chunk_layout(self):
        self.dimensions(19, 13)
        self.write()
        result = export_world(self.root)
        body = (
            result["map_assets.cpp"]
            .split("OUTDOOR_TILES[] = {", 1)[1]
            .split("};", 1)[0]
        )
        data = list(map(int, re.findall(r"\d+", body)))
        self.assertEqual(len(data), 3 * 4 * 32)
        for cy in range(4):
            for cx in range(3):
                for y in range(4):
                    for x in range(8):
                        expected = 0 if cx * 8 + x < 19 and cy * 4 + y < 13 else 255
                        self.assertEqual(data[(cy * 3 + cx) * 32 + y * 8 + x], expected)

    def test_rectangle_expansion_at_four_chunks(self):
        sign = self.sign()
        sign.set("x", "112")
        sign.set("y", "48")
        sign.set("width", "32")
        sign.set("height", "32")
        self.write()
        result = export_world(self.root)
        self.assertIn("GroveSignRegion{{1, 7, 3}, 2, 2}", result["map_positions.h"])
        for i, (cx, cy) in enumerate(((0, 0), (1, 0), (0, 1), (1, 1))):
            body = (
                result["chunk_dispatch.cpp"]
                .split(f"static void run_chunk_1_{cx}_{cy}()", 1)[1]
                .split("\ntemplate <HandlerStage STAGE>", 1)[0]
            )
            self.assertIn(f"Handler<positions::GroveSign_{i}, STAGE>", body)

    def test_fractional_rectangle_edges(self):
        self.sign().set("x", "127.9")
        self.sign().set("y", "63.9")
        self.sign().set("width", "0.2")
        self.sign().set("height", "0.2")
        self.write()
        self.assertIn(
            "GroveSignRegion{{1, 7, 3}, 2, 2}",
            export_world(self.root)["map_positions.h"],
        )

    def test_safe_spawn_and_duplicate_names(self):
        self.tree.find("properties/property[@name='spawn']").set("value", "GroveSign")
        self.fails("passable baseline")
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
        group = ET.SubElement(self.tree.getroot(), "objectgroup", name="Routes")
        route = ET.SubElement(group, "object", name="TestRoute", x="88", y="88")
        line = ET.SubElement(route, "polyline", points="0,0 0,16")
        self.write()
        result = export_world(self.root)
        self.assertIn("TestRoute = 1", result["npc_routes.h"])
        self.assertIn("{5, 5}", result["npc_routes.cpp"])
        self.assertIn("{5, 6}", result["npc_routes.cpp"])
        route.set("x", "88.5")
        self.write()
        self.assertEqual(result, export_world(self.root))
        route.set("x", "80")
        self.fails("inside tile boundaries")
        route.set("x", "88")
        line.set("points", "0,0")
        self.fails("2..255")


def write_handler_fixture(output):
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(
        prefix="handler-fixture-", dir=output.parent
    ) as temp:
        root = Path(temp)
        for name in ("assets", "assets_src"):
            shutil.copytree(ROOT / name, root / name)
        path = root / "assets/maps/outdoor.tmx"
        tree = ET.parse(path)
        group = tree.find("objectgroup")
        for name in ("Pending0", "Pending1", None):
            obj = ET.SubElement(
                group, "object", x="216", y="152", **({"name": name} if name else {})
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


if __name__ == "__main__":
    if len(sys.argv) == 3 and sys.argv[1] == "--write-handler-fixture":
        write_handler_fixture(Path(sys.argv[2]))
    else:
        unittest.main()
