// Full-device integration, without changes to AVM or Ardens. Host allocations
// below belong to the QA tool; the guest remains fixed-capacity C++20.
#include "absim.hpp"
#include "avm_spi_observer.hpp"

#pragma pack(push, 1)
#include "rpg.h"
#include "tile_ids.h"
#pragma pack(pop)

#include <filesystem>
#include <array>
#include <fstream>
#include <iostream>
#include <numeric>
#include <stdexcept>

namespace fs = std::filesystem;

static_assert(sizeof(Game) == 73 + WorldFlags::BYTE_COUNT + sizeof(NpcPool),
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
    uint8_t previous_phase = 0;
    uint64_t phase_begin = 0;
    Cost stages;
    uint8_t buttons = 0, previous_marker = 0;
    bool started = false;
    bool proof = false;
    uint64_t begin = 0, render_begin = 0;
    Cost last;
    std::vector<Cost> frames;
    std::vector<Cost> transitions;

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
        pins();
        ab->cycle();
        ab->core_state.cpu.update_all();

        auto const& cpu = ab->core_state.cpu;
        require(!cpu.should_autobreak(), "native interpreter fault");

        uint8_t marker = cpu.data[marker_addr];
        if(marker != previous_marker) {
            if(marker == 1) {
                begin = cpu.cycle_count;
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
                require(last.total <= 800000, "exploration frame exceeded 800000-cycle budget");
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
        return (ab->core_state.cpu.data[absim::reg::addr::TIMSK3] & 2) != 0;
    }

    void verify_music() {
        require(music_enabled(), "music sample timer is disabled");
        uint8_t low = 255, high = 0;
        buttons = 0;
        // Cover a full 480 ms beat: the detached score has intentional rests.
        uint64_t end = ab->core_state.cpu.cycle_count + 8000000;
        while(ab->core_state.cpu.cycle_count < end) {
            step();
            auto sample = ab->core_state.cpu.data[absim::reg::addr::OCR4A];
            low = std::min(low, sample);
            high = std::max(high, sample);
        }
        require(low < 128 && high > 128, "music produced no audible PWM waveform");
        require(low != 0 && high != 255, "peaceful music clipped the PWM output");
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
    fixture.player = {104, 88, SOUTH, 0};
    fixture.npcs = {};
    // Deliberately reverse the NPC slots relative to depth, with the player between.
    fixture.npcs.actors[0].id = NPC_VILLAGER;
    fixture.npcs.actors[0].actor = {116, 92, SOUTH, 0};
    fixture.npcs.actors[1].id = NPC_WANDERER;
    fixture.npcs.actors[1].actor = {112, 80, SOUTH, 0};
    d.set_state(fixture);
    d.frame();
    d.frame();
    Game rendered = d.state();
    require(rendered.player.y == 88 && rendered.npcs.actors[0].actor.y == 92 &&
                rendered.npcs.actors[1].actor.y == 80,
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
    require(worst <= 800000, "exploration frame exceeded 800000-cycle budget");
    std::cout << "{\"workload\":\"" << name << "\",\"frames\":" << n
              << ",\"average_cycles\":" << sum / n << ",\"worst_cycles\":" << worst
              << ",\"average_render_display_cycles\":" << render / n << ",\"observed_fps\":" << fps
              << ",\"average_reload_cycles\":" << reload / n
              << ",\"worst_reload_cycles\":" << reload_worst
              << ",\"average_chunk_cycles\":" << chunks / n
              << ",\"worst_chunk_cycles\":" << chunks_worst
              << ",\"average_collision_cycles\":" << collision / n
              << ",\"worst_collision_cycles\":" << collision_worst
              << ",\"budget_cycles\":800000}\n";
}

int main(int argc, char** argv) try {
    require(argc == 9, "device_tests package game_address marker_address serial_address "
                       "output_directory normal|fxc|proof dialogue_address phase_address");
    fs::path package = argv[1], out = argv[5];
    fs::create_directories(out);
    unsigned g = std::stoul(argv[2]), m = std::stoul(argv[3]), s = std::stoul(argv[4]);
    unsigned q = std::stoul(argv[7]);
    Device d(package, g, m, s, q, std::string(argv[6]) == "proof");
    d.phase_addr = std::stoul(argv[8]);
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
    d.verify_music();
    require(d.state().player.x == 112 && d.state().player.y == 80, "wrong 16x16 spawn");
    require(d.state().dimensions.width_tiles == 18 && d.state().dimensions.height_tiles == 12,
            "wrong sample map dimensions");
    require(d.cached_tile(7, 4) == TILE_SIGN, "sign atlas index changed");
    unsigned actors = 0;
    for(auto const& n : d.state().npcs.actors)
        actors += n.id != NPC_NONE;
    require(actors == 2, "sample NPCs did not activate");
    verify_actor_layers(d, out);
    summary("idle_grove", d, 30);
    d.capture(out / "grove.pgm");

    d.tap(BTN_A);
    require(d.prompt() != DIALOGUE_CLOSED, "facing sign plus A did not open dialogue");
    auto frozen = d.state();
    unsigned dialogue_serial = d.serial();
    d.advance(0, 3200000);
    auto paused = d.state();
    require(dialogue_serial == d.serial() && !memcmp(&frozen, &paused, sizeof(Game)),
            "sign dialogue did not freeze simulation");
    d.verify_music();
    d.capture(out / "sign-dialogue.pgm");
    for(unsigned i = 0; i < 8 && d.prompt() != DIALOGUE_CLOSED; ++i)
        d.tap(BTN_A);
    require(d.prompt() == DIALOGUE_CLOSED, "sign dialogue did not return to exploration");

    d.tap(BTN_B);
    auto menu_state = d.state();
    unsigned menu_serial = d.serial();
    d.advance(0, 3200000);
    auto after_wait = d.state();
    require(d.serial() == menu_serial && !memcmp(&menu_state, &after_wait, sizeof(Game)),
            "menu did not pause exploration");
    d.verify_music();
    d.capture(out / "menu.pgm");
    d.tap(BTN_A);
    d.capture(out / "status.pgm");
    require(d.serial() == menu_serial, "status advanced simulation");
    d.tap(BTN_B);
    d.tap(BTN_DOWN);
    d.tap(BTN_A);
    auto saved = d.state();
    auto sector = d.sector();
    require(sector[0] == SAVE_BYTES && sector[2] == 'R' && sector[3] == 'P' &&
                sector[4] == SAVE_VERSION,
            "no physical version-4 FX save record");
    Device restarted(package, g, m, s, q);
    restarted.phase_addr = d.phase_addr;
    restarted.import_sector(sector);
    restarted.advance(0, 3200000);
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

    sector[8] ^= 0x40;
    Device corrupt(package, g, m, s, q);
    corrupt.import_sector(sector);
    corrupt.advance(0, 3200000);
    corrupt.tap(BTN_DOWN);
    corrupt.tap(BTN_A);
    require(corrupt.serial() == 0, "corrupt save accepted");
    require(!corrupt.music_enabled(), "failed continue started music");
    std::cout << "{\"result\":\"PASS\",\"scenario\":\"16x16 world, sign, menus, save/restart and "
                 "scrolling with peaceful music\"}\n";
    return 0;
} catch(std::exception const& error) {

    std::cerr << "FAIL " << error.what() << '\n';
    return 1;
}
