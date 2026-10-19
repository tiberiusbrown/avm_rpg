# 16x16 measured evidence

Measured 2026-10-19 in RelWithDebInfo; existing MSVC/Ninja and AVM SDK.
Source fingerprint `e6f520aba316d56d0c4b5b8e59a219da8c20345c508cb54bcef82209677bde9b`.
Exact source/package hashes, complete link paths and LTO checks are in
[tiles16_evidence.json](tiles16_evidence.json). Raw LLDB logs, PGM captures
and complete source profiles remain in `build/verification-48u7f5h2`.

| Guest | saved | ordinary RAM | total static / 1024 | code | PROGMEM | complete stack / 256 |
|---|---:|---:|---:|---:|---:|---:|
| m0_proof | 0 | 12 | 12 | 489 | 1017 | 30 |
| rpg | 24 | 243 | 267 | 11051 | 39700 | 171 |
| m15_probe | 0 | 263 | 263 | 7794 | 59841 | 174 |
| m16_probe | 0 | 238 | 238 | 23549 | 321 | 219 |

Playable Game is 230 bytes. The cache uses 47 bytes (45 IDs plus origins),
saving 108 bytes from the former 17x9 cache. Eight live NPCs still reserve
156 bytes including requests. Character rows/talkability live in ROM definitions.
Every guest has a complete stack bound and zero analysis gaps. Full LTO verifies
one Game, no Game-reference helper parameters and no stored TilePos constants.
The separate framebuffer and native AVR hardware stack are excluded.

Full-device emulated AVR cycles at 16 MHz include simulation, rendering,
display transfer and interrupts; pacing waits are excluded. Exploration checks retain
an 800,000-cycle diagnostic ceiling. The RPG requests 30 FPS (33 ms periods);
the proof targets 20 FPS. All measured RPG frames also meet the 33 ms interval.

| Workload | frames | average cycles | worst observed | render/display | FPS |
|---|---:|---:|---:|---:|---:|
| m0_proof | 30 | 33,660 | 33,679 | 19098 | 20.0001 |
| idle_grove | 30 | 280,844 | 294,885 | 118907 | 30.3034 |
| pixel_scroll | 30 | 257,021 | 325,554 | 105360 | 30.3049 |
| dense_collision | 30 | 227,970 | 230,569 | 97316 | 30.3044 |
| fxc_idle_grove | 30 | 280,844 | 294,885 | 118907 | 30.3034 |
| fxc_pixel_scroll | 30 | 257,021 | 325,554 | 105360 | 30.3049 |
| fxc_dense_collision | 30 | 227,970 | 230,569 | 97316 | 30.3044 |

Ten CTest suites pass: core, essential_npcs, npc_lifecycle_hooks, flags,
chunk_world, tiled_pipeline, generated_assets, content, original_music and audio_timing. Pipeline tests check
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
measurements include music. The A2 score test decodes two complete loops of
each of the six original songs, checking every note attack, exact musical
duration, clean release tails, full channel resets, repeatability and mixer
headroom. Full-device QA cycles through all six songs and back to the default.
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
