"""Run genuine SDK-emulated guests, optional full-device QA, and source profiles."""

import argparse
import hashlib
import json
import re
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from elf import symbols
from generate_assets import load_world

TIMINGS = {}


def run(argv, log):
    started = time.monotonic()
    result = subprocess.run(
        [str(a) for a in argv],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        timeout=180,
    )
    log.write_text(result.stdout, encoding="utf-8")
    TIMINGS[log.stem] = round(time.monotonic() - started, 3)
    print(f"{log.stem}: {TIMINGS[log.stem]:.3f}s", flush=True)
    if result.returncode:
        raise RuntimeError(f"{argv[0]} failed ({result.returncode}):\n{result.stdout}")
    return result.stdout


def lldb(sdk, elf, commands, log):
    # Command files preserve actual newlines and paths with spaces.
    script = log.with_suffix(".lldb")
    script.write_text("\n".join(commands) + "\n")
    exe = sdk / "bin" / ("avm-lldb.exe" if sys.platform == "win32" else "avm-lldb")
    return run([exe, "--no-lldbinit", "--batch", "--source", script, elf], log)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--sdk", type=Path, required=True)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument(
        "--full", action="store_true",
        help="Run exhaustive guest/device QA and 8-second profiles",
    )
    args = parser.parse_args()
    sdk = args.sdk.resolve()
    build = args.build.resolve()
    artifacts = build / "artifacts"
    work = Path(tempfile.mkdtemp(prefix="verification-", dir=build))
    started = time.monotonic()
    print(f"Verification artifacts: {work}", flush=True)
    profiler = sdk / "bin" / ("avm-prof.exe" if sys.platform == "win32" else "avm-prof")
    # Fail before running expensive guests if the installed tools cannot load.
    run([profiler, "--help"], work / "profiler-preflight.log")
    lldb(sdk, artifacts / "m0_proof.elf", ["version"], work / "lldb-preflight.log")
    commands = [
        "breakpoint set --name loop",
        "run",
        "breakpoint delete 1",
        "avm button set",
        "avm run-for 150ms",
        "avm button set A",
        "avm run-for 100ms",
        "avm button set",
        "avm run-for 500ms",
        "avm button set A",
        "avm run-for 100ms",
        "avm button set",
        "avm run-for 300ms",
        "expr (int)game.ticks",
        "expr (int)game.player.x",
        "avm button set DOWN",
        "avm run-for 400ms",
        "expr (int)game.player.y",
        # Let an in-flight exploration frame finish before checking paused ticks.
        "avm button set B",
        "avm run-for 100ms",
        "expr (int)game.ticks",
        "avm run-for 100ms",
        "expr (int)game.ticks",
        "thread backtrace",
        "avm button set",
        "avm run-for 100ms",
        "avm button set DOWN",
        "avm run-for 100ms",
        "avm button set",
        "avm run-for 100ms",
        "avm button set DOWN",
        "avm run-for 100ms",
        "avm button set",
        "avm run-for 100ms",
        "avm button set A",
        "avm run-for 100ms",
        "avm button set",
        "avm run-for 400ms",
        "expr (int)game.party.count",
        "thread backtrace",
    ]
    output = lldb(sdk, artifacts / "rpg.elf", commands, work / "sdk-input.log")
    values = [int(v) for v in re.findall(r"\(int\) \$\d+ = (\d+)", output)]
    # The player starts on the map's spawn marker and walks down from it.
    maps, _, names = load_world(ROOT)
    spawn = names[next(m for m in maps if m.symbol == "OUTDOOR").spawn]
    assert (
        values[0] > 0
        and values[1] == spawn["tx"] * 16
        and values[2] > spawn["ty"] * 16
        and values[3] == values[4]
    ), f"AVM shared rules/input diverged: {values}"
    assert values[5] == 1
    assert "party_screen" in output and "explore" in output
    proof = [
        # M0 returns from loop every frame; setup is a one-shot stop during boot.
        "breakpoint set --name setup",
        "run",
        "breakpoint delete 1",
        "avm button set",
        "avm run-for 150ms",
        "avm button set RIGHT DOWN A",
        "avm run-for 200ms",
        "expr (int)x",
        "expr (int)y",
        "avm button set LEFT UP B",
        "avm run-for 100ms",
        "avm button set",
        "breakpoint set --name proof_present",
        "continue",
        "expr (int)x",
        "expr (int)y",
        "expr (int)area",
        "expr (int)action",
        f'avm display save "{work / "proof.pgm"}" --mode logical',
    ]
    output = lldb(sdk, artifacts / "m0_proof.elf", proof, work / "m0-proof.log")
    values = [int(v) for v in re.findall(r"\(int\) \$\d+ = (\d+)", output)]
    assert (
        values[0] > 24
        and values[1] > 24
        and values[2] < values[0]
        and values[3] < values[1]
        and values[4:] == [1, 0]
    ), f"M0 input failure: {values}"
    assert any((work / "proof.pgm").read_bytes()[len(b"P5\n128 64\n255\n") :]), (
        "blank M0 framebuffer"
    )
    measurements = []
    guest_checks = {}
    for name, minimum in [("m15_probe", 32), ("m16_probe", 1000)]:
        commands = ["breakpoint set --name probe_done", "run"]
        if name == "m15_probe" and args.full:
            commands = [
                "breakpoint set --name setup",
                "run",
                "breakpoint delete 1",
                "expr probe_exhaustive = true",
                "breakpoint set --name probe_done",
                "continue",
            ]
        # The check counter is 16 bits wide and can pass 32767.
        commands += ["expr (unsigned)probe_checks", "expr (unsigned)probe_failure"]
        if name == "m15_probe":
            commands += ["expr (unsigned long)probe_collision_hash"]
        output = lldb(
            sdk,
            artifacts / f"{name}.elf",
            commands,
            work / f"{name}.log",
        )
        values = [
            int(v) for v in re.findall(r"\(unsigned (?:int|long)\) \$\d+ = (\d+)", output)
        ]
        assert (
            len(values) == (3 if name == "m15_probe" else 2)
            and values[0] >= minimum
            and values[1] == 0
        ), f"{name}: {values}"
        if name == "m15_probe":
            expected = 2166136261
            for area in sorted(maps, key=lambda m: m.area):
                for tile in range(256):
                    mask = area.tiles[tile]["collision"] if tile != 255 else 0xFFFF
                    for byte in (mask & 255, mask >> 8):
                        expected = ((expected ^ byte) * 16777619) & 0xFFFFFFFF
            assert values[2] == expected, (
                "guest tile collision lookup differs from TSX metadata"
            )
        guest_checks[name] = values[0]
    device = build / (
        "rpg_device_tests.exe" if sys.platform == "win32" else "rpg_device_tests"
    )
    if device.exists():
        for name, mode, variant in [
            ("m0_proof", "proof", ""),
            ("rpg_qa", "normal", ""),
            ("rpg_qa", "fxc", ".fxc"),
        ]:
            refs = symbols(artifacts / f"{name}.elf")
            addresses = (
                [0, refs["proof_marker"]["address"], refs["proof_serial"]["address"]]
                if mode == "proof"
                else [
                    refs[k]["address"] for k in ["game", "frame_marker", "frame_serial"]
                ]
            )
            qa_keys = [
                "dialogue_prompt", "world_phase", "battle_qa_state", "battle_qa_ready",
                "dialogue_panel_open", "runtime_workspace",
            ]
            qa_addresses = [0 if mode == "proof" else refs[key]["address"] for key in qa_keys]
            cases = [(mode, mode if args.full or mode == "proof" else "smoke")]
            if mode != "proof":
                cases.append((f"{mode}-sounds", "sounds"))
                cases.append((f"{mode}-perk-ranks", "perk-ranks"))
                cases.append((f"{mode}-bram-meeting", "bram-meeting"))
                cases.append((f"{mode}-battle-classes", "battle-classes"))
                cases.append((f"{mode}-battle-formation", "battle-formation"))
                if args.full:
                    cases.append((f"{mode}-millcross", "millcross"))

            device_measurements = []
            for label, scenario in cases:
                output = run(
                    [
                        device, artifacts / f"{name}{variant}.arduboy", *addresses,
                        work / label, scenario, *qa_addresses,
                    ],
                    work / f"{label}.log",
                )
                device_measurements += [
                    json.loads(row) for row in output.splitlines() if row.startswith("{")
                ]
            if mode == "fxc":
                for measurement in device_measurements:
                    if "workload" in measurement:
                        measurement["workload"] = "fxc_" + measurement["workload"]
            measurements += device_measurements
        (work / "measurements.json").write_text(
            json.dumps(measurements, indent=2) + "\n"
        )
    else:
        print(
            "Full-device restart/persistence and per-frame metrics SKIPPED: configure AVM_SOURCE_ROOT/AVM_BUILD_ROOT."
        )
    replay = ROOT / "tests/replays/exploration.json"
    for name in ["m0_proof", "rpg"]:
        path = work / f"{name}.avmp"
        run(
            [
                profiler,
                "record",
                artifacts / f"{name}.elf",
                "--warmup",
                "1s",
                "--for",
                "8s" if args.full else "100ms",
                "--native",
                "--replay",
                replay,
                "-o",
                path,
            ],
            work / f"{name}-record.log",
        )
        value = json.loads(path.read_text())
        assert value["window"]["complete"]
        assert int(value["window"]["partial_cycles"]) == 0
        run(
            [profiler, "report", path, "--html", work / f"{name}.html"],
            work / f"{name}-profile.log",
        )
    report = {
        "result": "PASS",
        "suite": "full" if args.full else "fast",
        "sdk_input": True,
        "procedural_screen_calls": True,
        "m0_execution_and_drawing": True,
        "device_smoke": device.exists(),
        "device_smoke_fxc": device.exists(),
        "full_device": device.exists() and args.full,
        "full_device_fxc": device.exists() and args.full,
        "guest_checks": guest_checks,
        "measurements": measurements,
        "timings_seconds": TIMINGS,
        "elapsed_seconds": round(time.monotonic() - started, 3),
        "elf_sha256": {
            name: hashlib.sha256((artifacts / f"{name}.elf").read_bytes()).hexdigest()
            for name in ["m0_proof", "rpg", "m15_probe", "m16_probe"]
        },
    }
    (work / "results.json").write_text(json.dumps(report, indent=2) + "\n")
    (build / "latest-verification.txt").write_text(str(work) + "\n")
    if args.full:
        (build / "latest-full-verification.txt").write_text(str(work) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
