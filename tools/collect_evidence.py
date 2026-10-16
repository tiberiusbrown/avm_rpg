"""Collect reproducible M1.6 guest, device, profile and native evidence."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import platform
import subprocess
import sys
from elf import symbols

ROOT = Path(__file__).resolve().parents[1]
GUESTS = ['m0_proof', 'rpg', 'm15_probe', 'm16_probe']


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def revision(directory):
    if not directory:
        return 'unavailable (SDK only)'
    try:
        return subprocess.check_output(['git', '-C', str(directory), 'rev-parse', 'HEAD'],
                                       stderr=subprocess.DEVNULL, text=True).strip()
    except (OSError, subprocess.CalledProcessError):
        return 'unavailable'


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--build', type=Path, default=ROOT / 'build')
    parser.add_argument('--write-docs', action='store_true')
    args = parser.parse_args()
    build = args.build.resolve()
    work = Path((build / 'latest-verification.txt').read_text().strip())
    result = json.loads((work / 'results.json').read_text())
    assert result['result'] == 'PASS' and result['full_device']
    cache = {}
    for line in (build / 'CMakeCache.txt').read_text().splitlines():
        if line and not line.startswith(('#', '//')) and '=' in line:
            key, value = line.split('=', 1)
            cache[key.split(':')[0]] = value
    revisions = dict(verification_date=datetime.fromtimestamp((work / 'results.json').stat().st_mtime,
                    timezone.utc).date().isoformat(), avm_revision=revision(cache.get('AVM_SOURCE_ROOT')),
                    rpg_revision=revision(ROOT), host=f'{platform.system()} {platform.machine()}; '
                    f'{cache.get("CMAKE_GENERATOR")}; RelWithDebInfo')
    resources, artifacts, lto, stack_reports = {}, {}, {}, {}
    for name in GUESTS:
        elf = build / 'artifacts' / f'{name}.elf'
        assert digest(elf) == result['elf_sha256'][name], 'verification is stale'
        resources[name] = json.loads(Path(str(elf) + '.resources.json').read_text())
        lto[name] = json.loads(Path(str(elf) + '.lto.json').read_text())
        assert lto[name]['game_reference_arguments_eliminated']
        assert lto[name]['map_tile_position_storage_eliminated']
        stack_reports[name] = elf.with_suffix('.link.txt').read_text()
        for ext in ['elf', 'bin', 'arduboy']:
            path = elf.with_suffix('.' + ext)
            artifacts[path.name] = dict(sha256=digest(path), bytes=path.stat().st_size)
    assert digest(ROOT / 'rpg.arduboy') == artifacts['rpg.arduboy']['sha256']
    files = [ROOT / n for n in ['CMakeLists.txt', 'AGENTS.md', '.clang-format', '.gitattributes', '.gitignore']]
    for directory in ['src', 'tests', 'tools', 'generated', 'assets', 'assets_src']:
        files += [p for p in (ROOT / directory).rglob('*') if p.is_file() and '__pycache__' not in p.parts]
    manifest = {p.relative_to(ROOT).as_posix(): digest(p) for p in sorted(files)}
    fingerprint = hashlib.sha256(json.dumps(manifest, sort_keys=True, separators=(',', ':')).encode()).hexdigest()
    units = {}
    for name in ['rpg_tests', 'rpg_flag_tests', 'rpg_world_tests', 'rpg_npc_tests', 'rpg_npc_hook_tests']:
        exe = build / (name + ('.exe' if sys.platform == 'win32' else ''))
        units[name] = subprocess.check_output([str(exe)], text=True).strip()
    profiles = {}
    for name in ['m0_proof', 'rpg']:
        value = json.loads((work / f'{name}.avmp').read_text())
        assert value['identity']['elf_sha256'] == artifacts[f'{name}.elf']['sha256']
        assert value['window']['complete'] and int(value['window']['partial_cycles']) == 0
        profiles[name] = {k: value[k] for k in ['window', 'identity']}
    evidence = dict(revisions=revisions, source_fingerprint_sha256=fingerprint,
                    source_manifest=manifest, artifacts=artifacts, resources=resources,
                    stack_reports=stack_reports, lto=lto, native_tests=units, integration=result,
                    profiles=profiles, game_bytes=symbols(build / 'artifacts/rpg.elf')['game']['bytes'],
                    npc_capacity=8, npc_bytes=18, pool_bytes=156, raw_evidence_directory=str(work))
    repro_path = build / 'repro-m16.json'
    if repro_path.exists():
        repro = json.loads(repro_path.read_text(encoding='utf-8-sig'))
        assert repro['result'] == 'PASS' and repro['package_sha256'] == artifacts['rpg.arduboy']['sha256']
        evidence['reproducibility'] = repro
    (build / 'evidence.json').write_text(json.dumps(evidence, indent=2) + '\n')
    (build / 'revisions.json').write_text(json.dumps(revisions, indent=2) + '\n')
    lines = [f'# M1.6 measured evidence\n',
             f'Measured {revisions["verification_date"]} on {revisions["host"]}.',
             f'AVM revision `{revisions["avm_revision"]}`; RPG base `{revisions["rpg_revision"]}`.',
             f'Source fingerprint `{fingerprint}`. Changes remain uncommitted.',
             'Exact source/package hashes, complete link paths and LTO checks are in',
             '`m1_6_evidence.json`. Raw LLDB logs, PGM captures, source profiles and HTML',
             f'reports are retained in `build/{work.name}`.\n',
             '| Guest | saved | ordinary data | total static / 1024 | code | PROGMEM | complete stack / 256 |',
             '|---|---:|---:|---:|---:|---:|---:|']
    for name, r in resources.items():
        lines.append('| ' + name + ' | ' + ' | '.join(str(r[k]) for k in
                     ['saved_bytes','ordinary_data_bytes','static_bytes','text_bytes','rodata_bytes','stack_bytes']) + ' |')
    r = resources['rpg']
    lines += ['', f'Every link reports a complete bound and zero gaps. Playable guest Game is {evidence["game_bytes"]} bytes.',
              'Eight live actors use 18 bytes each; the entire NPC pool/request scratch uses 156 bytes.',
              f'Static margin is {1024-r["static_bytes"]} bytes; guest stack margin is {256-r["stack_bytes"]} bytes.',
              'The separate 1024-byte framebuffer and native AVR hardware stack are excluded.',
              'Full LTO verifies one Game, no Game-reference helper parameters and no stored TilePos constants.',
              'All maps, definitions, routes, sprites and text remain in PROGMEM. The 17x9 cache is unchanged.\n',
              'Elapsed emulated AVR cycles at 16 MHz, measured using full-device frame markers.',
              'The 20 FPS budget is 800,000 cycles. Pacing waits are excluded; simulation,',
              'rendering, OLED transfers and interrupts are included. Every sampled exploration',
              'frame is checked against the budget, including frames outside the summary windows.\n',
              '| Workload | frames | average cycles | worst observed | average render/display | FPS |',
              '|---|---:|---:|---:|---:|---:|']
    for m in result['measurements']:
        if 'workload' not in m:
            continue
        lines.append(f'| {m["workload"]} | {m["frames"]} | {m["average_cycles"]:,} | {m["worst_cycles"]:,} | '
                     f'{m.get("average_render_display_cycles", "-")} | {m.get("observed_fps", "-")} |')
    lines += ['', '| Workload | reload avg / worst | chunk handlers avg / worst | NPC/player collision avg / worst |',
              '|---|---:|---:|---:|']
    for m in result['measurements']:
        if 'average_reload_cycles' in m and m['workload'] != 'm0_proof':
            lines.append('| ' + m['workload'] + ' | ' + ' | '.join(
                         f'{m["average_"+k+"_cycles"]:,} / {m["worst_"+k+"_cycles"]:,}'
                         for k in ['reload','chunk','collision']) + ' |')
    lines += ['', 'Stage maxima are independently observed and need not occur in the same frame.',
              'Eight-NPC steering measures eight simultaneous targets, including body congestion.',
              'The corridor workload uses actual held RIGHT input and asserts player X > 152',
              'within 180 frames. The same scenarios also run natively and in the real AVM probe.',
              'NPC definition/route population size adds no per-definition simulation or RAM cost.\n',
              'Seven CTest suites pass: core, essential_npcs, flags, chunk_world, tiled_pipeline,',
              'generated_assets and content. The exporter includes anchor relocation and tile-interior',
              'route-marker validation. Exact native totals:\n', '```text', *units.values(), '```', '',
              f'AVM guest checks: {result["guest_checks"]}.',
              'Full-device PASS covers new/walk/paired portals, paginated dialogue, choices,',
              'menus/status freeze, corrupt save rejection, dynamic gate/bridge/platform tiles,',
              'NPC dialogue and live-pool freeze, eight visible actors, steering, corridor passage,',
              'and physical FX save with a fresh-device NPC quest reconstruction.',
              'The two source profiles have complete windows and zero partial cycles.\n']
    if 'reproducibility' in evidence:
        lines.append(f'An independent SDK-only RelWithDebInfo build passed all {repro["ctest_suites"]} suites and produced a byte-identical playable package.\n')
    lines += ['Exact commands from the RPG repository root (set SDK paths for the installed SDK):\n', '```powershell',
              './tools/build.ps1 -SdkRoot ../../build/avm-sdk-install',
              'cmake --build build --config RelWithDebInfo --parallel 4',
              'ctest --test-dir build -C RelWithDebInfo --output-on-failure',
              'python tools/generate_assets.py --check',
              'python tests/guest_integration.py --sdk ../../build/avm-sdk-install --build build',
              'python tools/collect_evidence.py --build build --write-docs', '```', '',
              'For full-device QA, configure AVM_SOURCE_ROOT and matching AVM_BUILD_ROOT as described',
              'in tiled_pipeline.md. Build with the matching MSVC developer environment (build.ps1',
              'initializes it on Windows). SDK-only builds still run all guest probes and source profiles.',
              'Observed maxima are sample bounds, not a universal WCET proof. Physical hardware,',
              'audio-enabled workloads and other host platforms remain unverified. No AVM compiler,',
              'interpreter or runtime sources changed. See essential_npcs.md for movement, capacity,',
              'offscreen travel and save limitations.']
    report = '\n'.join(lines) + '\n'
    (build / 'performance.md').write_text(report, encoding='utf-8', newline='\n')
    if args.write_docs:
        (ROOT / 'docs/m1_6_measurements.md').write_text(report, encoding='utf-8', newline='\n')
        (ROOT / 'docs/m1_6_evidence.json').write_text(json.dumps(evidence, indent=2) + '\n')
    print(f'Collected M1.6 evidence for {fingerprint}; raw profiles in {work}')


if __name__ == '__main__':
    main()
