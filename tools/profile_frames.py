"""Replay exact device frames with optional emulator-only guest-PC attribution.

The functional replay is unchanged. Tracing observes interpreter instruction boundaries;
it adds no guest instructions. Strict mode deliberately runs without tracing.
"""
import argparse
import csv
import hashlib
import json
import os
import re
import subprocess
import zipfile
from collections import Counter
from pathlib import Path

from elf import symbols
from profile_frame_regions import REGIONS, inline_regions

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--artifacts", type=Path, required=True)
    parser.add_argument("--sdk", type=Path, required=True)
    parser.add_argument("--build", type=Path, default=ROOT / "build")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--mode", choices=["normal", "fxc"], required=True)
    parser.add_argument("--strict", action="store_true")
    parser.add_argument("--scenario", choices=["full", "sounds"], default="full")
    parser.add_argument("--frame-only", action="store_true", help="Record markers/context with ordinary emulator stepping")
    parser.add_argument("--no-trace", action="store_true", help="Functional replay with ordinary emulator stepping")
    parser.add_argument("--dwarfdump", type=Path, help="Optional LLVM dwarfdump for inline region attribution")
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    sdk, artifacts = args.sdk.resolve(), args.artifacts.resolve()
    boundary = json.loads((sdk / "bin/avm/interp-boundary.json").read_text())
    assert (boundary["primary_table_avr_word"], boundary["primary_slot_words"],
            boundary["primary_slot_count"]) == (256, 4, 256)
    elf = artifacts / "rpg_qa.elf"
    package = artifacts / ("rpg_qa.fxc.arduboy" if args.mode == "fxc" else "rpg_qa.arduboy")
    asm = subprocess.check_output([sdk / "bin/avm-objdump", "-d", "-l", "-C", elf], text=True)
    (out / "guest.asm").write_text(asm)
    functions, locations, events = {}, {}, {}
    function, location, entry = "?", "?", 0
    for row in asm.splitlines():
        match = re.match(r"^([0-9a-f]+) <(.*)>:", row)
        if match:
            entry, function, location = int(match[1], 16), match[2], "?"
            if function.startswith(("npc_offer_candidate(", "npc_detail::offer_candidate(")):
                events[entry] = [0, 1]
        match = re.match(r"; (/.+:\d+)", row)
        if match:
            location = match[1]
        match = re.match(r"\s+([0-9a-f]+):\s+[0-9a-f]{2} ", row)
        if match:
            pc = int(match[1], 16)
            functions[pc], locations[pc] = function, location
            if (function.startswith("StaticChunk<") and "(HandlerStage)3>" in function
                    and any(target in row for target in ("<npc_offer_candidate(", "<npc_detail::offer_candidate("))):
                # Every specialized site has one offer call/tail call, including
                # policy-conditional sites. Native npc_site_checks counts these sites.
                events.setdefault(entry, [0, 0])[0] += 1
    intervals = []
    if args.dwarfdump:
        dwarf = subprocess.check_output([args.dwarfdump.resolve(), "--debug-info", elf], text=True)
        (out / "guest.dwarf").write_text(dwarf)
        intervals = inline_regions(dwarf)
    event_file = out / "events.txt"
    event_rows = []
    for pc in functions:
        regions = [(depth, region) for low, high, depth, region in intervals if low <= pc < high]
        region = max(regions)[1] if regions else 0
        sites, offer = events.get(pc, (0, 0))
        event_rows.append(f"{pc} {sites} {offer} {region}\n")
    event_file.write_text("".join(event_rows))
    refs = symbols(elf)
    mode = ("sounds" if args.strict else "sounds-functional") if args.scenario == "sounds" else (
        args.mode if args.strict else "functional")
    command = [args.build.resolve() / "rpg_device_tests", package,
               *[refs[k]["address"] for k in ("game", "frame_marker", "frame_serial")],
               out / "captures", mode,
               *[refs[k]["address"] for k in ("dialogue_prompt", "world_phase", "battle_qa_state",
                                             "battle_qa_ready", "dialogue_panel_open")]]
    env = dict(os.environ)
    env.pop("RPG_FRAME_TRACE", None)
    env.pop("RPG_FRAME_EVENTS", None)
    env.pop("RPG_FRAME_COARSE", None)
    if not args.strict and not args.no_trace:
        env.update(RPG_FRAME_TRACE=str(out / "trace"), RPG_FRAME_EVENTS=str(event_file))
        if args.frame_only:
            env["RPG_FRAME_COARSE"] = "1"
    with (out / "device.log").open("w") as log:
        result = subprocess.run(list(map(str, command)), env=env, stdout=log, stderr=subprocess.STDOUT)
    report = {"returncode": result.returncode, "mode": args.mode, "scenario": args.scenario, "strict": args.strict, "traced": not args.strict and not args.no_trace,
              "offer_counter": ("admission slow-path entries" if "npc_detail::offer_candidate(" in asm
                                else "npc_offer_candidate entries"),
              "regions": REGIONS,
              "attribution": ("ordinary frame markers only" if args.frame_only else
                              "exclusive guest instruction cycles; includes interrupt time; single AVR stepping"),
              "sha256": {str(p): hashlib.sha256(p.read_bytes()).hexdigest()
                         for p in (elf, package, sdk / "bin/avm/interp.hex")}}
    frames = []
    for path in (out / "trace").glob("frames-*.jsonl"):
        frames.extend(json.loads(row) for row in path.read_text().splitlines())
    frames = [frame for frame in frames if frame["exploration"]]
    if args.dwarfdump and not args.frame_only:
        assert all(sum(frame["regions"]) == frame["cycles"] for frame in frames)
    if frames:
        worst = max(frames, key=lambda f: f["cycles"])
        selected = [worst, *[f for f in frames if f["instance"] == 0 and f["serial"] == 74]]
        for frame in selected:
            by_function, by_line = Counter(), Counter()
            path = out / "trace" / f"pcs-{frame['instance']}-{frame['sequence']}.csv"
            if not path.exists() and args.frame_only:
                continue
            for row in csv.DictReader(path.open()):
                pc, cost = int(row["pc"]), int(row["cycles"])
                by_function[functions.get(pc, "unknown")] += cost
                by_line[locations.get(pc, "unknown")] += cost
            assert sum(by_function.values()) == frame["cycles"]
            frame["functions"] = by_function.most_common()
            frame["lines"] = by_line.most_common()
        report.update(frame_count=len(frames), worst=worst, selected=selected)
    report["measurements"] = [json.loads(row) for row in (out / "device.log").read_text().splitlines()
                              if row.startswith("{")]
    with zipfile.ZipFile(package) as archive:
        report["packaged_firmware_sha256"] = {
            name: hashlib.sha256(archive.read(name)).hexdigest()
            for name in archive.namelist() if name.endswith(".hex")}
    if args.strict or args.no_trace:
        report["attribution"] = None
    (out / "summary.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))
    return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
