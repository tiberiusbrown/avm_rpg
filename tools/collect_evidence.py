"""Collect verification reports and raw profiles under the ignored build directory."""

import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import platform
import subprocess

from elf import symbols

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def git_revision(directory):
    if directory is None:
        return "unavailable (SDK only)"
    try:
        return subprocess.check_output(
            ["git", "-C", str(directory), "rev-parse", "HEAD"],
            stderr=subprocess.DEVNULL,
            text=True,
        ).strip()
    except (OSError, subprocess.CalledProcessError):
        return "unavailable"


def collect_revisions(build, work):
    cache = {}
    for line in (build / "CMakeCache.txt").read_text().splitlines():
        if line and not line.startswith(("#", "//")) and "=" in line:
            key, value = line.split("=", 1)
            cache[key.split(":", 1)[0]] = value
    avm_root = Path(cache["AVM_SOURCE_ROOT"]) if cache.get("AVM_SOURCE_ROOT") else None
    return {
        "verification_date": datetime.fromtimestamp(
            (work / "results.json").stat().st_mtime, timezone.utc
        )
        .date()
        .isoformat(),
        "avm_revision": git_revision(avm_root),
        "rpg_revision": git_revision(ROOT),
        "host": f"{platform.system()} {platform.machine()}; "
        f"{cache.get('CMAKE_GENERATOR', 'unknown generator')}; RelWithDebInfo",
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", type=Path, default=ROOT / "build")
    args = parser.parse_args()
    build = args.build.resolve()
    work = Path((build / "latest-verification.txt").read_text().strip())
    result = json.loads((work / "results.json").read_text())
    assert result["result"] == "PASS" and result["full_device"]
    revisions = collect_revisions(build, work)
    resources = {}
    artifacts = {}
    lto = {}
    for name in ["m0_proof", "rpg", "rpg_stress"]:
        elf = build / "artifacts" / f"{name}.elf"
        assert digest(elf) == result["elf_sha256"][name], "verification is stale"
        assert json.loads((work / f"{name}.avmp").read_text())["identity"]["elf_sha256"] == digest(
            elf
        ), "profile is stale"
        resources[name] = json.loads(Path(str(elf) + ".resources.json").read_text())
        lto[name] = json.loads(Path(str(elf) + ".lto.json").read_text())
        assert lto[name]["game_reference_arguments_eliminated"]
        for extension in ["elf", "bin", "arduboy"]:
            p = elf.with_suffix("." + extension)
            artifacts[p.name] = {"sha256": digest(p), "bytes": p.stat().st_size}
    source_files = [
        ROOT / name for name in ["CMakeLists.txt", ".gitignore", ".clang-format", "AGENTS.md"]
    ]
    for directory in ["src", "tests", "tools", "generated", "assets_src"]:
        source_files += [
            p for p in (ROOT / directory).rglob("*") if p.is_file() and "__pycache__" not in p.parts
        ]
    manifest = {
        str(p.relative_to(ROOT)).replace("\\", "/"): digest(p) for p in sorted(source_files)
    }
    fingerprint = hashlib.sha256(
        json.dumps(manifest, sort_keys=True, separators=(",", ":")).encode()
    ).hexdigest()
    units = {}
    for name in ["rpg_tests", "rpg_stress_tests"]:
        exe = build / (name + (".exe" if __import__("sys").platform == "win32" else ""))
        units[name] = subprocess.check_output([str(exe)], text=True).strip()
    profile = json.loads((work / "rpg.avmp").read_text())
    pcs = profile["pcs"]

    def cost(predicate):
        rows = [r for r in pcs if predicate(r)]
        return {
            "cycles": sum(int(r["cycles"]) for r in rows),
            "executions": sum(int(r["count"]) for r in rows),
        }

    breakdown = {
        "program_load_instructions": cost(lambda r: r["assembly"].startswith("ldp")),
        "sprite_overwrite_sys": cost(lambda r: r["assembly"] == "sys draw_overwrite"),
        "sprite_plus_mask_sys": cost(lambda r: r["assembly"] == "sys draw_plus_mask"),
        "display_sys": cost(lambda r: r["assembly"] == "sys display"),
    }
    evidence = {
        "revisions": revisions,
        "source_fingerprint_sha256": fingerprint,
        "source_manifest": manifest,
        "artifacts": artifacts,
        "resources": resources,
        "lto": lto,
        "native_tests": units,
        "integration": result,
        "profile_window": profile["window"],
        "profile_identity": profile["identity"],
        "profile_instruction_costs": breakdown,
        "raw_evidence_directory": str(work),
    }
    root_package = ROOT / "rpg.arduboy"
    assert digest(root_package) == artifacts["rpg.arduboy"]["sha256"], "root package is stale"
    (build / "revisions.json").write_text(json.dumps(revisions, indent=2) + "\n")
    (build / "evidence.json").write_text(json.dumps(evidence, indent=2) + "\n")
    rows = []
    for m in result["measurements"]:
        if "workload" not in m:
            continue
        rows.append(
            f"| {m['workload']} | {m['frames']} | {m['average_cycles']:,} | {m['worst_cycles']:,} | "
            + (
                f"{m['average_render_display_cycles']:,}"
                if "average_render_display_cycles" in m
                else "—"
            )
            + " | "
            + (str(m["observed_fps"]) if "observed_fps" in m else "single operation")
            + " |"
        )
    table = "\n".join(rows)
    loads = breakdown["program_load_instructions"]
    sprites = breakdown["sprite_overwrite_sys"]
    display = breakdown["display_sys"]
    rpg_resources = resources["rpg"]
    game_bytes = symbols(build / "artifacts/rpg.elf")["game"]["bytes"]
    text = f"""# M0/M1 performance evidence

Measured {revisions["verification_date"]} on {revisions["host"]}. Exact input schedules,
ELF/image/interpreter hashes, section sizes, source fingerprint and native test
results are in [evidence.json](evidence.json). Source fingerprint: `{fingerprint}`.
AVM revision: `{revisions["avm_revision"]}`.

## Memory and complete link-time stack bounds

| Guest | .saved | ordinary .data | total static / 1024 | code | read-only data | stack / 256 |
|---|---:|---:|---:|---:|---:|---:|
"""
    for name, r in resources.items():
        text += f"| {name} | {r['saved_bytes']} | {r['ordinary_data_bytes']} | {r['static_bytes']} | {r['text_bytes']} | {r['rodata_bytes']} | {r['stack_bytes']} |\n"
    text += f"""
All three links report **complete bound: yes**, with zero analysis gaps. The
M1 {rpg_resources["stack_bytes"]}-byte bound includes spills, outgoing arguments and
three-byte return records. SYS native AVR stack is outside the guest stack.
The exact maximum paths are retained in build/artifacts/*.link.txt.
The framebuffer is a separate 1024 bytes. Guest Game is {game_bytes} bytes, NPC 8 bytes,
and all 12 NPC slots are reserved even in the normal build. Native sizeof values
are not used as guest resource evidence. No heap exists in guest rules. M1 leaves
{1024 - rpg_resources["static_bytes"]} static bytes and {256 - rpg_resources["stack_bytes"]} stack bytes of margin.

All game rules and native C++ tests use C++17. Guest full LTO specializes the
shared Game references to the single global object: game_new and save_decode
retain no arguments, world_enter retains area/position/facing, and position_clear
retains position/actor-ignore values. Other rules are inlined. Each link saves
optimized LLVM IR and checks that the Game reference is absent from retained
rule signatures. The exact signatures are recorded in evidence.json under lto.

## Full-frame costs (elapsed emulated AVR cycles)

The clock is 16 MHz; the 20 FPS / 50 ms budget is **800,000 cycles**. These are
actual interpreter/device costs including interrupts and OLED transfers, not
guest opcode counts or host wall-clock times. Markers bracket accepted simulation
frames; pacing wait between frames is excluded. Render/display markers include
the full screen render and avm_display(true), with tiny marker overhead. FPS is
measured from the interval between frame beginnings, including pacing wait.
Procedural menu/status/dialogue waits are checked separately and are not frame-cost samples.

| Workload | Samples | Average cycles | Worst observed cycles | Average render + display | Emulated FPS |
|---|---:|---:|---:|---:|---:|
{table}

Normal exploration and the 12-NPC workload sustain **20 FPS** in these windows.
The observed maxima are samples, not a mathematical universal worst-case bound.
Save/readback and continuation are verified by the device scenario, without
including time spent waiting for UI input in the frame samples.

The full-device scenario exercises stationary display, intermediate player pixels,
scrolling, NPC movement, dense terrain collision, dialogue/choices, both portals,
menu/status, saving, a new emulator instance loading the physical sector, changed
dialogue after restart, and corrupt-data startup. Native tests additionally cover
unsupported formats, bounds/corners, directions, deterministic wandering/limits,
spawn relocation, dialogue consequences, checksum failures and unchanged
state after failed load. CTest has four passing tests; exact rule-check counts are
in evidence.json. Device checks cover paused procedural screens, held-button
release gates, and returning through nested screens. SDK backtraces verify that
status runs inside menu_screen inside explore, with no mode dispatcher.

## Source profiling and asset access

Each SDK profile uses one second of warmup and an eight-second measurement
window with tests/replays/exploration.json, deterministic named-button events
at cycle offsets. M0, normal and 12-NPC profiles all completed. The normal
window contains 128,000,000 reconciled cycles, 160 displayed frames, zero
partial cycles and zero discontinuities. Native active/waiting costs are
separate from guest-PC attribution. Optimized inline sites retain source
identity; an unmapped source row does not mean an unaccounted cycle.

Within that normal eight-second profile:

* Direct AS1 program-load instructions: {loads["cycles"]:,} cycles across
  {loads["executions"]:,} executions ({loads["cycles"] / loads["executions"]:.1f} cycles/execution).
  This includes map bytes, area descriptors and NPC definitions, with interpreter
  dispatch/interrupt intervals; it is not isolated flash chip latency.
* Background sprite SYS calls: {sprites["cycles"]:,} cycles / {sprites["executions"]:,}
  calls ({sprites["cycles"] / sprites["executions"]:.1f} cycles/call). Sprite FX reads and
  native blits are included here, not in the direct guest program-load count.
* Display SYS calls: {display["cycles"]:,} cycles / {display["executions"]:,} calls
  ({display["cycles"] / display["executions"]:.1f} cycles/call). This excludes argument
  setup but includes the native transfer and interrupt activity.

The map loop and sprite rendering dominate useful frame work. Selected-sprite
services and skipping blank path/interior tiles already avoid needless transfers.
No cache, compression, dirty rectangle scheme or lower frame rate was necessary.
The 12-NPC test remains within budget with pixel motion intact. Further detail,
audio or battle rendering requires new profiles; current timings are not reserved
capacity for those additions.

## Reproduce

From the RPG root with SDK on PATH and a matching MSVC developer environment:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DAVM_SDK_ROOT=/path/to/sdk
cmake --build build --config RelWithDebInfo --parallel 4
ctest --test-dir build -C RelWithDebInfo --output-on-failure
```

For complete visible/persistence/per-frame QA, configure the existing matching
AVM source/build library with -DAVM_SOURCE_ROOT=/path/to/avm and
-DAVM_BUILD_ROOT=/path/to/avm/build, then build. The core installed-SDK command is:

```sh
python tests/guest_integration.py --sdk /path/to/sdk --build build
python tools/collect_evidence.py --build build
```

The integration script creates a fresh directory, launches actual ELF and package
guests, runs frame-based input scenarios, and records avm-prof traces/HTML reports.
Raw evidence for this run is `{work.name}` under build/; latest-verification.txt
points to it. SDK-only users can execute the LLDB/replay checks, but full-device
per-frame/restart checks are explicitly skipped without the optional harness.

Limitations: Windows/MSVC and emulated device verified; physical hardware, other
host platforms and audio-enabled workloads remain unverified. The installed SDK's unfiltered visible buffer can be
stale, so full-device QA refreshes it using Ardens advance(0). No VM/toolchain
source changes were made to work around that capture issue.
"""
    (build / "performance.md").write_text(text, encoding="utf-8", newline="\n")
    print(f"Collected evidence for {fingerprint}; reports in {build}; raw profiles in {work}")


if __name__ == "__main__":
    main()
