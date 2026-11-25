# Performance evidence

## Millcross quest integration

The RelWithDebInfo shipping guest uses 580 bytes of static RAM, including
261 saved bytes. Full LTO verifies a complete 251-byte stack bound, 24-bit
dispatch pointers, and no stored map positions.

| Device workload | Worst exploration cycles | Budget |
| --- | ---: | ---: |
| Full replay | 317,332 | 320,000 |
| Sound and mechanism replay | 316,621 | 320,000 |
| Millcross quest replay | 312,152 | 320,000 |

Normal and FXC produced the same peaks. A cached chunk can contain an NPC site
outside the tile window. The site now checks its tile against the cache before
evaluating the policy or requesting admission. This removes redundant admission
work while preserving the offscreen guard and actor-pool priority.

Reproduce with the `rpg_integration_full` CMake target. The target retains
logs, timing measurements, and captures in `build/verification-*`.


## Exploration regression diagnosis and correction (2026-11-18)

The starting RPG checkout is `6d57b70` (the previous NPC fixes, committed by the
repository owner before this follow-up). Archived pre-fix packages correspond to
`824452b`. This work leaves changes uncommitted. Both archives identify the same
AVM Clang 24 compiler build, `4b9f44402c8c3bcc40b921827e03e196b213c690`, with
`-O2 -flto=full`, RelWithDebInfo and the existing generator. The installed compiler
binary, interpreter firmware, Ardens library, guest ELF/package hashes and checkout
revisions are recorded in workspace `build/npc-regression/provenance.json`.
The source checkout's LLVM revision is not substituted for the installed compiler's
actual build identifier.

The device configuration is unchanged: 16 MHz AVR, deterministic initial state,
Cathy3K USB handoff, observed SPI writes, and normal or FXC/PORTE2 firmware. The
320,000-cycle exploration limit and the replay inputs remain unchanged.

### Demonstrated causes

The old global maximum was frame **90**, 323,373 cycles. The post-fix maximum was
frame **180**, 357,776 cycles; these are different workloads. Frame 0 was 307,312
cycles after the previous fix, with its three actors already cast during entry.
Frame 74 had the same three actor identities, positions and states in both archives.
Its 2,173-cycle regression was not an additional sprite or activation: source/PC
attribution shows the populated NPC chunk plus admission calls increasing from
4,869 to 8,218 attributed cycles. Previously active identities were rejected in the
specialized site; after centralization they made three generic admission calls.
There was still only one eligibility check per identity. Other code-generation and
interrupt-alignment differences partially offset that call-path cost.

Frame 180 moves the test player from (432,416) to (448,432). Camera changes from
(376,392) to (392,408), and cache origin from (22,23) to (23,24). Silas is at
(368,372), fully offscreen with uncached feet, while his home (384,384) has cached
feet and is safely offscreen. Both archives retire him. Only the corrected guard
permits immediate readmission at home; the newly initialized wanderer then steps to
(384,385). The old executable ends with two actors, the corrected executable with
three. That extra actor is **not drawn**: its sprite is vertically outside the view.
Actors/collision increases by **40,639 cycles** (88,129 to 128,768), whereas rendering
increases by only **343** (168,492 to 168,835). At equivalent frame 180, total cost
increases from 316,821 to 357,776. This establishes a lifecycle cost, rather than
attributing the 34,403-cycle change between different global maxima to rendering.

### Measured changes

1. Ordinary standing/wandering actors retain their slot and movement state while
   their same-area logical home's footprint is cached. Uncached feet freeze through
   the existing update path; once actor and home both leave the cache, safe retirement
   frees the slot. Policy changes are processed first. Targets/routes remain pinned;
   `NPC_TRAVEL` still completes and emits its event even with a cached destination.
   This removes frame 180's retirement/readmission rather than postponing activation.
2. Compile-time per-stage row spans bound dense chunk-table traversal to the first
   and last potentially occupied columns in each row. Empty entries outside those
   spans cannot run a handler. Cached-window intersection, ordering, dense 24-bit
   function tables and declarative policies remain intact. Native tests exercise the
   same bounds; their instrumentation still reports the requested cache window.
3. `terrain_clear()` selects the area's collision-atlas base once per footprint,
   retaining `cached_tile()` and its row-major short circuit for up to four cells.
   This removes three repeated area switches/address-base calculations in the
   four-cell case. Native collision overrides retain their original path.
4. `npc_offer_candidate()` owns an inline area/party/active-identity front door;
   `npc_detail::offer_candidate()` owns the outlined definition/cache/visibility/
   collision/priority work. Constant site IDs permit cheap rejection without a generic
   admission call for active residents. `NpcSite::run()` still checks only policy and
   offers the site. Eligibility is not duplicated; post-hook revalidation is unchanged.

All timings in this experiment table use ordinary device stepping:

| Experiment | Frame 74 | Main scenario maximum | Serial | Code | Read-only | Static RAM | Complete stack |
|---|---:|---:|---:|---:|---:|---:|---:|
| Starting NPC fixes | 322,592 | 357,776 | 180 | 57,471 | 329,581 | 668 | 231 |
| Row spans only | 311,027 | 343,961 | 180 | 57,735 | 330,063 | 668 | 233 |
| Lifecycle only | 323,037 | 325,727 | 90 | 57,561 | 329,581 | 668 | 231 |
| Lifecycle + row spans | 311,442 | 317,829 | 892 | 57,825 | 330,063 | 668 | 233 |
| Plus collision-atlas hoisting | 310,822 | 318,582 | 892 | 57,703 | 330,063 | 668 | 233 |
| Plus inline admission front door | 307,388 | 316,673 | 892 | 58,225 | 330,063 | 668 | 233 |

The full integration target exposed an additional **sounds** replay failure not
contained in the main scenario: frame 226, 322,476 cycles. It has no lifecycle churn,
but performs player overlap validation and NPC collision. Atlas hoisting lowers this
to 320,525; the inline admission front door lowers the sounds maximum to **316,022**.
Atlas hoisting is retained for the worse sounds workload despite its intermediate
main-scenario maximum increasing by 753 cycles. Both final workloads improve; an
improved frame 74 alone was never used as the acceptance criterion.

The final main maximum is **316,673 cycles at serial 892**, in both normal and FXC
modes: **3,327 cycles** below the unchanged limit. The sounds maximum is **316,022**.
Saved Game remains 309 bytes. Relative to the starting fixes, code grows 754 bytes,
read-only data grows 482 bytes (row metadata), static RAM is unchanged at 668 bytes,
and complete stack grows two bytes to **233/256**. QA uses 675 static bytes and the
same complete stack bound. No heap, identity-wide production arrays, dirty-chunk
state, actor/save-layout change or new eviction policy is introduced. The ROM tradeoff
is deliberate: predictable frame cost takes priority over code size.

### Final frame contexts and attribution

The ordinary marker breakdowns are directly comparable with the previous timings:

| Frame | Total | Render/display | Reload | Chunks | Actors/collision | Other |
|---|---:|---:|---:|---:|---:|---:|
| Prior fixes, 74 | 322,592 | 164,850 | 12,192 | 44,362 | 92,734 | 8,454 |
| Final, 74 | 307,388 | 164,495 | 12,254 | 37,773 | 84,200 | 8,666 |
| Prior fixes, 180 | 357,776 | 168,835 | 12,205 | 39,461 | 128,768 | 8,507 |
| Final, 180 | 287,803 | 167,690 | 12,262 | 28,007 | 71,593 | 8,251 |
| Final maximum, 892 | 316,673 | 176,221 | 12,724 | 37,841 | 82,714 | 7,173 |

Frame 74 retains Iona (496,416), Rowan (464,432), and Silas (383,372). Final frame 180
retains Silas at (368,372), without activation/retirement; his state is preserved.
Frame 892 is at player (496,400), camera (440,376), cache (26,22), with Iona traveling
at (580,442) and Rowan standing at (464,432). Neither that frame nor frame 74 has
lifecycle churn. All three frames visit four indexed sites. Frames 74 and 180 have
zero admission slow-path entries. Frame 892 has one, for uncached Silas, rejected
before terrain/collision work. Existing residents are rejected by the inline front door. Counter metadata in each profiling report identifies
whether `candidate_offers` counts the former generic function or the new slow path.

`tools/profile_frames.py` records every exploration frame's serial, elapsed cycles,
area/player/camera/cache, actor identities/positions/state, activation/retirement counts,
site checks and execution-region costs. Guest-PC histograms retain frame 74, frame 180,
all new maxima and frames over 310,000 cycles. DWARF inline ranges distinguish gameplay,
tiles, dialogue, NPC dispatch, overlap repair, lifecycle, simulation, movement/collision,
interaction, terrain/actor/overlay drawing and display. Categories are **exclusive**;
shared collision helpers have separate rows, and interrupt time is charged to the
interrupted guest instruction. Function size is not used as a proxy for hotness.

Instruction attribution explicitly restores Ardens `no_merged` after display captures
reset it. This changes peripheral/marker sampling slightly: final instruction profiles
peak at **316,678** at frame 892, versus **316,673** with ordinary stepping. Earlier
experiments differed by up to 1,526 cycles at that frame. `--frame-only` observes ordinary
stepping without PC attribution; `--no-trace` and `--strict` run ordinary tests. Final
budget claims use ordinary stepping and unchanged strict assertions, not diagnostic
figures. Both normal/FXC instruction profiles reconcile every frame's region and PC
cycles to its total.

At the final instruction-profile maximum, the largest exclusive regions are terrain
rendering 70,189, overlays 61,359, display submission 23,548, gameplay dispatch 16,222,
NPC dispatch 18,746, NPC simulation 13,548, NPC movement/collision 13,943 and shared
terrain collision 11,463 cycles. Actor drawing is 10,560. The remaining cost is now
predominantly rendering, not repeated NPC activation. Full region/function/line tables
are retained in the profiling summaries rather than inferred from the old source profile.

Native verification passes all 22 tests, including the 64-identity/128-site fixture,
all-direction fractional-camera approaches and the 16,120-frame continuity run.
New regression cases check retained identity/slot/timer/detour state and no activation
hook, eventual unloading, cached-destination travel events, and a saturated eight-slot
pool retaining ordinary residents while a higher-priority candidate waits.

Raw evidence, immutable experiment packages and final disassemblies are retained under
workspace `build/npc-regression/`. Final ordinary context is in `optimized-ordinary`,
instruction/source profiles in `fast-normal` and `fast-fxc`, and the sounds diagnosis
in `sounds-baseline` and `fast-sounds-profile`. The complete strict integration log is
`optimized-integration.log`; the final package inputs are `fast-admission-artifacts`.
Both playable packages are refreshed. The complete `rpg_integration_full` target
passes, including normal/FXC main and sounds strict replays, guest geometry/lookup
checks and fresh AVM source/native profiles. Existing 24-bit code pointers and complete
LTO stack/resource checks pass. The final formatting rebuild has byte-identical
allocated code/data sections to the measured packages; only embedded debug/source
metadata differs (`format-equivalence.json`).

Reproduce a diagnostic run from the workspace root (use a fresh output directory):

```sh
python3 projects/avm_rpg/tools/profile_frames.py \
  --artifacts projects/avm_rpg/build/artifacts --build projects/avm_rpg/build \
  --sdk build/avm-sdk-install --dwarfdump build/llvm-build/bin/llvm-dwarfdump \
  --output build/frame-profile --mode normal
```

Use `--mode fxc` for FXC, `--scenario sounds` for the separate sounds replay,
`--frame-only` for ordinary marker/context tracing, or `--strict` for unchanged timing
checks without tracing. Diagnostic timing is disabled only in the explicit functional
modes. No production instructions are added for profiling.

## NPC admission continuity and validation (2026-11-18)

Comparable local RelWithDebInfo builds use the existing `-O2 -flto=full` AVM settings.
Shipping resources, before and after this change:

| Resource | Before | After | Difference |
|---|---:|---:|---:|
| Saved live Game | 309 | 309 | 0 |
| Ordinary static RAM | 351 | 359 | +8 |
| Total static RAM / 1024 | 660 | 668 | +8 |
| Code | 58,104 | 57,471 | -633 |
| Read-only data | 329,581 | 329,581 | 0 |
| Complete stack bound / 256 | 231 | 231 | 0 |

The eight additional bytes cache candidate priorities. There is no heap allocation,
identity-sized production array or save-layout change. QA uses 675 static bytes with
the same complete 231-byte stack bound. Both playable packages were regenerated.

Optimized disassembly is retained in the workspace's `build/npc-before.asm` and
`build/npc-after.asm`. Site functions inline into chunk dispatch: the populated
`StaticChunk<0u, 183u, (HandlerStage)3>::run()` shrinks from 235 instructions to 27.
It contains policy flag checks and candidate calls, with no party or active-slot scans.
Admission performs one active-identity scan and one actor-collision scan for a clear
candidate. Sorting uses cached priorities, and duplicate/full-buffer rejection precedes
terrain work. `cast_npc` inlines into `npc_stage_end`; the first cast bypasses terrain
and actor revalidation, while later casts revalidate after hooks (including policy and
identity eligibility). Relative to the old site/admission/cast pipeline, active-slot
scan count does not increase. A full pool rejects casts before terrain revalidation.
Visibility uses signed map-coordinate differences and comparisons without divisions
or general geometry calls. The bounded 64-identity/128-site fixture also exercises hooks
that invalidate terrain, player, quest, party and actor-occupancy eligibility.

The complete native suite passes all 22 tests, including the scaling fixture and
16,120-frame continuity run. Device functional replays use the existing `functional`
mode (timing enforcement disabled); strict replays retain the 320,000-cycle limit.
Normal and FXC packages produce identical measurements in these replays:

| Workload | Before average / worst | After average / worst |
|---|---:|---:|
| Idle Grove | 280,130 / 299,155 | 282,368 / 301,340 |
| Pixel scroll | 289,783 / 297,962 | 290,925 / 298,916 |
| Dense collision | 294,223 / 300,566 | 295,741 / 302,378 |
| Cave music | 200,631 / 215,045 | 200,800 / 217,598 |
| Complete scenario: worst exploration | 323,373 | 357,776 |

Both functional scenarios pass, but the worst exploration cost **increases
by 34,403 cycles**. Strict replay still fails first at serial 74: **322,592 cycles**
(render 164,850; reload 12,192; chunks 44,362; actors/collision 92,734), versus the
baseline's 320,419 (render 165,708; reload 11,985; chunks 44,823; actors/collision
89,419). This change does **not** establish frame-budget compliance or an end-to-end
speedup. Corrected admission/retirement also changes which actors participate in the
replay; the existing counters combine NPC discovery with other chunk work, and NPC
movement with player collision. They cannot attribute the remaining cost to these
functions alone. No unrelated movement/render optimization was attempted.

Raw before/after disassemblies, resources and device logs are retained under the
workspace's `build/npc-*` paths; final replay inputs are copied to
`build/npc-final-artifacts`, with logs/captures in `build/npc-final-device`.

## Policy-derived NPC dispatch (2026-11-17)

RelWithDebInfo, full LTO, current AVM SDK built from this checkout. Shipping resources:

| Resource | Before | After |
|---|---:|---:|
| Saved live Game | 289 | 309 |
| Ordinary static RAM | 318 | 351 |
| Total static RAM / 1024 | 607 | 660 |
| Code | 55,373 | 58,104 |
| Read-only data | 311,848 | 329,581 |
| Complete stack bound / 256 | 233 | 231 |

RAM growth is fixed: 20 bytes in the reusable actor pool and 33 bytes of candidate scratch.
It does not depend on the identity count. The QA guest uses 667 static bytes and has the
same complete 231-byte stack bound. Optimized-IR checks verify immutable program-memory
chunk/resolver tables, 24-bit code pointers, one saved Game and no stored map-position objects.
The SDK stack analysis now resolves nonwrapping whole-element loads from immutable pointer
arrays, including packed arrays produced by constant splitting. The RPG raises analysis-only
target/iteration limits to 256/2048; it retains the 256-byte runtime stack limit.

Native tests include 64 identities with 128 policy-derived sites, reversed chunk traversal,
priority/ID selection and an eight-actor limit. Separate AVM compilation verifies render,
dialogue and existing portrait assets with 65 as the identity limit. Portrait art is unchanged.

Device-emulator functional verification passes in normal and FXC modes with timing enforcement
explicitly disabled for that diagnostic run. Observed QA workloads in either mode:

| Workload | Average cycles | Worst sampled cycles |
|---|---:|---:|
| Idle Grove | 280,130 | 299,155 |
| Pixel scroll | 289,783 | 297,962 |
| Dense collision | 294,223 | 300,566 |
| Cave music | 200,631 | 215,045 |

The complete functional scenario observed **323,373 exploration cycles**, exceeding the
320,000-cycle target. The ordinary strict integration target still fails its unchanged timing
assertion (first observed miss: 320,419 cycles). Performance optimization is explicitly deferred;
these results do not establish compliance with the 50 Hz worst-case target. No global settled
shortcut or camera/dirty-chunk skip is used for NPC discovery. The NPC stage runs each frame;
work depends on cached chunk sites and the fixed active pool.

The 22 native/asset tests, handler fixture, AVM builds, 184 AVM backend tests and 39 AVM
linker tests pass separately from this outstanding timing failure. The device QA executable's
`functional` mode records `within_frame_budget` while continuing behavior checks; its normal
and FXC modes retain strict timing assertions. Instruction, C and C++ benchmarks retain their generated
disassemblies and results in the normal benchmark reference location. These are emulator and
link-time measurements, not physical-device measurements.

## Historical M0/M1 evidence

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

The version-6 save refactor places the live `Game` directly in `.saved`, eliminating the
29-byte serialization buffer. A layout-version byte and the ordinary `save_exists` flag
left total static RAM 27 bytes lower than the pause-menu build. The version-7 Audio
menu adds three saved slider levels. Voluntary Pass adds two bytes to the shared search
action stack, leaving total shipping static RAM 22 bytes below the pause-menu build. Current RelWithDebInfo
links retain full LTO and report complete stack bounds, with the sole saved object verified
in optimized IR:

| Guest | .saved | ordinary .data | total static / 1024 | code | read-only data | stack / 256 |
|---|---:|---:|---:|---:|---:|---:|
| rpg | 268 | 211 | 479 | 44441 | 253075 | 242 |
| rpg_qa | 268 | 217 | 485 | 44582 | 253075 | 244 |

The cave and the dungeon add two bytes of static RAM: a second byte of world flags for their
ten mechanism flags, and the `cache_edited` byte, which the linker kept out while no handler
edited tiles. Their tilesets, phase sheets and maps are read-only data, and their handlers and
ambient effects are code.

The earlier M1 baseline below predates the current world, battle and save implementations:

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

### Combat class measurements

RelWithDebInfo with full LTO. Both builds use the same 82-byte action scratch and
226-byte battle search workspace. The shipping guest has a complete stack bound.

| Shipping guest resource | Before | After |
|---|---:|---:|
| Saved bytes | 261 | 261 |
| Static RAM, including saved data | 580 | 580 |
| Maximum provable stack bytes | 251 | 251 |
| Code bytes | 94,688 | 96,844 |
| Flash bytes (`.text` + `.rodata` + `.flash`) | 460,568 | 464,594 |

The AVM battle probe measured 15 existing search fixtures. Their decision costs
rose by 0.8–4.5%. The slowest increased from 35.22 to 36.72 million cycles
(2.20 to 2.30 seconds at 16 MHz). Two additional probe fixtures took 0.60
seconds for mixed four-on-four combat and 0.78 seconds for two-on-two combat
with Drain and Recoil. The search continues to use a deterministic node budget.

The standard and FX-C device tests exercised all seven classes through the normal
battle input loop. Each action averaged 319,032–320,000 cycles between display
presentations. The longest observed interval was 355,404 cycles (22.2 ms) at the
transition from Recoil to the enemy turn.

The second-rank perk update added 926 flash bytes. The shipping guest uses 580
bytes of static RAM and has a complete 255-byte stack bound. The slowest result
in the same AVM search probe rose from 2.30 to 2.32 seconds.

Completing rank 2 across all tracks added 1,602 flash bytes. The shipping guest
uses 580 bytes of static RAM and has a complete 256-byte stack bound. The longest
decision in the same probe took 2.35 seconds.

The rank-3 perks and Reach's Short Grip added 4,479 flash bytes. The shipping
guest uses 580 bytes of static RAM and has a complete 250-byte stack bound.
The longest of the same 17 timing-probe decisions took 2.52 seconds.

### Earlier battle measurements

The search and simulation figures below predate Rend's victim-dependent damage (2, or 4 below half HP),
which changes the wolf's attacks. Re-measure them with the probes and `tests/battle_simulation.cpp` before
comparing with them.

The original benchmark used three movement classes and five attack classes. The fixed
`BATTLE_TEST_GROVE` fixture fields a rogue and an archer against a wolf and a brute. Each activation
in that fixture reduces total living HP, which caps the run at 40 activations. The
[class catalog](game_logic.md#52-battle-movement-classes) describes the current rules.

The enemy chooses with a non-recursive negamax search with alpha-beta pruning (`src/battle_ai.cpp`),
deepened one ply at a time under a deterministic node budget: 480 positions with up to four living
units, scaled by four over the living units beyond that (320 with six, 240 with eight). Only a completed
iteration's choice is used; with none, the greedy choice is. Every guest link reports a complete stack
bound. In the earlier measurements, the budget allowed up to two seconds per enemy move: a position costs about 3.7 ms with four
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

The utility keeps its weights for units, HP and strikes. Full-byte HP is accumulated as a signed
health difference before multiplying by the common HP weight. Material and final static scores
saturate at +/-19,999, below terminal scores, without overflowing int16_t; tests cover 255 HP. It has no
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

The damage indicators add 149 bytes of code (31,527 → 31,676) and 578 of read-only data (nine 16x16
burst sprites with masks) to the build they were measured against, and no static RAM (500). The stack
bound rises from 240 to 244: the screen keeps each unit's lost HP in four bits, four bytes of
`BattleScreen`, which is live on the deepest path.

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
`BattleUnit` uses four bytes for board cell/team metadata, HP, source ID and remaining enemy AP.
An enemy's ID indexes the six-byte `EnemyInfo` ROM record with maximum HP, class IDs, sprite row,
packed ranks and starting AP. A source ID with its high bit set references the saved party member
for classes, maximum HP and AP. `BattleState` holds eight slots in 39 bytes, including a uint16
activation counter.
The separate movement/attack tables, derived rule combinations, gap table and action limits stay
in program memory. Search state uses 258 bytes for four four-byte plies, four killers, the 82-byte
shared action scratch and four 39-byte child positions. The ordering masks add 12 bytes to the
shared workspace. The blocking battle screen reuses a fixed
static workspace to keep the complete guest stack bound below 256 bytes. The shipping build measured at that point used 585 bytes of static
RAM (289 saved, 296 ordinary) with a 220-byte stack bound. Battle results
write only ally HP back to the saved party. Work counters remain limited to host/probe builds.

The deepest shipping path is explore → update_visible_world → run_chunk (14 bytes)
→ resolve (30) → draw_board (45) → battle_enumerate (40). The arrow's flight never overlaps the
search. Search positions and the single blocking battle screen are static workspaces; ordinary
frame-local input, animation and evaluation temporaries remain on the stack.

**Runtime workspace (2026-11-19).** `src/runtime_workspace.h` overlays exploration and battle
in one 226-byte union, and only one of them is active at a time. Exploration's share is the tile
window (79 bytes), camera (4), NPC admission list (41) and foot collision cache (7): 131 bytes.
Battle's share is the search positions (124), ply records (16), killer moves (4) and the shared
action stack (82): 226 bytes. A battle fades the world out, takes the block, and on exit fades to
black, reloads the camera and tile window through the ordinary reload and tile pass, invalidates
the cached foot and NPC collision answers, and fades back in. The window and camera leave the saved
`Game`, which shrinks by 83 bytes (SAVE_VERSION 13). Shipping static RAM falls from 669 to 538 bytes
(saved 310 to 227, ordinary data 359 to 311), and the QA guest from 676 to 545. The complete stack
bound rises from 239 to 243 bytes, below the 256-byte limit: the `update_visible_world` frame grows
from 38 to 42 bytes; the dialogue path through `rpg_render_world` is unchanged at 8 bytes.
Per-frame work is unchanged within measurement noise. Against clean HEAD on the full device
suite, the idle grove average goes from 275,942 to 275,805 cycles, pixel scrolling from 275,946
to 275,921, and the cave music average from 183,682 to 184,108. Battle entry and exit keep their
worst frames near 321.5 thousand cycles, as before. Dialogue draws the battlefield through its own
background, so exploration's renderer carries no battle check.

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
python tests/guest_integration.py --sdk /path/to/sdk --build build --full
python tools/collect_evidence.py --build build
```

The collector writes `evidence.json`, `revisions.json`, and `performance.md` under
the ignored build directory. It collects revision metadata from local checkouts;
the reports are not inputs to the build or tests. This document retains the
summary of the measurement above.

The integration script creates a fresh directory, launches actual ELF and package
guests, runs frame-based input scenarios, and records avm-prof traces/HTML reports.
The default `rpg_integration` target runs representative guest geometry, all-tile
lookup validation, normal/FXC device smoke scenarios and 100 ms profiles. Full
performance evidence requires `rpg_integration_full` (or the script's `--full` flag),
which retains exhaustive guest geometry, all device scenarios and 8-second profiles.
The collector reads `latest-full-verification.txt`; `latest-verification.txt` points
to the most recent run of either suite. Device battle scenarios wait for instrumented
input/result readiness with bounded deadlines rather than spending a fixed 120 million
cycles after every action. Both suites report stage and total wall times.
Pause opening, closing, and loading checks verify gameplay behavior and display timing,
without comparing framebuffer contents or prescribing animation steps. Menu display intervals
must stay below 336,000 cycles (the 50 Hz period plus interrupt jitter), and each observed
interaction must finish within one second. Resumed exploration uses its separate CPU budget.
Display captures remain available for visual review.

Raw evidence for this run is `verification-ior6tcas` under build/;
`latest-full-verification.txt` points to the latest exhaustive run. SDK-only users
can execute the LLDB/replay checks, but full-device
per-frame/restart checks are explicitly skipped without the optional harness.

Limitations: Windows/MSVC and emulated device verified; physical hardware, other
host platforms and audio-enabled workloads remain unverified. The installed SDK's unfiltered visible buffer can be
stale, so full-device QA refreshes it using Ardens advance(0). No VM/toolchain
source changes were made to work around that capture issue.

## Village shop and victory rewards (2026-11-20)

The shipping AVM guest uses RelWithDebInfo and full LTO.

| Resource | Before | After | Change |
|---|---:|---:|---:|
| Saved Game | 256 B | 256 B | 0 B |
| Static RAM, including saved Game | 572 B | 573 B | +1 B |
| Code (`.text`) | 80,451 B | 82,741 B | +2,290 B |
| Read-only data (`.rodata`) | 337,302 B | 337,465 B | +163 B |
| Complete stack bound | 251 B | 252 B | +1 B |

The transient counter request accounts for the RAM byte. The save format remains
version 18. The shop view lives on the stack, and the linker reports a complete
bound within the 256-byte limit.

The existing AVM battle probe recorded identical elapsed cycles between all 33
sampled phase marks before and after the change. The opening enemy decision took
33,938,584 cycles in both builds. Gold is calculated on battle finalization,
outside enemy decisions and ordinary battle frames.

The shop device replay's 16-frame battle exit peaked at 320,669 cycles on both
standard and FXC packages, within the existing 336,000-cycle transition check.

## Overworld encounters

RelWithDebInfo builds measured the following shipping-guest resources. Flash is the
sum of the program sections `.flash`, `.text` and `.rodata`.

| Resource | Before | With encounters |
| --- | ---: | ---: |
| Saved RAM | 256 B | 258 B |
| Other static RAM | 317 B | 319 B |
| Total static RAM | 573 B | 577 B |
| Program flash | 420,529 B | 424,648 B |
| Complete stack bound | 252 B | 251 B |

The three defeat flags fit in the existing flag bytes. `Npc` is 22 bytes and
`NpcPool` is 180 bytes. The normal and FX-C soundscape replays peaked at 308,179
cycles per exploration frame, compared with 309,450 before the change. Over 550 idle
frames at the starting spot, the average fell from 280,210 to 277,658 cycles and the
peak changed from 317,400 to 317,439. A 20-frame Hollow Mine replay with the scout
pursuing the player averaged 239,507 cycles and peaked at 244,854. The exploration
budget is 320,000 cycles at 50 FPS.

The full FX and FX-C gameplay replays peaked at 313,383 exploration cycles.
The separate audio timing replay stopped on a 321,620-cycle exploration frame.

## Exploration optimization round (2026-11-24)

Starting point: RPG HEAD `35fd00d` with the installed AVM compiler rebuilt from the workspace
sources (`-O2 -flto=full`, RelWithDebInfo). Goal: cut exploration frame cost without changing
behavior or static RAM. The baseline was already at the budget: the strict main replay peaked at
319,995 of 320,000 cycles, 126 of 1,419 frames were above 300,000, and the audio timing replay
stopped on an over-budget frame (320,345 at serial 388).

### Method

`tools/profile_frames.py` replays the device scenarios with per-instruction attribution. Two
opt-in environment switches were added to `tests/device_tests.cpp`: `RPG_FRAME_ALL=1` keeps the PC
histogram of every exploration frame (otherwise only the worst frames, serial 74 and 180), which
allows an average per-function and per-line profile over a whole replay, and
`RPG_NO_TIMING_CHECK=1` lets a manual measurement run past an over-budget frame.
Interrupt time lands on whichever guest instruction it interrupts, so a single instruction can
show a spike; averages over all frames are the reliable view. Baseline cost by region, averaged over
1,126 frames of the main scenario (250,639 cycles per frame):

| Region | Cycles | Share |
| --- | ---: | ---: |
| Terrain drawing (45 `draw_overwrite` calls, about 1,100 cycles each) | 67,716 | 27% |
| Overlay passes (gusts, birds, ambient creatures) | 27,839 | 11% |
| Display submission | 23,215 | 9% |
| Chunk stage traversal and handlers (gameplay, tiles, dialogue, NPC dispatch) | 48,545 | 19% |
| Tile window reload | 12,961 | 5% |
| NPC begin, update, movement, interaction | 29,497 | 12% |
| Actor drawing | 11,601 | 5% |

The chunk traversal was the largest guest-controlled cost. Every frame, each of the four stages
recomputed the window's chunk bounds, read two row-span bytes per row from program memory, and read
a 24-bit function pointer for every chunk in the span, empty or not. A program-memory read costs
about 300 cycles and an ordinary instruction about 40.

### Changes

1. **Chunk plan** (`src/chunk_dispatch.cpp`, `src/runtime_workspace.h`). The runtime workspace now
   holds a `ChunkPlan`: per stage, the window's chunks that have a handler, as byte distances from
   the window's first chunk. It is keyed by area and cache origin and rebuilt only when that key
   changes (and the chunks differ). The rebuild reads one 8-byte record of four 9-bit occupancy
   masks per block (compile time, per chunk) and one 7-byte ordered list per occupied stage.
   Handlers are still read from the dense ROM function tables by index, in the same chunk order,
   with the same teleport short-circuiting.
2. **Tile window reload skip** (`src/world.cpp`). `reload_tile_cache` keeps the window when no
   `set_tile` edit has been flagged since the last reload, the area is unchanged, and the origin
   is where the reload would put it. A one-byte `tile_area` in the workspace records the window's
   area. Every window change goes through `set_tile`, so the skipped reload would have produced
   identical bytes.
3. **`game_new` out of line** (`src/game.cpp`). The plan code changed the inlined frame of
   `update_visible_world` by two bytes, which pushed the battle probe guest `m17_battle_probe`
   (whose `setup` inlined `game_new`) from 254 to 258 bytes of stack. Outlining `game_new` brings
   that probe to 240.

Both caches are exploration-lifetime state in the workspace. A battle overwrites the workspace,
so `world_reclaim_workspace` and `world_place` clear `chunk_plan.area` and `tile_area` before any
traversal or reload (a lost battle returns to the title and re-enters through `world_place`,
not through reclaim).

### Results

Static RAM is unchanged: the exploration share of the 270-byte workspace grows from 131 to 166
bytes (104 bytes remain free for exploration-only state) and the union stays the battle search's
size.

| Resource (shipping guest) | Before | After | Change |
| --- | ---: | ---: | ---: |
| Static RAM (saved 265 + ordinary 456) | 721 B | 721 B | 0 |
| `.text` | 128,893 B | 129,531 B | +638 B |
| `.rodata` | 384,463 B | 414,387 B | +29,924 B |
| Complete stack bound | 254 B | 254 B (complete) | 0 |
| `m17_battle_probe` stack bound | 254 B | 240 B | -14 B |

The `.rodata` growth is the compile-time chunk tables: 8 bytes of occupancy per chunk
(2,033 chunks, 16.3 KB) and one 512-entry list table per area (3.6 KB each, 14.3 KB).
`rpg.arduboy` grows from 274,494 to 280,787 bytes.

Exploration frames, main device replay, ordinary stepping (cycles; budget 320,000):

| Metric | Before | After |
| --- | ---: | ---: |
| Mean | 258,394 | 223,147 |
| Median | 274,316 | 236,656 |
| 95th percentile | 307,602 | 268,427 |
| 99th percentile | 312,460 | 284,873 |
| Maximum | 319,995 | 307,658 |
| Frames above 300,000 | 126 | 2 |
| Frames above 310,000 | 35 | 0 |

FXC stepping gives the same distribution (mean 223,133, maximum 307,658). Over 1,418 frames with
identical area, player, camera and actors the mean saving is 35,337 cycles per frame, and the
mean render cost differs by 247 cycles, so the saving is not from different content.
Strict (asserting) replays: main 319,995 to 307,658 for both normal and FXC, sounds replay
317,498 to 298,945. The audio timing replay no longer stops on an over-budget frame during its
40-second idle phase (before: 320,345 at serial 388). It still fails afterwards with "Orrin did not
open his topics", which also happens on the baseline when the budget check is disabled.

Device workload rows (average / worst cycles, normal; FXC is within a few cycles):

| Workload | Before | After |
| --- | ---: | ---: |
| idle_grove | 288,710 / 303,353 | 249,152 / 263,511 |
| pixel_scroll | 273,909 / 281,975 | 239,599 / 246,701 |
| dense_collision | 278,504 / 290,740 | 235,883 / 259,297 |
| millcross_well | 253,782 / 266,638 | 223,167 / 245,964 |
| millcross_orchard | 235,786 / 249,343 | 217,214 / 235,539 |
| millcross_smithy | 235,067 / 240,885 | 196,785 / 201,124 |
| hollow_mine_mouth | 220,892 / 226,335 | 180,576 / 183,684 |
| hollow_mine_camp | 209,169 / 227,246 | 171,299 / 176,376 |
| cave_music | 200,379 / 205,322 | 164,446 / 171,273 |

The orchard row also shows 10,000 more render cycles than before: the workload's frames land on a
different animation phase, which the matched-frame comparison above separates out.
Average region costs: tile reload 12,961 to 4,231, gameplay stage 15,497 to 7,757, tile stage
8,643 to 2,887, dialogue stage 11,366 to 4,839, NPC dispatch 13,039 to 6,440.
Fade, battle-transition and pause intervals (320,500 to 329,000 cycles) did not change: they are
50 Hz presentation intervals with jitter, checked against 336,000, and `pause_open` (328,968) is
the one that overruns the period.

The remaining worst frames (307,658 at serial 540, 306,032 at 1,037) are the harness's own
jumps: the frame that moves the player, so the chunk plan rebuilds (about 9,000 cycles), the window
reloads (about 13,500), the player is ejected from terrain and an NPC retires and is cast in the
same update (the NPC work alone is about 85,000). Steady frames now peak near 287,000.

### Equivalence evidence

* All 27 native tests pass, including the chunk-window trace test and the 16,120-frame
  NPC continuity run.
* `rpg_integration_full` passes (normal and FXC strict replays, sounds, battle, shop, persistence,
  geometry), run twice on the changed code.
* A host golden-trace probe (kept in the session scratchpad, not in the tree) drives
  `game_update` with seeded random walks, A presses, area jumps to map edges and tile edits, and
  hashes all simulation state (`Game`, camera, tile window, admission list, foot cache) after every
  frame. Six configurations (up to 600,000 frames, 3,039 distinct windows, 4.3% of frames with
  edited tiles, and a variant that overwrites the whole workspace with junk and calls
  `world_reclaim_workspace` as a battle exit would) give identical hash streams on the baseline
  tree and the changed tree.
* The guest's allocated sections were compared byte for byte before and after the final formatting
  pass, and the stack bound is complete (0 analysis gaps) in every guest link. The final build removes
one change that measured neutral (a `memmove` in `retire_slot`: the slot scan, not the copy,
dominates a retirement); the full device, native and golden checks were repeated without it.

### Experiments that failed

* Caching the 24-bit handler pointers in the workspace made the link-time stack bound
  incomplete ("unresolved indirect call target", 15 gaps): the analysis proves indirect calls only
  through immutable tables. Byte-offset reads from the table fail the same way. The plan therefore
  stores chunk distances and keeps reading the tables.
* The first plan rebuilds cost far more than the traversal they replaced: a four-stage append loop
  took about 35,000 cycles, a per-row mask walk about 33,000, per-stage pointer passes about
  50,000. The generated code spends about 40 cycles on each of the hundreds of instructions that
  nested bounded loops compile to. Precomputing the per-block masks and ordered lists at compile
  time makes the rebuild about 9,000 cycles.

### Opportunities not taken

Needing RAM in the exploration share of the workspace (104 bytes free, no static RAM growth):

* A per-slot copy of the NPC definition fields that run every frame (sprite row, period, pause,
  walk, leash, talkable; about 64 bytes, keyed by identity). The definitions live in program
  memory, so each lookup is a pointer read plus a field read, about 650 cycles. Drawing, updating
  and `leashed` perform them every frame for every NPC; estimated saving 4,000 to 6,000 cycles in
  busy windows. It needs the slot index in `leashed`/`step` and invalidation when slots compact.
* A per-window flag that the visible tiles contain no `TILE_OUTSIDE` (1 byte), invalidated by
  `set_tile`, so `draw_tiles` can skip the sentinel compare and branch for 45 tiles: about 3,000
  cycles. Maps are not guaranteed to be fully painted (the generator fills unpainted cells with
  255), so the flag has to be computed, not assumed.
* The ambient creature pick per visible chunk (about 4 bytes each): about 2,000 cycles.
* The tileset id of the current area (1 byte), which saves the area compare chain at about ten
  call sites: about 1,400 cycles, but it must follow every write of `game.area`, including the
  device harness's.

Needing new static RAM in the saved `Game`: the definition cache above as fields of `Npc` would be
eight bytes per slot (64 bytes), which only matters if the 104 workspace bytes are needed
elsewhere.

Needing no RAM:

* Use the workspace camera in `protected_view` instead of calling `game_camera()` for every
  enemy and every offer (about 1,500 to 2,000 cycles each). It requires proving the camera is
  current at every call, and several native tests set the player without a reload.
* Link-time support for proving a RAM function-pointer array (a callee-set annotation) would let
  the plan hold handler pointers: about 5,000 cycles per frame in flash reads and index arithmetic.
* Shrink the new tables: sharing one list table across areas with row and column kept in one byte
  saves 10.8 KB of flash for about 1,300 cycles per frame; packing the four masks into 5 bytes per
  chunk saves 6 KB.
* NPC lifecycle events (see below). `plan_yield` and `player_can_detour` never ran in any
  replay, so their cost is not measured.

### NPC lifecycle frames

Measured on the replays (per-instruction traces of every lifecycle frame) and a host census of
1.8 million random-walk frames:

* Retiring one NPC adds about 7,300 to 9,500 cycles to the NPC begin region: the out-of-cache
  decision path (cache, protected-view and policy checks) plus `retire_slot` itself, about 2,200
  with its occupied-prefix scan (about 1,100) and slot compaction.
* Admitting one NPC adds about 17,000 to 20,000: `offer_candidate` 7,500 to 10,800 (a
  `game_camera()` call, the definition reads, two collision checks and the sorted insert) plus
  `cast_npc` 9,300 to 9,700, plus the new actor's first update.
* The worst replay frames (serials 540, 493, 253, 708, 911) retire two or three NPCs at once
  because the harness moves the player more than 100 tiles inside one area without `npc_reset`.
  With the plan rebuild (about 9,000), window reload (about 13,000) and overlap repair (about
  15,000) they reach 280,000 to 309,000. A real teleport clears the pool with `npc_reset` (no
  compaction) behind a fade, and walking moves the window at most one tile per frame.
* Walking frames: 0.42% have a lifecycle event, 68 of 1.8 million (0.004%) have two or more, and
  none has four. The busiest steady window peaks near 287,000, so one admission leaves it near
  307,000, an admission with a retirement near 316,000, and two admissions would exceed 320,000.
  Scene entry admits every candidate in one update on purpose, behind a black screen.

Options: capping casts at one per frame outside scene entry (and, if wanted, retirements)
bounds the multi-event frames at the price of a one-frame delay in appearance or removal and
changed tick-level timing; it leaves the single-event cost alone.

#### Behavior-neutral savings (measured)

Three candidate savings were implemented and measured on the all-frame instruction traces
(RPG_FRAME_ALL, 1,125 frames of the main scenario):

| Change | Effect | Kept |
| --- | --- | --- |
| `cast_npc` reads the `NpcDefinition` once (`memcpy_P` of 11 bytes) instead of one program-memory read per field | `npc_end` per admission 7,241 to 6,778 cycles (-463); jump frames -442; `.text` -70 B | yes |
| `retire_slot` with a pointer scan, one `memmove` and a `memset` | single-retire frames +1,072 cycles in the NPC begin region (measured together with the camera key below, which can only lower that region) | no |
| `protected_view` uses the workspace camera when a key (player position and map size) shows it is current | `cache_reload` +791 per frame to store the key; `npc_stage_begin` unchanged (`protected_view` ran in only about 7% of frames); mean +1,288 per frame with the retire change | no |

The workspace camera alone is not exact: `repair_player_overlap` ejects the player after the
reload without refreshing it, and native tests move the player directly, so a plain "camera is
current" flag failed `npc_index`; the key version is exact but costs more than it saves. The
retire rewrite is slower because the SYS block moves and the record-sized pointer steps cost
more than the field copies they replace for the one- or two-slot shifts that occur.

With the cast change alone the main replay's mean is 223,147 to 223,032, its worst frame
307,658 to 306,628 (both within the noise from interrupt alignment), strict sounds 298,945
unchanged, static RAM 721 B unchanged, stack bound 254 B. An admission's NPC cost is still about
17,000 to 19,000 cycles: the remaining time is spread over many small lines (the free-slot scan,
the offer's collision checks, the sorted insert) with no further single saving above 500 cycles.

### Tile window scrolling: doesn't work (2026-11-24)

Result: **doesn't work**, reverted. The idea was to keep six of the window's seven rows when the
origin moves one tile up or down (one `memmove` of 66 bytes and one 11-byte row read) instead of
reloading all seven rows. It was exact: a native test compared the window with a full reload after
every pixel of walks in all eight directions along every map edge (1.2 million checks, and three
different broken versions of the scroll each failed it), the host golden traces were identical,
and all 27 native tests, the strict replays and the stack bound were unchanged.

It did not pay for itself:

* Only vertical steps can scroll. A sideways step needs a program-memory read per new tile, seven
  of them, which costs as much as rereading the rows, so it reloads in full.
* A vertical crossing frame's reload fell from about 14,200 to 10,850 cycles, not the 8,000
  hoped for: the camera, the area lookup and the setup code stay.
* The extra code in `reload_tile_cache` raised the cost of every unchanged frame by 160 to 700
  cycles depending on how it was written. The version that avoided that moved the loading out of
  line; that also made the unchanged frame cheaper than before (4,177 to 3,877), but it added about
  900 to 1,500 cycles to each sideways crossing frame, because the call and a second check are
  paid on top of the same reload.

Final measurements (device walks of 96 frames, 6 tile crossings; cycles, before / after):

| Workload | Average | Median | Worst |
| --- | ---: | ---: | ---: |
| walk_north | 228,629 / 228,119 | 226,263 / 225,601 | 292,899 / 289,564 |
| walk_south | 235,566 / 235,040 | 230,795 / 230,471 | 272,078 / 271,596 |
| walk_east | 262,391 / 262,178 | 263,929 / 263,337 | 292,911 / 294,454 |
| walk_west | 240,318 / 240,059 | 238,756 / 238,505 | 281,345 / 282,293 |
| pixel_scroll | 239,599 / 239,043 | 239,655 / 239,344 | 246,701 / 246,205 |
| Whole main replay (1,804 frames) | 227,012 / 226,472 | 237,606 / 237,032 | 306,628 / 307,322 |

Strict replays: main 307,322 (was 306,628), sounds 300,066 (was 298,945). The average and
median gains (0.1 to 0.25%) come mostly from moving the loading code out of the hot function, not
from scrolling, and the whole-replay worst frame and the horizontal walks' worst frames got
slightly worse. The change also added 294 bytes of `.text`. The walking workloads and a median column
for the workload summary were added to the device harness for this measurement and were reverted
with the rest; they are worth re-adding if scrolling is revisited.

### Reproduce

```sh
cmake --build projects/avm_rpg/build --parallel
python3 projects/avm_rpg/tools/profile_frames.py --artifacts projects/avm_rpg/build/artifacts \
  --build projects/avm_rpg/build --sdk build/avm-sdk-install \
  --dwarfdump build/llvm-build/bin/llvm-dwarfdump --output <fresh dir> --mode normal --frame-only
RPG_FRAME_ALL=1 python3 projects/avm_rpg/tools/profile_frames.py ... --mode normal   # all-frame profile
```

