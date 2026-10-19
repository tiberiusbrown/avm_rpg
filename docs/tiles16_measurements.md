# 16x16 measured evidence

Measured 2026-10-19 in RelWithDebInfo; existing MSVC/Ninja and AVM SDK.
Source fingerprint `a7eb48c943b428657a4142b14a95659bea075b30810723775504785d299d1ee0`. Changes remain uncommitted.
Exact source/package hashes, complete link paths and LTO checks are in
[tiles16_evidence.json](tiles16_evidence.json). Raw LLDB logs, PGM captures
and complete source profiles remain in `build/verification-7kun5bdo`.

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
| m0_proof | 30 | 33,660 | 33,679 | 19098 | 20.0001 |
| idle_grove | 30 | 262,271 | 281,921 | 114864 | 30.3047 |
| pixel_scroll | 30 | 239,347 | 293,187 | 100380 | 30.3049 |
| dense_collision | 30 | 220,368 | 223,441 | 94060 | 30.3013 |
| fxc_idle_grove | 30 | 262,271 | 281,921 | 114864 | 30.3047 |
| fxc_pixel_scroll | 30 | 239,347 | 293,187 | 100380 | 30.3049 |
| fxc_dense_collision | 30 | 220,368 | 223,441 | 94060 | 30.3013 |

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
Startup and restart inherit enabled bootloader USB interrupts with a pending
suspend event, checking that interpreter startup prevents the former default-ISR freeze.
A read-only SPI observer rejects ignored writes and nonzero data at the cycle-17
boundary. Intentional dummy writes at 17 preserve the hardware WCOL flag and advance
the stream. Timing, status flags, transmitted bytes and received bytes use upstream
Ardens behavior without an emulation workaround.
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
sample bounds, not a universal worst-case timing proof. The user confirmed
the rebuilt ordinary FX-C package shows its title and starts a game on
physical hardware on 2026-10-18. Performance measurements remain emulator results.

From the RPG root:

```powershell
./tools/build.ps1 -SdkRoot ../../build/avm-sdk-install
cmake --build build --config RelWithDebInfo --target rpg_integration
python tools/collect_evidence.py --build build --write-docs
```
