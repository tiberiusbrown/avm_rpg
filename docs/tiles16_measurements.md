# 16x16 measured evidence

Measured 2026-10-19 in RelWithDebInfo; existing MSVC/Ninja and AVM SDK.
Source fingerprint `200a834669fc7085d5d2174a8f5188fe74d441453fb9ac8f37367d694a6c2e6e`. Changes remain uncommitted.
Exact source/package hashes, complete link paths and LTO checks are in
[tiles16_evidence.json](tiles16_evidence.json). Raw LLDB logs, PGM captures
and complete source profiles remain in `build/verification-3_oog24g`.

| Guest | saved | ordinary RAM | total static / 1024 | code | PROGMEM | complete stack / 256 |
|---|---:|---:|---:|---:|---:|---:|
| m0_proof | 0 | 12 | 12 | 489 | 1017 | 30 |
| rpg | 24 | 242 | 266 | 10782 | 24334 | 171 |
| m15_probe | 0 | 263 | 263 | 7794 | 59841 | 174 |
| m16_probe | 0 | 238 | 238 | 23549 | 321 | 219 |

Playable Game is 230 bytes. The cache uses 47 bytes (45 IDs plus origins),
saving 108 bytes from the former 17x9 cache. Eight live NPCs still reserve
156 bytes including requests. Character rows/talkability live in ROM definitions.
Every guest has a complete stack bound and zero analysis gaps. Full LTO verifies
one Game, no Game-reference helper parameters and no stored TilePos constants.
The separate framebuffer and native AVR hardware stack are excluded.

Full-device emulated AVR cycles at 16 MHz include simulation, rendering,
display transfer and interrupts; pacing waits are excluded. The 20 FPS budget
is 800,000 cycles, checked on every observed exploration frame.

| Workload | frames | average cycles | worst observed | render/display | FPS |
|---|---:|---:|---:|---:|---:|
| m0_proof | 30 | 33,654 | 33,673 | 19095 | 20.0001 |
| idle_grove | 30 | 262,213 | 281,869 | 114806 | 30.3047 |
| pixel_scroll | 30 | 239,296 | 293,136 | 100330 | 30.3049 |
| dense_collision | 30 | 220,328 | 223,397 | 94020 | 30.3013 |
| fxc_idle_grove | 30 | 262,213 | 281,869 | 114806 | 30.3047 |
| fxc_pixel_scroll | 30 | 239,296 | 293,136 | 100330 | 30.3049 |
| fxc_dense_collision | 30 | 220,328 | 223,397 | 94020 | 30.3013 |

Nine CTest suites pass: core, essential_npcs, npc_lifecycle_hooks, flags,
chunk_world, tiled_pipeline, generated_assets, content and peaceful_music. Pipeline tests check
every source bitmap/alpha pixel, all 255 usable indices, the reserved cell,
padding, maximum map dimensions, chunk relocation and default handlers.

```text
PASS 16x16 core: 8274 checks
PASS flag bitsets: exact storage, every bit, byte boundaries and invalid indices
PASS chunk/cache world: 65354 checks
PASS essential NPCs: 5448 checks
PASS independent NPC lifecycle policy: 20 checks
```

AVM guest checks: {'m15_probe': 72, 'm16_probe': 5448}.
Full-device PASS for FX and FX-C covers actual buttons, two sample NPCs, sign dialogue,
paused simulation, menus/status, physical FX save/restart, corrupt-save
rejection, terrain scrolling and actual masked actor overlap pixels.
Audio checks verify silent titles, audible unclipped PWM after New Game and
Continue, and uninterrupted playback through dialogue and menus. Exploration
measurements include music. The A2 score test decodes two full 48-second loops.
Both source profile windows are complete. The render sort needs at most nine
byte indices and 36 comparisons/shifts, with no added persistent RAM.
The 59,520-byte fixture reaches chunk 1859 and byte offset 59519. The NPC probe
covers 8x8 footprint edges, depth sorting/ties, wandering and targets,
terrain repair, walkaround, animated yielding and tight-corridor passage;
independent native hooks additionally cover route behavior.

The earlier map had different handlers and NPCs. Timing comparisons cannot
isolate tile size from the simultaneous content reset. Observed maxima are
sample bounds, not a universal worst-case timing proof. Physical hardware
remains unverified.

From the RPG root:

```powershell
./tools/build.ps1 -SdkRoot ../../build/avm-sdk-install
cmake --build build --config RelWithDebInfo --target rpg_integration
python tools/collect_evidence.py --build build --write-docs
```
