"""Archive an exploration-only source profile and full-device frame measurements."""

import argparse
import hashlib
import json
import shutil
import subprocess
import sys
import tempfile
from collections import defaultdict
from pathlib import Path

from elf import symbols

ROOT = Path(__file__).resolve().parents[1]


def run(argv, log):
    result = subprocess.run(
        [str(arg) for arg in argv], capture_output=True, text=True, timeout=180
    )
    log.write_text(result.stdout + result.stderr, encoding="utf-8")
    if result.returncode:
        raise RuntimeError(f"Command failed; see {log}:\n{result.stdout}{result.stderr}")
    return result.stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, default=ROOT / "build")
    parser.add_argument("--sdk", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--label", required=True)
    parser.add_argument("--resume", action="store_true", help="Finish an interrupted archived trial")
    args = parser.parse_args()
    build, sdk = args.build.resolve(), args.sdk.resolve()
    output = (args.output or build / "exploration-optimization").resolve()
    output.mkdir(parents=True, exist_ok=True)
    trial = output / args.label
    if not args.resume:
        trial.mkdir()  # Never overwrite an earlier experiment.
    artifacts = build / "artifacts"
    elf = trial / "rpg.elf"
    if not args.resume:
        for suffix in ["elf", "elf.resources.json", "elf.lto.json", "arduboy", "fxc.arduboy"]:
            shutil.copy2(artifacts / f"rpg.{suffix}", trial / f"rpg.{suffix}")
        for source in [*ROOT.glob("src/**/*.cpp"), *ROOT.glob("src/**/*.h"),
                       *ROOT.glob("generated/*.cpp"), *ROOT.glob("generated/*.h")]:
            target = trial / "source" / source.relative_to(ROOT)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, target)
        shutil.copy2(ROOT / "tests/replays/exploration.json", trial / "replay.json")
    profiler = sdk / "bin" / ("avm-prof.exe" if sys.platform == "win32" else "avm-prof")
    profile = trial / "exploration.avmp"
    if not args.resume or not profile.exists():
        run(
            [profiler, "record", elf, "--warmup", "1s", "--for", "8s", "--native",
             "--source-map", f"{ROOT}={trial / 'source'}",
             "--replay", trial / "replay.json", "-o", profile],
            trial / "record.log",
        )
    value = json.loads(profile.read_text())
    assert value["window"]["complete"]
    assert int(value["window"]["partial_cycles"]) == 0
    assert int(value["window"]["discontinuities"]) == 0
    functions = defaultdict(int)
    for pc in value["pcs"]:
        functions[pc["function"]] += int(pc["cycles"])
    assert not any("title" in name for name in functions), "title screen entered profile"
    assert functions.get("explore()", 0), "exploration did not run"
    with tempfile.TemporaryDirectory(dir=trial) as scratch:
        html = Path(scratch) / "profile.html"
        run([profiler, "report", profile, "--html", html], trial / "report.log")
        html.replace(trial / "profile.html")
    refs = symbols(elf)
    measurements = {}
    device = build / ("rpg_device_tests.exe" if sys.platform == "win32" else "rpg_device_tests")
    for mode, suffix in [("normal", ""), ("fxc", ".fxc")]:
        result = run(
            [device, trial / f"rpg{suffix}.arduboy",
             *[refs[key]["address"] for key in ["game", "frame_marker", "frame_serial"]],
             trial / mode, mode, refs["dialogue_prompt"]["address"], refs["world_phase"]["address"]],
            trial / f"{mode}.log",
        )
        measurements[mode] = [json.loads(row) for row in result.splitlines() if row.startswith("{")]
    summary = {
        "label": args.label,
        "elf_sha256": hashlib.sha256(elf.read_bytes()).hexdigest(),
        "window": value["window"],
        "functions": dict(sorted(functions.items(), key=lambda item: -item[1])),
        "resources": json.loads((trial / "rpg.elf.resources.json").read_text()),
        "measurements": measurements,
    }
    (trial / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
