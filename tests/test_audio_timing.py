"""Measure Timer1 latency in the actual RPG, with calibrated native AVR timing."""

import argparse
from collections import Counter, defaultdict
import csv
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from elf import symbols


def rows(path):
    with path.open(newline="") as stream:
        return list(csv.DictReader(stream))


def summarize(histogram):
    count = sum(histogram.values())
    def percentile(fraction):
        running = 0
        for value, frequency in sorted(histogram.items()):
            running += frequency
            if running >= count * fraction:
                return value
    return dict(count=count, minimum=min(histogram), maximum=max(histogram),
                mean=sum(value * n for value, n in histogram.items()) / count,
                p50=percentile(.5), p95=percentile(.95), p99=percentile(.99),
                p999=percentile(.999), at_least_10=sum(n for v, n in histogram.items() if v >= 10),
                histogram=dict(sorted(histogram.items())))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--device", type=Path, required=True)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--interpreter-build", type=Path, required=True)
    parser.add_argument("--fxc-interpreter-build", type=Path, required=True)
    parser.add_argument("--seconds", type=int, default=40)
    parser.add_argument("--fxc-seconds", type=int, default=8)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--only-calibration", action="store_true")
    parser.add_argument("--no-plot", action="store_true", help="write CSV/JSON without optional matplotlib charts")
    parser.add_argument("--analyze-only", action="store_true", help="reanalyze existing CSVs without rerunning the emulator")
    parser.add_argument("--comparison", type=Path, action="append", default=[],
                        help="additional archived timing summary to include in the comparison")
    parser.add_argument("--baseline", type=Path, help="compare with an archived Timer3 summary.json")
    args = parser.parse_args()
    build = args.build.resolve()
    out = (args.output or build / "audio-timing").resolve()
    out.mkdir(parents=True, exist_ok=True)
    firmware = {"normal": args.interpreter_build.resolve(), "fxc": args.fxc_interpreter_build.resolve()}
    refs = symbols(build / "artifacts/rpg.elf")
    identity = {}
    for variant, suffix in (("normal", ""), ("fxc", ".fxc")):
        package = build / f"artifacts/rpg{suffix}.arduboy"
        with zipfile.ZipFile(package) as archive:
            actual = archive.read("interp.hex")
            guest_hash = hashlib.sha256(archive.read("fxdata.bin")).hexdigest()
        assert actual == (firmware[variant] / "interp.hex").read_bytes(), "firmware/listing mismatch"
        identity[variant] = dict(package_sha256=hashlib.sha256(package.read_bytes()).hexdigest(),
                                 firmware_sha256=hashlib.sha256(actual).hexdigest(),
                                 guest_image_sha256=guest_hash,
                                 score_sha256={score.stem: hashlib.sha256(score.read_bytes()).hexdigest()
                                               for score in sorted((ROOT / "assets/music").glob("*.avms"))})

    if args.analyze_only:
        assert json.loads((out / "summary.json").read_text())["identity"] == identity, "stale timing artifacts"
    cases = [] if args.analyze_only else [("normal", "", 0, True)]
    if not args.only_calibration and not args.analyze_only:
        cases += [("normal", "", args.seconds, False), ("fxc", ".fxc", args.fxc_seconds, False)]
    for variant, suffix, seconds, calibration in cases:
        command = [args.device.resolve(), build / f"artifacts/rpg{suffix}.arduboy",
                   firmware[variant] / "interp.lst",
                   *[refs[name]["address"] for name in
                     ("game", "frame_marker", "frame_serial", "dialogue_prompt", "world_phase")],
                   out, seconds, variant, int(calibration)]
        result = subprocess.run([str(item) for item in command], stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, text=True, timeout=600)
        label = "calibration" if calibration else variant
        (out / f"{label}.log").write_text(result.stdout)
        print(result.stdout, end="", flush=True)
        if result.returncode:
            raise RuntimeError(f"{label} timing measurement failed")

    aggregate = defaultdict(Counter)
    per_scenario = defaultdict(Counter)
    for variant in ("normal", "fxc") if not args.only_calibration else ():
        for row in rows(out / f"{variant}-histogram.csv"):
            key = (variant, row["path"], row["metric"])
            delta, count = int(row["delta_cycles"]), int(row["count"])
            aggregate[key][delta] += count
            per_scenario[(variant, row["scenario"], row["path"], row["metric"])][delta] += count
    summary = dict(audio_timer=1, clock_hz=16000000, output_period_cycles=2048, synthesis_period_cycles=2048,
                   entry_baseline_cycles=7, pwm_baseline_cycles=21,
                   definition="Entry lateness = first ISR body instruction - Timer1 compare deadline - 7; "
                              "PWM lateness subtracts 21; output and synthesis interval errors subtract 2048.",
                   identity=identity,
                   distributions={"/".join(key): summarize(hist) for key, hist in aggregate.items()},
                   scenarios={"/".join(key): summarize(hist) for key, hist in per_scenario.items()})
    # Attribute tails using the CLI instruction's native address, or the active ISR.
    causes = defaultdict(Counter)
    blackouts = {}
    for variant in ("normal", "fxc") if not args.only_calibration else ():
        listing = (firmware[variant] / "interp.lst").read_text()
        labels = sorted((int(address, 16), name) for address, name in
                        re.findall(r"^([0-9a-f]+)\s+\w\s+\.text\s+[0-9a-f]+\s+(\S+)\s*$", listing, re.M))
        def symbol(address):
            if not address:
                return ""
            if any(pc == address + 2 and name == "audio_commit_atomic_begin" for pc, name in labels):
                return "audio_commit/CLI"
            matches = [(pc, name) for pc, name in labels if pc <= address]
            pc, name = matches[-1]
            return f"{name}+0x{address - pc:x}"
        blackouts[variant] = [dict(pc_bytes=int(row["cli_pc_bytes"]),
                                   symbol=symbol(int(row["cli_pc_bytes"])),
                                   count=int(row["count"]), total_cycles=int(row["total_cycles"]),
                                   max_cycles=int(row["max_cycles"]))
                             for row in rows(out / f"{variant}-blackouts.csv")]
        attributed = []
        for row in rows(out / f"{variant}-outliers.csv"):
            address = int(row["mask_pc_bytes"])
            row["mask_symbol"] = symbol(address)
            row["due_symbol"] = symbol(int(row["due_pc_bytes"]))
            causes[(variant, row["blocker"], row["mask_symbol"])][int(row["lateness_cycles"])] += 1
            attributed.append(row)
        if attributed:
            with (out / f"{variant}-outliers-symbolized.csv").open("w", newline="") as stream:
                writer = csv.DictWriter(stream, fieldnames=list(attributed[0]))
                writer.writeheader()
                writer.writerows(attributed)
    summary["outlier_causes"] = {"/".join(key): summarize(hist) for key, hist in causes.items()}
    summary["blackouts"] = blackouts
    baseline = None
    if args.baseline:
        baseline = json.loads(args.baseline.read_text())
        assert baseline["audio_timer"] == 3
        assert all(baseline["identity"][v]["score_sha256"] == identity[v]["score_sha256"]
                   for v in ("normal", "fxc")), "baseline score differs"
        assert all(baseline["identity"][v]["guest_image_sha256"] == identity[v]["guest_image_sha256"]
                   for v in ("normal", "fxc")), "Timer3 baseline guest image differs"
        summary["timer3_baseline"] = str(args.baseline.resolve())
        for path in args.comparison:
            reference = json.loads(path.read_text())
            assert all(reference["identity"][v]["score_sha256"] == identity[v]["score_sha256"]
                       for v in ("normal", "fxc")), "comparison score differs"
        summary["comparisons"] = [str(p.resolve()) for p in args.comparison]
    (out / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    if aggregate and not args.no_plot:
        try:
            plot(aggregate, out)
            if baseline:
                plot_comparison(summary, [baseline] + [json.loads(p.read_text()) for p in args.comparison], out)
        except ModuleNotFoundError as error:
            if error.name != "matplotlib":
                raise
            print("CSV/JSON distributions complete; PNG/SVG plotting requires matplotlib.")
    print(f"Timing artifacts: {out}")


def plot(distributions, out):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, axes = plt.subplots(2, 2, figsize=(14, 8), layout="constrained")
    for column, variant in enumerate(("normal", "fxc")):
        for path, color in (("synthesis", "#167caa"), ("midpoint", "#db7825")):
            if (variant, path, "entry_lateness") not in distributions:
                continue
            hist = distributions[variant, path, "entry_lateness"]
            total = sum(hist.values())
            axes[0, column].plot(sorted(hist), [100 * hist[v] / total for v in sorted(hist)],
                                 ".", markersize=4, label=path, color=color)
            interval = distributions[variant, path, "pwm_interval_error"]
            total = sum(interval.values())
            axes[1, column].plot(sorted(interval), [100 * interval[v] / total for v in sorted(interval)],
                                 ".", markersize=3, label=path, color=color)
        axes[0, column].axvline(10, color="#a33", linestyle="--", linewidth=1, label="10-cycle threshold")
        axes[0, column].set_title(f"{variant.upper()}: ISR entry lateness")
        axes[0, column].set_xlabel("Cycles beyond compare + 7 (zero is ideal)")
        axes[1, column].set_title(f"{variant.upper()}: PWM interval error")
        axes[1, column].set_xlabel("Actual interval minus 2048 cycles (zero is ideal)")
        for row in range(2):
            axes[row, column].set_yscale("log")
            axes[row, column].set_ylabel("Interrupts (%) — logarithmic scale")
            axes[row, column].grid(alpha=.2)
            axes[row, column].legend(fontsize=8)
    fig.suptitle("AVM RPG / original soundtrack — emulated Timer1 timing at 16 MHz\n"
                 "1 cycle = 62.5 ns; output and synthesis 7,812.5 Hz (unsmoothed)", fontsize=13)
    fig.savefig(out / "audio-timing.png", dpi=160)
    fig.savefig(out / "audio-timing.svg")
    plt.close(fig)


def plot_comparison(current, references, out):
    import matplotlib.pyplot as plt

    styles = {3: ("Timer3", "#bc5733", "o"), 1: ("Timer1", "#167caa", "x"),
              0: ("Unified Timer0", "#568b36", "+")}
    datasets = {d["audio_timer"]: d for d in references}
    datasets[current["audio_timer"]] = current
    configurations = [(datasets[timer], *styles[timer]) for timer in (3, 1, 0) if timer in datasets]
    fig, axes = plt.subplots(2, 2, figsize=(14, 8), layout="constrained")
    for column, variant in enumerate(("normal", "fxc")):
        for row, metric in enumerate(("pwm_lateness", "pwm_interval_error")):
            ax = axes[row, column]
            for data, name, color, marker in configurations:
                hist = Counter()
                for path in ("synthesis", "midpoint"):
                    stats = data["distributions"].get(f"{variant}/{path}/{metric}")
                    if stats is None:
                        continue
                    hist.update({int(v): n for v, n in stats["histogram"].items()})
                count = sum(hist.values())
                tail = sum(n for v, n in hist.items() if v >= 10)
                label = name if row else f"{name}: {100 * tail / count:.3f}% >=10 cycles"
                ax.plot(sorted(hist), [100 * hist[v] / count for v in sorted(hist)],
                        marker=marker, linestyle="none", markersize=4, label=label, color=color)
            if row == 0:
                ax.axvline(10, color="#777", linestyle="--", linewidth=1)
            ax.set(title=f"{variant.upper()}: " + ("PWM write lateness" if row == 0 else "PWM interval error"),
                   xlabel="Extra cycles beyond each firmware's fixed latency (zero is ideal)" if row == 0 else
                          "Actual interval minus nominal period (cycles)",
                   ylabel="PWM writes (%) - logarithmic scale", yscale="log")
            ax.grid(alpha=.2)
            ax.legend(fontsize=8)
    fig.suptitle("AVM RPG: Timer3 / Timer1 / unified Timer0 (emulated AVR, 16 MHz)\n"
                 "1 cycle = 62.5 ns; output rate and fixed latency are recorded per firmware")
    fig.savefig(out / "timer-comparison.png", dpi=160)
    fig.savefig(out / "timer-comparison.svg")
    plt.close(fig)


if __name__ == "__main__":
    main()
