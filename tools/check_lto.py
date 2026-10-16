"""Check the actual full-LTO IR for singleton Game reference elimination."""

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
        # The compile preserves IR value names. Every Game reference is named g.
        assert not re.search(r"%g(?:[., ]|$)", arguments), (
            f"Game argument survived LTO: {name}({arguments})"
        )
        signatures.append({"function": name, "optimized_arguments": arguments})
    if elf.stem != "m0_proof":
        assert "@game = internal global %struct.Game " in ir, "expected one guest Game object"
        assert signatures, "no retained Game rule signatures to verify"
    result = {
        "game_reference_arguments_eliminated": True,
        "retained_game_rule_functions": signatures,
        "optimized_ir": ir_path.name,
    }
    Path(str(elf) + ".lto.json").write_text(json.dumps(result, indent=2) + "\n")
    return result
