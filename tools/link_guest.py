import argparse
import json
from pathlib import Path
import subprocess
from resources import inspect
from check_lto import inspect_lto

parser = argparse.ArgumentParser()
parser.add_argument("--compiler", required=True)
parser.add_argument("--image", required=True)
parser.add_argument("--output", required=True)
parser.add_argument("--fxc-interpreter", type=Path)
parser.add_argument("objects", nargs="+")
args = parser.parse_args()
elf = Path(args.output)
command = [
    args.compiler,
    "-O2",
    "-g",
    "-flto=full",
    "--avm-startup=crt0_sketch.o",
    "-Wl,--avm-print-stack-usage,--avm-print-stack-gaps,--save-temps=opt",
    *args.objects,
    "-o",
    str(elf),
]
run = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
print(run.stdout, end="")
elf.with_suffix(".link.txt").write_text(run.stdout)
if run.returncode:
    raise SystemExit(run.returncode)
result = inspect(elf, run.stdout)
inspect_lto(args.compiler, elf)
Path(str(elf) + ".resources.json").write_text(json.dumps(result, indent=2) + "\n")
print(json.dumps(result, indent=2))
packages = [(elf.with_suffix(".arduboy"), [])]
if args.fxc_interpreter:
    packages.append(
        (elf.with_suffix(".fxc.arduboy"), ["--interpreter", str(args.fxc_interpreter)])
    )
for package, interpreter_options in packages:
    subprocess.run(
        [
            args.image,
            str(elf),
            "-o",
            str(package),
            *interpreter_options,
            "--title",
            "AVM Wayfarer " + elf.stem,
            "--author",
            "AVM RPG",
            "--description",
            "Pixel exploration technical core",
            "--game-version",
            "0.1",
        ],
        check=True,
    )
