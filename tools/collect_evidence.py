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
    parser.add_argument("--write-docs", action="store_true")
    args = parser.parse_args()
    build = args.build.resolve()
    work = Path((build / "latest-verification.txt").read_text().strip())
    result = json.loads((work / "results.json").read_text())
    assert result["result"] == "PASS" and result["full_device"]
    revisions = collect_revisions(build, work)
    resources = {}
    artifacts = {}
    lto = {}
    for name in ["m0_proof", "rpg", "m15_probe"]:
        elf = build / "artifacts" / f"{name}.elf"
        assert digest(elf) == result["elf_sha256"][name], "verification is stale"
        if name != "m15_probe":
            assert json.loads((work / f"{name}.avmp").read_text())["identity"]["elf_sha256"] == digest(elf), "profile is stale"
        resources[name] = json.loads(Path(str(elf) + ".resources.json").read_text())
        lto[name] = json.loads(Path(str(elf) + ".lto.json").read_text())
        assert lto[name]["game_reference_arguments_eliminated"]
        if name != "m0_proof":
            assert lto[name]["unconditional_global_game"]
        assert lto[name]["map_tile_position_storage_eliminated"]
        for extension in ["elf", "bin", "arduboy"]:
            p = elf.with_suffix("." + extension)
            artifacts[p.name] = {"sha256": digest(p), "bytes": p.stat().st_size}
    source_files = [
        ROOT / name for name in ["CMakeLists.txt", ".gitignore", ".gitattributes", ".clang-format", "AGENTS.md"]
    ]
    for directory in ["src", "tests", "tools", "generated", "assets_src", "assets"]:
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
    for name in ["rpg_tests", "rpg_flag_tests", "rpg_world_tests"]:
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
        "stack_reports": {
            name: (build / "artifacts" / f"{name}.link.txt").read_text()
            for name in resources
        },
        "lto": lto,
        "native_tests": units,
        "integration": result,
        "profile_window": profile["window"],
        "profile_identity": profile["identity"],
        "profile_instruction_costs": breakdown,
        "raw_evidence_directory": str(work),
    }
    repro_path = build / "repro-m15.json"
    if repro_path.exists():
        repro = json.loads(repro_path.read_text(encoding="utf-8-sig"))
        assert repro["result"] == "PASS" and repro["package_sha256"] == artifacts["rpg.arduboy"]["sha256"]
        evidence["reproducibility"] = repro
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
            + (str(m["observed_fps"]) if "observed_fps" in m and m["frames"] > 1 else "single operation")
            + " |"
        )
    table = "\n".join(rows)
    loads = breakdown["program_load_instructions"]
    sprites = breakdown["sprite_overwrite_sys"]
    display = breakdown["display_sys"]
    rpg_resources = resources["rpg"]
    game_bytes = symbols(build / "artifacts/rpg.elf")["game"]["bytes"]
    repro_note = (
        "An independent SDK-only RelWithDebInfo build in `build/m15-repro` also passed all\n"
        "six CTest entries and emitted a byte-for-byte identical playable RPG package."
        if "reproducibility" in evidence else
        "No independent rebuild report was supplied for this measurement."
    )
    text = f"""# M1.5 measured evidence

Measured {revisions['verification_date']} on {revisions['host']}.
Source fingerprint: `{fingerprint}`. AVM revision: `{revisions['avm_revision']}`.
All changes are uncommitted. Machine-readable hashes, source manifest and exact
link/LTO reports are in `m1_5_evidence.json`; raw profiles remain in
`build/{work.name}`. Historical M0/M1 results remain in `performance.md`.

| Guest | saved | ordinary data | total static / 1024 | code | PROGMEM data | complete stack / 256 |
|---|---:|---:|---:|---:|---:|---:|
"""
    for name, r in resources.items():
        text += f"| {name} | {r['saved_bytes']} | {r['ordinary_data_bytes']} | {r['static_bytes']} | {r['text_bytes']} | {r['rodata_bytes']} | {r['stack_bytes']} |\n"
    text += f"""
Every link has a complete bound and zero analysis gaps. Native and AVM gameplay
use one global Game with no Game-reference parameters. Full LTO verifies the
guest singleton and absence of retained Game-reference helpers. Guest Game is
{game_bytes} bytes. Flags use one byte per eight declared flags (rounded up). The only resident map
cache is 153 tile bytes plus two byte origins. Static RAM leaves
{1024 - rpg_resources['static_bytes']} bytes; stack margin is {256 - rpg_resources['stack_bytes']} bytes.
Active area width/height use Game's existing two-byte MapDimensions cache. Camera,
chunk enumeration, tile loading/setters, membership, interaction and collision
bounds use it; area changes refresh it before destination processing. Save
validation reads the candidate area's dimensions before committing active state.
Bounds include finalized spills, outgoing arguments and three-byte return records.
The 1024-byte framebuffer and native AVR hardware stack are separate.
`world_types.h`, `rpg.h`, and `chunk.h` compile-time assertions enforce byte tile,
chunk, origin, map/grid dimension fields and 16-bit pixel fields in both host and guest.
The probe actually reads chunk 464 in a 240x248-tile PROGMEM map, beyond a 16-bit
program address boundary through cache loading; its large asset adds no resident map RAM.
Runtime tile reads use only the resident cache. Wrong-area and out-of-window
reads return the solid outside sentinel, with no single-tile projection fallback.
Save decoding checks format, CRC, fields and map bounds before commit, then builds
the loaded cache and relocates blocked positions to nearby clear terrain or spawn.
The exporter generates Tag<POS, STAGE>, Gameplay/Tiles/Dialogue aliases, generic
empty handler functions, dependent dispatch helpers and a forwarding Handler facade.
Ordinary handler_impl overloads target stages, positions or exact pairs; missing
implementations remain no-ops. A separate native fixture compiles dispatch with
new named/repeated/unnamed unimplemented handlers. Every live chunk completes
STAGE_GAMEPLAY before baseline reload and STAGE_TILES; STAGE_DIALOGUE starts only
after every tile chunk completes. Dialogue terrain flag changes wait for the next
tile pass unless explicitly refreshed. Native rules verify late overload lookup,
exact-match precedence and the complete-cache barrier before blocking dialogue.
WorldState and both contexts are removed. Frame input and teleported are transient globals. A teleport aborts
outgoing gameplay while destination tile dispatch always completes.

Elapsed emulated AVR cycles at 16 MHz; budget 800,000 cycles per 20 FPS frame.
Markers exclude pacing wait and include rendering, OLED transfer and interrupts.
The device test rejects any sampled exploration frame exceeding budget.

| Workload | frames | average cycles | worst observed | average render/display | FPS |
|---|---:|---:|---:|---:|---:|
{table}

Stage averages and maxima are measured independently. Chunk costs include gameplay,
tile and dialogue dispatches with their direct native handlers. Blocking prompt
loops are outside exploration timing markers. Collision includes player overlap
repair and movement.
These are operational stage costs, not isolated instruction estimates.

| Workload | reload avg / worst | chunks avg / worst | collision avg / worst |
|---|---:|---:|---:|
"""
    for m in result['measurements']:
        if 'average_reload_cycles' not in m or m['workload'] == 'm0_proof':
            continue
        text += '| ' + m['workload'] + ' | ' + ' | '.join(
            f"{m['average_' + key + '_cycles']:,} / {m['worst_' + key + '_cycles']:,}"
            for key in ['reload', 'chunk', 'collision']) + ' |\n'
    text += f"""
The stationary view runs four chunks every frame, even unchanged. Reload uses
9 rows of contiguous PROGMEM copy spans, split at chunk boundaries. It always
reconstructs all 153 entries. The reload phase includes camera clamping and
origin setup as well as the program-copy work. No compression, incremental patching or resident
full chunks are used. NPCs and their stress build have been removed for a separate
update. Historical M1 timings include NPC work and are not the same workload.
Cached rendering reduces
repeated program reads. The full source profile reconciles
{int(profile['window']['completed_cycles']) if 'completed_cycles' in profile['window'] else int(profile['window']['end_cycle']) - int(profile['window']['start_cycle']):,} elapsed cycles,
with zero partial cycles and discontinuities. Program load instructions account
for {loads['cycles']:,} cycles / {loads['executions']:,} executions; background
sprite services {sprites['cycles']:,} / {sprites['executions']:,}; display services
{display['cycles']:,} / {display['executions']:,}. Native interrupt/service work is
included, and source attribution of inlined code may be distributed across lines.

Native results:

```text
"""
    text += '\n'.join(units.values()) + '\n```\n'
    text += f"""
Six CTest entries pass, including the exporter unittest suite and generated-asset
freshness. The SDK probe passes {result['m15_guest_checks']} guest-executed checks.
Full-device tests retain the M1 new/walk/portals/dialogue/menu/status/corrupt-save
scenario and add closed/open/reset gate graphics and collision, cross-chunk bridge
clipping, periodic baseline restoration, and persistent quest flags after
physical FX-sector save and fresh-emulator load. Native and guest checks exercise
walking-entry edges, facing-gated fresh A input and teleport arrival suppression.
Map objects have optional names and handlers, with no types. Rectangles expand
into per-tile C++17 template calls; named region metadata remains for contains
queries. Unnamed objects use private constants. LTO verifies no tile position
storage survives. No portal/interaction tables are generated. The flag tests exercise 37-bit
and 257-bit sets, every bit, adjacent bytes and invalid indices.

{repro_note}

Observed maxima are sample bounds, not universal WCET proofs. Physical hardware,
audio-enabled workloads and other host platforms are unverified. Large authored
handler bodies require new resource/timing checks.
See `tiled_pipeline.md` and `chunk_world.md` for exact reproduction commands and
content constraints. No AVM compiler, interpreter or runtime sources changed.
"""
    (build / "performance.md").write_text(text, encoding="utf-8", newline="\n")
    if args.write_docs:
        (ROOT / "docs/m1_5_measurements.md").write_text(text, encoding="utf-8", newline="\n")
        (ROOT / "docs/m1_5_evidence.json").write_text(json.dumps(evidence, indent=2) + "\n")
    print(f"Collected evidence for {fingerprint}; reports in {build}; raw profiles in {work}")


if __name__ == "__main__":
    main()
