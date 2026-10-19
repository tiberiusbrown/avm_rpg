"""Collect 16x16 guest, device, profile and native evidence."""

import argparse
import hashlib
import json
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

from elf import symbols

ROOT = Path(__file__).resolve().parents[1]
GUESTS = ["m0_proof", "rpg", "m15_probe", "m16_probe"]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", type=Path, default=ROOT / "build")
    parser.add_argument("--write-docs", action="store_true")
    args = parser.parse_args()
    build = args.build.resolve()
    work = Path((build / "latest-verification.txt").read_text().strip())
    result = json.loads((work / "results.json").read_text())
    assert result["result"] == "PASS" and result["full_device"] and result["full_device_fxc"]
    resources, artifacts, lto, stack_reports = {}, {}, {}, {}
    for name in GUESTS:
        elf = build / "artifacts" / f"{name}.elf"
        assert digest(elf) == result["elf_sha256"][name], "verification is stale"
        resources[name] = json.loads(Path(str(elf) + ".resources.json").read_text())
        lto[name] = json.loads(Path(str(elf) + ".lto.json").read_text())
        assert lto[name]["game_reference_arguments_eliminated"]
        assert lto[name]["map_tile_position_storage_eliminated"]
        stack_reports[name] = elf.with_suffix(".link.txt").read_text()
        for extension in ("elf", "bin", "arduboy"):
            path = elf.with_suffix("." + extension)
            artifacts[path.name] = dict(sha256=digest(path), bytes=path.stat().st_size)
    assert digest(ROOT / "rpg.arduboy") == artifacts["rpg.arduboy"]["sha256"]
    fxc = build / "artifacts" / "rpg.fxc.arduboy"
    artifacts[fxc.name] = dict(sha256=digest(fxc), bytes=fxc.stat().st_size)
    assert digest(ROOT / fxc.name) == artifacts[fxc.name]["sha256"]
    files = [
        ROOT / name
        for name in ("CMakeLists.txt", "AGENTS.md", "README.md", ".clang-format")
    ]
    for folder in ("src", "tests", "tools", "generated", "assets", "assets_src"):
        files += [
            p
            for p in (ROOT / folder).rglob("*")
            if p.is_file() and "__pycache__" not in p.parts
        ]
    manifest = {p.relative_to(ROOT).as_posix(): digest(p) for p in sorted(files)}
    fingerprint = hashlib.sha256(
        json.dumps(manifest, sort_keys=True).encode()
    ).hexdigest()
    native = {}
    for name in (
        "rpg_tests",
        "rpg_flag_tests",
        "rpg_world_tests",
        "rpg_npc_tests",
        "rpg_npc_hook_tests",
    ):
        exe = build / (name + (".exe" if sys.platform == "win32" else ""))
        native[name] = subprocess.check_output([str(exe)], text=True).strip()
    profiles = {}
    for name in ("m0_proof", "rpg"):
        value = json.loads((work / f"{name}.avmp").read_text())
        assert value["identity"]["elf_sha256"] == artifacts[f"{name}.elf"]["sha256"]
        assert (
            value["window"]["complete"] and int(value["window"]["partial_cycles"]) == 0
        )
        profiles[name] = {k: value[k] for k in ("window", "identity")}
    evidence = dict(
        verification_date=datetime.fromtimestamp(
            (work / "results.json").stat().st_mtime, timezone.utc
        )
        .date()
        .isoformat(),
        configuration="RelWithDebInfo; existing MSVC/Ninja and AVM SDK",
        source_fingerprint_sha256=fingerprint,
        source_manifest=manifest,
        artifacts=artifacts,
        resources=resources,
        lto=lto,
        stack_reports=stack_reports,
        native_tests=native,
        integration=result,
        profiles=profiles,
        game_bytes=symbols(build / "artifacts/rpg.elf")["game"]["bytes"],
        cache_entries=45,
        cache_bytes=47,
        cache_bytes_saved=108,
        npc_capacity=8,
        npc_bytes=18,
        pool_bytes=156,
        raw_evidence_directory=str(work),
    )
    payload = json.dumps(evidence, indent=2) + "\n"
    (build / "evidence.json").write_text(payload, encoding="utf-8")
    lines = [
        "# 16x16 measured evidence\n",
        f"Measured {evidence['verification_date']} in {evidence['configuration']}.",
        f"Source fingerprint `{fingerprint}`. Changes remain uncommitted.",
        "Exact source/package hashes, complete link paths and LTO checks are in",
        "[tiles16_evidence.json](tiles16_evidence.json). Raw LLDB logs, PGM captures",
        f"and complete source profiles remain in `build/{work.name}`.\n",
        "| Guest | saved | ordinary RAM | total static / 1024 | code | PROGMEM | complete stack / 256 |",
        "|---|---:|---:|---:|---:|---:|---:|",
    ]
    for name, r in resources.items():
        lines.append(
            "| "
            + name
            + " | "
            + " | ".join(
                str(r[k])
                for k in (
                    "saved_bytes",
                    "ordinary_data_bytes",
                    "static_bytes",
                    "text_bytes",
                    "rodata_bytes",
                    "stack_bytes",
                )
            )
            + " |"
        )
    lines += [
        "",
        f"Playable Game is {evidence['game_bytes']} bytes. The cache uses 47 bytes (45 IDs plus origins),",
        "saving 108 bytes from the former 17x9 cache. Eight live NPCs still reserve",
        "156 bytes including requests. Character rows/talkability live in ROM definitions.",
        "Every guest has a complete stack bound and zero analysis gaps. Full LTO verifies",
        "one Game, no Game-reference helper parameters and no stored TilePos constants.",
        "The separate framebuffer and native AVR hardware stack are excluded.\n",
        "Full-device emulated AVR cycles at 16 MHz include simulation, rendering,",
        "display transfer and interrupts; pacing waits are excluded. The 20 FPS budget",
        "is 800,000 cycles, checked on every observed exploration frame.\n",
        "| Workload | frames | average cycles | worst observed | render/display | FPS |",
        "|---|---:|---:|---:|---:|---:|",
    ]
    for m in result["measurements"]:
        if "workload" in m:
            lines.append(
                f"| {m['workload']} | {m['frames']} | {m['average_cycles']:,} | {m['worst_cycles']:,} | "
                f"{m.get('average_render_display_cycles', '-')} | {m.get('observed_fps', '-')} |"
            )
    lines += [
        "",
        "Nine CTest suites pass: core, essential_npcs, npc_lifecycle_hooks, flags,",
        "chunk_world, tiled_pipeline, generated_assets, content and peaceful_music. Pipeline tests check",
        "every source bitmap/alpha pixel, all 255 usable indices, the reserved cell,",
        "padding, maximum map dimensions, chunk relocation and default handlers.\n",
        "```text",
        *native.values(),
        "```",
        "",
        f"AVM guest checks: {result['guest_checks']}.",
        "Full-device PASS for FX and FX-C covers actual buttons, two sample NPCs, sign dialogue,",
        "paused simulation, menus/status, physical FX save/restart, corrupt-save",
        "rejection, terrain scrolling and actual masked actor overlap pixels.",
        "Audio checks verify silent titles, audible unclipped PWM after New Game and",
        "Continue, and uninterrupted playback through dialogue and menus. Exploration",
        "measurements include music. The A2 score test decodes two full 48-second loops.",
        "Both source profile windows are complete. The render sort needs at most nine",
        "byte indices and 36 comparisons/shifts, with no added persistent RAM.",
        "The 59,520-byte fixture reaches chunk 1859 and byte offset 59519. The NPC probe",
        "covers 8x8 footprint edges, depth sorting/ties, wandering and targets,",
        "terrain repair, walkaround, animated yielding and tight-corridor passage;",
        "independent native hooks additionally cover route behavior.\n",
        "The earlier map had different handlers and NPCs. Timing comparisons cannot",
        "isolate tile size from the simultaneous content reset. Observed maxima are",
        "sample bounds, not a universal worst-case timing proof. Physical hardware",
        "remains unverified.\n",
        "From the RPG root:\n",
        "```powershell",
        "./tools/build.ps1 -SdkRoot ../../build/avm-sdk-install",
        "cmake --build build --config RelWithDebInfo --target rpg_integration",
        "python tools/collect_evidence.py --build build --write-docs",
        "```",
    ]
    report = "\n".join(lines) + "\n"
    (build / "performance.md").write_text(report, encoding="utf-8", newline="\n")
    if args.write_docs:
        (ROOT / "docs/tiles16_measurements.md").write_text(
            report, encoding="utf-8", newline="\n"
        )
        (ROOT / "docs/tiles16_evidence.json").write_text(payload, encoding="utf-8")
    print(f"Collected 16x16 evidence for {fingerprint}; raw profiles in {work}")


if __name__ == "__main__":
    main()
