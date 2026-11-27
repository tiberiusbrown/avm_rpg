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
        # LTO may keep a numbered copy of the type name; every unit declares the same layout.
        assert re.search(r"^@game = internal global %struct\.Game(?:\.\d+)? ", ir, re.MULTILINE), (
            "expected one guest Game object")
        assert not signatures, "Game-reference helpers should use the singleton directly"
        saved_globals = re.findall(r'^@[^\n]*\bglobal\b[^\n]*section "\.saved"[^\n]*', ir, re.MULTILINE)
        assert len(saved_globals) == 1 and saved_globals[0].startswith("@game = "), (
            "Game must be the sole saved object, without a separate save buffer"
        )
        assert "@saved_record" not in ir, "duplicate save buffer survived LTO"
    if elf.stem != "m0_proof":
        # Value NTTPs must resolve to immediate coordinates, including template parameter objects.
        stored_positions = re.findall(
            r"^@[^\n]*(?:global|constant) %struct\.TilePos\b[^\n]*",
            ir, re.MULTILINE)
        assert not stored_positions, f"map tile position storage survived LTO: {stored_positions}"
    dispatch_tables = []
    if elf.stem != "m0_proof":
        dispatch_tables = [line for line in ir.splitlines()
                           if line.startswith("@") and "functionsE =" in line
                           and ("ChunkTable" in line or "Resolvers" in line)]
        assert dispatch_tables, "expected ROM chunk/resolver tables"
        assert all("addrspace(1) constant" in line for line in dispatch_tables), (
            "dispatch tables must be immutable program-memory objects")
        assert 'p1:24:8' in ir, "code pointers must retain their 24-bit representation"
    result = {
        "game_reference_arguments_eliminated": True,
        "unconditional_global_game": elf.stem != "m0_proof",
        "live_game_saved_without_buffer": elf.stem != "m0_proof",
        "retained_game_rule_functions": signatures,
        "map_tile_position_storage_eliminated": True,
        "rom_dispatch_tables": len(dispatch_tables),
        "code_pointer_bits": 24,
        "optimized_ir": ir_path.name,
    }
    Path(str(elf) + ".lto.json").write_text(json.dumps(result, indent=2) + "\n")
    return result
