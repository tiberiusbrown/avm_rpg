// Full-device integration, without changes to AVM or Ardens. Host allocations
// below belong to the QA tool; the guest remains fixed-capacity C++20.
#include "absim.hpp"
#include "avm_spi_observer.hpp"

#pragma pack(push, 1)
#include "rpg.h"
#include "map_positions.h"
#include "world_assets.h"
#include "tile_ids.h"
#pragma pack(pop)
// BattleUnit uses byte-sized bit-field allocation units; packing it would change those offsets.
#include "battle.h"
#include "music_catalog.h"

#include <filesystem>
#include <array>
#include <fstream>
#include <iostream>
#include <numeric>
#include <stdexcept>

namespace fs = std::filesystem;

static_assert(sizeof(Game) == 106 + WorldFlags::BYTE_COUNT + sizeof(NpcPool) + sizeof(AudioOptions),
              "update QA reader if the guest state layout changes");

static void require(bool ok, char const* message) {
    if(!ok)
        throw std::runtime_error(message);
}

struct Cost {
    uint64_t total = 0, render = 0, begin = 0, end = 0;
    uint64_t reload = 0, chunks = 0, collision = 0;
};

struct Device {
    std::unique_ptr<absim::arduboy_t> ab = std::make_unique<absim::arduboy_t>();
    unsigned game_addr, marker_addr, serial_addr, dialogue_addr, phase_addr = 0;
    unsigned battle_pointer_addr = 0;
    uint8_t previous_phase = 0;
    uint64_t phase_begin = 0;
    Cost stages;
    uint8_t buttons = 0, previous_marker = 0;
    bool started = false;
    bool proof = false;
    uint8_t frame_area = 0;
    uint64_t begin = 0, render_begin = 0;
    Cost last;
    std::vector<Cost> frames;
    std::vector<Cost> transitions;
    void (*cycle_observer)(void*, bool) = nullptr;
    void* observer_context = nullptr;

    Device(fs::path const& package, unsigned g, unsigned m, unsigned s, unsigned q, bool p = false)
        : game_addr(g), marker_addr(m), serial_addr(s), dialogue_addr(q), proof(p) {
        if(package.stem().extension() == ".fxc") {
            ab->program_state.cfg.fxport_reg = absim::reg::addr::PORTE;
            ab->program_state.cfg.fxport_mask = absim::reg::bit::PORTE::PORTE2;
        }

        std::ifstream f(package, std::ios::binary);
        auto error = ab->load_file(package.string().c_str(), f);
        if(!error.empty())
            throw std::runtime_error(error);

        ab->core_state.cpu.no_merged = true;
        ab->core_state.cpu.autobreaks.reset();
        avm_test::observe_spi_writes(ab->core_state.cpu);
        bootloader_usb_handoff();
    }

    void bootloader_usb_handoff() {
        // Cathy3K detaches USB before jumping to the sketch, but leaves its
        // USB interrupt enables set. A pending suspend is valid at handoff.
        auto& cpu = ab->core_state.cpu;
        cpu.st(absim::reg::addr::USBCON,
               absim::reg::bit::USBCON::USBE | absim::reg::bit::USBCON::OTGPADE);
        cpu.st(absim::reg::addr::UDCON, absim::reg::bit::UDCON::DETACH);
        cpu.st(absim::reg::addr::UDIEN,
               absim::reg::bit::UDIEN::EORSTE | absim::reg::bit::UDIEN::SUSPE);
        cpu.data[absim::reg::addr::UDINT] = absim::reg::bit::UDINT::SUSPI;
    }

    Game state() const {
        Game g;
        memcpy(&g, &ab->core_state.cpu.data[game_addr], sizeof(g));
        return g;
    }

    void set_state(Game const& g) {
        memcpy(&ab->core_state.cpu.data[game_addr], &g, sizeof(g));
    }

    unsigned battle_address() const {
        require(battle_pointer_addr, "missing battle QA pointer address");
        auto const& ram = ab->core_state.cpu.data;
        return ram[battle_pointer_addr] | (unsigned)ram[battle_pointer_addr + 1] << 8;
    }

    void set_battle(BattleState const& state) {
        unsigned addr = battle_address();
        require(addr != 0, "no active battle");
        memcpy(&ab->core_state.cpu.data[addr], &state, sizeof(state));
    }

    BattleState battle_state() const {
        unsigned addr = battle_address();
        require(addr != 0, "no active battle");
        BattleState state;
        memcpy(&state, &ab->core_state.cpu.data[addr], sizeof(state));
        return state;
    }

    uint8_t cached_tile(uint8_t x, uint8_t y) const {
        Game g = state();
        int dx = int(x) - g.cache.origin_tx, dy = int(y) - g.cache.origin_ty;
        require(dx >= 0 && dx < CACHE_WIDTH && dy >= 0 && dy < CACHE_HEIGHT,
                "QA tile is outside the live cache");
        return g.cache.tiles[dy * CACHE_WIDTH + dx];
    }

    unsigned serial() const {
        auto const& d = ab->core_state.cpu.data;
        return d[serial_addr] | unsigned(d[serial_addr + 1]) << 8;
    }

    void pins() {
        auto& d = ab->core_state.cpu.data;
        d[absim::reg::addr::PINF] = uint8_t(0xf0 & ~buttons);
        d[absim::reg::addr::PINE] = (buttons & BTN_A) ? 0 : 0x40;
        d[absim::reg::addr::PINB] = (buttons & BTN_B) ? 0 : 0x10;
    }

    void step() {
        if(cycle_observer)
            cycle_observer(observer_context, false);
        pins();
        ab->cycle();
        ab->core_state.cpu.update_all();
        if(cycle_observer)
            cycle_observer(observer_context, true);

        auto const& cpu = ab->core_state.cpu;
        require(!cpu.should_autobreak(), "native interpreter fault");

        uint8_t marker = cpu.data[marker_addr];
        if(marker != previous_marker) {
            if(marker == 1) {
                begin = cpu.cycle_count;
                frame_area = proof ? 0 : state().area;
                stages = {};
                previous_phase = 0;
                phase_begin = begin;
                if(proof || (state().area == AREA_OUTDOOR && state().hp == 20))
                    started = true;
            }
            if(marker == 2)
                render_begin = cpu.cycle_count;
            if(marker == 0 && previous_marker == 3 && begin) {
                last = {cpu.cycle_count - begin, cpu.cycle_count - render_begin, begin,
                        cpu.cycle_count};
                last.reload = stages.reload;
                last.chunks = stages.chunks;
                last.collision = stages.collision;
                // Cross-area updates now include a blocking fade. FadeTrace checks each of its
                // display frames; ordinary exploration still has the single-frame CPU budget.
                require((!proof && state().area != frame_area) || last.total <= 320000,
                        "exploration frame exceeded the 320000-cycle budget");
                frames.push_back(last);
            }
            previous_marker = marker;
        }
        if(phase_addr && marker) {
            uint8_t phase = cpu.data[phase_addr];
            if(phase != previous_phase) {
                uint64_t elapsed = cpu.cycle_count - phase_begin;
                if(previous_phase == 4)
                    stages.reload += elapsed;
                if(previous_phase == 5)
                    stages.chunks += elapsed;
                if(previous_phase == 6)
                    stages.collision += elapsed;
                previous_phase = phase;
                phase_begin = cpu.cycle_count;
            }
        }
    }

    DIALOGUE_PROMPT prompt() const {
        return proof ? DIALOGUE_CLOSED : (DIALOGUE_PROMPT)ab->core_state.cpu.data[dialogue_addr];
    }

    void advance(uint8_t held, uint64_t cycles = 800000) {
        buttons = held;
        uint64_t end = ab->core_state.cpu.cycle_count + cycles;
        while(ab->core_state.cpu.cycle_count < end)
            step();
    }

    bool music_enabled() const {
        return ab->core_state.cpu.data[absim::reg::addr::TCCR4B] != 0;
    }

    void verify_music() {
        require(music_enabled(), "music sample timer is disabled");
        uint8_t low = 255, high = 0;
        buttons = 0;
        // Cover more than a full beat, including gaps between percussion hits.
        uint64_t end = ab->core_state.cpu.cycle_count + 10000000;
        while(ab->core_state.cpu.cycle_count < end) {
            step();
            auto sample = ab->core_state.cpu.data[absim::reg::addr::OCR4A];
            low = std::min(low, sample);
            high = std::max(high, sample);
        }
        require(low < 128 && high > 128, "music produced no audible PWM waveform");
    }

    void frame(uint8_t held = 0) {
        if(prompt() != DIALOGUE_CLOSED) {
            advance(held);
            return;
        }
        buttons = held;
        uint64_t deadline = ab->core_state.cpu.cycle_count + 32000000;
        while(!started) {
            step();
            require(ab->core_state.cpu.cycle_count < deadline, "boot timeout");
        }

        unsigned old = serial();
        uint8_t previous_area = proof ? 0 : state().area;
        do {
            step();
            require(ab->core_state.cpu.cycle_count < deadline, "frame timeout");
            if(prompt() != DIALOGUE_CLOSED)
                return;
        } while(serial() == old);
        if(!proof && previous_area != state().area)
            transitions.push_back(last);
    }

    void hold(uint8_t held, unsigned n) {
        while(n--)
            frame(held);
        frame();
    }

    void tap(uint8_t held) {
        advance(0);
        advance(held);
        advance(0, 1600000);
    }

    // Lets letters finish typing until the overlay reaches a prompt.
    void wait_for_typing() {
        uint64_t deadline = ab->core_state.cpu.cycle_count + 200000000;
        while(prompt() == DIALOGUE_TYPING) {
            advance(0, 533334);
            require(ab->core_state.cpu.cycle_count < deadline, "dialogue typing timeout");
        }
        require(prompt() != DIALOGUE_CLOSED, "dialogue closed while typing");
    }

    // Waits for typing, then half a second so the advance guard has passed.
    void wait_for_prompt() {
        wait_for_typing();
        advance(0, 8000000);
    }

    void capture(fs::path const& path) {
        // Let the OLED scan out a newly displayed modal page while simulation is paused.
        if(prompt() != DIALOGUE_CLOSED)
            advance(0, 1600000);

        // cycle() advances the OLED; advance(0) refreshes its unfiltered visible array.
        ab->advance(0);
        auto const& pixels = ab->peripherals.display.filtered_pixels;
        if(!std::any_of(pixels.begin(), pixels.end(), [](auto p) { return p != 0; })) {
            auto const& disp = ab->peripherals.display;
            std::cerr << "display on=" << disp.display_on << " pump=" << disp.enable_charge_pump
                      << " filter=" << disp.enable_filter << " row=" << unsigned(disp.row)
                      << " clock=" << disp.ps_per_clk
                      << " cycles=" << ab->core_state.cpu.cycle_count
                      << " avr_pc=" << ab->core_state.cpu.pc * 2
                      << " UDIEN=" << unsigned(ab->core_state.cpu.data[absim::reg::addr::UDIEN])
                      << " UDINT=" << unsigned(ab->core_state.cpu.data[absim::reg::addr::UDINT])
                      << " PORTB=" << unsigned(ab->core_state.cpu.data[absim::reg::addr::PORTB])
                      << " DDRB=" << unsigned(ab->core_state.cpu.data[absim::reg::addr::DDRB])
                      << " SPCR=" << unsigned(ab->core_state.cpu.data[absim::reg::addr::SPCR])
                      << " SPSR=" << unsigned(ab->core_state.cpu.data[absim::reg::addr::SPSR])
                      << " PLLCSR=" << unsigned(ab->core_state.cpu.data[absim::reg::addr::PLLCSR])
                      << " ram_nonzero="
                      << std::count_if(disp.ram.begin(), disp.ram.end(),
                                       [](auto p) { return p != 0; })
                      << '\n';
            throw std::runtime_error("blank visible display");
        }

        std::ofstream out(path, std::ios::binary);
        out << "P5\n128 64\n255\n";
        out.write(reinterpret_cast<char const*>(pixels.data()), pixels.size());
    }

    std::vector<uint8_t> sector() const {
        absim::w25q128_t::fx_data_save_layout_t layout{};
        require(absim::w25q128_t::make_data_save_layout(ab->program_state.fxdata.size(),
                                                        ab->program_state.fxsave.size(), layout),
                "FX layout");

        std::vector<uint8_t> bytes(4096);
        for(unsigned i = 0; i < 4096; ++i)
            bytes[i] = ab->peripherals.fx.read_byte(layout.save_offset + i);
        return bytes;
    }

    void import_sector(std::vector<uint8_t> const& bytes) {
        ab->program_state.fxsave = bytes;
        ab->reload_fx();
        ab->reset();
        ab->core_state.cpu.no_merged = true;
        ab->core_state.cpu.autobreaks.reset();
        avm_test::observe_spi_writes(ab->core_state.cpu);
        bootloader_usb_handoff();
    }
};

static void verify_actor_layers(Device& d, fs::path const& out) {
    Game original = d.state();
    Game fixture = original;
    // The fixture stands half a tile left of the spawn and one tile below it.
    uint16_t const spawn_x = positions::PlayerStart.x * TILE,
                   spawn_y = positions::PlayerStart.y * TILE;
    fixture.player = {(uint16_t)(spawn_x - 8), (uint16_t)(spawn_y + 8), SOUTH, 0};
    fixture.npcs = {};
    // Deliberately reverse the NPC slots relative to depth, with the player between.
    fixture.npcs.actors[0].id = NPC_VILLAGER;
    fixture.npcs.actors[0].actor = {(uint16_t)(spawn_x + 4), (uint16_t)(spawn_y + 12), SOUTH, 0};
    fixture.npcs.actors[1].id = NPC_WANDERER;
    fixture.npcs.actors[1].actor = {spawn_x, spawn_y, SOUTH, 0};
    d.set_state(fixture);
    d.frame();
    d.frame();
    Game rendered = d.state();
    require(rendered.player.y == spawn_y + 8 && rendered.npcs.actors[0].actor.y == spawn_y + 12 &&
                rendered.npcs.actors[1].actor.y == spawn_y,
            "depth fixture unexpectedly moved");

    std::array<uint8_t, 1024> expected{}, covered{}, old_order{};
    auto compose = [&](std::array<uint8_t, 1024>& pixels, Actor const& actor, uint8_t row) {
        uint16_t frame = (uint16_t)row * 12 + SOUTH * 3;
        uint8_t const* bytes = ACTOR_SPRITES + 2 + frame * 64;
        int left = int(actor.x) - rendered.camera.x, top = int(actor.y) - rendered.camera.y;
        for(int y = 0; y < 16; ++y) {
            for(int x = 0; x < 16; ++x) {
                int sx = left + x, sy = top + y;
                if(sx < 0 || sx >= 128 || sy < 0 || sy >= 64)
                    continue;
                unsigned source = ((y / 8) * 16 + x) * 2;
                if(!(bytes[source + 1] & (1u << (y % 8))))
                    continue;
                unsigned dest = (sy / 8) * 128 + sx;
                uint8_t bit = (uint8_t)(1u << (sy % 8));
                covered[dest] |= bit;
                if(bytes[source] & (1u << (y % 8)))
                    pixels[dest] |= bit;
                else
                    pixels[dest] &= (uint8_t)~bit;
            }
        }
    };
    compose(expected, rendered.npcs.actors[1].actor, 2);
    compose(expected, rendered.player, 0);
    compose(expected, rendered.npcs.actors[0].actor, 1);
    compose(old_order, rendered.npcs.actors[0].actor, 1);
    compose(old_order, rendered.npcs.actors[1].actor, 2);
    compose(old_order, rendered.player, 0);
    require(expected != old_order, "depth fixture does not distinguish the previous draw order");
    d.capture(out / "actor-depth.pgm");
    auto const& actual = d.ab->peripherals.display.ram;
    for(unsigned i = 0; i < expected.size(); ++i) {
        // OLED segment remapping reverses physical RAM columns on the Arduboy.
        unsigned column = d.ab->peripherals.display.segment_remap ? 127 - i % 128 : i % 128;
        unsigned physical = (i / 128) * 128 + column;
        if((actual[physical] & covered[i]) != expected[i])
            std::cerr << "Depth pixel byte=" << i << " actual=" << unsigned(actual[physical])
                      << " expected=" << unsigned(expected[i]) << " mask=" << unsigned(covered[i])
                      << " camera=" << rendered.camera.x << ',' << rendered.camera.y << '\n';
        require((actual[physical] & covered[i]) == expected[i],
                "masked actor occlusion has the wrong depth order");
    }
    d.set_state(original);
    d.frame();
}

static void summary(char const* name, Device& d, unsigned n, uint8_t held = 0) {
    std::vector<Cost> costs;
    uint64_t sum = 0, worst = 0, render = 0, first = 0, last = 0;
    uint64_t reload = 0, chunks = 0, collision = 0;
    uint64_t reload_worst = 0, chunks_worst = 0, collision_worst = 0;

    for(unsigned i = 0; i < n; ++i) {
        d.frame(held);
        auto c = d.last;
        sum += c.total;
        render += c.render;
        reload += c.reload;
        chunks += c.chunks;
        collision += c.collision;
        reload_worst = std::max(reload_worst, c.reload);
        chunks_worst = std::max(chunks_worst, c.chunks);
        collision_worst = std::max(collision_worst, c.collision);
        worst = std::max(worst, c.total);
        if(!i)
            first = c.begin;
        last = c.begin;
        costs.push_back(c);
    }

    double fps = n > 1 ? double(n - 1) * 16000000.0 / double(last - first) : 0;
    require(worst <= 320000, "exploration frame exceeded the 320000-cycle budget");
    std::cout << "{\"workload\":\"" << name << "\",\"frames\":" << n
              << ",\"average_cycles\":" << sum / n << ",\"worst_cycles\":" << worst
              << ",\"average_render_display_cycles\":" << render / n << ",\"observed_fps\":" << fps
              << ",\"average_reload_cycles\":" << reload / n
              << ",\"worst_reload_cycles\":" << reload_worst
              << ",\"average_chunk_cycles\":" << chunks / n
              << ",\"worst_chunk_cycles\":" << chunks_worst
              << ",\"average_collision_cycles\":" << collision / n
              << ",\"worst_collision_cycles\":" << collision_worst
              << ",\"budget_cycles\":320000}\n";
}

// The Grove battle always plays the same way, because the enemy chooses deterministically. Each
// entry is one party action: the hero's cell, the cell it targets, in battlefield tiles, and
// whether that cell holds an enemy. After a strike the cursor returns to the hero, as it does in
// the battle.
struct GroveCell {
    uint8_t x, y;
};

struct GroveAction {
    GroveCell hero, target;
    bool strike;
};

// Host-only SPI observation of the frames actually sent to the OLED. It adds no guest markers or
// buffers. The framebuffer's absolute address is part of the AVM memory map.
struct FadeTrace {
    struct Frame {
        std::array<uint8_t, 1024> pixels;
        uint64_t cycle;
    };

    Device& device;
    std::vector<Frame> frames;
    Frame pending{};
    unsigned bytes = 0;
    uint64_t previous_transfer = 0;
    bool synchronized;

    explicit FadeTrace(Device& d)
        : device(d),
          synchronized((d.ab->core_state.cpu.data[absim::reg::addr::PORTD] & 0x40) != 0) {
        require(!d.cycle_observer, "another device observer is attached");
        std::copy_n(d.ab->core_state.cpu.data.begin() + 0x500, 1024, pending.pixels.begin());
        pending.cycle = d.ab->core_state.cpu.cycle_count;
        frames.push_back(pending);
        d.observer_context = this;
        d.cycle_observer = [](void* context, bool after) {
            static_cast<FadeTrace*>(context)->observe(after);
        };
    }

    ~FadeTrace() {
        device.cycle_observer = nullptr;
        device.observer_context = nullptr;
    }

    void observe(bool after) {
        auto const& cpu = device.ab->core_state.cpu;
        if(!after) {
            previous_transfer = cpu.spi_transmit_zero_cycle;
            return;
        }
        uint8_t port = cpu.data[absim::reg::addr::PORTD];
        // An observer may attach in the middle of a display transfer. Start at a complete
        // chip-select transaction so captured frames cannot wrap around the framebuffer.
        if(port & 0x40) {
            synchronized = true;
            bytes = 0;
        }
        if(!synchronized)
            return;
        if(!cpu.spi_busy || cpu.spi_transmit_zero_cycle == previous_transfer ||
           (port & 0x50) != 0x10)
            return;
        if(!bytes)
            pending.cycle = cpu.cycle_count;
        pending.pixels[bytes++] = cpu.spi_data_byte;
        if(bytes == pending.pixels.size()) {
            frames.push_back(pending);
            bytes = 0;
        }
    }

    void verify(char const* name) const {
        auto black = std::find_if(frames.begin() + 1, frames.end(), [](Frame const& f) {
            return std::all_of(f.pixels.begin(), f.pixels.end(), [](uint8_t p) { return p == 0; });
        });
        require(black != frames.end() && black - frames.begin() >= 8 && frames.end() - black > 8,
                "battle transition did not fade through black");
        auto first = black - 8;
        auto last = black + 8;
        require(first->pixels != black->pixels && last->pixels != black->pixels,
                "battle transition has an empty endpoint");
        constexpr uint8_t BAYER[4][4] = {
            {0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
        uint64_t worst = 0;
        for(auto frame = first + 1; frame <= last; ++frame) {
            unsigned step = frame <= black ? unsigned(frame - first) : unsigned(last - frame);
            auto const& source = frame <= black ? first->pixels : last->pixels;
            for(unsigned i = 0; i < source.size(); ++i) {
                uint8_t expected = source[i];
                for(unsigned bit = 0; bit < 8; ++bit) {
                    if(BAYER[bit % 4][i % 4] < step * 2)
                        expected &= uint8_t(~(1 << bit));
                }
                require(frame->pixels[i] == expected,
                        "battle dither changed or leaked scene pixels");
            }
            if(frame != first + 1)
                worst = std::max(worst, frame->cycle - (frame - 1)->cycle);
        }
        // Allow interrupt jitter around the 320,000-cycle frame period, but no missed frame.
        require(worst < 336000, "battle transition missed its 50 Hz frame budget");
        std::cout << "{\"workload\":\"" << name << "\",\"fade_frames\":16,"
                  << "\"worst_frame_cycles\":" << worst << ",\"budget_cycles\":320000}\n";
    }
};

static void verify_pause_animation(FadeTrace const& trace, bool closing, bool loading = false) {
    constexpr int WIDTH = 39; // BR5D's widest label is 25 pixels, plus cursor and panel padding.
    constexpr uint8_t BAYER[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
    auto matches = [&](FadeTrace::Frame const& frame, FadeTrace::Frame const& source, int visible) {
        unsigned fade = visible * 4 / WIDTH;
        if(loading)
            fade = 8 - fade;
        for(unsigned i = WIDTH; i < frame.pixels.size(); ++i) {
            if(i % 128 < WIDTH)
                continue;
            uint8_t expected = source.pixels[i];
            for(unsigned bit = 0; bit < 8; ++bit)
                if(BAYER[bit % 4][i % 4] < fade * 2)
                    expected &= uint8_t(~(1 << bit));
            if(frame.pixels[i] != expected)
                return false;
        }
        for(int x = 0; x < visible; ++x) {
            bool white = x >= visible - WIDTH + 1 && x < visible - 1;
            if((frame.pixels[x] & 3) != (white ? 2 : 0) ||
               (frame.pixels[7 * 128 + x] & 0xc0) != (white ? 0x40 : 0))
                return false;
        }
        return true;
    };
    // The closing trace ends in the restored world. The opening trace begins in the world.
    auto const& source = closing && !loading ? trace.frames.back() : trace.frames[1];
    int position = closing ? WIDTH * 16 : 0;
    size_t at = 1;
    bool first = true;
    uint64_t previous = 0;
    do {
        int target = closing ? 0 : WIDTH * 16;
        int distance = target - position;
        position = distance >= -1 && distance <= 1 ? target : position + distance / 2;
        int visible = (position + 8) / 16;
        if(first) {
            while(at < trace.frames.size() && !matches(trace.frames[at], source, visible))
                ++at;
            first = false;
        }
        require(at < trace.frames.size() && matches(trace.frames[at], source, visible),
                "pause panel easing, outline or world fade mismatch");
        auto cycle = trace.frames[at++].cycle;
        require(!previous || cycle - previous < 336000, "pause animation missed its 50 Hz budget");
        previous = cycle;
    } while(position != (closing ? 0 : WIDTH * 16));
    if(loading) {
        auto black = trace.frames.begin() + at - 1;
        require(
            std::all_of(black->pixels.begin(), black->pixels.end(), [](uint8_t p) { return !p; }),
            "Load did not hide the world completely before loading");
        while(black + 1 != trace.frames.end() && (black + 1)->pixels == black->pixels)
            ++black;
        require(trace.frames.end() - black > 8, "Load did not finish fading in the saved world");
        auto const& destination = (black + 8)->pixels;
        for(unsigned frame = 1; frame <= 8; ++frame) {
            for(unsigned i = 0; i < destination.size(); ++i) {
                uint8_t expected = destination[i];
                for(unsigned bit = 0; bit < 8; ++bit)
                    if(BAYER[bit % 4][i % 4] < (8 - frame) * 2)
                        expected &= uint8_t(~(1 << bit));
                require((black + frame)->pixels[i] == expected,
                        "Load did not fade the saved world completely back in");
            }
        }
    }
}

static FadeTrace::Frame pause_frame(Device& d) {
    FadeTrace trace(d);
    d.advance(0, 800000);
    return trace.frames.back();
}

static bool pause_row_visible(Device& d, unsigned row) {
    auto pixels = pause_frame(d).pixels;
    for(unsigned y = 12 + 7 * row; y < 18 + 7 * row; ++y)
        for(unsigned x = 4; x < 35; ++x)
            if(pixels[(y / 8) * 128 + x] & (1 << (y % 8)))
                return true;
    return false;
}

static void open_audio(Device& d, bool can_load = false) {
    d.tap(BTN_B);
    d.advance(0, 1600000);
    d.tap(BTN_UP);
    if(can_load)
        d.tap(BTN_UP);
    d.tap(BTN_A);
    d.advance(0, 9600000);
}

static void verify_options_handoff(FadeTrace const& trace) {
    auto pixel = [](FadeTrace::Frame const& frame, int x, int y) {
        return (frame.pixels[y / 8 * 128 + x] >> (y % 8)) & 1;
    };
    auto left_edge = [&](FadeTrace::Frame const& frame, int visible) {
        for(int x = 0; x < visible; ++x)
            if(pixel(frame, x, 0) || pixel(frame, x, 1) != (x < visible - 1))
                return false;
        return true;
    };
    auto const& source = trace.frames[1];
    size_t at = 1;
    while(at < trace.frames.size() && !left_edge(trace.frames[at], 20))
        ++at;
    int position = 39 * 16;
    uint64_t previous = 0;
    do {
        position -= position / 2;
        int visible = (position + 8) / 16;
        require(at < trace.frames.size() && left_edge(trace.frames[at], visible),
                "pause did not ease out before Audio");
        auto const& frame = trace.frames[at++];
        for(unsigned i = 0; i < source.pixels.size(); ++i)
            if(i % 128 >= 39)
                require(frame.pixels[i] == source.pixels[i],
                        "world brightened during Audio handoff");
        require(!previous || frame.cycle - previous < 336000, "pause handoff missed a frame");
        previous = frame.cycle;
    } while((position + 8) / 16);

    position = 0;
    do {
        bool first_options_frame = position == 0;
        int distance = AUDIO_WIDTH * 16 - position;
        position = distance <= 1 ? AUDIO_WIDTH * 16 : position + distance / 2;
        int left = 128 - (position + 8) / 16;
        require(at < trace.frames.size(), "Audio did not ease in immediately");
        auto const& frame = trace.frames[at++];
        for(int x = left; x < 128; ++x)
            require(!pixel(frame, x, 0) &&
                        pixel(frame, x, 1) == (x > left && x < left + AUDIO_WIDTH - 1),
                    "Audio did not ease in from the right");
        for(unsigned page = 0; page < 8; ++page)
            require(frame.pixels[page * 128 + 39] == source.pixels[page * 128 + 39],
                    "Audio changed the world's half fade");
        // The first Audio image has more drawing work than the outgoing empty pause image.
        // Subsequent presentations must keep the usual 50 Hz cadence.
        require(frame.cycle - previous < (first_options_frame ? 480000 : 336000),
                "Audio handoff missed a frame");
        previous = frame.cycle;
    } while(position != AUDIO_WIDTH * 16);
}

static void verify_audio_options(Device const& d, unsigned music, unsigned sfx, unsigned lowpass) {
    auto const& ram = d.ab->core_state.cpu.data;
    require(ram[0xa71] == music && ram[0xa72] == sfx && ram[0xa6e] == lowpass,
            "Audio did not apply independent audio settings");
    require(ram[0xa17] == 255, "Audio changed the shared master gain");
}

static void verify_options(Device& d, fs::path const& out) {
    require(d.state().options.music == 15 && d.state().options.sfx == 15 &&
                !d.state().options.lowpass,
            "new game did not initialize audio defaults");
    verify_audio_options(d, 128, 128, 0);
    d.tap(BTN_B);
    d.advance(0, 1600000);
    d.tap(BTN_UP);
    auto frozen = d.state();
    auto serial = d.serial();
    {
        FadeTrace trace(d);
        d.advance(0, 800000);
        d.tap(BTN_A);
        d.advance(0, 9600000);
        verify_options_handoff(trace);
    }
    d.capture(out / "options.pgm");
    d.tap(BTN_LEFT);
    verify_audio_options(d, 119, 128, 0);
    auto pixels = pause_frame(d).pixels;
    int knob = 128 - AUDIO_WIDTH + 10 + 14 * (AUDIO_WIDTH - 23) / 30;
    for(int x = 50; x < 118; ++x)
        require(((pixels[2 * 128 + x] >> 4) & 1) == (x >= knob && x < knob + 3),
                "music slider thumb did not move with its volume");
    for(unsigned i = 0; i < 17; ++i)
        d.tap(BTN_LEFT);
    verify_audio_options(d, 0, 128, 0);
    d.capture(out / "options-music-muted.pgm");
    d.advance(BTN_RIGHT, 32000000);
    d.advance(0, 800000);
    verify_audio_options(d, 255, 128, 0);
    require(d.state().options.music == 30, "music did not reach 200 percent");
    d.tap(BTN_RIGHT);
    verify_audio_options(d, 255, 128, 0);
    d.capture(out / "options-music-200.pgm");

    d.tap(BTN_DOWN);
    d.tap(BTN_LEFT);
    d.tap(BTN_LEFT);
    verify_audio_options(d, 255, 111, 0);
    for(unsigned i = 0; i < 17; ++i)
        d.tap(BTN_LEFT);
    verify_audio_options(d, 255, 0, 0);
    d.advance(BTN_RIGHT, 32000000);
    d.advance(0, 800000);
    verify_audio_options(d, 255, 255, 0);
    require(d.state().options.sfx == 30, "SFX did not reach 200 percent");

    d.tap(BTN_DOWN);
    for(unsigned i = 0; i < 5; ++i)
        d.tap(BTN_RIGHT);
    verify_audio_options(d, 255, 255, 64);
    d.capture(out / "options-lowpass.pgm");
    d.advance(BTN_RIGHT, 19200000);
    d.advance(0, 800000);
    verify_audio_options(d, 255, 255, 1);
    d.advance(BTN_LEFT, 19200000);
    d.advance(0, 800000);
    verify_audio_options(d, 255, 255, 0);
    d.tap(BTN_DOWN); // Wrap to Music.
    d.tap(BTN_LEFT);
    verify_audio_options(d, 247, 255, 0);
    d.tap(BTN_RIGHT);
    d.tap(BTN_UP); // Wrap back to Low-pass.
    d.tap(BTN_RIGHT);
    verify_audio_options(d, 255, 255, 128);
    d.tap(BTN_LEFT);

    // Restore the half-scale native defaults (100 percent on the expanded sliders).
    d.tap(BTN_DOWN);
    for(unsigned i = 0; i < 15; ++i)
        d.tap(BTN_LEFT);
    d.tap(BTN_DOWN);
    for(unsigned i = 0; i < 15; ++i)
        d.tap(BTN_LEFT);
    verify_audio_options(d, 128, 128, 0);
    d.tap(BTN_A);
    auto paused = d.state();
    require(d.serial() == serial && !memcmp(&paused, &frozen, sizeof(Game)),
            "Audio advanced or altered the paused world");
    d.tap(BTN_B);
    d.frame();
    require(d.serial() != serial, "B from Audio did not return directly to gameplay");
}

static void set_test_options(Device& d) {
    open_audio(d);
    for(unsigned i = 0; i < 3; ++i)
        d.tap(BTN_LEFT);
    d.tap(BTN_DOWN);
    for(unsigned i = 0; i < 5; ++i)
        d.tap(BTN_LEFT);
    d.tap(BTN_DOWN);
    for(unsigned i = 0; i < 5; ++i)
        d.tap(BTN_RIGHT);
    verify_audio_options(d, 102, 85, 64);
    d.tap(BTN_B);
    d.frame();
}

static void verify_pause_party(Device& d) {
    d.tap(BTN_B);
    d.advance(0, 1600000);
    auto before = pause_frame(d).pixels;
    Game frozen = d.state();
    d.tap(BTN_UP);
    d.tap(BTN_UP);
    d.tap(BTN_A); // Map
    d.tap(BTN_UP);
    d.tap(BTN_A); // Journal
    d.tap(BTN_LEFT);
    d.tap(BTN_RIGHT);
    d.tap(BTN_UP); // Party
    d.tap(BTN_A);
    d.tap(BTN_B);
    auto after = pause_frame(d).pixels;
    Game returned = d.state();
    require(!memcmp(&frozen, &returned, sizeof(Game)), "Party changed paused game state");
    for(unsigned i = 0; i < before.size(); ++i) {
        if(i % 128 >= 39 && before[i] != after[i]) {
            std::cerr << "Party pixels at " << i << ": " << unsigned(before[i]) << " -> "
                      << unsigned(after[i]) << '\n';
            require(false, "Party changed the paused world image");
        }
    }
    d.tap(BTN_B);
    d.frame();
}

static void verify_battle_menu(Device& d, fs::path const& out) {
    d.advance(0, 6400000);
    BattleState initial = d.battle_state();
    Game world = d.state();
    unsigned serial = d.serial();
    d.tap(BTN_B);
    d.advance(0, 3200000);
    require(pause_row_visible(d, 3) && !pause_row_visible(d, 4) && !pause_row_visible(d, 5),
            "battle pause menu did not hide exploration items and unavailable Load");
    d.capture(out / "battle-menu.pgm");
    // Resign remains visible and does nothing.
    for(unsigned i = 0; i < 3; ++i)
        d.tap(BTN_DOWN);
    d.tap(BTN_A);
    BattleState paused = d.battle_state();
    require(!memcmp(&initial, &paused, sizeof(initial)) && pause_row_visible(d, 3),
            "Resign changed the battle");
    d.tap(BTN_UP); // Audio
    d.tap(BTN_A);
    d.advance(0, 9600000);
    d.capture(out / "battle-audio.pgm");
    d.tap(BTN_LEFT);
    verify_audio_options(d, 119, 128, 0);
    d.tap(BTN_RIGHT);
    paused = d.battle_state();
    require(!memcmp(&initial, &paused, sizeof(initial)) && d.serial() == serial,
            "battle advanced while Audio was open");
    d.tap(BTN_B);
    d.advance(0, 6400000);
    paused = d.battle_state();
    require(!memcmp(&initial, &paused, sizeof(initial)), "Audio did not resume the same battle");

    auto wait_activation = [&] {
        for(unsigned frame = 0; d.battle_state().activations == 0; ++frame) {
            require(frame < 300, "battle pass did not resolve");
            d.advance(0, 320000);
        }
        d.advance(0, 160000); // Let the pass finish updating HP before its visual reaction.
        return d.battle_state();
    };
    d.tap(BTN_B);
    d.advance(0, 1600000);
    d.tap(BTN_DOWN); // Pass voluntarily, even with legal actions.
    d.tap(BTN_A);
    BattleState passed = wait_activation();
    require(passed.activations == 1 && passed.turn == BATTLE_ENEMY,
            "player Pass did not consume one activation");
    for(unsigned i = 0; i < initial.unit_count; ++i)
        require(passed.units[i].hp == initial.units[i].hp - (initial.units[i].team == BATTLE_PARTY),
                "player Pass did not damage every party member exactly once");
    // Restore before the short reaction ends so the ordinary Grove plan can still be exercised.
    d.set_battle(initial);
    d.advance(0, 8000000);

    BattleState blocked = initial;
    blocked.turn = BATTLE_ENEMY;
    blocked.unit_count = 6;
    blocked.units[0] = {battle_cell(0, 0), 2, BATTLE_TYPE_ARCHER, BATTLE_ENEMY};
    blocked.units[1] = {battle_cell(7, 3), 3, BATTLE_TYPE_ARCHER, BATTLE_ENEMY};
    blocked.units[2] = {battle_cell(1, 0), 8, BATTLE_TYPE_FIGHTER, BATTLE_PARTY};
    blocked.units[3] = {battle_cell(0, 1), 8, BATTLE_TYPE_FIGHTER, BATTLE_PARTY};
    blocked.units[4] = {battle_cell(6, 3), 8, BATTLE_TYPE_FIGHTER, BATTLE_PARTY};
    blocked.units[5] = {battle_cell(7, 2), 8, BATTLE_TYPE_FIGHTER, BATTLE_PARTY};
    d.set_battle(blocked);
    d.tap(BTN_B);
    d.advance(0, 1600000);
    d.tap(BTN_B); // Resume hands control back to the forced-pass check.
    passed = wait_activation();
    require(passed.activations == 1 && passed.turn == BATTLE_PARTY && passed.units[0].hp == 1 &&
                passed.units[1].hp == 2,
            "AI forced Pass did not damage its whole team");
    for(unsigned i = 2; i < blocked.unit_count; ++i)
        require(passed.units[i].hp == 8, "AI Pass damaged the opposing team");
    d.set_battle(initial);
    d.advance(0, 8000000);

    // This archer has legal moves, but its completed search prefers staying put and passing.
    BattleState waiting = initial;
    waiting.turn = BATTLE_ENEMY;
    waiting.unit_count = 2;
    waiting.units[0] = {battle_cell(0, 0), 4, BATTLE_TYPE_BRUTE, BATTLE_PARTY};
    waiting.units[1] = {battle_cell(0, 1), 12, BATTLE_TYPE_ARCHER, BATTLE_ENEMY};
    d.set_battle(waiting);
    d.tap(BTN_B);
    d.advance(0, 1600000);
    d.tap(BTN_B);
    passed = wait_activation();
    require(passed.turn == BATTLE_PARTY && passed.units[0].hp == 4 && passed.units[1].hp == 11 &&
                passed.units[1].cell == battle_cell(0, 1),
            "AI did not execute the voluntary Pass selected by its search");
    d.set_battle(initial);
    d.advance(0, 8000000);
    Game after = d.state();
    require(d.serial() == serial && !memcmp(&world, &after, sizeof(world)),
            "battle menu advanced exploration or changed saved options");
}

static void verify_house_fades(Device& d) {
    Game original = d.state();
    Game at_door = original;
    at_door.player = {(uint16_t)(positions::ElderDoor.x * TILE),
                      (uint16_t)((positions::ElderDoor.y + 1) * TILE), NORTH, 0};
    d.set_state(at_door);
    d.advance(0, 1600000);
    require(d.state().can_interact, "house door did not become interactive");
    uint8_t door_tile = d.cached_tile(positions::ElderDoor.x, positions::ElderDoor.y);
    {
        FadeTrace trace(d);
        d.tap(BTN_A);
        d.advance(0, 4800000);
        trace.verify("house_entry");
    }
    Game inside = d.state();
    require(inside.area == positions::ElderExit.area &&
                inside.player.x == positions::ElderExit.x * TILE &&
                inside.player.y == (positions::ElderExit.y - 1) * TILE &&
                inside.player.facing == NORTH &&
                d.cached_tile(positions::ElderExit.x, positions::ElderExit.y) ==
                    HOUSE_TILE_EDGE_TOP_EXIT,
            "house entry revealed the wrong destination or tile cache");
    d.tap(BTN_B);
    d.advance(0, 1600000);
    require(pause_row_visible(d, 4) && !pause_row_visible(d, 5),
            "interior pause menu did not hide Map and Load");
    d.tap(BTN_B);
    d.frame();
    {
        FadeTrace trace(d);
        for(unsigned frames = 0; d.state().area == inside.area; ++frames) {
            require(frames < 2 * TILE, "walking onto the house exit did not teleport");
            d.frame(BTN_DOWN);
        }
        trace.verify("house_exit");
    }
    Game outside = d.state();
    require(outside.area == original.area && outside.player.x == positions::ElderDoor.x * TILE &&
                outside.player.y == (positions::ElderDoor.y + 1) * TILE &&
                outside.player.facing == SOUTH && !outside.player.walking &&
                d.cached_tile(positions::ElderDoor.x, positions::ElderDoor.y) == door_tile,
            "house exit revealed the wrong destination or tile cache");
    d.set_state(original);
    d.advance(0, 1600000);
}

// The shortest plan that wins against the enemy's search, found by a breadth-first search over
// party actions with the enemy's replies applied. The rogue (10 HP) begins at (2,1) and the archer
// (9) at (1,2); the wolf (9) at (6,1) and the brute (12) at (5,2). Trees stand at (4,0) and (6,0),
// rocks at (1,3), and a boulder at (4,3). Every move costs its mover 1 HP and hits each adjacent
// enemy for 1. The battle ends in victory on the enemy's turn: its last unit, the brute at 1 HP,
// has no attack, and the move it must make costs its last HP.
constexpr GroveAction GROVE_VICTORY_PLAN[] = {
    {{1, 2}, {5, 2}, true},  // The archer shoots the brute. The brute steps to (6,2).
    {{2, 1}, {4, 2}, false}, // The rogue leaps to (4,2). The wolf runs to (4,1), biting the rogue.
    {{4, 2}, {4, 1}, true},  // The rogue strikes the wolf, which strikes back.
    {{4, 2}, {4, 1}, true},  // The rogue strikes the wolf again. The 1 HP wolf runs to (6,1).
    {{1, 2}, {2, 2}, false}, // The archer steps east. The brute steps to (5,2), beside the rogue.
    {{4, 2}, {5, 2}, true},  // The rogue strikes the brute, which knocks it back to (3,2).
    {{3, 2}, {5, 1}, false}, // The rogue leaps to (5,1), engaging the wolf, which falls, and the
                             // brute, which steps to (5,3).
    {{5, 1}, {6, 3}, false}, // The 1 HP rogue leaps beside the brute and falls. The 1 HP brute must
                             // move, and falls.
};

// After each hero action the test advances past both sides' reactions: the hero's own animation,
// the enemy pause, the enemy's search, the enemy's animation, and the enemy's result pause. The
// search's node budget keeps it under about 31 million cycles on the AVM at the positions measured
// in docs/performance.md, under two seconds of thinking. 120 million cycles (375 frames of
// 320,000 cycles each) leaves a margin for every entry.
constexpr unsigned GROVE_ENEMY_TURN_CYCLES = 120000000;

// The cursor moves one tile per press, so moving it takes one press per tile.
static void grove_move_cursor(Device& d, GroveCell& cursor, GroveCell target) {
    while(cursor.x < target.x) {
        d.tap(BTN_RIGHT);
        ++cursor.x;
    }
    while(cursor.x > target.x) {
        d.tap(BTN_LEFT);
        --cursor.x;
    }
    while(cursor.y < target.y) {
        d.tap(BTN_DOWN);
        ++cursor.y;
    }
    while(cursor.y > target.y) {
        d.tap(BTN_UP);
        --cursor.y;
    }
}

// Plays the sign's battle from its first board through the victory overlay, then closes the
// outcome line. The battle starts after the welcome line closes, so the caller resumes with
// exploration.
static void play_grove_battle(Device& d, bool verify_transition = false) {
    // A caller may have just closed the welcome; let the entry transition finish before acting.
    d.advance(0, 6400000);
    GroveCell cursor{2, 1}; // The battle opens with the cursor on the rogue.
    for(auto const& action : GROVE_VICTORY_PLAN) {
        grove_move_cursor(d, cursor, action.hero);
        d.tap(BTN_A); // Select the hero.
        grove_move_cursor(d, cursor, action.target);
        d.tap(BTN_A); // Confirm the destination.
        // A move leaves the cursor on the hero's new cell, and a strike returns it to the hero.
        cursor = action.strike ? action.hero : action.target;
        d.advance(0, GROVE_ENEMY_TURN_CYCLES);
    }
    // The victory overlay waits for a fresh A. Its outcome line then opens over exploration.
    d.advance(0, 1600000);
    if(verify_transition) {
        FadeTrace trace(d);
        auto frozen = d.state();
        d.tap(BTN_A);
        d.advance(0, 4000000);
        trace.verify("battle_exit");
        auto paused = d.state();
        require(!memcmp(&frozen, &paused, sizeof(Game)), "battle exit advanced world simulation");
    } else {
        d.tap(BTN_A);
        d.advance(0, 4000000);
    }
    require(d.prompt() == DIALOGUE_TYPING, "grove battle did not return to its outcome line");
    d.wait_for_prompt();
    d.tap(BTN_A);
    require(d.prompt() == DIALOGUE_CLOSED, "grove outcome line did not close");
}

#ifndef RPG_DEVICE_TEST_LIBRARY
static void press_with_music_fade(Device& d) {
    struct Trace {
        Device& device;
        std::vector<uint8_t> levels;
        uint8_t previous = 128;
        void (*observer)(void*, bool);
        void* observer_context;
    } trace{d, {}, 128, d.cycle_observer, d.observer_context};

    d.observer_context = &trace;
    d.cycle_observer = [](void* context, bool after) {
        auto& trace = *static_cast<Trace*>(context);
        if(trace.observer)
            trace.observer(trace.observer_context, after);
        if(!after)
            return;
        auto const& ram = trace.device.ab->core_state.cpu.data;
        uint8_t level = ram[0xa71];
        if(level != trace.previous) {
            trace.levels.push_back(level);
            trace.previous = level;
        }
        require(ram[0xa17] == 255 && ram[0xa72] == 128, "music fade changed master or SFX gain");
    };
    d.tap(BTN_A);
    // A battle entry also has a 320 ms visual transition; wait for its input loop while the
    // longer music fade is still running, so the right/left pair reaches the same screen.
    d.advance(0, 4000000);
    require(d.ab->core_state.cpu.data[0xa73] != 0,
            "fade did not return while the song was still fading");
    auto display = d.ab->peripherals.display.ram;
    d.tap(BTN_RIGHT);
    require(d.ab->peripherals.display.ram != display,
            "music fade blocked input or display updates");
    d.tap(BTN_LEFT); // Restore the menu selection or battle cursor.
    d.advance(0, 16000000);
    d.cycle_observer = trace.observer;
    d.observer_context = trace.observer_context;
    require(trace.levels.size() > 40 && trace.levels.back() == 128 &&
                d.ab->core_state.cpu.data[0xa73] == 0,
            "song switch did not complete its fade and restore the configured volume");
    for(unsigned i = 1; i + 1 < trace.levels.size(); ++i)
        require(trace.levels[i] < trace.levels[i - 1], "music fade did not ramp down");
}

int main(int argc, char** argv) try {
    require(argc == 9 || argc == 10,
            "device_tests package game_address marker_address serial_address "
            "output_directory normal|fxc|proof|pause|battle dialogue_address phase_address "
            "[battle_pointer_address]");
    fs::path package = argv[1], out = argv[5];
    fs::create_directories(out);
    unsigned g = std::stoul(argv[2]), m = std::stoul(argv[3]), s = std::stoul(argv[4]);
    unsigned q = std::stoul(argv[7]);
    Device d(package, g, m, s, q, std::string(argv[6]) == "proof");
    d.phase_addr = std::stoul(argv[8]);
    if(argc == 10)
        d.battle_pointer_addr = std::stoul(argv[9]);
    if(d.proof) {
        d.frame();
        summary("m0_proof", d, 30);
        d.capture(out / "proof-visible.pgm");
        return 0;
    }

    d.advance(0, 3200000);
    require(d.serial() == 0, "title unexpectedly advanced exploration");
    require(!d.music_enabled(), "music started before a game was selected");
    d.capture(out / "title.pgm");
    d.tap(BTN_A);
    require(d.serial() != 0 && d.state().ticks != 0, "new game did not start exploration");
    verify_options(d, out);
    verify_pause_party(d);
    if(std::string(argv[6]) == "pause") {
        std::cout << "{\"result\":\"PASS\",\"scenario\":\"Audio and pause redraw\"}\n";
        return 0;
    }
    if(std::string(argv[6]) == "battle") {
        // Also check that the battle menu hides Load when a physical save is available.
        d.tap(BTN_B);
        d.advance(0, 1600000);
        d.tap(BTN_DOWN);
        d.tap(BTN_A);
        d.tap(BTN_B);
        d.frame();
        d.tap(BTN_A);
        d.wait_for_prompt();
        d.tap(BTN_A);
        verify_battle_menu(d, out);
        std::cout << "{\"result\":\"PASS\",\"scenario\":\"battle pause, Audio and team passes\"}\n";
        return 0;
    }
    require(d.ab->core_state.cpu.timer1.ocrNa == 127, "new game did not default to 16 kHz audio");
    d.verify_music();
    require(d.state().player.x == positions::PlayerStart.x * TILE &&
                d.state().player.y == positions::PlayerStart.y * TILE,
            "wrong spawn");
    require(d.state().dimensions.width_tiles == OUTDOOR_WIDTH &&
                d.state().dimensions.height_tiles == OUTDOOR_HEIGHT,
            "wrong map dimensions");
    require(d.cached_tile(positions::GroveSign.x, positions::GroveSign.y) == TILE_SIGN,
            "sign atlas index changed");
    unsigned actors = 0;
    for(auto const& n : d.state().npcs.actors)
        actors += n.id != NPC_NONE;
    require(actors == 2, "sample NPCs did not activate");
    verify_actor_layers(d, out);
    summary("idle_grove", d, 30);
    d.capture(out / "grove.pgm");

    d.tap(BTN_A);
    require(d.prompt() == DIALOGUE_TYPING, "facing sign plus A did not open dialogue");
    auto frozen = d.state();
    unsigned dialogue_serial = d.serial();
    d.advance(0, 3200000);
    auto paused = d.state();
    require(dialogue_serial == d.serial() && !memcmp(&frozen, &paused, sizeof(Game)),
            "sign dialogue did not freeze simulation");
    d.verify_music();
    require(d.prompt() == DIALOGUE_TYPING, "sign dialogue finished without animating letters");
    d.capture(out / "sign-dialogue-typing.pgm");
    d.wait_for_typing();
    require(d.prompt() == DIALOGUE_DONE, "sign dialogue did not reach its final prompt");
    // A pressed inside the advance guard is ignored; a later fresh press closes the dialogue.
    d.advance(BTN_A, 800000);
    d.advance(0, 1600000);
    require(d.prompt() == DIALOGUE_DONE, "advance guard did not ignore an early A press");
    d.capture(out / "sign-dialogue.pgm");
    d.advance(0, 8000000);
    {
        FadeTrace trace(d);
        auto frozen = d.state();
        press_with_music_fade(d);
        trace.verify("battle_entry");
        auto paused = d.state();
        require(!memcmp(&frozen, &paused, sizeof(Game)), "battle entry advanced world simulation");
    }
    // Closing the welcome starts the Grove battle, which returns only after its outcome line.
    d.capture(out / "grove-battle.pgm");
    verify_battle_menu(d, out);
    play_grove_battle(d, true);
    require(d.prompt() == DIALOGUE_CLOSED, "sign dialogue did not return to exploration");
    verify_house_fades(d);

    // Stand one tile north of the villager, facing it, and talk with A.
    auto npc_state = d.state();
    auto villager = std::find_if(npc_state.npcs.actors, npc_state.npcs.actors + NPC_CAPACITY,
                                 [](Npc const& n) { return n.id == NPC_VILLAGER; });
    require(villager != npc_state.npcs.actors + NPC_CAPACITY, "villager is not active");
    npc_state.player.x = villager->actor.x;
    npc_state.player.y = villager->actor.y - TILE;
    npc_state.player.facing = SOUTH;
    npc_state.player.walking = 0;
    d.set_state(npc_state);
    d.advance(0, 800000);
    d.tap(BTN_A);
    require(d.prompt() == DIALOGUE_TYPING, "villager talk did not open dialogue");
    d.wait_for_typing();
    require(d.prompt() == DIALOGUE_DONE, "villager dialogue did not reach its final prompt");
    d.capture(out / "npc-dialogue.pgm");
    d.advance(0, 8000000);
    d.tap(BTN_A);
    require(d.prompt() == DIALOGUE_CLOSED, "villager dialogue did not close");

    // Each conversation sends the villager to the other end of the clearing. It walks while
    // visible, and leaving view never teleports it: it waits in its slot until seen again.
    auto villager_in = [](Game const& g) -> Npc const* {
        for(auto const& n : g.npcs.actors)
            if(n.id == NPC_VILLAGER)
                return &n;
        return nullptr;
    };
    auto place_player = [&](uint8_t tx, uint8_t ty, DIRECTION facing) {
        Game g = d.state();
        g.player.x = (uint16_t)tx * TILE;
        g.player.y = (uint16_t)ty * TILE;
        g.player.facing = facing;
        g.player.walking = 0;
        d.set_state(g);
        d.advance(0, 800000);
    };
    auto close_villager_dialogue = [&]() {
        d.wait_for_typing();
        require(d.prompt() == DIALOGUE_DONE, "villager dialogue did not reach its final prompt");
        d.advance(0, 8000000);
        d.tap(BTN_A);
        require(d.prompt() == DIALOGUE_CLOSED, "villager dialogue did not close");
    };
    TilePos const west = positions::VillagerWest, east = positions::VillagerEast;

    // A device slice must keep the villager in its slot and move it at most a couple of
    // pixels. A teleport jumps by a tile or more, and a pop-in creates a new slot.
    uint16_t vx = 0, vy = 0;
    auto track_villager = [&]() {
        Game g = d.state();
        auto const* v = villager_in(g);
        require(v, "villager missing before its walk");
        vx = v->actor.x;
        vy = v->actor.y;
    };
    auto step_villager = [&]() {
        d.advance(0, 533334);
        Game g = d.state();
        auto const* v = villager_in(g);
        require(v, "villager lost its slot instead of walking");
        int dx = (int)v->actor.x - (int)vx, dy = (int)v->actor.y - (int)vy;
        dx = dx < 0 ? -dx : dx;
        dy = dy < 0 ? -dy : dy;
        require(dx + dy <= 2, "villager jumped instead of walking");
        vx = v->actor.x;
        vy = v->actor.y;
    };
    auto near_stop = [&](uint16_t x, uint16_t y) {
        int dx = (int)vx - (int)x, dy = (int)vy - (int)y;
        dx = dx < 0 ? -dx : dx;
        dy = dy < 0 ? -dy : dy;
        return dx + dy <= 4;
    };
    auto villager_visible = [&](Game const& g) {
        auto const* v = villager_in(g);
        return v && v->actor.x < g.camera.x + 128 && v->actor.x + TILE > g.camera.x &&
               v->actor.y < g.camera.y + 64 && v->actor.y + TILE > g.camera.y;
    };

    // Pixel placement for talks, which need the player directly beside the villager.
    auto place_at = [&](uint16_t x, uint16_t y, DIRECTION facing) {
        Game g = d.state();
        g.player.x = x;
        g.player.y = y;
        g.player.facing = facing;
        g.player.walking = 0;
        d.set_state(g);
        d.advance(0, 800000);
    };

    // Leg one: the first talk sent it east. With the player standing still, the villager walks
    // out of view and stays in its slot, where it can walk no farther. It never teleports home.
    {
        Game g = d.state();
        require(g.npcs.residence[NPC_VILLAGER] == east, "talk did not send the villager east");
        auto const* v = villager_in(g);
        require(v && v->behavior == NPC_TRAVEL, "villager did not walk toward the east corner");
    }
    track_villager();
    unsigned walked = 0;
    while(villager_visible(d.state()) && walked < 900) {
        step_villager();
        ++walked;
    }
    require(walked > 30, "villager left view without walking");
    for(unsigned i = 0; i < 60; ++i)
        step_villager();
    require(villager_in(d.state()), "villager vanished after leaving view");
    require(d.state().npcs.residence[NPC_VILLAGER] == east, "villager lost its east residence");

    // The player walks back east. The villager resumes from where it stopped, not from home.
    uint16_t stopped_x = vx, stopped_y = vy;
    place_player(east.x - 1, east.y, EAST);
    track_villager();
    require(near_stop(stopped_x, stopped_y), "villager was not where it stopped");
    for(unsigned i = 0; i < 120; ++i)
        step_villager();
    d.capture(out / "npc-east.pgm");

    // Leg two: a talk where it stopped sends it west. It walks out of view on its own, stays in
    // its slot, and resumes from the same place when the player brings it back into view.
    {
        Game g = d.state();
        require(villager_in(g), "villager missing before the second talk");
    }
    place_at(vx, vy - TILE, SOUTH);
    d.tap(BTN_A);
    require(d.prompt() == DIALOGUE_TYPING, "talking to the villager did not open dialogue");
    close_villager_dialogue();
    require(d.state().npcs.residence[NPC_VILLAGER] == west,
            "second talk did not send the villager west");
    track_villager();
    walked = 0;
    while(villager_visible(d.state()) && walked < 900) {
        step_villager();
        ++walked;
    }
    require(walked > 30, "villager left view without walking west");
    for(unsigned i = 0; i < 60; ++i)
        step_villager();
    require(villager_in(d.state()), "villager vanished after leaving view on the way west");
    stopped_x = vx;
    stopped_y = vy;
    place_player(west.x + 2, west.y, EAST);
    track_villager();
    require(near_stop(stopped_x, stopped_y), "villager was not where it stopped");
    for(unsigned i = 0; i < 120; ++i)
        step_villager();
    d.capture(out / "npc-west.pgm");
    set_test_options(d);

    {
        FadeTrace trace(d);
        d.advance(0, 800000);
        d.tap(BTN_B);
        d.advance(0, 3200000);
        verify_pause_animation(trace, false);
    }
    auto menu_state = d.state();
    unsigned menu_serial = d.serial();
    d.advance(0, 3200000);
    auto after_wait = d.state();
    require(d.serial() == menu_serial && !memcmp(&menu_state, &after_wait, sizeof(Game)),
            "menu did not pause exploration");
    d.verify_music();
    d.capture(out / "menu.pgm");
    auto menu_pixels = pause_frame(d).pixels;
    require(pause_row_visible(d, 5) && !pause_row_visible(d, 6),
            "overworld menu did not show Map or hid an invalid save incorrectly");

    // Up wraps to Audio. Pass it to exercise the remaining placeholders.
    d.tap(BTN_UP);
    d.tap(BTN_UP); // Map
    d.tap(BTN_A);
    d.tap(BTN_UP); // Journal
    d.tap(BTN_A);
    d.tap(BTN_LEFT);
    d.tap(BTN_RIGHT);
    auto after_placeholders = d.state();
    require(d.serial() == menu_serial && !memcmp(&menu_state, &after_placeholders, sizeof(Game)),
            "pause placeholders advanced simulation");
    require(d.ab->core_state.cpu.timer1.ocrNa == 127, "Audio changed the audio rate");

    d.tap(BTN_UP); // Party
    d.tap(BTN_A);
    d.capture(out / "status.pgm");
    require(d.serial() == menu_serial, "Party advanced simulation");
    d.tap(BTN_B);
    auto after_party_pixels = pause_frame(d).pixels;
    for(unsigned i = 0; i < menu_pixels.size(); ++i)
        if(i % 128 >= 39)
            require(menu_pixels[i] == after_party_pixels[i],
                    "Party changed the paused world image");
    d.tap(BTN_UP); // Save
    d.tap(BTN_A);
    require(pause_row_visible(d, 6), "successful Save did not reveal Load immediately");
    d.capture(out / "menu-saved.pgm");
    auto saved = d.state();
    auto sector = d.sector();
    require((sector[0] | (uint16_t)sector[1] << 8) == sizeof(Game) && sector[2] == SAVE_VERSION &&
                !memcmp(sector.data() + 2, &saved, sizeof(Game)),
            "physical save did not contain the live Game object");
    // Closing with B reverses the slide and restores the world. Reopen and resume with A.
    {
        FadeTrace trace(d);
        d.tap(BTN_B);
        d.frame();
        verify_pause_animation(trace, true);
    }
    require(d.serial() != menu_serial, "B did not resume exploration");
    d.tap(BTN_B);
    d.advance(0, 1600000);
    menu_serial = d.serial();
    d.tap(BTN_A);
    d.frame();
    require(d.serial() != menu_serial, "Resume did not resume exploration");

    // Load restores persistent state and refreshes the cache before exploration resumes.
    Game changed = d.state();
    changed.gold = saved.gold + 123;
    changed.player = {(uint16_t)(positions::ElderDoor.x * TILE),
                      (uint16_t)((positions::ElderDoor.y + 1) * TILE), NORTH, 0};
    d.set_state(changed);
    d.advance(0, 1600000);
    d.tap(BTN_A);
    d.advance(0, 4800000);
    require(d.state().area == AREA_INTERIORS, "load fixture did not enter the house");
    open_audio(d, true);
    d.tap(BTN_LEFT);
    d.tap(BTN_DOWN);
    d.tap(BTN_LEFT);
    d.tap(BTN_DOWN);
    d.tap(BTN_RIGHT);
    verify_audio_options(d, 94, 77, 56);
    d.tap(BTN_B);
    d.frame();
    d.tap(BTN_B);
    d.advance(0, 1600000);
    require(pause_row_visible(d, 5) && !pause_row_visible(d, 6),
            "interior menu did not show Load while hiding Map");
    d.tap(BTN_UP); // Load
    {
        FadeTrace trace(d);
        d.tap(BTN_A);
        d.frame();
        verify_pause_animation(trace, true, true);
    }
    auto menu_loaded = d.state();
    require(menu_loaded.gold == saved.gold && menu_loaded.area == saved.area &&
                menu_loaded.player.x == saved.player.x && menu_loaded.player.y == saved.player.y &&
                menu_loaded.cache.origin_tx == saved.cache.origin_tx &&
                menu_loaded.cache.origin_ty == saved.cache.origin_ty,
            "pause Load did not restore the saved world");
    require(menu_loaded.options.music == 11 && menu_loaded.options.sfx == 9 &&
                menu_loaded.options.lowpass == 6,
            "Load overwrote options edited during this session");
    verify_audio_options(d, 94, 77, 56);
    d.capture(out / "menu-loaded.pgm");

    Device restarted(package, g, m, s, q);
    restarted.phase_addr = d.phase_addr;
    restarted.import_sector(sector);
    restarted.advance(0, 3200000);
    require(restarted.state().gold == 0, "startup validation did not restore new-game defaults");
    require(restarted.state().options.music == 12 && restarted.state().options.sfx == 10 &&
                restarted.state().options.lowpass == 5,
            "startup did not load saved options");
    verify_audio_options(restarted, 102, 85, 64);
    require(restarted.serial() == 0, "restart did not show title");
    require(!restarted.music_enabled(), "music started on the continue title");
    restarted.tap(BTN_DOWN);
    restarted.tap(BTN_A);
    auto loaded = restarted.state();
    require(restarted.serial() != 0 && loaded.area == saved.area &&
                loaded.player.x == saved.player.x && loaded.player.y == saved.player.y &&
                loaded.gold == saved.gold,
            "fresh emulator save/load mismatch");
    restarted.verify_music();
    restarted.capture(out / "restarted.pgm");
    restarted.hold(BTN_DOWN, 20);
    require(restarted.state().player.y > saved.player.y, "16x16 pixel movement failed");
    summary("pixel_scroll", restarted, 30, BTN_DOWN);
    summary("dense_collision", restarted, 30, BTN_LEFT);
    restarted.capture(out / "forest.pgm");

    Device new_with_save(package, g, m, s, q);
    new_with_save.import_sector(sector);
    new_with_save.advance(0, 3200000);
    new_with_save.tap(BTN_A);
    require(new_with_save.state().options.music == 12 && new_with_save.state().options.sfx == 10 &&
                new_with_save.state().options.lowpass == 5,
            "New Game reset startup options");
    verify_audio_options(new_with_save, 102, 85, 64);
    new_with_save.tap(BTN_B);
    new_with_save.advance(0, 1600000);
    require(pause_row_visible(new_with_save, 6),
            "starting a new game lost the save flag checked at startup");

    sector[2] ^= 0x40; // Invalid Game layout version; there is no checksum.
    Device corrupt(package, g, m, s, q);
    corrupt.import_sector(sector);
    corrupt.advance(0, 3200000);
    verify_audio_options(corrupt, 128, 128, 0);
    corrupt.tap(BTN_A); // Startup validation must already have hidden the invalid Load entry.
    corrupt.tap(BTN_B);
    corrupt.advance(0, 1600000);
    require(!pause_row_visible(corrupt, 6), "startup accepted an invalid save payload");
    corrupt.tap(BTN_B);
    corrupt.frame();

    Device invalid_continue(package, g, m, s, q);
    invalid_continue.import_sector(sector);
    invalid_continue.advance(0, 3200000);
    invalid_continue.tap(BTN_DOWN);
    invalid_continue.tap(BTN_A);
    require(invalid_continue.serial() == 0, "corrupt save accepted");
    require(!invalid_continue.music_enabled(), "failed continue started music");
    std::cout << "{\"result\":\"PASS\",\"scenario\":\"16x16 world, sign, menus, save/restart and "
                 "scrolling with the animated pause menu\"}\n";
    return 0;
} catch(std::exception const& error) {

    std::cerr << "FAIL " << error.what() << '\n';
    return 1;
}
#endif
