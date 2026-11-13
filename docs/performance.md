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
four units a side in eight unit slots. The Grove battle pits the party's rogue (10 HP) and archer (9)
against a wolf (9) and a brute (12). Normal attacks deal 3, and the brute's 2 plus knockback. Every
voluntary move costs its mover 1 HP (the blood price), paid after it deals 1 engagement damage to each
enemy orthogonally adjacent to its destination; a move whose engagement defeats the last opponent wins
before the price is due. A forced pass costs the passing side's first living unit 1 HP. There is no
exhaustion threshold: every activation lowers the total living HP, so a battle lasts at most as many
activations as its units' starting HP (40 in the Grove). The Grove battlefield has four blocking
tiles, a pine at (4,0), a round tree at (6,0), rocks at (1,3), and a boulder at (4,3): no unit stands
on them or moves through them, they stop shots, thrusts, and knockback, and a rogue's leap passes over
them but cannot land on them.

The enemy chooses with a non-recursive negamax search with alpha-beta pruning (`src/battle_ai.cpp`),
deepened one ply at a time under a deterministic node budget: 480 positions with up to four living
units, scaled by four over the living units beyond that (320 with six, 240 with eight). Only a completed
iteration's choice is used; with none, the greedy choice is. Every guest link reports a complete stack
bound. The budget allows up to two seconds per enemy move: a position costs about 3.7 ms with four
living units, 5.4 ms with six, and 7.9 ms with eight, so a spent budget takes 1.75 to 1.9 s. It was
240 (160, 120), which kept every decision under a second.

While the budgeted search runs it reports progress every budget / 10 positions, the first after one
interval, so a spent budget gives ten steps, about five a second, and a decision under 0.2 s gives
none. Each step calls `battle_think_progress` (`src/battle_thinking.cpp`), which redraws only a 21x9
box at the screen's center over the board already in the display buffer, lights one more of its three
dots, and pushes the buffer with `avm_display(false)`. A step costs about 25 thousand cycles (1.6 ms),
under 1.5 percent of the search. The countdown is one byte of the search's frame, and the interval and
step number come from the budget and the positions already counted, so the indicator takes no static
RAM.

Measured on the AVM emulator with `tests/battle_probe.cpp` (`avm time` at each probe mark, the
progress steps drawn), and on the host with `rpg_battle_bench`. "Before" is the 240-position budget.
The opening has the enemy to act before any party move. The in-play position is the first enemy turn
after the archer's opening shot at the brute. The engagement positions put the wolf one run from one
party unit, or from a cell beside both; the sacrifice gives the 1 HP wolf a run beside the party's last
unit, a 1 HP archer. The four-a-side position is fighter, rogue, archer, and spearman against wolf,
brute, archer, and fighter, four columns apart; three a side drops each side's last unit; "in contact"
moves the enemy line two columns closer.

| Position | Before (seconds, nodes) | After (cycles, seconds) | Nodes | Deepest completed | Budget reached | Progress steps | Host µs |
|---|---:|---:|---:|---:|---|---:|---:|
| Initial encounter (opening) | 0.55 s, 149 | 25,642,951 (1.60 s) | 407 | 4 | no | 8 | 13.9 |
| In-play first enemy turn | 0.55 s, 149 | 25,642,918 (1.60 s) | 407 | 4 | no | 8 | 13.1 |
| Mid-battle adjacent combat | 0.75 s, 209 | 12,074,459 (0.75 s) | 209 | 3 | no | 4 | 6.4 |
| Many ranged options (archers and a spearman) | 0.81 s, 219 | 13,137,527 (0.82 s) | 219 | 4 | no | 4 | 7.1 |
| Wolf sprint options (two wolves) | 0.71 s, 199 | 11,396,685 (0.71 s) | 199 | 3 | no | 4 | 6.4 |
| Knockback available (rogue beside the brute) | 0.61 s, 167 | 29,997,341 (1.87 s) | 480 | 3 | yes | 10 | 16.3 |
| One engagement target | 0.62 s, 174 | 30,365,671 (1.90 s) | 480 | 3 | yes | 10 | 16.9 |
| Two engagement targets | 0.48 s, 130 | 26,495,381 (1.66 s) | 408 | 4 | no | 8 | 14.4 |
| 1 HP sacrifice that wins | 0.009 s, 1 | 139,232 (0.009 s) | 1 | 1 | no | 0 | 0.1 |
| Opening after 19 activations | 0.55 s, 149 | 25,642,924 (1.60 s) | 407 | 4 | no | 8 | 14.2 |
| Adjacent combat after 21 activations | 0.75 s, 209 | 12,074,459 (0.75 s) | 209 | 3 | no | 4 | 6.3 |
| One survivor per side | 0.34 s, 119 | 5,420,353 (0.34 s) | 119 | 4 | no | 2 | 3.0 |
| Four a side (budget 240) | 0.49 s, 75 | 26,107,383 (1.63 s) | 240 | 2 | yes | 10 | 13.7 |
| Three a side (budget 320) | 0.86 s, 160 | 22,212,066 (1.39 s) | 260 | 3 | no | 8 | 12.7 |
| Four a side in contact (budget 240) | 0.95 s, 120 | 27,873,675 (1.74 s) | 224 | 3 | no | 9 | 14.7 |

Every measured decision takes under 1.9 s. The opening, in-play, and two-engagement positions now
complete depth four (1.6 to 1.7 s), and the three-a-side and in-contact positions depth three. Where
the doubling rule predicts that depth four would not fit (adjacent combat, the wolf sprint), the
search stops at depth three as before, so those decisions are unchanged. The knockback and one-target
positions start depth four and run out of budget, so they spend 1.9 s and keep depth three's choice.
The activation count does not change the rules, so the late positions search exactly like the early
ones. Over 300 random enemy-to-act positions from Grove play, the bounded search visits 373 positions
on average, completes depth four in 167 and depth three in 133, and reaches the budget in 73. A
complete depth-four search there averages 363 positions (at most 662).

The complete search at each depth on the in-play position: depth one 0.49 M cycles (9 nodes), depth
two 1.83 M (30), depth three 7.51 M (128), depth four 19.34 M (302 nodes, 1.21 s). The deepening run
needs more, 407 positions, because each iteration repeats the shallower ones.

Terrain costs the rules little. `Battlefield` holds a four-byte blocked mask, one byte per row, and a
compile-time 128-byte table of the cell pairs that terrain screens, so a line check adds one
program-memory load and the enumerator starts its occupancy masks from the blocked mask. Both are
derived from the battlefield's tiles at compile time; the tests check them against the tiles'
collision in the world tileset.

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
* Ordering: wins first, then other kills, then the remaining attacks, whether the defeats come from an
  attack or from a move's engagement; then, at nodes whose children are not all leaves, moves that
  engage an enemy, then quiet moves that shrink the mover's attack gap, then other quiet moves. Two
  four-byte row masks per node mark the cells beside an opponent and beside an opponent at 1 HP, so a
  move's engagement is one bit test; without a 1 HP opponent no move can kill, and that pass is skipped.
  At nodes whose children are all leaves the moves after the attacks stay unordered: on the AVM,
  ordering them there cost more than the leaves it saved (the in-play decision was 1.5 percent slower).
  Each ply keeps one killer move, recorded from the child that cut the node off and promoted only among
  the next node's unordered or quiet moves, so an engaging move is never treated as a quiet one. The
  root tries the previous iteration's choice first.
* The search fails hard, so a ply keeps only its alpha and two indices: its beta is its parent's alpha
  negated, which cannot change while the ply is searched. An immediate win ends a node at once, and a
  decided score ends the deepening.
* An iteration starts only when the budget left is at least twice the last completed iteration's nodes.
  Predicting the next iteration from the growth of the last two instead stopped the three-a-side
  position before its doomed third iteration, but it also gave up depth four in over half of the
  random two-on-two positions that complete it, so the doubling rule stays.

### Evaluation

The utility keeps its weights for units, HP, and strikes; the larger HP keeps every static score far
inside int16_t and below the terminal scores (the extremes at the 15 HP limit are tested). It has no
movement term: `battle_apply` already charges the blood price and deals the engagement damage, so the
search sees both, and an extra penalty would count the price twice. Its positional term is the attack
gap: the cells a unit must still travel to line up on a row or column and come within its attack
range, ignoring blockers. A fast mover (wolf, rogue) counts one half-move per cell, others two. Strikes
count each opposing pair on one row or column whose distance is in range with nothing between (the
rules' `battle_line_blocked`). A compile-time table of the distance outside each type's range replaces
the range arithmetic, and every term is added to one sum as it is found.

The greedy fallback scores a win (by attack or engagement, a winning sacrifice included) above
everything and a move that costs the side's last unit without winning below everything; otherwise the
net exchange, 100 per opponent defeated and 10 per HP dealt, less 10 for the blood price and 100 when
the mover falls, then the move's attack gap.

### Costs per operation

Per call on the AVM, copy overhead removed, on the in-play position: evaluation 33.8 thousand cycles
(31.9 thousand on the open field), enumeration 46.4 thousand (43.2), ordering with approach moves 84.0
thousand (90.5; terrain leaves fewer moves to order), and an action's apply 9.4 thousand. Terrain adds
one screening load to each line check and seeds the enumerator's occupancy from the blocked mask. Cells are adjacent when they differ by a row (8) or by
one within a row, a cheaper test than comparing columns and rows. Unlike RAM, a load from a constant
object costs 230 to 320 cycles on the AVM, because every constant object sits in program memory.

### Memory and stack

| Guest | Static RAM | Code | Read-only data | Stack / 256 |
|---|---:|---:|---:|---:|
| rpg | 473 → 501 | 24,393 → 28,945 | 121,449 → 121,869 | 243 → 232 |
| rpg_qa | 477 → 505 | 24,549 → 29,101 | 121,449 → 121,869 | 245 → 234 |

The thinking indicator and the arcing leap add 269 bytes of code (28,676 → 28,945) and no static RAM
or read-only data. The stack bound rises from 228 to 232: the progress countdown and the values that
number a step stay live in deepen's frame (40 → 44 bytes), which lies on the deepest path. Keeping the
countdown, interval, and step in statics instead saved two of those bytes for three bytes of static
RAM. The indicator's own calls run from deepen and stay shallower than the evaluation below it.

Terrain and the arrow animation leave static RAM at 501 bytes. They add 266 bytes of code (28,410 →
28,676) and 394 of read-only data (the four 16x16 arrow sprites with masks, 256 bytes, and the
battlefield's blocked mask and screening table, 132), and the stack bound rises from 219 to 228: the
terrain check makes `battle_line_blocked` a 14-byte out-of-line frame below the evaluation. Walking
the blocked mask cell by cell instead of the screening table cost 229 to 238 bytes depending on
inlining, and inlining `battle_line_blocked` with the table saved one byte (227) for 256 bytes more
code. The blood-price update changed neither static RAM nor the stack
bound; its code was 1,574 bytes, mostly the engagement-aware ordering and greedy choice.
`BattleUnit` packs its cell, HP (at most 15), type, and team into two bytes, so `BattleState` holds eight
slots in 22 bytes, as it held four before. The six `BattleUnitType` records (7 bytes each), the derived
rule copy, the gap table, and the per-type action limits stay in program memory. The static search
workspace is 188 bytes (160 before): four 4-byte plies, four killer bytes, the 80-byte shared action
scratch, and four 22-byte positions for the children of each ply. The work counters are compiled only
into the host and the battle probe (`RPG_BATTLE_STATS`), so the shipping guest carries none. The
enumerator's occupancy masks, the ordering's target cells (eight bytes), and the evaluation's party
mask are frame locals. The static RAM increase is the 28 bytes of workspace.

The deepest shipping path is explore → update_visible_world → run_chunk (the battle screen, 45 bytes)
→ enemy_turn (9) → deepen (44, the search loop inlined) → battle_score (42) → battle_line_blocked
(14). The arrow's flight runs inside the battle screen's resolve and never overlaps the search. The search parameters are arguments, and the
loop has one caller, so it shares its frame with the deepening loop.

### Decisions

The enemy-behavior tests in `tests/test_battle.cpp` check that the search discovers each mechanic from
the rules: a lone wolf closes two cells in one run; the brute's blow knocks a fighter away from a
cornered 3 HP ally and saves it; an enemy beside an archer strikes it instead of stepping into its
line; a wounded wolf leaves a spearman's two-cell reach; an injured enemy leaves an archer's open line;
a healthy enemy beside a hero attacks and takes a kill; the party's rogue leaps over its own fighter and
the enemy brute, engaging the brute, to win in three plies; a 1 HP wolf runs beside the party's last
1 HP archer to win by engagement; and a 1 HP last wolf beside a sturdy archer bites rather than move
into a losing sacrifice. Four-a-side tests cover an eight-unit encounter, the per-type limits, the depth
limits, and the search against the reference.

### Simulations

`tests/battle_simulation.cpp` plays the enemy against five party policies on the Grove battle. Random
plays 64 seeded games; rows are averages per game, pairs are party/enemy, and the wins, losses, and
longest retreat run are totals or maxima. An engaging move is one that damaged an opponent; the price
is the HP movers paid; a sacrifice is a move by a 1 HP unit, and a sacrifice win is one whose engagement
won the battle. Every simulated activation is checked to lower the total HP, and every battle to end
within its starting HP. The "240 budget" rows are the same games under the one-second budget.

| Party policy | Activations | Attacks | Moves | Engaging moves | Engagement damage | Price | Sacrifices | Sacrifice wins | Passes | Damage taken | Enemy retreats | Party wins / losses |
|---|---:|---|---|---|---|---|---|---|---|---|---:|---:|
| Aggressive | 24 | 3 / 2 | 9 / 10 | 2 / 2 | 2 / 2 | 9 / 10 | 0 / 1 | 0 / 0 | 0 / 0 | 16 / 21 | 7 | 1 / 0 |
| Defensive | 30 | 2 / 0 | 13 / 15 | 0 / 5 | 0 / 5 | 13 / 15 | 0 / 2 | 0 / 0 | 0 / 0 | 18 / 21 | 8 | 1 / 0 |
| Greedy | 23 | 3 / 3 | 9 / 8 | 1 / 1 | 1 / 1 | 9 / 8 | 1 / 1 | 0 / 0 | 0 / 0 | 19 / 18 | 5 | 0 / 1 |
| Search | 24 | 4 / 2 | 8 / 10 | 1 / 1 | 1 / 1 | 8 / 10 | 1 / 1 | 0 / 0 | 0 / 0 | 15 / 21 | 3 | 1 / 0 |
| Random (64 games) | 18.1 | 0.5 / 2.8 | 8.7 / 6.1 | 1.5 / 3.0 | 1.5 / 3.1 | 8.7 / 5.8 | 0.5 / 0.1 | 0 / 0 | 0 / 0 | 19.0 / 8.7 | 0.9 | 0 / 64 |
| 240 budget: aggressive | 27 | 3 / 3 | 11 / 10 | 1 / 0 | 1 / 0 | 11 / 10 | 1 / 1 | 0 / 0 | 0 / 0 | 19 / 20 | 7 | 0 / 1 |
| 240 budget: defensive | 34 | 1 / 0 | 16 / 17 | 0 / 3 | 0 / 3 | 16 / 16 | 1 / 1 | 0 / 1 | 0 / 0 | 19 / 19 | 1 | 0 / 1 |
| 240 budget: greedy | 25 | 4 / 3 | 9 / 9 | 1 / 1 | 1 / 1 | 9 / 9 | 1 / 0 | 0 / 0 | 0 / 0 | 19 / 20 | 5 | 0 / 1 |
| 240 budget: search | 24 | 4 / 2 | 8 / 10 | 1 / 1 | 1 / 1 | 8 / 10 | 1 / 1 | 0 / 0 | 0 / 0 | 15 / 21 | 3 | 1 / 0 |
| 240 budget: random | 17.9 | 0.5 / 2.9 | 8.5 / 5.9 | 1.6 / 2.7 | 1.7 / 2.9 | 8.5 / 5.6 | 0.5 / 0.0 | 0 / 0 | 0.1 / 0 | 18.9 / 8.5 | 1.0 | 0 / 64 |

Terrain narrows the field to the lane through rows 1 and 2, so few moves engage and the blood price
makes up much of the damage. Each deterministic policy plays a single game, so one changed enemy
choice can flip its result: under the larger budget the aggressive and defensive parties now win,
while the random party still loses all 64 games, and the enemy's engagement damage against the
defensive party rises from 3 to 5. Device QA wins with an eight-action plan found by breadth-first
search against the real enemy (`GROVE_VICTORY_PLAN` in `tests/device_tests.cpp`); its last turn
leaves the 1 HP brute with moves but no attack, so it must pay its last HP.

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
