# M0/M1 performance evidence

The measurements below are retained historical evidence. Current 16x16 resource,
stage-cost and full-device results are in [tiles16_measurements.md](tiles16_measurements.md)
and [tiles16_evidence.json](tiles16_evidence.json). Reproduce them with the commands
in [tiled_pipeline.md](tiled_pipeline.md). The current sample has two NPCs and a sign.

Measured 2026-10-15 on Windows x64; MSVC 19.51.36260.0; Ninja; RelWithDebInfo. Exact input schedules,
ELF/image/interpreter hashes, section sizes, source fingerprint and native test
results are generated locally in `build/evidence.json`. Source fingerprint for this
documented measurement: `61bbfda485bc2dba8c2bbd3dd1222e3185b669b442b493f68526d6f225f98221`.
AVM revision: `3ceedda679c03c00a24d0353be2bedce0478ed4e`. RPG changes remain uncommitted.

## Memory and complete link-time stack bounds

| Guest | .saved | ordinary .data | total static / 1024 | code | read-only data | stack / 256 |
|---|---:|---:|---:|---:|---:|---:|
| m0_proof | 0 | 12 | 12 | 489 | 1017 | 30 |
| rpg | 24 | 131 | 155 | 5427 | 2739 | 172 |
| rpg_stress | 24 | 131 | 155 | 5422 | 2739 | 172 |

All three links report **complete bound: yes**, with zero analysis gaps. The
M1 maximum path is CRT -> loop -> save_decode -> game_new -> world_enter ->
position_clear; its 172-byte bound includes spills, outgoing arguments and
three-byte return records. SYS native AVR stack is outside the guest stack.
The framebuffer is a separate 1024 bytes. Guest Game is 123 bytes, NPC 8 bytes,
and all 12 NPC slots are reserved even in the normal build. Host Game is 124
bytes; no host sizeof value is used as guest resource evidence. No heap exists
in guest rules. M1 leaves 869 static bytes and 84 stack bytes of margin.

All game rules and native C++ tests use C++20. Guest full LTO specializes the
shared Game references to the single global object: game_new and save_decode
retain no arguments, world_enter retains area/position/facing, and position_clear
retains position/actor-ignore values. Other rules are inlined. Each link saves
optimized LLVM IR and checks that the Game reference is absent from retained
rule signatures. Each link records the exact signatures in its
`build/artifacts/*.elf.lto.json` report.

## Full-frame costs (elapsed emulated AVR cycles)

The clock is 16 MHz; the 20 FPS / 50 ms budget is **800,000 cycles**. These are
actual interpreter/device costs including interrupts and OLED transfers, not
guest opcode counts or host wall-clock times. Markers bracket accepted simulation
frames; pacing wait between frames is excluded. Render/display markers include
the full screen render and avm_display(true), with tiny marker overhead. FPS is
measured from the interval between frame beginnings, including pacing wait.

| Workload | Samples | Average cycles | Worst observed cycles | Average render + display | Emulated FPS |
|---|---:|---:|---:|---:|---:|
| m0_proof | 120 | 33,630 | 33,655 | 19,095 | 20 |
| stationary_world | 120 | 417,963 | 438,144 | 366,294 | 20 |
| dialogue | 60 | 318,667 | 318,673 | 314,702 | 20 |
| area_transitions | 2 | 369,314 | 447,755 | — | single operation |
| save_and_readback | 1 | 543,773 | 543,773 | — | single operation |
| continue_load | 1 | 522,475 | 522,475 | — | single operation |
| pixel_scroll | 70 | 415,637 | 466,359 | 348,197 | 20 |
| dense_collision | 120 | 391,078 | 428,048 | 332,527 | 20 |
| twelve_npcs_stationary | 240 | 564,570 | 664,255 | 387,770 | 20 |
| twelve_npcs_collision | 120 | 564,790 | 638,554 | 390,873 | 20 |

Normal exploration and the 12-NPC workload sustain **20 FPS** in these windows.
The observed maxima are samples, not a mathematical universal worst-case bound.
Safe-point saving/loading were measured separately, on an initially erased save
sector. Saving is deliberate rather than performed every frame.

The full-device scenario exercises stationary display, intermediate player pixels,
scrolling, NPC movement, dense terrain collision, dialogue/choices, both portals,
menu/status, saving, a new emulator instance loading the physical sector, changed
dialogue after restart, and corrupt-data startup. Native tests additionally cover
unsupported formats, bounds/corners, directions, deterministic wandering/limits,
spawn relocation, UI freeze, refusal choices, checksum failures and unchanged
state after failed load. CTest has four passing tests; exact rule-check counts are
in the local evidence report. An independent build/repro directory also built and passed all four,
and its playable package is byte-for-byte identical to the primary package.

## Source profiling and asset access

Each SDK profile uses one second of warmup and an eight-second measurement
window with tests/replays/exploration.json, deterministic named-button events
at cycle offsets. M0, normal and 12-NPC profiles all completed. The normal
window contains 128,000,000 reconciled cycles, 160 displayed frames, zero
partial cycles and zero discontinuities. Native active/waiting costs are
separate from guest-PC attribution. Optimized inline sites retain source
identity; an unmapped source row does not mean an unaccounted cycle.

Within that normal eight-second profile:

* Direct AS1 program-load instructions: 7,834,375 cycles across
  27,320 executions (286.8 cycles/execution).
  This includes map bytes, area descriptors and NPC definitions, with interpreter
  dispatch/interrupt intervals; it is not isolated flash chip latency.
* Background sprite SYS calls: 11,971,114 cycles / 21,717
  calls (551.2 cycles/call). Sprite FX reads and
  native blits are included here, not in the direct guest program-load count.
* Display SYS calls: 2,982,767 cycles / 160 calls
  (18642.3 cycles/call). This excludes argument
  setup but includes the native transfer and interrupt activity.

The map loop and sprite rendering dominate useful frame work. Selected-sprite
services and skipping blank path/interior tiles already avoid needless transfers.
No cache, compression, dirty rectangle scheme or lower frame rate was necessary.
The 12-NPC test remains within budget with pixel motion intact. Further detail,
audio or battle rendering requires new profiles; current timings are not reserved
capacity for those additions.

## Battle search

The creatures use a four-activation negamax search with alpha-beta pruning
(`src/battle_ai.cpp`). It runs on an explicit stack and never recurses. The guest link
reports a complete stack bound for every image.

Measured on the AVM emulator with `tests/battle_probe.cpp` (`avm time` at each probe mark).
The opening position has the creatures to act before any party move. The in-play position is
the first enemy turn after the heroes' opening move, which is the turn the 2.25 second figure
described before this work.

| Position (depth four) | Before (cycles, seconds) | After (cycles, seconds) | Nodes / leaves before | Nodes / leaves after |
|---|---:|---:|---:|---:|
| Initial encounter (opening) | 63,969,104 (4.00 s) | 22,914,708 (1.43 s) | 570 / 421 | 448 / 336 |
| Adjacent combat | 45,355,789 (2.83 s) | 16,148,782 (1.01 s) | 405 / 284 | 274 / 174 |
| One survivor per side | 7,610,179 (0.48 s) | 2,701,776 (0.17 s) | 88 / 54 | 73 / 47 |
| Near exhaustion | 62,357,486 (3.90 s) | 24,103,274 (1.51 s) | 558 / 415 | 448 / 336 |
| Exhaustion active | 58,972,440 (3.69 s) | 25,457,648 (1.59 s) | 517 / 374 | 483 / 368 |
| In-play first enemy turn | 36,062,515 (2.25 s) | 18,075,328 (1.13 s) | not measured | 347 / 246 |

The initial encounter is 64 percent faster but still takes 1.43 s, so the one-second goal is not
met. Near exhaustion and exhaustion active are now the costliest measured positions, at 1.51 s and
1.59 s.

The in-play "before" figure was measured in-game with `avm-lldb` on the first enemy turn,
using the same search before this work. The probe measures the opening position directly, so
its before and after figures come from the same method. The depth increments on the opening
position are 0.28 M cycles for depth one, 1.19 M for depth two, 3.80 M for depth three, and
22.9 M for depth four.

Per-operation costs on the AVM, measured by the probe's kernels with copy overhead included:
the utility is about 19 thousand cycles per call, the one-pass legal-action masks about 29
thousand, and a child's apply about 7 thousand. The old cursor enumeration was about 72
thousand per full pass, and the search no longer uses it.

Memory on the guest: static RAM is 473 bytes, down from 498 (the search workspace is 160
bytes, down from 185). Every type has byte alignment on the AVM, so the old `SearchFrame` was 37
bytes: a 22-byte position, the side, the window, the best score, the current and best actions, the
legal-action mask, the slot and the stage. Five frames made the 185 bytes. The new workspace has
four `SearchPly` records of 18 bytes (window, best score, five class counts, stage and a pass flag)
and four 22-byte positions, one for each of plies one to three and one for the leaf, 160 bytes in
all. Only the root keeps an action. The search counters are not in the guest image, because nothing
in the guest reads them. The host bench reports 196 bytes for the old layout because host alignment
pads each frame to 38 bytes. Code grew by 809 bytes, to 25,301. Read-only data is unchanged at
120,935 bytes.

Stack on the guest: the shipping `rpg` bound is 243 bytes (205 before this change), and the
device-QA image is 245 bytes (207 before), against a 256-byte budget. The deepest path is `battle_search` into `battle_score`, and the
utility's frame is the largest part of it.

Host measurements from `rpg_battle_bench`, built with the same flags before and after, with the
enemy to act at depth four:

| Position | Nodes (before → after) | Leaves (before → after) | Unpruned | Host µs (before → after) |
|---|---:|---:|---:|---:|
| Initial encounter | 570 → 448 | 421 → 336 | 2,434 | 67.4 → 18.4 |
| Adjacent combat | 405 → 274 | 284 → 174 | 3,134 | 37.0 → 12.6 |
| One survivor per side | 88 → 73 | 54 → 47 | 265 | 7.8 → 2.0 |
| Near exhaustion (19 activations) | 558 → 448 | 415 → 336 | 2,434 | 49.5 → 21.1 |
| Adjacent, exhaustion next | 202 → 222 | 126 → 137 | 3,126 | 20.7 → 11.1 |

Over 300 random legal positions with the enemy to act, the mean is 440 nodes (570 before) and the
largest is 855 (1,035 before), against an unpruned mean of 2,677. The exhaustion-next position
searches about 10 percent more nodes than before, but each node is cheaper, so it runs faster.

### Decisions before and after

These are the positions that `enemy_behavior()` in `tests/test_battle.cpp` checks. The "before"
column is the committed negamax search with the previous evaluation.

| Position | Before | After |
|---|---|---|
| A. Healthy creature beside hero A | steps back to (4,0) | attacks hero A from (4,1) |
| B. Nothing in reach | sidesteps to (6,2), no closer to a hero | advances to (5,1) |
| C. Creature A at 1 HP beside hero A, healthy creature B beside hero B | creature A steps back to (4,0) | attacks; creature A and creature B score the same (−1620 for the enemy, against −1770 for a retreat), and the tie goes to creature A |
| E. Creature A at 1 HP beside hero A, creature B far away | steps back to (5,1) | steps back to (4,0) |
| Kill available on hero B | attacks from (4,1) | same |
| Last creature at 1 HP | steps back to (4,0) | same |

### Simulations

`tests/battle_simulation.cpp` plays the negamax enemy against five party policies on the Grove
battle: aggressive, defensive, greedy, search, and random. Random plays 64 seeded games; the others
play one game each. A retreat is an enemy move that increases its Manhattan distance to the nearest
living hero. Entries are before → after. Random rows are averages per game, except the wins and
losses, which are totals. Wins and losses are the party's.

| Party policy | Actions | Enemy attacks | Enemy moves | Enemy retreats | Longest retreat run | Damage to party | Damage to enemies | Party wins / losses |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Aggressive | 28 → 22 | 2 → 5 | 12 → 6 | 10 → 5 | 6 → 5 | 4 → 9 | 4 → 6 | 1 / 0 → 0 / 1 |
| Defensive | 36 → 36 | 0 → 0 | 18 → 18 | 6 → 1 | 3 → 1 | 0 → 0 | 0 → 0 | 1 / 0 → 1 / 0 |
| Greedy | 28 → 22 | 2 → 5 | 12 → 6 | 10 → 5 | 6 → 5 | 4 → 9 | 4 → 6 | 1 / 0 → 0 / 1 |
| Search | 32 → 36 | 1 → 0 | 15 → 18 | 7 → 2 | 3 → 2 | 2 → 0 | 2 → 0 | 1 / 0 → 1 / 0 |
| Random (64 games) | 33.1 → 25.5 | 1.4 → 3.9 | 14.9 → 8.5 | 4.3 → 1.1 | 5 → 3 | 2.5 → 7.0 | 0.2 → 1.6 | 24 / 40 → 1 / 63 |

The enemy now attacks healthy targets: attacks rise in the aggressive and greedy games, and the party
loses both games that it won before. Against the defensive party, retreats fall from 6 to 1, but the
enemy never attacks, because the party stays out of reach. When both sides search, neither attacks;
the enemy advances 18 times, retreats twice, and the party wins at exhaustion. The longest enemy
retreat run in the aggressive and greedy games is still five. It is a lone injured creature stepping
back from a stronger hero until exhaustion, and changing the strike or distance weights in the sweeps
did not remove it. The encounter is harder for the naive policies; the combat rules and the encounter
data are unchanged. No game timed out. Exhaustion was reached in every single-game row and in 58 of
the 64 random games, against 64 before.

## Reproduce

From the RPG root with SDK on PATH and a matching MSVC developer environment:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DAVM_SDK_ROOT=/path/to/sdk
cmake --build build --config RelWithDebInfo --parallel 4
ctest --test-dir build -C RelWithDebInfo --output-on-failure
```

For complete visible/persistence/per-frame QA, configure the existing matching
AVM source/build library with -DAVM_SOURCE_ROOT=/path/to/avm and
-DAVM_BUILD_ROOT=/path/to/avm/build, then build. The core installed-SDK command is:

```sh
python tests/guest_integration.py --sdk /path/to/sdk --build build
python tools/collect_evidence.py --build build
```

The collector writes `evidence.json`, `revisions.json`, and `performance.md` under
the ignored build directory. It collects revision metadata from local checkouts;
the reports are not inputs to the build or tests. This document retains the
summary of the measurement above.

The integration script creates a fresh directory, launches actual ELF and package
guests, runs frame-based input scenarios, and records avm-prof traces/HTML reports.
Raw evidence for this run is `verification-ior6tcas` under build/; latest-verification.txt
points to it. SDK-only users can execute the LLDB/replay checks, but full-device
per-frame/restart checks are explicitly skipped without the optional harness.

Limitations: Windows/MSVC and emulated device verified; physical hardware, other
host platforms and audio-enabled workloads remain unverified. The installed SDK's unfiltered visible buffer can be
stale, so full-device QA refreshes it using Ardens advance(0). No VM/toolchain
source changes were made to work around that capture issue.
