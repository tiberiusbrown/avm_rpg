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

The Grove battle has six data-defined archetypes (fighter, rogue, archer, spearman, wolf, brute) and
pits the party's rogue and archer against a wolf and a brute. The enemy chooses with a non-recursive
negamax search with alpha-beta pruning (`src/battle_ai.cpp`), deepened one ply at a time under a
deterministic node budget of 300 positions. Only a completed iteration's choice is used; with none, the
greedy choice is. Every guest link reports a complete stack bound.

Measured on the AVM emulator with `tests/battle_probe.cpp` (`avm time` at each probe mark), and on
the host with `rpg_battle_bench`. "Before" is the four-ply search over the earlier encounter (two
identical heroes against two identical creatures, one-step moves and adjacent attacks only), measured
again at the start of this work with the same probe; the positions are the analogous ones in the new
encounter. The opening has the enemy to act before any party move. The in-play position is the first
enemy turn after the archer's opening shot at the brute from range four (before: after hero A's step
north).

| Position | Before (cycles, seconds, nodes) | After (cycles, seconds) | Nodes | Deepest completed | Budget reached | Host µs |
|---|---:|---:|---:|---:|---|---:|
| Initial encounter (opening) | 22,914,708 (1.43 s, 448) | 10,911,093 (0.68 s) | 236 | 3 | no | 6.1 |
| In-play first enemy turn | 18,075,328 (1.13 s, 347) | 10,911,065 (0.68 s) | 236 | 3 | no | 5.8 |
| Mid-battle adjacent combat | 16,148,782 (1.01 s, 274) | 11,245,266 (0.70 s) | 238 | 3 | no | 6.2 |
| Many ranged options (archers and a spearman) | — | 7,158,899 (0.45 s) | 154 | 3 | no | 4.4 |
| Wolf sprint options (two wolves) | — | 13,427,816 (0.84 s) | 300 | 2 | yes | 7.3 |
| Knockback available (rogue beside the brute) | — | 10,225,284 (0.64 s) | 216 | 3 | no | 5.6 |
| Near exhaustion (19 activations) | 24,103,274 (1.51 s, 448) | 10,952,424 (0.68 s) | 236 | 3 | no | 6.6 |
| Exhaustion active (21, adjacent) | 25,457,648 (1.59 s, 483) | 11,569,803 (0.72 s) | 246 | 3 | no | 8.3 |
| One survivor per side | 2,701,776 (0.17 s, 73) | 5,327,752 (0.33 s) | 164 | 4 | no | 3.4 |

Every measured decision now takes under 0.85 s, and the routine ones about 0.7 s. The budget caps
the worst case at 300 positions, about 13.4 million cycles (0.84 s) at the measured cost per
position; the cap is a count of positions, not a time guarantee. Over 300 random enemy-to-act
positions from play, the bounded search visits 209 positions on average, completes depth three in
281 and depth four in 6, falls back to depth two in 13, and reaches the budget in 17. A complete
depth-four search there averages 509 positions (at most 1,038).

The complete search at each depth on the in-play position shows why the depth is bounded: depth one
0.46 M cycles (10 nodes), depth two 2.09 M (45), depth three 9.64 M (208), depth four 34.98 M (756
nodes, 2.19 s). Depth four alone would freeze the display for over two seconds.

### Search structure

* One canonical enumerator (`battle_actions`) lists every legal action exactly once: attacks first, by
  attacker and victim, then moves, by unit, along four rays or eight knight offsets. Player input,
  rendering, the greedy fallback, and the search all use it.
* Actions are packed into one byte (destination in bits 0 to 4, unit in bits 5 and 6), and bit 7 marks
  a forced pass. The plies share one 80-byte action stack: each ply's list starts where its parent's
  ends. Sides alternate and never gain units, so a two-on-two battle (20 actions a side) bounds four
  plies at 80; any other split of the four slots bounds lower, and a side with no living opponent lists
  only at the root.
* Ordering: kills (wins, when one opponent is left) first, then other attacks, then moves that shrink the
  mover's attack gap to its target, then other moves, by in-place partitions. Nodes whose children are
  all leaves skip the approach ordering, which costs more than the leaves it saves. Each ply keeps one
  killer move, tried first among its moves. The root tries the previous iteration's choice first.
* The search fails hard, so a ply needs only its window and two indices. An immediate win ends a node at
  once, and a decided score ends the deepening.
* An iteration starts only when the budget left is at least twice the last completed iteration's nodes.

### Evaluation

The utility keeps the earlier weights for units, HP, and strikes. Its positional term is now the attack
gap: the cells a unit must still travel to line up on a row or column and come within its attack
range, ignoring blockers. An archer adjacent to its target has a gap of one, and a spearman two cells
away has none. A fast mover (wolf, rogue) counts one half-move per cell, others two. Strikes count each
opposing pair on one row or column whose distance is in range with nothing between (the rules'
`battle_line_blocked`), so blocked lines, the archer's minimum range, and the spearman's reach all
count correctly. A compile-time table of the distance outside each type's range replaces the range
arithmetic. The search simulates knockback through `battle_apply`; there is no separate knockback term.

### Costs per operation

Per call on the AVM, copy overhead removed, on the in-play position: evaluation 34.0 thousand cycles
(19 thousand before, with adjacency-only strikes), enumeration 38.8 thousand (the one-pass masks were
29 thousand), approach ordering 44.3 thousand, and an action's apply 4.2 thousand (7 thousand before).
Unlike RAM, a load from a constant object costs 230 to 320 cycles on the AVM, because every constant
object sits in program memory. Copying the type rules and the gap table into RAM saved 5 percent and
was rejected to keep RAM low. A move no longer recounts both sides, because only a defeat can end the
battle.

### Memory and stack

| Guest | Static RAM | Code | Read-only data | Stack / 256 |
|---|---:|---:|---:|---:|
| rpg | 473 → 509 | 24,393 → 25,993 | 121,449 → 121,475 | 243 → 223 |
| rpg_qa | 477 → 513 | 24,549 → 26,149 | 121,449 → 121,475 | 245 → 225 |

`sizeof(BattleState)` is still 22 bytes and `BattleUnit` 4; no ability data is copied with the state.
The six `BattleUnitType` records (7 bytes each) stay in program memory, and so do the derived rule copy
and gap table. The static search workspace is 196 bytes (160 before): four 6-byte plies, four killer
bytes, the 80-byte action stack, and four 22-byte positions for the children of each ply. The work
counters are compiled only into the host and the battle probe (`RPG_BATTLE_STATS`), so the shipping
guest carries none. The enumerator's occupancy masks, the ordering's targets, and the evaluation's
party mask are frame locals. The static RAM increase is the 36 bytes of workspace.

The deepest shipping path is now explore → run_chunk (the battle screen, 45 bytes) → enemy_turn (9) →
deepen (36, the search loop inlined) → battle_score (58). The search parameters are arguments, and the
loop has one caller, so it shares its frame with the deepening loop.

### Decisions

The enemy-behavior tests in `tests/test_battle.cpp` check that the search discovers each mechanic from
the rules: a lone wolf closes two cells in one run; the brute's blow knocks a fighter away from a
cornered, 1 HP ally and saves it; an enemy beside an archer strikes it instead of stepping into its
line; a 2 HP wolf leaves a spearman's two-cell reach; an injured enemy leaves an archer's open line; a
healthy enemy beside a hero attacks and takes a kill; and the party's rogue leaps over its own fighter
and the enemy to win a cornered fight in three plies.

### Simulations

`tests/battle_simulation.cpp` plays the enemy against five party policies on the Grove battle. Random
plays 64 seeded games and its rows are averages, except the totals for wins and losses (the party's).
The last four columns count enemy two-cell runs, enemy knockbacks that moved their victim, party shots
from two or more cells, and party knight leaps.

| Party policy | Actions | Enemy attacks | Enemy moves | Enemy retreats | Damage to party | Damage to enemies | Party wins / losses | Runs | Knockbacks | Shots | Leaps |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Aggressive | 23 | 6 | 5 | 4 | 7 | 6 | 0 / 1 | 2 | 3 | 1 | 7 |
| Defensive | 25 | 4 | 8 | 1 | 6 | 4 | 0 / 1 | 4 | 1 | 1 | 11 |
| Greedy | 23 | 5 | 6 | 3 | 7 | 8 | 0 / 1 | 1 | 3 | 1 | 5 |
| Search | 35 | 1 | 16 | 2 | 1 | 2 | 0 / 1 | 4 | 0 | 1 | 11 |
| Random (64 games) | 24.6 | 4.1 | 7.9 | 1.2 | 6.4 | 0.9 | 0 / 64 | 2.7 | 0.7 | 0.2 | 6.6 |

A retreat is an enemy move that increases its attack gap to the nearest hero. The new encounter favors
the enemy: the brute's 6 HP and knockback outlast these policies, and no game times out. Device QA
wins with a ten-action plan found by breadth-first search against the real enemy
(`GROVE_VICTORY_PLAN` in `tests/device_tests.cpp`).

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
