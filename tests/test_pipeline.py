"""Authoring contract, deterministic regeneration and automatic native relocation."""

from pathlib import Path
import re
import shutil
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from generate_assets import export_world, load_world


class Pipeline(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="tiled-test-", dir=ROOT / "build")
        self.root = Path(self.temp.name)
        for name in ("assets", "assets_src", "src/logic"):
            shutil.copytree(ROOT / name, self.root / name)
        self.path = self.root / "assets/maps/outdoor.tmx"
        self.tree = ET.parse(self.path)

    def tearDown(self):
        self.temp.cleanup()

    def write(self):
        self.tree.write(self.path, encoding="utf-8", xml_declaration=True)

    def gate(self):
        return self.tree.find(".//object[@name='NorthGate']")

    def fails(self, text):
        self.write()
        with self.assertRaisesRegex(ValueError, text):
            export_world(self.root)

    def dimensions(self, w, h):
        self.tree.getroot().set("width", str(w))
        self.tree.getroot().set("height", str(h))
        layer = self.tree.find("layer")
        layer.set("width", str(w))
        layer.set("height", str(h))
        layer.find("data").text = ",".join(["1"] * (w * h))

    def test_gate_relocation_has_no_handwritten_changes(self):
        before = export_world(self.root)
        handwritten = {p: p.read_bytes() for p in (self.root / "src/logic").glob("*")}
        self.gate().set("x", "132")  # tile 15 -> 16; chunk 0 -> 1.
        self.write()
        after = export_world(self.root)
        self.assertIn("NorthGate{1, 15, 2}", before["map_positions.h"])
        self.assertIn("NorthGate{1, 16, 2}", after["map_positions.h"])
        def chunk(text, name):
            return text.split("static void " + name + "(", 1)[1].split("\ntemplate <HandlerStage STAGE>", 1)[0]
        self.assertIn("Handler<positions::NorthGate, STAGE>", chunk(before["chunk_dispatch.cpp"], "run_chunk_1_0_0"))
        self.assertNotIn("Handler<positions::NorthGate, STAGE>", chunk(after["chunk_dispatch.cpp"], "run_chunk_1_0_0"))
        self.assertIn("Handler<positions::NorthGate, STAGE>", chunk(after["chunk_dispatch.cpp"], "run_chunk_1_1_0"))
        self.assertEqual(handwritten, {p: p.read_bytes() for p in handwritten})

    def test_maximum_and_high_flattened_index(self):
        self.dimensions(240, 248)
        self.gate().set("x", "1912")
        self.gate().set("y", "1976")
        self.write()
        result = export_world(self.root)
        self.assertIn("uint8_t OUTDOOR_CHUNK_COLUMNS = 15", result["world_assets.h"])
        self.assertIn("uint8_t OUTDOOR_CHUNK_ROWS = 31", result["world_assets.h"])
        self.assertIn("NorthGate{1, 239, 247}", result["map_positions.h"])
        self.assertIn("case 464: run_chunk_1_14_30<STAGE>()", result["chunk_dispatch.cpp"])
        self.assertIn("switch((uint16_t)coord.y * 15 + coord.x)", result["chunk_dispatch.cpp"])
        data = result["map_assets.cpp"].split("OUTDOOR_TILES[] = {", 1)[1].split("};", 1)[0]
        self.assertEqual(len(re.findall(r"\d+", data)), 15 * 31 * 128)

    def test_reject_dimensions(self):
        for w, h in ((248, 248), (256, 248), (241, 8), (16, 249), (16, 256), (0, 8)):
            with self.subTest(w=w, h=h):
                self.dimensions(w, h)
                self.fails("240x248")

    def test_non_aligned_padding(self):
        self.dimensions(33, 21)
        self.write()
        result = export_world(self.root)
        self.assertIn("uint8_t OUTDOOR_WIDTH = 33", result["world_assets.h"])
        self.assertIn("uint8_t OUTDOOR_CHUNK_COLUMNS = 3", result["world_assets.h"])
        data = result["map_assets.cpp"].split("OUTDOOR_TILES[] = {", 1)[1].split("};", 1)[0]
        values = list(map(int, re.findall(r"\d+", data)))
        self.assertEqual(len(values), 3 * 3 * 128)
        for cy in range(3):
            for cx in range(3):
                for ly in range(8):
                    for lx in range(16):
                        self.assertEqual(values[(cy * 3 + cx) * 128 + ly * 16 + lx],
                                         0 if cx * 16 + lx < 33 and cy * 8 + ly < 21 else 255)

    def test_rectangle_expands_into_its_owning_chunks(self):
        result = export_world(self.root)
        dispatch = result["chunk_dispatch.cpp"]
        live = dispatch
        self.assertEqual(live.count("Handler<positions::EasternBridge_"), 6)
        expected = {(0, 0): [0], (1, 0): [1, 2], (0, 1): [3], (1, 1): [4, 5]}
        for (x, y), indices in expected.items():
            body = live.split(f"static void run_chunk_1_{x}_{y}(", 1)[1].split("\ntemplate <HandlerStage STAGE>", 1)[0]
            for index in range(6):
                self.assertEqual(f"Handler<positions::EasternBridge_{index}, STAGE>" in body, index in indices)
        self.assertIn("EasternBridgeRegion{{1, 15, 7}, 3, 2}", result["map_positions.h"])
        self.assertNotIn("ctx.owns", dispatch)
        self.assertNotIn("Region", dispatch)
        self.assertEqual(result, export_world(self.root))
        self.assertNotIn("function<", dispatch)

    def test_authored_point_centers_and_area_ids(self):
        for path in (self.root / "assets/maps").glob("*.tmx"):
            for obj in ET.parse(path).findall(".//object"):
                self.assertNotIn("type", obj.attrib)
                self.assertNotIn("class", obj.attrib)
                if obj.find("point") is not None:
                    self.assertEqual(float(obj.get("x")) % 8, 4)
                    self.assertEqual(float(obj.get("y")) % 8, 4)
        result = export_world(self.root)
        self.assertIn("uint8_t AREA_OUTDOOR = 1", result["area_ids.h"])
        self.assertNotRegex(result["area_ids.h"], r"uint8_t OUTDOOR =")
        self.assertNotIn("Npc", "".join(result.values()))

    def test_partial_and_fractional_rectangles(self):
        gate = self.gate()
        for x, y, width, height, region in (
            ("121.5", "17.25", "5", "2", "15, 2}, 1, 1"),
            ("127.9", "23.9", "0.2", "0.2", "15, 2}, 2, 2"),
            ("120.1", "16.1", "7.9", "7.9", "15, 2}, 1, 1"),
            ("119.9999", "16", "0.0002", "1", "14, 2}, 2, 1")):
            with self.subTest(x=x):
                for k, v in zip(("x", "y", "width", "height"), (x, y, width, height)):
                    gate.set(k, v)
                self.write()
                self.assertIn("NorthGateRegion{{1, " + region, export_world(self.root)["map_positions.h"])

    def test_duplicate_name(self):
        self.gate().set("name", "PlayerStart")
        self.fails("duplicate world location")

    def test_duplicate_generated_identifier(self):
        self.tree.find(".//object[@name='VillageWell']").set("name", "EasternBridge_0")
        self.fails("duplicate generated identifier")

    def test_spawn_validation(self):
        self.tree.find("properties/property[@name='spawn']").set("value", "Missing")
        self.fails("spawn property")

    def test_fractional_point_is_a_containing_tile(self):
        self.tree.find(".//object[@name='PlayerStart']").set("x", "36.5")
        self.write()
        result = export_world(self.root)
        self.assertIn("TilePos PlayerStart{1, 4, 5}", result["map_positions.h"])
        self.assertNotIn("PixelPos", result["map_positions.h"])

    def test_one_shot_expands_to_separate_tile_handlers(self):
        result = export_world(self.root)
        live = result['chunk_dispatch.cpp']
        for i in range(2):
            self.assertEqual(live.count(f'Handler<positions::Waystone_{i}, STAGE>'), 1)
        self.assertIn('template void run_chunk<STAGE_GAMEPLAY>(ChunkCoord);', live)
        self.assertIn('template void run_chunk<STAGE_TILES>(ChunkCoord);', live)
        self.assertIn('template void run_chunk<STAGE_DIALOGUE>(ChunkCoord);', live)

    def test_objects_only_have_native_handlers(self):
        for path in (self.root / "assets/maps").glob("*.tmx"):
            for obj in ET.parse(path).findall(".//object"):
                self.assertNotIn("type", obj.attrib)
                self.assertNotIn("class", obj.attrib)
                self.assertLessEqual({p.get("name") for p in obj.findall("properties/property")}, {"handler"})
        result = export_world(self.root)
        self.assertNotIn("portal_destination", result["map_assets.cpp"])
        self.assertNotIn("interaction_target", result["chunk_dispatch.cpp"])
        self.assertIn("Handler<positions::HouseDoor, STAGE>", result["chunk_dispatch.cpp"])
        self.assertIn("Handler<GENERATED_TILE_2_2_0, STAGE>", result["chunk_dispatch.cpp"])
        self.assertIn("Handler<positions::NorthGate, STAGE>", result["chunk_dispatch.cpp"])

    def test_alignment_and_overflow(self):
        self.gate().set("x", "121")
        self.write()
        self.assertIn("NorthGate{1, 15, 2}", export_world(self.root)["map_positions.h"])
        self.gate().set("x", "2048")
        self.fails("out of bounds")

    def test_invalid_footprint(self):
        self.gate().set("width", "2048")
        self.gate().set("height", "1")
        self.fails("footprint")

    def test_overlapping_handlers_share_one_ordered_stage_list(self):
        self.gate().set("y", "56")
        self.write()
        dispatch = export_world(self.root)["chunk_dispatch.cpp"]
        body = dispatch.split("static void run_chunk_1_0_0()", 1)[1].split("\ntemplate <HandlerStage STAGE>", 1)[0]
        self.assertLess(body.index(".update_bridge()"), body.index(".update_north_gate()"))
        self.assertIn("Handler<positions::NorthGate, STAGE>", body)

    def test_unimplemented_handler_gets_an_empty_default(self):
        group = self.tree.find("objectgroup")
        for i in range(2):
            obj = ET.SubElement(group, "object", name=f"Pending{i}", x=str(172 + i * 8), y="92")
            ET.SubElement(ET.SubElement(obj, "properties"), "property", name="handler", value="pending_action")
        self.write()
        # Export must not depend on implementations, declarations or header scanning.
        (self.root / "src/logic/handlers.h").unlink()
        result = export_world(self.root)
        defaults = result["handler_defaults.h"]
        self.assertIn("template <TilePos const& POS, HandlerStage STAGE>", defaults)
        self.assertEqual(defaults.count("void pending_action(Tag<POS, STAGE>) {}"), 1)
        for i in range(2):
            self.assertIn(f"Handler<positions::Pending{i}, STAGE>", result["chunk_dispatch.cpp"])
        self.assertEqual(result, export_world(self.root))

    def test_handler_names_must_not_conflict_with_the_generated_class(self):
        prop = self.gate().find("properties/property[@name='handler']")
        for name in ("Handler", "POS", "STAGE", "Tag", "Gameplay", "Tiles", "Dialogue"):
            prop.set("value", name)
            self.fails("conflicts with the generated Handler")

    def test_reused_handler_and_unnamed_objects(self):
        group = self.tree.find("objectgroup")
        ET.SubElement(group, "object", name="SouthGate", x="68", y="92")
        obj = group[-1]
        ET.SubElement(ET.SubElement(obj, "properties"), "property", name="handler", value="update_north_gate")
        ET.SubElement(obj, "point")
        for i in range(2):
            obj = ET.SubElement(group, "object", x=str(164 + i * 8), y="92", width="12", height="4")
            if i:
                obj.set("name", "")
            ET.SubElement(ET.SubElement(obj, "properties"), "property", name="handler", value="update_waystone")
        ET.SubElement(group, "object", x="188", y="92")  # Unnamed marker with no behavior emits nothing.
        self.write()
        result = export_world(self.root)
        dispatch = result['chunk_dispatch.cpp']
        self.assertIn('Handler<positions::SouthGate, STAGE>', dispatch)
        self.assertIn('Handler<positions::NorthGate, STAGE>', dispatch)
        self.assertEqual(result['handler_defaults.h'].count('inline constexpr TilePos GENERATED_TILE_1_'), 4)
        self.assertNotIn('GENERATED_TILE', result['map_positions.h'])
        self.assertEqual(result, export_world(self.root))

    def test_object_types_are_not_supported(self):
        for attribute in ('type', 'class'):
            self.gate().set(attribute, 'TileFeature')
            self.fails('object types are unsupported')
            self.gate().attrib.pop(attribute)

    def test_invalid_cpp_handler(self):
        self.gate().find("properties/property[@name='handler']").set("value", "oops();bad")
        self.fails("invalid C\\+\\+ identifier")

    def test_obsolete_properties_are_rejected(self):
        props = self.gate().find("properties")
        for name in ("destination", "stable_id", "facing", "interaction", "interaction_radius"):
            prop = ET.SubElement(props, "property", name=name, value="obsolete")
            self.fails("only the handler property")
            props.remove(prop)

    def test_invalid_area(self):
        self.tree.find("properties/property[@name='area_id']").set("value", "256")
        self.fails("area_id")

    def test_unsupported_tile_and_flips(self):
        payload = self.tree.find("layer/data")
        original = payload.text
        for gid, error in ((999, "unsupported tile reference"), (0x80000001, "flipped/rotated")):
            payload.text = str(gid) + original[original.index(","):]
            self.fails(error)

    def test_infinite_and_compressed(self):
        self.tree.getroot().set("infinite", "1")
        self.fails("finite orthogonal")
        self.tree.getroot().set("infinite", "0")
        self.tree.find("layer/data").set("compression", "zlib")
        self.fails("uncompressed finite CSV")


def write_handler_fixture(output):
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="handler-fixture-", dir=output.parent) as temp:
        root = Path(temp)
        for name in ("assets", "assets_src"):
            shutil.copytree(ROOT / name, root / name)
        path = root / "assets/maps/outdoor.tmx"
        tree = ET.parse(path)
        group = tree.find("objectgroup")
        for i in range(2):
            obj = ET.SubElement(group, "object", name=f"Pending{i}", x=str(172 + i * 8), y="92")
            ET.SubElement(ET.SubElement(obj, "properties"), "property", name="handler", value="pending_action")
        obj = ET.SubElement(group, "object", x="124", y="140", width="12", height="4")
        ET.SubElement(ET.SubElement(obj, "properties"), "property", name="handler", value="pending_unnamed")
        tree.write(path, encoding="utf-8", xml_declaration=True)
        for name, content in export_world(root).items():
            target = output / name
            if not target.exists() or target.read_text() != content:
                target.write_text(content)


if __name__ == "__main__":
    if len(sys.argv) == 3 and sys.argv[1] == "--write-handler-fixture":
        write_handler_fixture(Path(sys.argv[2]))
    else:
        unittest.main()
