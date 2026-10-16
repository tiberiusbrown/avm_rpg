// Full-device integration, without changes to AVM or Ardens. Host allocations
// below belong to the QA tool; the guest remains fixed-capacity C++17.
#include "absim.hpp"

#pragma pack(push, 1)
#include "rpg.h"
#include "tile_ids.h"
#pragma pack(pop)

#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#include <stdexcept>

namespace fs = std::filesystem;

static_assert(sizeof(Game) == 181 + WorldFlags::BYTE_COUNT,
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
        std::ifstream f(package, std::ios::binary);
        auto error = ab->load_file(package.string().c_str(), f);
        if(!error.empty())
            throw std::runtime_error(error);

        ab->core_state.cpu.no_merged = true;
        ab->core_state.cpu.autobreaks.reset();
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
        require(dx >= 0 && dx < 17 && dy >= 0 && dy < 9, "QA tile is outside the live cache");
        return g.cache.tiles[dy * 17 + dx];
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

    uint8_t prompt() const {
        return proof ? uint8_t(DIALOGUE_CLOSED) : ab->core_state.cpu.data[dialogue_addr];
    }

    void advance(uint8_t held, uint64_t cycles = 800000) {
        buttons = held;
        uint64_t end = ab->core_state.cpu.cycle_count + cycles;
        while(ab->core_state.cpu.cycle_count < end)
            step();
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
                      << " cycles=" << ab->core_state.cpu.cycle_count << " ram_nonzero="
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
    }
};

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
                       "output_directory normal|m15|proof dialogue_address phase_address");

    fs::path package = argv[1], out = argv[5];
    fs::create_directories(out);
    unsigned g = std::stoul(argv[2]), m = std::stoul(argv[3]), s = std::stoul(argv[4]);
    unsigned q = std::stoul(argv[7]);
    Device d(package, g, m, s, q, std::string(argv[6]) == "proof");
    d.phase_addr = std::stoul(argv[8]);
    if(d.proof) {
        d.frame();
        d.frame();
        summary("m0_proof", d, 120);
        d.capture(out / "proof-visible.pgm");
        return 0;
    }

    d.advance(0, 3200000);
    require(d.serial() == 0, "title unexpectedly advanced exploration");
    d.capture(out / "title.pgm");
    d.tap(BTN_A);
    require(d.serial() != 0 && d.state().ticks != 0, "new game did not start exploration");
    if(std::string(argv[6]) == "m15") {
        auto changed = d.state();
        changed.player = {112, 16, EAST, 0};
        d.set_state(changed);
        d.frame();
        require(d.cached_tile(15, 2) == TILE_WALL, "gate baseline was not closed");
        d.capture(out / "gate-closed.pgm");
        auto closed = d.ab->peripherals.display.ram;
        d.hold(BTN_RIGHT, 4);
        require(d.state().player.x == 113, "closed gate did not block pixel collision");
        changed = d.state();
        changed.flags = {};
        changed.set_flag(FLAG_GATE_OPEN);
        d.set_state(changed);
        d.frame();
        require(d.cached_tile(15, 2) == TILE_PATH, "gate flag did not project next frame");
        d.capture(out / "gate-open.pgm");
        require(d.ab->peripherals.display.ram != closed, "effective gate did not change graphics");
        d.hold(BTN_RIGHT, 6);
        require(d.state().player.x > 120, "open gate still blocked movement");
        changed = d.state();
        changed.player = {120, 16, EAST, 0};
        changed.flags = {};
        d.set_state(changed);
        summary("occupied_gate_closing", d, 1);
        require(d.cached_tile(15, 2) == TILE_WALL, "baseline reset left a stale gate mutation");
        require(d.state().player.x != 120 || d.state().player.y != 16,
                "closing gate failed deterministic player ejection");

        changed = d.state();
        changed.player = {200, 72, EAST, 0};
        changed.flags = {};
        changed.set_flag(FLAG_BRIDGE_REPAIRED);
        d.set_state(changed);
        d.frame();
        require(d.state().cache.origin_tx == 16 && d.cached_tile(16, 7) == TILE_PATH &&
                    d.cached_tile(17, 8) == TILE_PATH,
                "partial multi-chunk bridge did not project into the live cache");
        d.capture(out / "bridge-repaired.pgm");
        summary("partial_dynamic_bridge", d, 120);
        changed = d.state();
        changed.flags = {};
        d.set_state(changed);
        d.frame();
        require(d.cached_tile(16, 7) == TILE_WATER, "bridge baseline failed to reset");
        d.capture(out / "bridge-baseline.pgm");

        changed = d.state();
        changed.player = {208, 144, NORTH, 0};
        changed.ticks = 0;
        d.set_state(changed);
        d.frame();
        require(d.cached_tile(21, 16) == TILE_PATH, "per-frame platform failed its open phase");
        d.capture(out / "platform-open.pgm");
        for(unsigned i = 0; i < 16; ++i)
            d.frame();
        require(d.cached_tile(21, 16) == TILE_WATER,
                "per-frame platform did not return to baseline");
        d.capture(out / "platform-closed.pgm");
        summary("periodic_platform", d, 120);

        changed = d.state();
        changed.player = {92, 40, EAST, 0};
        changed.flags = {};
        changed.set_flag(FLAG_GATE_OPEN);
        changed.set_flag(FLAG_BRIDGE_REPAIRED);
        changed.previous_player_tile = {AREA_OUTDOOR, 11, 5};
        changed.player.walking = 1;
        d.set_state(changed);
        d.frame();
        require(d.state().area == AREA_INTERIOR, "portal transition did not enter the interior");
        d.tap(BTN_B);
        d.tap(BTN_DOWN);
        d.tap(BTN_A);
        auto sector = d.sector();
        Device restarted(package, g, m, s, q);
        restarted.import_sector(sector);
        restarted.advance(0, 3200000);
        restarted.tap(BTN_DOWN);
        restarted.tap(BTN_A);
        require(restarted.state().get_flag(FLAG_GATE_OPEN) &&
                    restarted.state().get_flag(FLAG_BRIDGE_REPAIRED) &&
                    restarted.state().area == AREA_INTERIOR,
                "fresh-device save/load lost quest flags or area");
        restarted.capture(out / "quest-reloaded.pgm");
        std::cout
            << "{\"result\":\"PASS\",\"scenario\":\"M1.5 gate/bridge/timer/physical-save\"}\n";
        return 0;
    }

    summary("stationary_world", d, 120);
    d.frame(BTN_RIGHT);
    require(d.state().player.x == 34, "player did not advance through intermediate pixels");
    d.hold(BTN_RIGHT, 29);
    require(d.state().area == AREA_INTERIOR, "door entry failed");
    d.capture(out / "interior.pgm");

    d.hold(BTN_RIGHT, 16);
    d.hold(BTN_UP, 8);
    d.capture(out / "before-dialogue.pgm");
    auto world_screen = d.ab->peripherals.display.ram;
    d.frame(BTN_A);
    require(d.prompt() == DIALOGUE_MORE, "Lantern did not open the paginated overlay");
    auto frozen = d.state();
    auto first_screen = d.ab->peripherals.display.ram;
    for(unsigned i = 0; i < 128 * 4; ++i) {
        uint8_t mask = i < 128 * 3 ? 0xff : 0x3f;
        require((first_screen[i] & mask) == (world_screen[i] & mask),
                "dialogue overlay overwrote the world above its fixed panel");
    }
    d.hold(BTN_A, 8);
    require(d.prompt() == DIALOGUE_MORE && d.ab->peripherals.display.ram == first_screen,
            "held opening A skipped a dialogue page");
    d.hold(BTN_RIGHT, 20);
    auto paused = d.state();
    require(paused.ticks == frozen.ticks && paused.rng == frozen.rng &&
                !memcmp(&paused.player, &frozen.player, sizeof(Actor)),
            "dialogue advanced the world");
    d.capture(out / "dialogue.pgm");
    d.tap(BTN_B);
    require(d.prompt() == DIALOGUE_CLOSED && !d.state().get_flag(FLAG_HELPED),
            "early dialogue dismissal did not return to its caller");

    d.tap(BTN_A);
    unsigned pages = 1;
    while(d.prompt() == DIALOGUE_MORE && pages < 10) {
        d.tap(BTN_A);
        d.capture(out / ("dialogue-page-" + std::to_string(pages++) + ".pgm"));
    }
    require(pages > 1 && d.prompt() == DIALOGUE_DONE, "automatic dialogue pagination failed");
    d.tap(BTN_A);
    require(d.prompt() == DIALOGUE_CHOICE, "callable question did not follow the message");
    d.capture(out / "choice.pgm");
    d.tap(BTN_A);
    require(d.state().get_flag(FLAG_HELPED) && d.state().gold == 5 && d.prompt() == DIALOGUE_DONE,
            "synchronous choice consequence failed");
    d.capture(out / "consequence.pgm");

    d.tap(BTN_A);
    require(d.prompt() == DIALOGUE_CLOSED, "dialogue did not return");
    d.hold(BTN_LEFT, 16);
    d.hold(BTN_DOWN, 9);
    require(d.state().area == AREA_OUTDOOR, "return path failed");
    {
        uint64_t sum = 0, worst = 0;
        for(auto c : d.transitions) {
            sum += c.total;
            worst = std::max(worst, c.total);
        }
        require(d.transitions.size() == 2, "expected paired portal transitions");
        std::cout << "{\"workload\":\"area_transitions\",\"frames\":2,\"average_cycles\":"
                  << sum / 2 << ",\"worst_cycles\":" << worst << "}\n";
    }

    d.tap(BTN_B);
    auto menu_state = d.state();
    unsigned menu_serial = d.serial();
    d.advance(0, 3200000);
    auto after_menu_wait = d.state();
    require(d.serial() == menu_serial && !memcmp(&menu_state, &after_menu_wait, sizeof(Game)),
            "menu did not pause exploration");
    d.capture(out / "menu.pgm");
    auto menu_pixels = d.ab->peripherals.display.ram;
    d.advance(BTN_A, 4000000);
    auto after_status_wait = d.state();
    require(d.serial() == menu_serial && !memcmp(&menu_state, &after_status_wait, sizeof(Game)),
            "status did not pause exploration or held A returned through multiple screens");
    require(d.ab->peripherals.display.ram != menu_pixels, "status did not draw its own screen");
    d.capture(out / "status.pgm");

    d.tap(BTN_B);
    d.tap(BTN_DOWN);
    d.tap(BTN_A);
    d.capture(out / "saved.pgm");

    auto saved = d.state();
    auto sector = d.sector();
    require(sector[0] == SAVE_BYTES && sector[2] == 'R' && sector[3] == 'P',
            "no physical FX save record");
    {
        std::ofstream f(out / "fxsave.bin", std::ios::binary);
        f.write(reinterpret_cast<char const*>(sector.data()), sector.size());
    }
    Device restarted(package, g, m, s, q);
    restarted.import_sector(sector);
    restarted.advance(0, 3200000);
    require(restarted.serial() == 0 && restarted.state().get_flag(FLAG_HELPED) &&
                restarted.state().gold == 5 && restarted.state().rng == saved.rng,
            "title did not validate the saved game");
    restarted.tap(BTN_DOWN);
    restarted.tap(BTN_A);
    auto loaded = restarted.state();
    require(restarted.serial() != 0 && loaded.area == saved.area &&
                loaded.player.x == saved.player.x && loaded.player.y == saved.player.y &&
                loaded.get_flag(FLAG_HELPED) && loaded.gold == 5 && loaded.rng == saved.rng,
            "fresh emulator save/load mismatch");
    restarted.capture(out / "restarted.pgm");
    restarted.hold(BTN_UP, 4);
    restarted.hold(BTN_RIGHT, 16);
    restarted.hold(BTN_UP, 8);
    restarted.tap(BTN_A);
    require(restarted.prompt() == DIALOGUE_DONE && restarted.state().get_flag(FLAG_HELPED),
            "persisted dialogue inaccessible");
    restarted.capture(out / "persisted-dialogue.pgm");
    restarted.tap(BTN_B);
    require(restarted.prompt() == DIALOGUE_CLOSED, "B did not dismiss the final page");

    // Corrupt physical payload with unchanged record length; startup must reject.
    sector[8] ^= 0x40;
    Device corrupt(package, g, m, s, q);
    corrupt.import_sector(sector);
    corrupt.advance(0, 3200000);
    corrupt.tap(BTN_DOWN);
    corrupt.tap(BTN_A);
    require(corrupt.serial() == 0 && corrupt.state().ticks == 0 &&
                !corrupt.state().get_flag(FLAG_HELPED),
            "corrupt save accepted or disabled Continue entered exploration");

    // Representative pixel scrolling and dense terrain, after closing the menu.
    d.tap(BTN_B);
    require(d.serial() > menu_serial, "B did not return from menu to exploration");
    d.tap(BTN_B);
    unsigned return_serial = d.serial();
    d.tap(BTN_UP);
    d.advance(BTN_A, 4000000);
    require(d.serial() > return_serial && d.prompt() == DIALOGUE_CLOSED,
            "Return did not resume exploration or its held A leaked into gameplay");
    d.advance(0);

    summary("pixel_scroll", d, 70, BTN_DOWN);
    summary("dense_collision", d, 120, BTN_LEFT);
    d.capture(out / "outdoor.pgm");
    std::cout << "{\"result\":\"PASS\",\"scenario\":\"actual AVM "
                 "new/walk/enter/dialogue/choice/exit/save/fresh-device/load/corrupt\"}\n";
    return 0;
} catch(std::exception const& e) {

    std::cerr << "FAIL " << e.what() << '\n';
    return 1;
}
