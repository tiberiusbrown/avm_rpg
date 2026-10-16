"""Verify singleton state and compile-time map positions in full-LTO IR."""

import json
from pathlib import Path
import re
import subprocess


def inspect_lto(compiler, elf):
    elf = Path(elf)
    bitcode = Path(str(elf) + ".0.4.opt.bc")
    ir_path = elf.with_suffix(".optimized.ll")
    subprocess.run(
        [str(compiler), "-S", "-emit-llvm", str(bitcode), "-o", str(ir_path)],
        check=True,
    )
    ir = ir_path.read_text()
    signatures = []
    for line in ir.splitlines():
        match = re.match(r"define.*?@([^ (]+)\(", line)
        if not match:
            continue
        name = match[1]
        if not re.search(r"RK?4Game", name):
            continue
        start = match.end()
        end = start
        depth = 1
        while depth:
            assert end < len(line), "unterminated LLVM function signature"
            depth += (line[end] == "(") - (line[end] == ")")
            end += 1
        arguments = line[start : end - 1]
        assert not re.search(r"\bptr\b[^,]*%(?:g|game)(?:[., ]|$)", arguments), (
            f"Game argument survived LTO: {name}({arguments})"
        )
        signatures.append({"function": name, "optimized_arguments": arguments})
    if elf.stem != "m0_proof":
        assert "@game = internal global %struct.Game " in ir, "expected one guest Game object"
        assert not signatures, "Game-reference helpers should use the singleton directly"
    if elf.stem != "m0_proof":
        # Reference NTTPs must resolve to immediate coordinates, not RAM position objects.
        stored_positions = re.findall(
            r"^@[^\n]*(?:positions|GENERATED_TILE)[^\n]*(?:global|constant) %struct\.TilePos[^\n]*",
            ir, re.MULTILINE)
        assert not stored_positions, f"map tile position storage survived LTO: {stored_positions}"
    result = {
        "game_reference_arguments_eliminated": True,
        "unconditional_global_game": elf.stem != "m0_proof",
        "retained_game_rule_functions": signatures,
        "map_tile_position_storage_eliminated": True,
        "optimized_ir": ir_path.name,
    }
    Path(str(elf) + ".lto.json").write_text(json.dumps(result, indent=2) + "\n")
    return result
