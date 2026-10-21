"""Compile editable AVMS with the SDK compiler and emit C++ constexpr assets."""

import argparse
from pathlib import Path
import runpy


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--name", required=True)
    args = parser.parse_args()
    compiler = runpy.run_path(str(args.compiler))
    data = compiler["compile_source"](args.source.read_text(encoding="utf-8"), args.source)
    header = compiler["c_array"](data, args.name).replace("static const ", "static constexpr ", 1)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if not args.output.exists() or args.output.read_text(encoding="utf-8") != header:
        args.output.write_text(header, encoding="utf-8", newline="\n")


if __name__ == "__main__":
    main()
