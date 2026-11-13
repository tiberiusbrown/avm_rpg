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

The battle has six data-defined archetypes (fighter, rogue, archer, spearman, wolf, brute) and up to
four units a side in eight unit slots. The Grove battle pits the party's rogue and archer against a
wolf and a brute. The enemy chooses with a non-recursive negamax search with alpha-beta pruning
(`src/battle_ai.cpp`), deepened one ply at a time under a deterministic node budget: 300 positions
with up to four living units, scaled by four over the living units beyond that (200 with six, 150 with
eight), because a position costs more to search the more units live. Only a completed iteration's
choice is used; with none, the greedy choice is. Every guest link reports a complete stack bound.

Measured on the AVM emulator with `tests/battle_probe.cpp` (`avm time` at each probe mark), and on
the host with `rpg_battle_bench`. "Before" is the four-ply search over the earlier encounter (two
identical heroes against two identical creatures, one-step moves and adjacent attacks only), measured
with the same probe; the positions are the analogous ones in the new encounter. The opening has the
enemy to act before any party move. The in-play position is the first enemy turn after the archer's
opening shot at the brute from range four (before: after hero A's step north). The four-a-side
position is fighter, rogue, archer, and spearman against wolf, brute, archer, and fighter, four columns
apart; three a side drops each side's last unit; "in contact" moves the enemy line two columns closer.

| Position | Before (cycles, seconds, nodes) | After (cycles, seconds) | Nodes | Deepest completed | Budget reached | Host µs |
|---|---:|---:|---:|---:|---|---:|
| Initial encounter (opening) | 22,914,708 (1.43 s, 448) | 9,609,382 (0.60 s) | 191 | 3 | no | 5.4 |
| In-play first enemy turn | 18,075,328 (1.13 s, 347) | 9,609,427 (0.60 s) | 191 | 3 | no | 5.2 |
| Mid-battle adjacent combat | 16,148,782 (1.01 s, 274) | 11,733,950 (0.73 s) | 238 | 3 | no | 6.9 |
| Many ranged options (archers and a spearman) | — | 6,561,114 (0.41 s) | 134 | 3 | no | 3.6 |
| Wolf sprint options (two wolves) | — | 11,966,188 (0.75 s) | 245 | 3 | no | 6.6 |
| Knockback available (rogue beside the brute) | — | 10,359,179 (0.65 s) | 208 | 3 | no | 5.9 |
| Near exhaustion (19 activations) | 24,103,274 (1.51 s, 448) | 9,706,265 (0.61 s) | 191 | 3 | no | 5.4 |
| Exhaustion active (21, adjacent) | 25,457,648 (1.59 s, 483) | 12,158,051 (0.76 s) | 246 | 3 | no | 6.8 |
| One survivor per side | 2,701,776 (0.17 s, 73) | 4,656,683 (0.29 s) | 121 | 4 | no | 2.8 |
| Four a side (budget 150) | — | 10,717,647 (0.67 s) | 116 | 2 | no | 6.9 |
| Three a side (budget 200) | — | 14,321,559 (0.90 s) | 200 | 2 | yes | 8.4 |
| Four a side in contact (budget 150) | — | 7,267,327 (0.45 s) | 65 | 2 | no | 4.0 |

Every measured decision takes under 0.9 s, and the routine two-on-two ones about 0.6 to 0.75 s. A
position costs about 50 thousand cycles with four units, 72 thousand with six, and 107 thousand with
eight, so the scaled budget caps each composition at roughly 15 million cycles; the cap is a count of
positions, not a time guarantee. Over 300 random enemy-to-act positions from Grove play, the bounded
search visits 190 positions on average, completes depth three in 290 and depth four in 7, falls back
to depth two in 3, and reaches the budget in 9. A complete depth-four search there averages 414
positions (at most 835).

The complete search at each depth on the in-play position: depth one 0.47 M cycles (10 nodes), depth
two 1.93 M (38), depth three 8.54 M (170), depth four 21.71 M (407 nodes, 1.36 s). Depth four alone
would freeze the display for over a second.

### Search structure

* One canonical enumerator (`battle_actions`) lists every legal action exactly once: attacks first, by
  attacker and victim, then moves, by unit, along four rays or eight knight offsets. Player input,
  rendering, the greedy fallback, and the search all use it.
* Actions are packed into one byte: the destination in bits 0 to 4 and the unit in bits 5 to 7. Every
  byte value names an action, so a node with no legal action stores nothing: its plies record marks its
  one forced pass as pending (next one past end) and taken (two past end).
* The plies share one action stack, `battle_action_scratch` (80 bytes); each ply's list starts where its
  parent's ends. The battle screen's destination markers, `battle_can_act`, and the greedy choice use the
  same buffer between searches, so none of them keeps a list on its own frame.
* The stack's depth is set per search from the living units at the root: sides alternate and never gain
  units, so the per-type action limits bound every list below it. The limits come from the rules and the
  8x4 board at compile time: 4 for a fighter or brute, 7 for an archer, spearman, or wolf, and 10 for a
  rogue (at most six leaps on four rows), each capped by its moves plus its opponents. 80 bytes hold
  four plies of any two-on-two battle (the Grove needs 50), three of a mixed four-on-four battle, and two
  of any battle of up to four a side, even four rogues each (`battle_search_depth_limit`).
* Ordering: kills (wins, when one opponent is left) first, then other attacks, then moves that shrink the
  mover's attack gap to its target, then other moves, by in-place partitions. Nodes whose children are
  all leaves skip the approach ordering. Each ply keeps one killer move, tried first among its moves,
  recorded from the child that actually cut the node off. The root tries the previous iteration's choice
  first.
* The search fails hard, so a ply keeps only its alpha and two indices: its beta is its parent's alpha
  negated, which cannot change while the ply is searched. An immediate win ends a node at once, and a
  decided score ends the deepening.
* An iteration starts only when the budget left is at least twice the last completed iteration's nodes.

### Evaluation

The utility keeps the earlier weights for units, HP, and strikes. Its positional term is the attack gap:
the cells a unit must still travel to line up on a row or column and come within its attack range,
ignoring blockers. An archer adjacent to its target has a gap of one, and a spearman two cells away has
none. A fast mover (wolf, rogue) counts one half-move per cell, others two. Strikes count each opposing
pair on one row or column whose distance is in range with nothing between (the rules'
`battle_line_blocked`), so blocked lines, the archer's minimum range, and the spearman's reach all count
correctly. A compile-time table of the distance outside each type's range replaces the range
arithmetic, and every term is added to one sum as it is found. The search simulates knockback through
`battle_apply`; there is no separate knockback term.

### Costs per operation

Per call on the AVM, copy overhead removed, on the in-play position: evaluation 32.0 thousand cycles
(19 thousand before, with adjacency-only strikes), enumeration 43.2 thousand (the one-pass masks were
29 thousand), approach ordering 66.3 thousand, and an action's apply 5.3 thousand (7 thousand before).
The packed units cost a few instructions per field access, and the ordering recomputes each mover's
current gap rather than keeping it, which saves eight bytes on its frame. Unlike RAM, a load from a
constant object costs 230 to 320 cycles on the AVM, because every constant object sits in program
memory. Copying the type rules and the gap table into RAM saved 5 percent and was rejected to keep RAM
low. A move does not recount both sides, because only a defeat can end the battle.

### Memory and stack

| Guest | Static RAM | Code | Read-only data | Stack / 256 |
|---|---:|---:|---:|---:|
| rpg | 473 → 501 | 24,393 → 26,836 | 121,449 → 121,475 | 243 → 219 |
| rpg_qa | 477 → 505 | 24,549 → 26,992 | 121,449 → 121,475 | 245 → 221 |

`BattleUnit` packs its cell, HP (at most 15), type, and team into two bytes, so `BattleState` holds eight
slots in 22 bytes, as it held four before. The six `BattleUnitType` records (7 bytes each), the derived
rule copy, the gap table, and the per-type action limits stay in program memory. The static search
workspace is 188 bytes (160 before): four 4-byte plies, four killer bytes, the 80-byte shared action
scratch, and four 22-byte positions for the children of each ply. The work counters are compiled only
into the host and the battle probe (`RPG_BATTLE_STATS`), so the shipping guest carries none. The
enumerator's occupancy masks, the ordering's target cells (eight bytes), and the evaluation's party
mask are frame locals. The static RAM increase is the 28 bytes of workspace.

The deepest shipping path is explore → run_chunk (the battle screen, 45 bytes) → enemy_turn (9) →
deepen (40, the search loop inlined) → battle_score (50). The search parameters are arguments, and the
loop has one caller, so it shares its frame with the deepening loop.

### Decisions

The enemy-behavior tests in `tests/test_battle.cpp` check that the search discovers each mechanic from
the rules: a lone wolf closes two cells in one run; the brute's blow knocks a fighter away from a
cornered, 1 HP ally and saves it; an enemy beside an archer strikes it instead of stepping into its
line; a 2 HP wolf leaves a spearman's two-cell reach; an injured enemy leaves an archer's open line; a
healthy enemy beside a hero attacks and takes a kill; and the party's rogue leaps over its own fighter
and the enemy to win a cornered fight in three plies. Four-a-side tests cover an eight-unit encounter,
the per-type limits against the enumeration, the depth limits, and the search against the reference.

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
| Search | 32 | 0 | 16 | 8 | 0 | 4 | 1 / 0 | 5 | 0 | 1 | 9 |
| Random (64 games) | 24.3 | 4.2 | 7.7 | 1.1 | 6.5 | 1.0 | 0 / 64 | 2.7 | 0.6 | 0.2 | 6.8 |

A retreat is an enemy move that increases its attack gap to the nearest hero. The encounter favors the
enemy against simple policies; the searching party wins at exhaustion without taking damage. No game
times out. Device QA wins with a ten-action plan found by breadth-first search against the real enemy
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
