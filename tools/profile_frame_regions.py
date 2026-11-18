"""Classify exclusive frame cycles using DWARF inline ranges, including full LTO."""
import re

REGIONS = ["other", "gameplay", "cache_reload", "tiles", "overlap_repair", "npc_frame_begin",
           "npc_begin", "npc_dispatch", "npc_end", "npc_update", "npc_movement_collision",
           "npc_player_begin", "npc_player_collision", "player_movement", "npc_interaction",
           "dialogue", "terrain_render", "actor_render", "overlay_render", "display",
           "terrain_collision", "player_terrain_collision"]


def operation(name):
    if any(key in name for key in ("process_area", "process_selected_area", "process_all_chunks",
                                   "StaticChunk")):
        match = re.search(r"L12HandlerStage([0-3])", name)
        if match:
            return ["gameplay", "tiles", "dialogue", "npc_dispatch"][int(match[1])]
    for key, value in [
        ("npc_stage_begin", "npc_begin"), ("npc_stage_end", "npc_end"),
        ("npc_offer_candidate", "npc_dispatch"), ("npc_detail15offer_candidate", "npc_dispatch"), ("npc_update", "npc_update"),
        ("npc_begin_frame", "npc_frame_begin"), ("npc_interact", "npc_interaction"),
        ("npc_player_clear", "npc_player_collision"), ("npc_player_begin", "npc_player_begin"),
        ("actor_move", "player_movement"), ("repair_player_overlap", "overlap_repair"),
        ("reload_tile", "cache_reload"), ("draw_tiles", "terrain_render"),
        ("draw_actors", "actor_render"), ("draw_ambient", "overlay_render"),
        ("draw_gust", "overlay_render"), ("draw_birds", "overlay_render"),
        ("avm_display", "display"), ("player_terrain_clear", "player_terrain_collision"),
        ("terrain_clear", "terrain_collision"), ("step_actor", "player_movement"),
        ("sidestep_corner", "player_movement"), ("sidestep_admissible", "player_movement"),
        ("player_leading_edge", "player_movement"), ("eject_actor", "overlap_repair"),
        ("position_clear", "npc_movement_collision"),
        ("step", "npc_movement_collision"), ("wander_walk", "npc_movement_collision"),
    ]:
        if key in name:
            return value
    return None


def inline_regions(dwarf):
    nodes, stack, node = {}, [], None
    for row in dwarf.splitlines():
        match = re.match(r"0x([0-9a-f]+):( +)(DW_TAG_\w+|NULL)", row)
        if match:
            offset, depth, tag = int(match[1], 16), len(match[2]), match[3]
            while stack and stack[-1]["depth"] >= depth:
                stack.pop()
            node = {"depth": depth, "ranges": []}
            nodes[offset] = node
            if tag != "NULL":
                stack.append(node)
            continue
        if node is None:
            continue
        match = re.search(r'DW_AT_(name|linkage_name)\s+\("(.*)"\)', row)
        if match:
            node[match[1]] = match[2]
        match = re.search(r"DW_AT_abstract_origin\s+\(0x([0-9a-f]+)", row)
        if match:
            node["ref"] = int(match[1], 16)
        match = re.search(r"DW_AT_low_pc\s+\(0x([0-9a-f]+)\)", row)
        if match:
            node["low"] = int(match[1], 16)
        match = re.search(r"DW_AT_high_pc\s+\(0x([0-9a-f]+)\)", row)
        if match:
            node["ranges"].append((node.get("low", 0), int(match[1], 16)))
        match = re.search(r"\[0x([0-9a-f]+), 0x([0-9a-f]+)\)", row)
        if match:
            node["ranges"].append((int(match[1], 16), int(match[2], 16)))

    def name(item):
        reference = nodes.get(item.get("ref"))
        return name(reference) if reference else item.get("linkage_name", item.get("name", ""))

    result = []
    for item in nodes.values():
        region = operation(name(item))
        if region:
            result.extend((low, high, item["depth"], REGIONS.index(region))
                          for low, high in item["ranges"])
    return result
