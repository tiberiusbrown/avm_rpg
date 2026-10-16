"""Run genuine SDK-emulated guests, optional full-device QA, and source profiles."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from elf import symbols


def run(argv, log):
    result = subprocess.run(
        [str(a) for a in argv],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        timeout=180,
    )
    log.write_text(result.stdout, encoding="utf-8")
    if result.returncode:
        raise RuntimeError(f"{argv[0]} failed ({result.returncode}):\n{result.stdout}")
    return result.stdout


def lldb(sdk, elf, commands, log):
    # Command files preserve actual newlines and paths with spaces.
    script = log.with_suffix(".lldb")
    script.write_text("\n".join(commands) + "\n")
    exe = sdk / "bin" / ("avm-lldb.exe" if sys.platform == "win32" else "avm-lldb")
    return run([exe, "--batch", "--source", script, elf], log)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--sdk", type=Path, required=True)
    parser.add_argument("--build", type=Path, required=True)
    args = parser.parse_args()
    sdk = args.sdk.resolve()
    build = args.build.resolve()
    artifacts = build / "artifacts"
    work = Path(tempfile.mkdtemp(prefix="verification-", dir=build))
    print(f"Verification artifacts: {work}")
    commands = [
        "breakpoint set --name loop",
        "run",
        "breakpoint delete 1",
        "avm button set",
        "avm run-for 150ms",
        "avm button set A",
        "avm run-for 100ms",
        "avm button set",
        "avm run-for 100ms",
        "expr (int)game.ticks",
        "expr (int)game.player.x",
        "avm button set DOWN",
        "avm run-for 400ms",
        "expr (int)game.player.y",
        "expr (int)game.ticks",
        "avm button set B",
        "avm run-for 100ms",
        "expr (int)game.ticks",
        "thread backtrace",
        "avm button set",
        "avm run-for 100ms",
        "avm button set A",
        "avm run-for 100ms",
        "thread backtrace",
    ]
    output = lldb(sdk, artifacts / "rpg.elf", commands, work / "sdk-input.log")
    values = [int(v) for v in re.findall(r"\(int\) \$\d+ = (\d+)", output)]
    assert values[0] > 0 and values[1] == 32 and values[2] in [54, 56] and values[3] == values[4], (
        f"AVM shared rules/input diverged: {values}"
    )
    assert "menu_screen" in output and "status_screen" in output and "explore" in output
    proof = [
        "breakpoint set --name loop",
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
    output = lldb(sdk, artifacts / "m15_probe.elf", [
        "breakpoint set --name probe_done", "run", "expr (int)probe_checks",
        "expr (int)probe_failure"], work / "m15-probe.log")
    values = [int(v) for v in re.findall(r"\(int\) \$\d+ = (\d+)", output)]
    assert len(values) == 2 and values[0] >= 32 and values[1] == 0, f"M1.5 guest probe: {values}"
    device = build / ("rpg_device_tests.exe" if sys.platform == "win32" else "rpg_device_tests")
    if device.exists():
        for name, mode in [("m0_proof", "proof"), ("rpg", "normal"), ("rpg", "m15")]:
            refs = symbols(artifacts / f"{name}.elf")
            addresses = (
                [0, refs["proof_marker"]["address"], refs["proof_serial"]["address"]]
                if mode == "proof"
                else [refs[k]["address"] for k in ["game", "frame_marker", "frame_serial"]]
            )
            output = run(
                [
                    device,
                    artifacts / f"{name}.arduboy",
                    *addresses,
                    work / mode,
                    mode,
                    0 if mode == "proof" else refs["dialogue_prompt"]["address"],
                    0 if mode == "proof" else refs["world_phase"]["address"],
                ],
                work / f"{mode}.log",
            )
            measurements += [json.loads(row) for row in output.splitlines() if row.startswith("{")]
        (work / "measurements.json").write_text(json.dumps(measurements, indent=2) + "\n")
    else:
        print(
            "Full-device restart/persistence and per-frame metrics SKIPPED: configure AVM_SOURCE_ROOT/AVM_BUILD_ROOT."
        )
    profiler = sdk / "bin" / ("avm-prof.exe" if sys.platform == "win32" else "avm-prof")
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
                "8s",
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
        "sdk_input": True,
        "procedural_screen_calls": True,
        "m0_execution_and_drawing": True,
        "full_device": device.exists(),
        "m15_guest_checks": values[0],
        "measurements": measurements,
        "elf_sha256": {
            name: hashlib.sha256((artifacts / f"{name}.elf").read_bytes()).hexdigest()
            for name in ["m0_proof", "rpg", "m15_probe"]
        },
    }
    (work / "results.json").write_text(json.dumps(report, indent=2) + "\n")
    (build / "latest-verification.txt").write_text(str(work) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
