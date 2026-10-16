# AVM v1 contract — M0

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
8×8 frames are eight page-major column bytes. Plus-mask frames interleave each
image byte with its mask. `avm_draw_filled_rect_white/black` accept signed x/y and
8-bit width/height. RPG renders partial tiles directly; no RAM tile cache exists.

`avm_set_text_font(AVM_FONT_BR5D)` selects the preferred variable-width ASCII
font, with five pixels above the baseline, descenders, and seven-pixel line
spacing. BR4, BR5 and BR5N alternatives exist. `avm_set_text_mode` selects overwrite/white
transparent/black transparent. `avm_draw_text_P(x, baseline_y, flash_string)` and
`avm_draw_textf_P` draw program text, including newline advances; returned cursors
contain x/baseline_y. Dialogue streams flash strings through a five-byte chunk
buffer, measures them with `avm_text_width`, and automatically wraps/pages within
a fixed overlay. Oversized words split at glyph boundaries. No copied glyph
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
`0xbabe` for repeatable QA and serialized for continuation. Ambient state is
reconstructed, rather than persisting all NPC positions.

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

**Verified and Measured:** RPG stores a 24-byte, explicit little-endian format:

| Offset | Bytes | Value |
|---:|---:|---|
| 0 | 2 | `RP` magic |
| 2 | 1 | version 1 |
| 3 | 1 | record length 24 |
| 4 | 1 | stable area ID (1 outdoor, 2 interior) |
| 5 | 1 | facing 0..3 |
| 6,8 | 2 each | player x,y in pixels |
| 10,11,12 | 1 each | HP, max HP, level |
| 13 | 1 | reserved zero |
| 14,16,18 | 2 each | gold, flags, nonzero RNG |
| 20 | 2 | reserved zero |
| 22 | 2 | CRC-16/CCITT-FALSE, bytes 0..21 |

Load validates magic, version, length, CRC, fields, terrain, bounds and the
stationary NPC's collision box before mutating live state. No pointers, padding,
UI state or NPC scratch are serialized. Ambient spawns relocate within authored
bounds if a loaded player occupies them. Save feedback requires physical load
readback with the expected checksum, not merely SAVE_EXISTS. CRC is corruption
detection, not authentication. Invalid saves leave the title's Continue disabled.
A fresh-device restart with the actual exported sector was exercised; see QA logs.

### Audio and future use

**Verified:** `runtime/include/avm/audio.h`, `docs/audio.md`, and
`projects/audio_demo/main.c` expose autonomous three-tonal-plus-noise audio.
`avm_audio_play` / `avm_audio_sfx` accept program pointers and byte sizes;
pause/resume/volume/stop/direct-tone services exist. Native foreground updates
and Timer3/Timer4 audio operation require no game update call. RPG intentionally
leaves audio disabled; added audio IRQ and FX-read cost must be reprofiled in M3.
Audible behavior on this RPG and physical speakers is **Unverified**.

## Compilation, resource reporting, debug and replay

**Verified and Measured:** official driver commands are:

```sh
avm-clang -O2 -g -std=c++17 -flto=full -c source.cpp -o source.o
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
Recorded windows include interpreter, display and interrupt behavior. Full-device
per-frame markers exclude pacing wait and include simulation, render and display.
At 16 MHz the 20 FPS budget is 800000 cycles. Results and qualification limits
are documented separately; physical hardware and non-Windows hosts remain
**Unverified**.
