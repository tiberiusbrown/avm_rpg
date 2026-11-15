# AVM v1 contract — M0 through M1.5

M1.5 changes only RPG code/assets. AVM machine/compiler/runtime contracts remain
unchanged. Current application resource proofs and emulator timings are in
[m1_5_measurements.md](m1_5_measurements.md); the architecture is in
[chunk_world.md](chunk_world.md). Earlier revision observations below are retained
as historical qualification, not a claim that the remote repositories are unchanged.

Evidence labels: **Verified** means checked against the cited repository source
and/or compiled interfaces; **Measured** means observed in this build/emulator;
**Unverified** means a remaining target/platform qualification. Performance values
and their windows are in [performance.md](performance.md). The supplied design is
preserved in [game_design.md](game_design.md); the assignment's pixel movement,
20 Hz, and spatial turn-based combat requirements override its provisional choices.

## Revisions and workflow

**Verified:** AVM development revision is
`3ceedda679c03c00a24d0353be2bedce0478ed4e`. On 2026-10-14, remote HEAD matched.
The installed compiler reports LLVM/Clang 22.1.8, fork revision
`ca60b2d1d0b48b9554f553e63856f92c3946cef5`, matching the local LLVM submodule.
RPG local base is `601dbeb8b6459ea6ea92c32b47cae5baa80087bf` (includes the supplied
design); remote HEAD was `22b0c3fe12ac3115dab95600669fb2a5ea5ceefc` (initial README).
Implementation changes are prepared without commits or pushes, as instructed by
AVM `.agents/AGENTS.md`. Git metadata required read access outside the restricted
process sandbox; this did not prevent source development.

**Verified:** AVM `.gitmodules` names `projects/avm_rpg` as the implementation
submodule. All changes are inside that submodule. It has a standalone CMake build
using the installed SDK, so AVM/LLVM need no changes or rebuild. From the parent:
`cmake -S projects/avm_rpg -B projects/avm_rpg/build ...`. The parent's existing
projects CMake file does not register RPG; use the standalone command. A clean RPG
checkout works independently of its parent, given the installed prerequisites.

**Verified:** To acquire/build the SDK, initialize AVM's LLVM and optional Ardens
submodules, configure the parent in `RelWithDebInfo`, build `avm_sdk`, and install:

```sh
git submodule update --init --recursive deps/llvm-project deps/Ardens
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DAVM_WITH_ARDENS=ON
cmake --build build --config RelWithDebInfo --target avm_sdk --parallel
cmake --install build --config RelWithDebInfo --component avm-sdk --prefix /path/to/sdk
```

Preserve an existing generator/compiler; initialize its matching MSVC developer
shell for Ninja/MSVC. `AVM_WITH_ARDENS=OFF` permits a compiler-only SDK, omitting
emulator/debugger/profiler support. An installed SDK can be reused; LLVM rebuilding
was unnecessary here. See AVM `runtime/README.md`, root `CMakeLists.txt`, and
`docs/lldb.md`. Python is needed for RPG asset/build/QA scripts; the SDK's native
compiler/image tools do not require it.

## Machine and address spaces

**Verified:** AVM `docs/arch.md` §§9–16 specifies 128×64 one-bit framebuffer,
eight 16-bit registers, 24-bit read-only program space, 16-bit RAM pointers,
16-bit C `int`/`size_t`, and one-byte alignment. Program/function pointers in
memory occupy exactly three bytes; RAM pointers occupy two. Implicit address
space conversions are forbidden. Use fixed-width types and `AVM_PROGMEM`, not
ordinary AVR pointer assumptions.

| Guest RAM | Budget | Meaning |
|---|---:|---|
| `0x100..0x4ff` | 1024 B | `.saved` followed by ordinary `.data`, explicit initializers |
| `0x500..0x8ff` | 1024 B | Separate framebuffer; not part of static storage |
| `0x900..0x9ff` | 256 B | Downward VM stack, initial SP `0xa00` |

**Verified:** No architectural heap or runtime `.bss`; loader initializes all
static bytes. Guest source uses no allocations, exceptions, RTTI, recursion,
virtual dispatch, or automatic content arrays. Native QA tools may allocate on
the host. Framebuffer mapping is `0x500+x+128*(y>>3)`, bit `y&7`.

## Guest services

All following interfaces are **Verified** in `runtime/include/avm.h`, with
wrappers/state in `runtime/lib/avm.c` and native services in `interp/interp.asm`.
M0/M1 execute input, sprite, text, display and persistence services on the actual
interpreter. Audio and entropy are source-verified, unused by this game.

### Input and transitions

`avm_buttons()` returns the current mask: B `0x04`, A `0x08`, DOWN `0x10`,
LEFT `0x20`, RIGHT `0x40`, UP `0x80`. `avm_poll_buttons()` updates previous/current
runtime masks; `avm_just_pressed()` / `avm_just_released()` query transitions.
`avm_pressed`, `avm_any_pressed`, `avm_not_pressed` query the current hardware mask.
Game rules instead compute edge bits from their explicitly supplied held mask.
The adapter polls every accepted frame. This keeps host tests and guest rules
identical. LLDB replay masks use named buttons, with a different internal debug
mask; never pass that internal debug mask to game rules.

### Timing and display

```c
void setup(void) { avm_set_frame_rate(20); }
void loop(void) {
    if (!avm_next_frame()) return;
    avm_poll_buttons();
    /* fixed simulation step and draw */
    avm_display(true);
}
```

`avm_set_frame_rate` computes integer `1000/rate` milliseconds; rates below 4
clamp to 4. Thus 20 is exactly 50 ms. `avm_set_frame_duration` sets milliseconds
directly. `avm_next_frame` compares an 8-bit millisecond clock, idles when early,
and accepts one step when due. It does not perform catch-up simulation. Workloads
under 50 ms produce stable fixed-step motion; overloaded frames slow simulation.
`avm_millis` is 16-bit, `avm_millis32` 32-bit. `avm_idle` is the native idle service.
Blocking saves are safe-point operations rather than continuous per-frame writes.

`avm_display(clear)` sends the complete framebuffer to the OLED and clears it
afterward when true. Drawing happens first. `display_update_func` in the
interpreter owns the SPI transfer; interpreter startup configures the display.

### Sprites, rectangles and fonts

`avm_draw_sprite_overwrite`, `_plus_mask`, `_self_masked`, `_erase` accept signed
pixel x/y, a program-space sprite pointer, and a frame index. `avm_set_sprite`
plus `avm_draw_overwrite` / `avm_draw_plus_mask` removes repeated pointer arguments.
Services clip signed coordinates. Sprite assets begin with width,height;
16×16 frames are 32 page-major column bytes (16 columns for each eight-pixel page). Plus-mask frames interleave each
image byte with its mask. `avm_draw_filled_rect_white/black` accept signed x/y and
8-bit width/height. The current RPG renders effective partial tiles from its 77-byte live
cache (11x7 tiles: the 9x5 view span plus a margin tile on every side), with byte tile origins
and a 16-bit pixel camera snapshot.

`avm_set_text_font(AVM_FONT_BR5D)` selects the preferred variable-width ASCII
font, with five pixels above the baseline, descenders, and seven-pixel line
spacing. BR4, BR5 and BR5N alternatives exist. `avm_set_text_mode` selects overwrite/white
transparent/black transparent. `avm_draw_text_P(x, baseline_y, flash_string)` and
`avm_draw_textf_P` draw program text, including newline advances; returned cursors
contain x/baseline_y. Dialogue reveals flash strings one letter per frame,
measures words with `avm_text_width` before placing them, and automatically
wraps/pages within a fixed overlay. Each visible letter sounds a one-frame
`avm_audio_tone_triangle` blip. Oversized words split at glyph boundaries. No copied glyph
metrics or offline dialogue-fit checks are needed. Source: public header,
`runtime/lib/font_br5d_x5.c`,
interpreter text services.

### Program data and random numbers

**Verified:** `runtime/include/avm/pgmspace.h` defines address-space-one
`AVM_PROGMEM` / `PROGMEM` and `AVM_PSTR` / `PSTR` / `F` flash string literals.
Direct typed reads generate program loads. `memcpy_P`, `strlen_P`, `strcmp_P`
and other `_P` routines are available; C++ overloads use pointer type to select
RAM versus program access. RPG uses the SDK's `PROGMEM` and `F()` macros directly;
native builds define `PROGMEM` as empty and `F(text)` as `(text)`. Maps, descriptors,
sprite atlases and strings are immutable program assets with stable numeric IDs.

`avm_generate_random_seed` supplies platform entropy. `avm_init_random_seed` and
`avm_random` implement a 16-bit xorshift generator in the runtime. RPG uses the
same compact xorshift rule with explicit nonzero state inside `Game`, seeded
`0xbabe` for repeatable QA and serialized for continuation. Ambient state is reconstructed on load even though the saved live object contains the
NPC slots and cache.

### Physical persistence versus application validity

**Verified:** `__attribute__((section(".saved")))` places a global in the
persistent prefix at `0x100`. `avm_save()` writes *all* prefix bytes and returns
void. `avm_load()` replaces that prefix from the newest complete matching record
and returns bool. `avm_save_exists()` checks whether the first physical record's
size equals the image saveSize; it does **not** validate RPG data. Saved bytes
remain ordinary static RAM and are initialized by the loader before an explicit
load. Source: architecture static layout/service contract and
`interp/interp.asm` `sys_save_impl`, `sys_load_impl`, `sys_save_exists_impl`.

Physical records are a little-endian size word followed by saveSize bytes in a
selected 4096-byte FX-flash sector. The append log leaves the last two bytes
erased. SAVE appends while possible, erases when full or the next header is not
erased, and blocks on flash page programming. LOAD scans matching-size records;
SAVE_EXISTS only checks the first size word. Changing `.saved` length changes physical
compatibility. Equal-length new versions still require application validation.

RPG save version 8 stores the complete 268-byte `Game` object directly in `.saved`.
It contains no pointers and uses the AVM byte-aligned struct layout; changing that layout
requires a version bump. There is no separate static save buffer, encoder, decoder or
checksum. The version byte is the first payload byte. The native host's `sizeof(Game)`
is not the guest format; device QA reads the packed AVM layout.

Startup calls `avm_load()` and validates the loaded version, area, pixel bounds, facing,
stats, defined flag bits, NPC residences and the three audio slider levels. The result initializes the ordinary global
`save_exists` flag before new-game defaults are prepared for the title screen. Starting a
new game preserves that flag and the current audio options. Save calls `avm_save()` and sets the flag to true, without
readback or validation; the UI reports `Saved`. The title and pause menu use the cached
flag without loading or checking storage again. Continue and Load explicitly load and
validate the object, reset NPC casts and animation ticks, refresh the area cache and repair
blocked player positions. Audio options are initialized and applied at startup (native
music/SFX gains 128, displayed as 100%; filter off unless a valid save supplies other values). Later loads retain
the current options instead of replacing them with the record's values. A rejected load clears the flag and prepares new-game defaults.
The absence of a checksum means corruption that passes these field checks is not detected.

Version-1–6 records are incompatible with the new saved size and layout. Full-device QA
checks that the physical payload equals the live `Game`, that a fresh restart restores
progress, and that an invalid version is rejected during startup.

### Audio

**Verified:** `runtime/include/avm/audio.h`, `docs/audio.md`, and
`projects/audio_demo/main.c` expose autonomous three-tonal-plus-noise audio.
`avm_audio_play` accepts terminating/looping music; `avm_audio_sfx` accepts
compact one-shot tonal/noise effects on voices 1/3. Assets store raw uint16 phase
increments, and pitch changes preserve oscillator phase;
pause/resume/volume/stop/direct-tone services exist. Native foreground updates
and Timer1/Timer4 audio operation require no game update call. RPG starts
Glade and Hearth after New Game or a successful Continue, switches to Steel at
Dawn for battles, and returns to Glade and Hearth at the result. `music_play` leaves the current
song alone or queues a different song with `avm_audio_play_fade`. The interpreter
lowers music attenuation by one every 4 ms and restores the request's captured
gain before starting the queued song (1.02 seconds at gain 255). Input, display
and gameplay continue during the fade. Shared master gain stays 255 and
SFX is unaffected. Runtime getters read the existing music start pointer and
gain bytes directly from RAM; the RPG stores no duplicate song selection.
The title leaves audio disabled; dialogue and menus let playback continue. The Audio
panel adjusts music and SFX attenuation independently in 31 positions, from 0–200%,
leaving shared gain unchanged. Defaults are native 128 (100%); the maximum is native 255
(200%). Stored level 15 remains 100%, preserving previous saved slider percentages. Its low-pass strength slider maps bypass and progressively smaller coefficients
(128 down to 1), using the native tonal filter, which includes tonal SFX and bypasses noise.
Settings live in the saved `Game`, are loaded once at startup and are changed only in Audio
thereafter; saving writes them alongside progress.
Full-device integration checks song switching, PWM output, and exploration with music enabled.
Listening on physical speakers remains **Unverified**.

## Compilation, resource reporting, debug and replay

**Verified and Measured:** official driver commands are:

```sh
avm-clang -O2 -g -std=c++20 -flto=full -c source.cpp -o source.o
avm-clang -O2 -g -flto=full --avm-startup=crt0_sketch.o source.o \
  -Wl,--avm-print-stack-usage,--avm-print-stack-gaps -o game.elf
# Driver also emits game.bin (development layout).
avm-image game.elf -o game.arduboy
avm-readobj --sections --symbols game.elf
avm-objdump -d game.elf
```

The driver finds its relocatable sysroot, adds CRT/libavm/builtins and section
GC, and builds the image. The sketch CRT calls `setup` once and `loop` forever.
`.arduboy` contains interpreter HEX, fxdata, metadata and an erased fxsave when
saveSize is nonzero. The development `.bin` is for the AVM-aware debug workflow;
use the package in a normal Ardens/FX device workflow. `tools/link_guest.py`
records linker output and independently reads ELF section sizes. It fails the
build on static overflow, stack overflow or an incomplete stack bound.

**Verified:** AVM `docs/stack-analysis.md` explains finalized frames, outgoing
arguments, three-byte return records, tail transfers, indirect/recursive gaps
and CRT dispatch. `complete bound: no` is a lower bound, never sufficient proof.
SYS has no guest return record; native AVR hardware-stack use is excluded from
the guest 256-byte bound. Current M0 and M1 link with zero gaps and complete bounds.

**Verified and Measured:** installed `avm-lldb` packages the ELF with the matching
interpreter and executes it in Ardens. `avm button set`, `avm run-for`, `avm replay`
and `avm display save` are supported. Replay version 1 contains ordered
`{"cycle":...,"pressed":["RIGHT"]}` events relative to load. Explicitly set
released buttons at entry: the SDK adapter's initial pins otherwise looked held
in this environment. Capture logical pixels before `avm_display(true)` clears
them. Raw controller captures ignore OLED remap and appear mirrored. The current
SDK adapter's `visible` buffer remained stale black with unfiltered display;
full-device QA calls Ardens `advance(0)` to refresh that buffer and confirms
visible output without modifying AVM/Ardens. This distinction is tested, not
treated as a game rendering failure. See `tests/device_tests.cpp`.

**Verified:** debugger ABI uses three-byte DWARF program addresses and tagged
host handles: RAM is `0x01000000 + raw_address`; explicit untyped program reads
use `0x02000000 + program_address`. `expr` can inspect state but cannot call guest
functions. See AVM `docs/debugger_abi.md` and `docs/lldb.md`.

**Verified and Measured:** `avm-prof record game.elf --warmup 1s --for 8s
--replay tests/replays/exploration.json --native -o run.avmp` collects elapsed
emulated AVR cycles, not native guest instruction estimates or host wall time.
`avm-prof report run.avmp --html report.html` resolves source/inlined costs.
Recorded windows include interpreter, display and interrupt behavior. Device QA
per-frame markers, recorded only by the `rpg_qa` guest, exclude pacing wait and include
simulation, render and display.
At 16 MHz the 20 FPS budget is 800000 cycles. Results and qualification limits
are documented separately; physical hardware and non-Windows hosts remain
**Unverified**.
