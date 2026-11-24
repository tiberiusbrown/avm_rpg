// Full-device integration, without changes to AVM or Ardens. Host allocations
// below belong to the QA tool; the guest remains fixed-capacity C++20.
#include "absim.hpp"
#include "avm_spi_observer.hpp"

#pragma pack(push, 1)
#include "rpg.h"
#include "pause_menu.h"
#include "soundscape.h"
#include "map_positions.h"
#include "world_assets.h"
#include "tile_ids.h"
#pragma pack(pop)
// BattleUnit uses byte-sized bit-field allocation units; packing it would change those offsets.
#include "battle.h"
#include "runtime_workspace.h"
#include "battle_fixtures.h"
#include "logic/area_village.h"
#include "music_catalog.h"

#include <filesystem>
#include <array>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string_view>

namespace fs = std::filesystem;

// Functional diagnostics may continue past measured timing failures. The
// default device suite still enforces every existing frame budget.
// RPG_NO_TIMING_CHECK keeps a manual measurement going past a frame that exceeds its budget.
static bool check_timing = std::getenv("RPG_NO_TIMING_CHECK") == nullptr;
static uint64_t worst_exploration_cycles;

static_assert(sizeof(Game) == 23 + sizeof(Party) + sizeof(Supplies) + WorldFlags::BYTE_COUNT + sizeof(NpcPool) + sizeof(AudioOptions) + sizeof(QUEST_ID),
              "update QA reader if the guest state layout changes");

static void require(bool ok, char const* message) {
    if(!ok)
        throw std::runtime_error(message);
}

struct Cost {
    uint64_t total = 0, render = 0, begin = 0, end = 0;
    uint64_t reload = 0, chunks = 0, collision = 0;
};

// Optional emulator-only tracing; no guest instructions are added.
// Exact PC attribution uses single AVR stepping; marker-only tracing preserves ordinary stepping.
// The interpreter boundary is checked against interp-boundary.json by the driver.
// The guest's runtime workspace address, from the command line; see main.
static unsigned workspace_address_arg;

// The saved Game, and the exploration view that the runtime workspace holds beside it.
struct Snapshot : Game {
    ExplorationWorkspace view;
};

struct FrameTrace {
    static inline unsigned next_instance;
    unsigned instance = next_instance++, sequence = 0, active_pc = 0;
    fs::path directory;
    std::ofstream frames;
    std::vector<uint64_t> cycles;
    std::array<bool, 256> present{};
    unsigned activated = 0, retired = 0, offers = 0, site_checks = 0;
    std::vector<std::pair<unsigned, unsigned>> events;
    std::vector<unsigned> region_ids;
    std::array<uint64_t, 22> regions{};
    unsigned active_region = 0;
    uint64_t worst = 0;
    bool instruction_profile = std::getenv("RPG_FRAME_COARSE") == nullptr;
    Snapshot initial{};

    FrameTrace() {
        if(auto path = std::getenv("RPG_FRAME_TRACE")) {
            directory = path;
            fs::create_directories(directory);
            frames.open(directory / ("frames-" + std::to_string(instance) + ".jsonl"));
            ensure_pc(65535);
            if(auto file = std::getenv("RPG_FRAME_EVENTS")) {
                std::ifstream input(file);
                unsigned pc, sites, offer, region;
                while(input >> pc >> sites >> offer >> region)
                    if(region < regions.size()) {
                        ensure_pc(pc);
                        events[pc] = {sites, offer};
                        region_ids[pc] = region;
                    }
            }
        }
    }

    explicit operator bool() const {
        return frames.is_open();
    }

    void ensure_pc(unsigned pc) {
        require(pc < (1u << 24), "frame trace PC exceeds AVM code address range");
        if(pc < cycles.size())
            return;
        cycles.resize(pc + 1);
        events.resize(pc + 1);
        region_ids.resize(pc + 1);
    }

    void begin(Snapshot const& g) {
        initial = g;
        std::fill(cycles.begin(), cycles.end(), 0);
        regions.fill(0);
        present.fill(false);
        for(auto const& n : g.npcs.actors)
            if(n.id)
                present[(uint8_t)n.id] = true;
        activated = retired = offers = site_checks = 0;
    }

    void boundary(Snapshot const& g, unsigned pc) {
        active_pc = pc;
        ensure_pc(pc);
        if(pc < events.size()) {
            active_region = region_ids[pc];
            site_checks += events[pc].first;
            offers += events[pc].second;
        }
        std::array<bool, 256> now{};
        for(auto const& n : g.npcs.actors)
            if(n.id)
                now[(uint8_t)n.id] = true;
        for(unsigned id = 1; id < NPC_ID_LIMIT; ++id) {
            activated += now[id] && !present[id];
            retired += !now[id] && present[id];
        }
        present = now;
    }

    void finish(Snapshot const& g, unsigned serial, Cost const& cost, bool exploration) {
        frames << "{\"instance\":" << instance << ",\"sequence\":" << sequence
               << ",\"serial\":" << serial << ",\"cycles\":" << cost.total
               << ",\"instruction_profile\":" << instruction_profile
               << ",\"exploration\":" << exploration << ",\"render\":" << cost.render
               << ",\"reload\":" << cost.reload << ",\"chunks\":" << cost.chunks
               << ",\"actors_collision\":" << cost.collision << ",\"area\":" << unsigned(g.area)
               << ",\"player\":[" << g.player.x << ',' << g.player.y << "],\"camera\":["
               << g.view.camera.x << ',' << g.view.camera.y << "],\"cache\":[" << unsigned(g.view.cache.origin_tx)
               << ',' << unsigned(g.view.cache.origin_ty) << "],\"initial_camera\":[" << initial.view.camera.x
               << ',' << initial.view.camera.y << "],\"initial_cache\":["
               << unsigned(initial.view.cache.origin_tx) << ',' << unsigned(initial.view.cache.origin_ty)
               << "],\"activations\":" << (instruction_profile ? std::to_string(activated) : "null")
               << ",\"retirements\":" << (instruction_profile ? std::to_string(retired) : "null")
               << ",\"site_checks\":"
               << (instruction_profile ? std::to_string(site_checks) : "null")
               << ",\"candidate_offers\":"
               << (instruction_profile ? std::to_string(offers) : "null") << ",\"actors\":[";
        bool comma = false;
        for(auto const& n : g.npcs.actors) {
            if(!n.id)
                break;
            if(comma)
                frames << ',';
            comma = true;
            frames << '[' << unsigned(n.id) << ',' << n.actor.x << ',' << n.actor.y << ','
                   << unsigned(n.behavior) << ',' << unsigned(n.state) << ']';
        }
        frames << "],\"regions\":[";
        for(unsigned i = 0; i < regions.size(); ++i) {
            if(i)
                frames << ',';
            frames << regions[i];
        }
        frames << "]}\n";
        if(instruction_profile && exploration &&
           (cost.total > worst || cost.total >= 310000 || serial == 74 || serial == 180 ||
            std::getenv("RPG_FRAME_ALL"))) {
            std::ofstream out(directory / ("pcs-" + std::to_string(instance) + "-" +
                                           std::to_string(sequence) + ".csv"));
            out << "pc,cycles\n";
            for(unsigned pc = 0; pc < cycles.size(); ++pc)
                if(cycles[pc])
                    out << pc << ',' << cycles[pc] << '\n';
        }
        if(exploration)
            worst = std::max(worst, cost.total);
        ++sequence;
    }
};

struct Device {
    std::unique_ptr<absim::arduboy_t> ab = std::make_unique<absim::arduboy_t>();
    unsigned game_addr, marker_addr, serial_addr, dialogue_addr, phase_addr = 0;
    unsigned battle_pointer_addr = 0, battle_ready_addr = 0, panel_addr = 0;
    // The runtime workspace, which holds the exploration view. Every device in this process reads it.
    unsigned workspace_addr = workspace_address_arg;
    uint8_t previous_phase = 0;
    uint64_t phase_begin = 0;
    Cost stages;
    FrameTrace trace;
    uint8_t buttons = 0, previous_marker = 0;
    bool started = false;
    bool proof = false;
    uint8_t frame_area = 0;
    uint64_t begin = 0, render_begin = 0;
    // Journal redraws, from the QA marks the Journal sets. They are not frames.
    uint64_t journal_begin = 0, journal_render = 0;
    unsigned journal_redraws = 0;
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

    Snapshot state() const {
        require(workspace_addr || proof, "missing runtime workspace address");
        Snapshot g{};
        memcpy(&g, &ab->core_state.cpu.data[game_addr], sizeof(Game));
        if(workspace_addr)
            memcpy(&g.view, &ab->core_state.cpu.data[workspace_addr], sizeof(g.view));
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
        Snapshot g = state();
        int dx = int(x) - g.view.cache.origin_tx, dy = int(y) - g.view.cache.origin_ty;
        require(dx >= 0 && dx < CACHE_WIDTH && dy >= 0 && dy < CACHE_HEIGHT,
                "QA tile is outside the live cache");
        return g.view.cache.tiles[dy * CACHE_WIDTH + dx];
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
        // Display refreshes call advance(0), which resets no_merged. Exact PC
        // attribution needs single AVR instructions throughout diagnostic runs.
        if(trace && trace.instruction_profile)
            ab->core_state.cpu.no_merged = true;
        auto const& before_cpu = ab->core_state.cpu;
        uint64_t before_cycle = before_cpu.cycle_count;
        if(trace && trace.instruction_profile && previous_marker) {
            unsigned pc = before_cpu.pc;
            if(pc >= 256 && pc < 1280 && (pc - 256) % 4 == 0)
                trace.boundary(state(), before_cpu.data[4] | unsigned(before_cpu.data[5]) << 8 |
                                            unsigned(before_cpu.data[6]) << 16);
        }
        ab->cycle();
        ab->core_state.cpu.update_all();
        if(cycle_observer)
            cycle_observer(observer_context, true);

        auto const& cpu = ab->core_state.cpu;
        require(!cpu.should_autobreak(), "native interpreter fault");
        if(trace && trace.instruction_profile && previous_marker &&
           trace.active_pc < trace.cycles.size()) {
            uint64_t elapsed = cpu.cycle_count - before_cycle;
            trace.cycles[trace.active_pc] += elapsed;
            trace.regions[trace.active_region] += elapsed;
        }

        uint8_t marker = cpu.data[marker_addr];
        if(marker != previous_marker) {
            if(marker == 1) {
                begin = cpu.cycle_count;
                frame_area = proof ? 0 : state().area;
                stages = {};
                if(trace)
                    trace.begin(state());
                previous_phase = 0;
                phase_begin = begin;
                if(proof || state().party.count)
                    started = true;
            }
            if(marker == 2)
                render_begin = cpu.cycle_count;
            if(marker == 5)
                journal_begin = cpu.cycle_count;
            if(marker == 6) {
                journal_render = cpu.cycle_count - journal_begin;
                ++journal_redraws;
            }
            if(marker == 0 && previous_marker == 3 && begin) {
                last = {cpu.cycle_count - begin, cpu.cycle_count - render_begin, begin,
                        cpu.cycle_count};
                last.reload = stages.reload;
                last.chunks = stages.chunks;
                last.collision = stages.collision;
                // Cross-area updates now include a blocking fade. FadeTrace checks each of its
                // display frames; ordinary exploration still has the single-frame CPU budget.
                if(check_timing && (proof || state().area == frame_area) && last.total > 320000)
                    std::cerr << "Over-budget frame: total=" << last.total
                              << " render=" << last.render << " reload=" << last.reload
                              << " chunks=" << last.chunks << " actors/collision=" << last.collision
                              << " serial=" << serial() << " area=" << unsigned(frame_area)
                              << " player=" << state().player.x << ',' << state().player.y
                              << '\n';
                if(proof || state().area == frame_area)
                    worst_exploration_cycles = std::max(worst_exploration_cycles, last.total);
                require(!check_timing || (!proof && state().area != frame_area) ||
                            last.total <= 320000,
                        "exploration frame exceeded the 320000-cycle budget");
                if(trace)
                    trace.finish(state(), serial(), last, proof || state().area == frame_area);
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

    void wait_for_battle(unsigned previous_activations = 256) {
        require(battle_ready_addr, "missing battle QA readiness address");
        buttons = 0;
        uint64_t deadline = ab->core_state.cpu.cycle_count + 120000000;
        for(;;) {
            auto phase = ab->core_state.cpu.data[battle_ready_addr];
            if(phase != BATTLE_QA_BUSY &&
               (previous_activations == 256 || battle_state().activations != previous_activations))
                return;
            if(ab->core_state.cpu.cycle_count >= deadline) {
                auto state = battle_state();
                std::cerr << "battle wait: phase=" << unsigned(phase)
                          << " previous=" << previous_activations
                          << " activations=" << unsigned(state.activations)
                          << " turn=" << unsigned(state.turn) << " ended=" << state.ended
                          << " prompt=" << unsigned(prompt()) << '\n';
                require(false, "battle readiness timeout");
            }
            step();
        }
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

        // The guest writes the serial one byte at a time. At a carry, observing each
        // byte separately can count the same frame twice; the completed marker cannot.
        auto old_frames = frames.size();
        uint8_t previous_area = proof ? 0 : state().area;
        do {
            step();
            require(ab->core_state.cpu.cycle_count < deadline, "frame timeout");
            if(prompt() != DIALOGUE_CLOSED)
                return;
        } while(frames.size() == old_frames);
        // Marker zero precedes the serial increment. Return only after its full
        // value is visible, so callers also observe the completed frame's serial.
        unsigned completed_serial = (serial() + 1) & 0xFFFF;
        while(serial() != completed_serial) {
            step();
            require(ab->core_state.cpu.cycle_count < deadline, "frame serial timeout");
        }
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

    void dismiss_dialogue(bool resume_exploration = true) {
        for(unsigned page = 0; page < 16; ++page) {
            wait_for_prompt();
            auto current = prompt();
            require(current == DIALOGUE_MORE || current == DIALOGUE_DONE,
                    "expected a text dialogue prompt");
            tap(BTN_A);
            if(current == DIALOGUE_DONE) {
                require(prompt() == DIALOGUE_CLOSED, "dialogue did not close");
                // Closing animation finishes before the next exploration update.
                if(resume_exploration)
                    frame();
                return;
            }
        }
        throw std::runtime_error("dialogue exceeded the page bound");
    }

    void capture(fs::path const& path, bool wait_for_scanout = false) {
        // A new scene or dialogue panel may still be scanning out.
        if(wait_for_scanout || prompt() != DIALOGUE_CLOSED)
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
    fixture.npcs.actors[0].id = NPC_TESSA;
    fixture.npcs.actors[0].assigned_site = positions::TessaWell;
    fixture.npcs.actors[0].actor = {(uint16_t)(spawn_x + 4), (uint16_t)(spawn_y + 12), SOUTH, 0};
    fixture.npcs.actors[1].id = NPC_ORRIN;
    fixture.npcs.actors[1].assigned_site = positions::OrrinLane;
    fixture.npcs.actors[1].actor = {spawn_x, spawn_y, SOUTH, 0};
    d.set_state(fixture);
    d.frame();
    d.frame();
    Snapshot rendered = d.state();
    require(rendered.player.y == spawn_y + 8 && rendered.npcs.actors[0].actor.y == spawn_y + 12 &&
                rendered.npcs.actors[1].actor.y == spawn_y,
            "depth fixture unexpectedly moved");

    std::array<uint8_t, 1024> expected{}, covered{}, old_order{};
    auto compose = [&](std::array<uint8_t, 1024>& pixels, Actor const& actor, uint8_t row) {
        uint16_t frame = (uint16_t)row * 12 + SOUTH * 3;
        uint8_t const* bytes = ACTOR_SPRITES + 2 + frame * 64;
        int left = int(actor.x) - rendered.view.camera.x, top = int(actor.y) - rendered.view.camera.y;
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
    compose(expected, rendered.npcs.actors[1].actor, NpcContent<NPC_ORRIN>::definition.sprite_row);
    compose(expected, rendered.player, 0);
    compose(expected, rendered.npcs.actors[0].actor, NpcContent<NPC_TESSA>::definition.sprite_row);
    compose(old_order, rendered.npcs.actors[0].actor, NpcContent<NPC_TESSA>::definition.sprite_row);
    compose(old_order, rendered.npcs.actors[1].actor, NpcContent<NPC_ORRIN>::definition.sprite_row);
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
                      << " camera=" << rendered.view.camera.x << ',' << rendered.view.camera.y << '\n';
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
    require(!check_timing || worst <= 320000, "exploration frame exceeded the 320000-cycle budget");
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
        require(!check_timing || worst < 336000, "battle transition missed its 50 Hz frame budget");
        std::cout << "{\"workload\":\"" << name << "\",\"fade_frames\":16,"
                  << "\"worst_frame_cycles\":" << worst << ",\"budget_cycles\":320000}\n";
    }
};

// Observe display cadence independently of pixel contents, layout and animation length.
struct PauseTiming {
    struct Presentation {
        uint64_t cycle;
        bool exploration;
    };

    Device& device;
    std::vector<Presentation> presentations;
    Presentation pending{};
    unsigned bytes = 0;
    uint64_t previous_transfer = 0;
    uint64_t begin;
    bool synchronized;

    explicit PauseTiming(Device& d)
        : device(d), begin(d.ab->core_state.cpu.cycle_count),
          synchronized((d.ab->core_state.cpu.data[absim::reg::addr::PORTD] & 0x40) != 0) {
        require(!d.cycle_observer, "another device observer is attached");
        d.observer_context = this;
        d.cycle_observer = [](void* context, bool after) {
            static_cast<PauseTiming*>(context)->observe(after);
        };
    }

    ~PauseTiming() {
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
        if(port & 0x40) {
            synchronized = true;
            bytes = 0;
        }
        if(!synchronized || !cpu.spi_busy ||
           cpu.spi_transmit_zero_cycle == previous_transfer || (port & 0x50) != 0x10)
            return;
        if(!bytes)
            pending = {cpu.cycle_count, cpu.data[device.marker_addr] != 0};
        if(++bytes == 1024) {
            presentations.push_back(pending);
            bytes = 0;
        }
    }

    void verify(char const* name) const {
        uint64_t worst = 0;
        unsigned intervals = 0;
        for(size_t i = 1; i < presentations.size(); ++i) {
            // Resumed exploration has its own CPU budget checks in Device::step.
            if(presentations[i - 1].exploration || presentations[i].exploration)
                continue;
            worst = std::max(worst, presentations[i].cycle - presentations[i - 1].cycle);
            ++intervals;
        }
        require(intervals != 0, "pause timing captured no display intervals");
        // Preserve the 50 Hz budget with interrupt jitter, without prescribing animation steps.
        if(worst >= 336000)
            std::cerr << name << " worst display interval: " << worst << " cycles\n";
        require(!check_timing || worst < 336000, "pause display missed its 50 Hz frame budget");
        uint64_t elapsed = device.ab->core_state.cpu.cycle_count - begin;
        require(elapsed < 16000000, "pause interaction exceeded one second");
        std::cout << "{\"workload\":\"" << name << "\",\"display_intervals\":" << intervals
                  << ",\"worst_frame_cycles\":" << worst << ",\"budget_cycles\":320000,"
                  << "\"interaction_cycles\":" << elapsed << "}\n";
    }
};

static FadeTrace::Frame pause_frame(Device& d) {
    FadeTrace trace(d);
    d.advance(0, 800000);
    return trace.frames.back();
}

static bool pause_row_visible(Device& d, unsigned row) {
    require(row < PAUSE_ROWS, "pause test row outside the viewport");
    auto pixels = pause_frame(d).pixels;
    unsigned baseline = PAUSE_FIRST_BASELINE + PAUSE_ROW_STEP * row;
    for(unsigned y = baseline - 5; y <= baseline; ++y)
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
    int width = 2;
    while(width < 128 && pixel(source, width - 1, 1))
        ++width;
    require(width > 2 && width < 128 && !pixel(source, 0, 1), "pause panel is missing");
    size_t at = 1;
    while(at < trace.frames.size() && !left_edge(trace.frames[at], (width + 1) / 2))
        ++at;
    int position = width * 16;
    uint64_t previous = 0;
    do {
        position -= position / 2;
        int visible = (position + 8) / 16;
        require(at < trace.frames.size() && left_edge(trace.frames[at], visible),
                "pause did not ease out before Audio");
        auto const& frame = trace.frames[at++];
        for(unsigned i = 0; i < source.pixels.size(); ++i)
            if(i % 128 >= (unsigned)width)
                require(frame.pixels[i] == source.pixels[i],
                        "world brightened during Audio handoff");
        require(!check_timing || !previous || frame.cycle - previous < 336000,
                "pause handoff missed a frame");
        previous = frame.cycle;
    } while((position + 8) / 16);

    auto const& faded_world = trace.frames[at - 1];
    constexpr int OUTSIDE_AUDIO = 128 - AUDIO_WIDTH - 1;
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
            require(frame.pixels[page * 128 + OUTSIDE_AUDIO] ==
                        faded_world.pixels[page * 128 + OUTSIDE_AUDIO],
                    "Audio changed the world's half fade");
        // The first Audio image has more drawing work than the outgoing empty pause image.
        // Subsequent presentations must keep the usual 50 Hz cadence.
        require(!check_timing || frame.cycle - previous < (first_options_frame ? 480000 : 336000),
                "Audio handoff missed a frame");
        previous = frame.cycle;
    } while(position != AUDIO_WIDTH * 16);
}

static void verify_audio_options(Device const& d, unsigned music, unsigned sfx) {
    auto const& ram = d.ab->core_state.cpu.data;
    require(ram[0xa71] == music && ram[0xa72] == sfx,
            "Audio did not apply independent audio settings");
    require(ram[0xa17] == 255, "Audio changed the shared master gain");
}

static void verify_options(Device& d, fs::path const& out) {
    require(d.state().options.music == 15 && d.state().options.sfx == 15,
            "new game did not initialize audio defaults");
    verify_audio_options(d, 128, 128);
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
    verify_audio_options(d, 119, 128);
    auto pixels = pause_frame(d).pixels;
    int knob = 128 - AUDIO_WIDTH + 10 + 14 * (AUDIO_WIDTH - 23) / 30;
    for(int x = 50; x < 118; ++x)
        require((pixels[3 * 128 + x] & 1) == (x >= knob && x < knob + 3), // Row y = 24, the knob
                "music slider thumb did not move with its volume");
    for(unsigned i = 0; i < 17; ++i)
        d.tap(BTN_LEFT);
    verify_audio_options(d, 0, 128);
    d.capture(out / "options-music-muted.pgm");
    d.advance(BTN_RIGHT, 32000000);
    d.advance(0, 800000);
    verify_audio_options(d, 255, 128);
    require(d.state().options.music == 30, "music did not reach 200 percent");
    d.tap(BTN_RIGHT);
    verify_audio_options(d, 255, 128);
    d.capture(out / "options-music-200.pgm");

    d.tap(BTN_DOWN);
    d.tap(BTN_LEFT);
    d.tap(BTN_LEFT);
    verify_audio_options(d, 255, 111);
    for(unsigned i = 0; i < 17; ++i)
        d.tap(BTN_LEFT);
    verify_audio_options(d, 255, 0);
    d.advance(BTN_RIGHT, 32000000);
    d.advance(0, 800000);
    verify_audio_options(d, 255, 255);
    require(d.state().options.sfx == 30, "SFX did not reach 200 percent");

    d.tap(BTN_DOWN); // Wrap to Music.
    d.tap(BTN_LEFT);
    verify_audio_options(d, 247, 255);
    d.tap(BTN_RIGHT);
    d.tap(BTN_UP); // Wrap back to SFX.
    d.tap(BTN_DOWN); // And forward to Music again.

    // Restore the half-scale native defaults (100 percent on the expanded sliders).
    for(unsigned i = 0; i < 15; ++i)
        d.tap(BTN_LEFT);
    d.tap(BTN_DOWN);
    for(unsigned i = 0; i < 15; ++i)
        d.tap(BTN_LEFT);
    verify_audio_options(d, 128, 128);
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
    verify_audio_options(d, 102, 85);
    d.tap(BTN_B);
    d.frame();
}

static void verify_pause_party(Device& d, fs::path const& out) {
    d.tap(BTN_B);
    d.advance(0, 1600000);
    Game frozen = d.state();
    unsigned serial = d.serial();
    d.tap(BTN_UP);
    d.tap(BTN_UP);
    d.tap(BTN_A); // Map
    d.advance(0, 8000000);
    d.capture(out / "world-map.pgm");
    d.advance(BTN_RIGHT | BTN_DOWN, 16000000);
    d.advance(BTN_RIGHT, 32000000);
    d.advance(0, 800000);
    d.capture(out / "world-map-scrolled.pgm");
    Game mapped = d.state();
    require(!memcmp(&frozen, &mapped, sizeof(Game)), "Map changed paused game state");
    d.verify_music();
    d.tap(BTN_B);
    d.advance(0, 8000000);
    require(d.serial() != serial, "Map did not return directly to gameplay");
    d.tap(BTN_B);
    d.advance(0, 1600000);
    frozen = d.state();
    serial = d.serial();
    d.tap(BTN_UP);
    d.tap(BTN_UP);
    d.tap(BTN_UP);
    d.tap(BTN_A); // Journal
    d.advance(0, 8000000);
    Game journal = d.state();
    require(!memcmp(&frozen, &journal, sizeof(Game)) && d.serial() == serial,
            "Journal changed paused game state");
    d.tap(BTN_B); // Journal returns directly to gameplay, where B reopens the pause menu.
    d.advance(0, 8000000);
    require(d.serial() != serial, "Journal B did not return directly to gameplay");
    d.tap(BTN_B);
    d.advance(0, 1600000);
    frozen = d.state();
    serial = d.serial();
    d.tap(BTN_UP);
    d.tap(BTN_UP);
    d.tap(BTN_UP);
    d.tap(BTN_UP);
    d.tap(BTN_UP); // Party
    d.tap(BTN_A);
    d.advance(0, 8000000);
    Game shown = d.state();
    require(!memcmp(&frozen, &shown, sizeof(Game)), "Party changed paused game state");
    d.tap(BTN_B);
    d.advance(0, 8000000);
    require(d.serial() != serial, "Party B did not return directly to gameplay");
    // A on the overview opens the Specials category, and the panel stays open. Gameplay resumes only
    // when B has climbed back to the overview and closed the panel.
    d.tap(BTN_B);
    d.advance(0, 1600000);
    d.tap(BTN_DOWN);
    d.tap(BTN_DOWN);
    d.tap(BTN_A);
    d.advance(0, 8000000);
    serial = d.serial();
    d.tap(BTN_A);
    d.advance(0, 8000000);
    require(d.serial() == serial, "Party A on the overview did not open a category");
    d.tap(BTN_B);
    d.advance(0, 800000);
    require(d.serial() == serial, "Party B from Specials closed the whole panel");
    d.tap(BTN_B);
    d.advance(0, 8000000);
    require(d.serial() != serial, "Party B from the overview did not return to gameplay");
}

static void verify_perk_choice(Device& d, fs::path const& out) {
    Game milestone = d.state();
    milestone.party.members[0].level = 2;
    milestone.party.members[0].hp = 16;
    d.set_state(milestone);
    d.tap(BTN_B);
    d.advance(0, 1600000);
    d.capture(out / "pause-perk-pending.pgm");
    d.tap(BTN_DOWN);
    d.tap(BTN_DOWN);
    d.tap(BTN_A); // Party
    d.advance(0, 8000000);
    d.capture(out / "perk-overview.pgm");
    d.tap(BTN_DOWN);
    d.tap(BTN_A); // Perks
    d.tap(BTN_UP);
    d.tap(BTN_A); // Learn Perk
    d.tap(BTN_A); // Inspect Hold Together
    d.advance(BTN_A, 8000000); // Open confirmation and hold A across multiple frames.
    require(d.state().party.members[0].perk_ranks == 0, "holding A learned the perk");
    d.capture(out / "perk-confirm.pgm");
    d.tap(BTN_B);
    require(d.state().party.members[0].perk_ranks == 0, "canceling changed the perk ranks");
    d.tap(BTN_A);
    d.tap(BTN_A);
    PartyMember learned = d.state().party.members[0];
    require(party_movement_perk_rank(learned) == 1 && party_attack_perk_rank(learned) == 0 &&
                party_unspent_perks(learned) == 0 && learned.hp == 16 &&
                party_max_hp(learned) == 20,
            "confirmed Hold Together did not spend exactly one choice");
    d.tap(BTN_A);
    require(d.state().party.members[0].perk_ranks == learned.perk_ranks,
            "another press learned an unavailable rank");
    d.capture(out / "perk-learned.pgm");
    d.tap(BTN_B);
    d.tap(BTN_B);
    d.tap(BTN_B);
    d.advance(0, 8000000);
}

static void start_grove_contact(Device& d);

static void grove_move_cursor(Device& d, GroveCell& cursor, GroveCell target);

static void verify_party_recruitment(Device& d, fs::path const& out) {
    Game original = d.state();
    require(original.party.count == 1, "new game did not start with one party member");
    Game nearby = original;
    nearby.player = {(uint16_t)((positions::RowanHome.x - 1) * TILE),
                     (uint16_t)(positions::RowanHome.y * TILE), EAST, 0};
    d.set_state(nearby);
    d.frame();
    auto open_choices = [&] {
        d.tap(BTN_A);
        require(d.prompt() != DIALOGUE_CLOSED, "talking to Rowan did not open dialogue");
        d.wait_for_prompt();
        d.tap(BTN_A);
        require(d.prompt() == DIALOGUE_OPTIONS, "Rowan did not offer dialogue options");
    };
    open_choices();
    d.tap(BTN_B);
    d.advance(0, 4000000);
    require(d.prompt() == DIALOGUE_CLOSED && d.state().party.count == 1,
            "backing out of Rowan's options recruited Rowan");

    open_choices();
    d.tap(BTN_DOWN);
    d.tap(BTN_DOWN);
    d.tap(BTN_DOWN);
    d.capture(out / "rowan-dialogue-options.pgm");
    d.tap(BTN_A);
    d.wait_for_prompt();
    require(d.prompt() == DIALOGUE_DONE && d.state().party.count == 1,
            "selecting Rowan's fourth option did not show a topic");
    d.tap(BTN_A);
    d.advance(0, 4000000);

    open_choices();
    Snapshot before = d.state();
    require(d.panel_addr && d.ab->core_state.cpu.data[d.panel_addr], "dialogue panel is not open");

    // The choices and recruitment announcement must share a single open panel.
    struct PanelTrace {
        Device& device;
        bool closed = false;
        bool reloaded = false;
    } trace{d};

    require(!d.cycle_observer, "another device observer is attached");
    d.observer_context = &trace;
    d.cycle_observer = [](void* context, bool after) {
        auto& trace = *static_cast<PanelTrace*>(context);
        if(after) {
            auto const& ram = trace.device.ab->core_state.cpu.data;
            if(!ram[trace.device.panel_addr])
                trace.closed = true;
            if(ram[trace.device.phase_addr] == 4)
                trace.reloaded = true;
        }
    };
    d.tap(BTN_A);
    d.wait_for_prompt();
    d.cycle_observer = nullptr;
    d.observer_context = nullptr;
    require(!trace.closed, "panel closed between choices and recruitment announcement");
    require(!trace.reloaded, "dialogue reloaded the tile cache");
    Snapshot announced = d.state();
    require(announced.ticks == before.ticks &&
                !memcmp(&announced.view.cache, &before.view.cache, sizeof(TileCache)) &&
                !memcmp(&announced.view.camera, &before.view.camera, sizeof(Camera)),
            "dialogue moved the gameplay camera, reloaded tiles or advanced simulation");
    d.capture(out / "party-joined.pgm");
    // Directional input during closing cannot move the player or advance simulation/cache work.
    d.buttons = BTN_B | BTN_RIGHT;
    uint64_t deadline = d.ab->core_state.cpu.cycle_count + 4000000;
    while(d.ab->core_state.cpu.data[d.panel_addr]) {
        d.step();
        require(d.ab->core_state.cpu.cycle_count < deadline,
                "dialogue close did not finish promptly");
        if(d.ab->core_state.cpu.data[d.panel_addr]) {
            require(d.ab->core_state.cpu.data[d.phase_addr] != 4,
                    "dialogue closing reloaded the tile cache");
            Snapshot closing = d.state();
            require(closing.ticks == announced.ticks &&
                        !memcmp(&closing.player, &announced.player, sizeof(Actor)) &&
                        !memcmp(&closing.view.camera, &announced.view.camera, sizeof(Camera)) &&
                        !memcmp(&closing.view.cache, &announced.view.cache, sizeof(TileCache)),
                    "dialogue closing changed gameplay state");
        }
    }
    d.advance(0, 1600000);
    auto joined = d.state().party;
    require(joined.count == 2 && joined.members[1].id == NPC_ROWAN && joined.members[1].hp == 18 &&
                joined.members[1].ap == 4 &&
                joined.members[1].movement_class == MOVEMENT_STEP &&
                joined.members[1].attack_class == ATTACK_SHOOT,
            "Rowan did not join with the expected statistics");
    auto recruited = d.state();
    for(auto const& actor : recruited.npcs.actors)
        require(actor.id != NPC_ROWAN, "recruited Rowan remained on the map");
    d.tap(BTN_A);
    require(d.prompt() == DIALOGUE_CLOSED && d.state().party.count == 2,
            "absent Rowan could still be talked to or recruited twice");
    d.tap(BTN_B);
    d.tap(BTN_DOWN);
    d.tap(BTN_DOWN);
    d.tap(BTN_A);
    d.advance(0, 8000000);
    d.capture(out / "party-recruited.pgm");
    // Left and right turn pages, which wrap across the two members, without changing statistics.
    d.tap(BTN_RIGHT);
    d.advance(0, 800000);
    d.capture(out / "party-second-page.pgm");
    d.tap(BTN_RIGHT);
    d.advance(0, 800000);
    auto shown = d.state().party;
    require(!memcmp(&joined, &shown, sizeof(Party)), "Party panel changed member statistics");
    d.tap(BTN_B);
    d.advance(0, 8000000);
    d.frame();
    // Start a real encounter with a recruited party at its maximum and a wounded member, then
    // verify result writeback.
    Game combat = d.state();
    combat.party.members[0].hp = 20;
    combat.party.members[0].ap = 4;
    combat.party.members[0].level = BATTLE_STARTER_LEVEL;
    combat.party.members[0].movement_class = MOVEMENT_LEAP;
    combat.party.members[0].attack_class = ATTACK_SHOOT;
    combat.party.members[1].hp = 17;
    combat.party.members[1].ap = 4;
    combat.party.members[1].movement_class = MOVEMENT_CHARGE;
    combat.party.members[1].attack_class = ATTACK_IMPACT;
    combat.player = {(uint16_t)(positions::PlayerStart.x * TILE),
                     (uint16_t)(positions::PlayerStart.y * TILE), NORTH, 0};
    d.set_state(combat);
    start_grove_contact(d);
    auto deployed = d.battle_state();
    require((BATTLEFIELDS_WORLD & battlefield_bit(deployed.field)) &&
                deployed.unit_count == 3 && deployed.units[0].id == battle_party_id(0) &&
                deployed.units[1].id == battle_party_id(1) && deployed.units[0].hp == 20 &&
                deployed.units[1].hp == 17 && deployed.units[2].id == ENEMY_WOLF,
            "real battle did not deploy the recruited party with its saved HP");
    d.advance(0, 1600000);
    d.capture(out / "battle-saved-party.pgm");
    // The shot scenario uses a fixed position after checking the real deployment.
    GroveCell cursor{battle_column(deployed.units[0].cell), battle_row(deployed.units[0].cell)};
    deployed.field = BATTLEFIELD_GROVE;
    deployed.units[0].cell = battle_cell(2, 1);
    deployed.units[1].cell = battle_cell(1, 2);
    deployed.units[2].cell = battle_cell(6, 1);
    d.set_battle(deployed);
    grove_move_cursor(d, cursor, {2, 1});
    // The traveler's Shot reaches the wolf along row 1, so A on the wolf opens the offer. B closes
    // it without acting, and A on the still-selected traveler opens it again. Opening Shot costs 2
    // AP and deals 7 to the wolf at full HP, leaving it at 2.
    d.tap(BTN_A); // Select the traveler under the cursor.
    for(unsigned i = 0; i < 4; ++i)
        d.tap(BTN_RIGHT);
    d.tap(BTN_A); // Open the offer on the wolf.
    d.advance(0, 800000);
    d.capture(out / "battle-special-offer.pgm");
    d.tap(BTN_B);
    require(d.battle_state().units[2].hp == 9 && d.state().party.members[0].ap == 4,
            "closing the offer with B acted on the battle");
    d.tap(BTN_A);
    d.tap(BTN_DOWN); // Highlight Opening Shot.
    d.capture(out / "battle-special-selected.pgm");
    unsigned activations = d.battle_state().activations;
    d.tap(BTN_A);
    d.wait_for_battle(activations);
    // The wolf may move after the shot and pay its blood price, so it is at 2 HP or 1. A normal attack
    // would leave it at 5 or 6.
    require(d.battle_state().units[2].hp <= 2 && d.state().party.members[0].ap == 2,
            "Opening Shot did not deal its damage or spend its AP");
    deployed.units[0].hp = 11;
    deployed.units[1].hp = 0;
    d.set_battle(deployed);
    d.tap(BTN_B);
    d.tap(BTN_DOWN); // Resign
    d.tap(BTN_A);
    d.advance(0, 4800000);
    d.tap(BTN_A);
    d.advance(0, 8000000);
    combat.party.members[0].hp = 11;
    combat.party.members[1].hp = 0;
    combat.party.members[0].ap = 2;
    auto finished = d.state().party;
    require(!memcmp(&combat.party, &finished, sizeof(Party)),
            "battle result did not persist HP and AP while preserving member classes");
    d.tap(BTN_A);
    d.advance(0, 4000000);
    d.tap(BTN_A); // Enter the next run from the title.
    d.frame();
    d.set_state(original);
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
    auto confirmation_visible = [&] {
        auto pixels = pause_frame(d).pixels;
        auto pixel = [&](int x, int y) { return (pixels[y / 8 * 128 + x] >> (y % 8)) & 1; };
        int left = 64, right = 64;
        if(!pixel(64, 19))
            return false;
        while(left > 1 && pixel(left - 1, 19))
            --left;
        while(right < 126 && pixel(right + 1, 19))
            ++right;
        if(right - left < 60 || left + right != 127)
            return false;
        for(int x = left; x <= right; ++x)
            if(pixel(x, 18) || !pixel(x, 44) || pixel(x, 45))
                return false;
        for(int y = 19; y <= 44; ++y)
            if(pixel(left - 1, y) || !pixel(left, y) || !pixel(right, y) || pixel(right + 1, y))
                return false;
        return true;
    };
    auto unchanged = [&] {
        BattleState paused = d.battle_state();
        return !memcmp(&initial, &paused, sizeof(initial)) && d.serial() == serial;
    };
    // Resign is now second. Holding the opening A cannot accept, even past the guard.
    d.tap(BTN_DOWN);
    d.advance(BTN_A, 6400000);
    require(unchanged() && confirmation_visible(),
            "held opening A accepted Resign or its confirmation was not centered");
    d.capture(out / "battle-resign-confirmation.pgm");
    d.tap(BTN_B);
    require(unchanged() && !confirmation_visible(),
            "cancelled Resign changed the battle or left its panel open");
    // B cancels immediately, including when pressed together with A during the guard.
    d.tap(BTN_A);
    d.advance(BTN_A | BTN_B, 320000);
    d.advance(0, 800000);
    require(unchanged() && !confirmation_visible(), "B did not cancel Resign confirmation");
    BattleState paused = d.battle_state();
    d.tap(BTN_DOWN); // Audio remains third.
    d.tap(BTN_A);
    d.advance(0, 9600000);
    d.capture(out / "battle-audio.pgm");
    d.tap(BTN_LEFT);
    verify_audio_options(d, 119, 128);
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
    d.tap(BTN_UP); // Pass is last, even with legal actions available.
    d.advance(BTN_A, 320000);
    d.advance(0, 320000);
    // A fresh press inside the guard is consumed, never deferred until it expires.
    d.advance(BTN_A, 6400000);
    require(unchanged() && confirmation_visible(), "early A accepted Pass confirmation");
    d.capture(out / "battle-pass-confirmation.pgm");
    d.tap(BTN_B);
    require(unchanged() && !confirmation_visible(), "cancelled Pass consumed an activation");
    // Repeat and accept only with a fresh press after the guard.
    d.tap(BTN_A);
    d.advance(0, 4800000);
    require(unchanged() && confirmation_visible(), "Pass ran before confirmation");
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
    blocked.field = BATTLEFIELD_GROVE;
    blocked.turn = BATTLE_ENEMY;
    blocked.unit_count = 6;
    blocked.units[0] = {battle_cell(0, 0), 2, BATTLE_TEST_ARCHER, BATTLE_ENEMY};
    blocked.units[1] = {battle_cell(7, 3), 3, BATTLE_TEST_ARCHER, BATTLE_ENEMY};
    blocked.units[2] = {battle_cell(1, 0), 8, BATTLE_TEST_FIGHTER, BATTLE_PARTY};
    blocked.units[3] = {battle_cell(0, 1), 8, BATTLE_TEST_FIGHTER, BATTLE_PARTY};
    blocked.units[4] = {battle_cell(6, 3), 8, BATTLE_TEST_FIGHTER, BATTLE_PARTY};
    blocked.units[5] = {battle_cell(7, 2), 8, BATTLE_TEST_FIGHTER, BATTLE_PARTY};
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
    waiting.field = BATTLEFIELD_GROVE;
    waiting.turn = BATTLE_ENEMY;
    waiting.unit_count = 2;
    waiting.units[0] = {battle_cell(0, 0), 4, ENEMY_BRUTE, BATTLE_PARTY};
    waiting.units[1] = {battle_cell(0, 1), 12, BATTLE_TEST_ARCHER, BATTLE_ENEMY};
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

static void verify_game_over_title(Device& d, fs::path const& out, unsigned serial,
                                   std::vector<uint8_t> const& save) {
    d.advance(0, 1600000);
    require(d.battle_address() != 0 && d.prompt() == DIALOGUE_CLOSED,
            "the battle-ending input also dismissed the defeat screen");
    d.advance(BTN_A, 4000000);
    require(d.battle_address() == 0 && d.prompt() == DIALOGUE_CLOSED,
            "Game Over did not close the battle and dialogue");
    d.advance(BTN_A, 4000000);
    require(d.serial() == serial, "defeat acknowledgment also dismissed the title");
    d.capture(out / "battle-resigned-title.pgm");
    d.advance(0, 1600000);
    d.tap(BTN_A);
    d.frame();
    require(d.serial() != serial, "title did not resume after a fresh A press");
    require(d.sector() == save, "Game Over changed the stored save");
    Game next = d.state();
    if((save[0] | (uint16_t)save[1] << 8) == sizeof(Game) && save[2] == SAVE_VERSION) {
        Game expected_save;
        memcpy(&expected_save, save.data() + 2, sizeof(Game));
        require(next.gold == expected_save.gold && next.player.x == expected_save.player.x &&
                    next.player.y == expected_save.player.y &&
                    !memcmp(&next.party, &expected_save.party, sizeof(Party)),
                "title did not load the saved party and location after Game Over");
    } else {
        require(next.player.x == positions::PlayerStart.x * TILE &&
                    next.player.y == positions::PlayerStart.y * TILE &&
                    next.party.count == 1 && next.party.members[0].hp == 20,
                "title did not start a fresh party after Game Over");
    }
}

static void verify_battle_resign(Device& d, fs::path const& out) {
    d.advance(0, 6400000);
    BattleState expected = d.battle_state();
    Game world = d.state();
    auto save = d.sector();
    unsigned serial = d.serial();
    d.tap(BTN_B);
    d.advance(0, 1600000);
    d.tap(BTN_DOWN);
    d.tap(BTN_A);
    d.advance(0, 4800000);
    // Keep the accepting A held through menu exit: it must not dismiss the defeat screen.
    d.advance(BTN_A, 8000000);
    expected.ended = true;
    expected.result = BATTLE_DEFEAT;
    BattleState resigned = d.battle_state();
    require(!memcmp(&expected, &resigned, sizeof(expected)),
            "Resign did not end as defeat without consuming a turn or changing units");
    Game paused = d.state();
    require(d.serial() == serial && !memcmp(&world, &paused, sizeof(world)),
            "Resign advanced exploration before dismissing the result");
    d.capture(out / "battle-resigned.pgm");
    verify_game_over_title(d, out, serial, save);
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
    require(pause_row_visible(d, 5) && !pause_row_visible(d, 6),
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
// enemy for 1. The wolf's Rend deals 2 to a healthy victim and 4 to one below half HP, and the
// brute's Impact knocks back a victim it strikes. The battle ends in victory on the enemy's turn:
// its last unit, the brute at 1 HP, has nobody in reach, so its only action is a move, which costs
// its last HP.
constexpr GroveAction GROVE_VICTORY_PLAN[] = {
    {{1, 2}, {5, 2}, true},  // The archer shoots the brute. The brute steps to (6,2).
    {{2, 1}, {4, 2}, false}, // The rogue leaps to (4,2). The wolf runs to (4,1), beside it.
    {{4, 2}, {4, 1}, true},  // The rogue strikes the wolf, which bites back for 2.
    {{4, 2}, {4, 1}, true},  // The rogue strikes the wolf again. The 2 HP wolf runs to (6,1).
    {{4, 2}, {5, 0}, false}, // The rogue leaps to (5,0), out of the brute's reach. The brute steps
                             // to (7,2).
    {{5, 0}, {4, 2}, false}, // The rogue leaps back to (4,2). The brute steps to (6,2).
    {{1, 2}, {1, 1}, false}, // The archer steps north to (1,1). The brute steps to (5,2).
    {{4, 2}, {5, 2}, true},  // The rogue strikes the brute, which strikes back, knocking the rogue
                             // to (3,2).
    {{3, 2}, {5, 1}, false}, // The 1 HP rogue leaps to (5,1), engaging the wolf, which falls, and
                             // the brute, which drops to 1 HP. The rogue falls paying its last HP.
                             // The brute steps to (5,1) and falls paying its last HP.
};

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

constexpr NPC_ID GROVE_WOLF = (NPC_ID)NPC_ENEMY_FIRST;
constexpr FLAG GROVE_WOLF_DEFEATED = FLAG_ENEMY_FIRST;

static void place_grove_contact(Device& d) {
    // Inject a new scene between exploration frames, including after The World Remembers.
    d.frame();
    Game world = d.state();
    require(world.area == positions::GroveWolf.area, "Grove contact fixture is in the wrong area");
    world.flags.clear_flag(GROVE_WOLF_DEFEATED);
    world.remember_timer = 0;
    world.player = {(uint16_t)(positions::GroveWolf.x * TILE + FOOT_WIDTH + 2),
                    (uint16_t)(positions::GroveWolf.y * TILE), WEST, 0};
    world.npcs.entering = 1;
    d.set_state(world);
    d.frame();
    auto ready = d.state();
    auto* wolf = std::find_if(ready.npcs.actors, ready.npcs.actors + NPC_CAPACITY,
                              [](Npc const& npc) { return npc.id == GROVE_WOLF; });
    require(wolf != ready.npcs.actors + NPC_CAPACITY, "Grove Wolf did not spawn via its chunk");
    // Keep a stable contact edge while the frame and fade observers attach.
    wolf->state = NPC_STATE_NONE;
    wolf->timer = 30;
    ready.player.x = wolf->actor.x + FOOT_WIDTH;
    ready.player.y = wolf->actor.y;
    d.set_state(ready);
}

static void start_grove_contact(Device& d) {
    place_grove_contact(d);
    d.tap(BTN_LEFT);
    d.wait_for_battle();
}

static void verify_consecutive_perk_ranks(Device& d, fs::path const& out) {
    Game training = d.state();
    training.party.members[0].movement_class = MOVEMENT_STEP;
    training.party.members[0].level = 8;
    d.set_state(training);
    d.tap(BTN_B);
    d.advance(0, 1600000);
    d.tap(BTN_DOWN);
    d.tap(BTN_DOWN);
    d.tap(BTN_A); // Party
    d.advance(0, 8000000);
    d.tap(BTN_DOWN);
    d.tap(BTN_A); // Perks
    d.tap(BTN_UP);
    d.tap(BTN_A); // Learn Perk
    for(uint8_t rank = 1; rank <= PERK_TRACK_RANKS; ++rank) {
        d.tap(BTN_A); // Inspect the next movement perk.
        if(rank == 2)
            d.capture(out / "footwork-details.pgm");
        else if(rank == 3)
            d.capture(out / "shield-wall-details.pgm");
        else if(rank == 4)
            d.capture(out / "shield-rebound-details.pgm");
        d.tap(BTN_A); // Open confirmation.
        d.tap(BTN_A); // Learn the perk.
        PartyMember learned = d.state().party.members[0];
        require(party_movement_perk_rank(learned) == rank &&
                    party_unspent_perks(learned) == PERK_TRACK_RANKS - rank,
                "Party did not learn the next Step rank");
    }
    std::cout << "{\"result\":\"PASS\",\"scenario\":\"learning consecutive perk ranks\"}\n";
}

static void verify_battle_classes(Device& d, fs::path const& out) {
    start_grove_contact(d);

    BattleState inspection = d.battle_state();
    GroveCell inspect_cursor{battle_column(inspection.units[0].cell),
                             battle_row(inspection.units[0].cell)};
    inspection.field = BATTLEFIELD_GROVE;
    inspection.units[0].cell = battle_cell(2, 1);
    inspection.units[1] = {battle_cell(5, 1), 10, ENEMY_SKELETON, BATTLE_ENEMY, 2};
    inspection.units[2] = {battle_cell(6, 2), 9, ENEMY_WOLF, BATTLE_ENEMY};
    inspection.unit_count = 3;
    d.set_battle(inspection);
    Game original = d.state();
    Game training = original;
    training.party.members[0].level = BATTLE_STARTER_LEVEL - 1;
    training.party.members[0].ap = 4;
    d.set_state(training);
    grove_move_cursor(d, inspect_cursor, {2, 1});
    d.capture(out / "battle_party_before_ap_unlock.pgm", true);
    training.party.members[0].level = BATTLE_STARTER_LEVEL;
    training.party.members[0].ap = 0;
    d.set_state(training);
    d.capture(out / "battle_party_ap_unlocked.pgm", true);
    d.set_state(original);
    grove_move_cursor(d, inspect_cursor, {5, 1});
    d.capture(out / "battle_enemy_ranks_and_ap.pgm");
    grove_move_cursor(d, inspect_cursor, {6, 2});
    d.capture(out / "battle_enemy_rank_zero.pgm");

    struct Case {
        char const* name;
        MOVEMENT_CLASS movement;
        ATTACK_CLASS attack;
        uint8_t actor, target, victim, final_cell, actor_hp, victim_hp;
        bool special;
        uint8_t rank = 2;
        bool signature = false;
        uint8_t level = 0;
    };

    static constexpr Case CASES[] = {
        {"battle_before_starter", MOVEMENT_STEP, ATTACK_STRIKE, battle_cell(2, 1), battle_cell(3, 1),
         battle_cell(3, 1), battle_cell(2, 1), 15, 7, false, 0, false, 2},
        {"battle_starter_unlocked", MOVEMENT_STEP, ATTACK_STRIKE, battle_cell(2, 1), battle_cell(3, 1),
         battle_cell(3, 1), battle_cell(2, 1), 15, 4, true, 0, false, 3},
        {"battle_step", MOVEMENT_STEP, ATTACK_STRIKE, battle_cell(1, 1), battle_cell(2, 1),
         battle_cell(3, 1), battle_cell(2, 1), 15, 9, false},
        {"battle_charge", MOVEMENT_CHARGE, ATTACK_STRIKE, battle_cell(1, 1), battle_cell(3, 1),
         battle_cell(4, 1), battle_cell(3, 1), 14, 7, false},
        {"battle_vault", MOVEMENT_VAULT, ATTACK_STRIKE, battle_cell(1, 1), battle_cell(3, 1),
         battle_cell(2, 1), battle_cell(3, 1), 14, 7, false},
        {"battle_glide", MOVEMENT_GLIDE, ATTACK_STRIKE, battle_cell(1, 0), battle_cell(3, 2),
         battle_cell(3, 1), battle_cell(3, 2), 14, 8, false},
        {"battle_arc", MOVEMENT_STEP, ATTACK_ARC, battle_cell(3, 1), battle_cell(4, 2),
         battle_cell(4, 2), battle_cell(3, 1), 15, 3, true},
        {"battle_drain", MOVEMENT_STEP, ATTACK_DRAIN, battle_cell(2, 1), battle_cell(3, 1),
         battle_cell(3, 1), battle_cell(2, 1), 18, 3, true, 3},
        {"battle_reach", MOVEMENT_STEP, ATTACK_REACH, battle_cell(1, 1), battle_cell(3, 1),
         battle_cell(3, 1), battle_cell(1, 1), 15, 3, true, 3},
        {"battle_recoil", MOVEMENT_LEAP, ATTACK_RECOIL, battle_cell(2, 1), battle_cell(3, 1),
         battle_cell(3, 1), battle_cell(1, 1), 15, 2, true},
        {"battle_flank", MOVEMENT_STEP, ATTACK_FLANK, battle_cell(2, 1), battle_cell(3, 1),
         battle_cell(3, 1), battle_cell(2, 1), 15, 4, false},
        {"battle_blast", MOVEMENT_STEP, ATTACK_BLAST, battle_cell(2, 1), battle_cell(5, 1),
         battle_cell(5, 1), battle_cell(2, 1), 15, 4, true, 3},
        {"battle_trample", MOVEMENT_CHARGE, ATTACK_STRIKE, battle_cell(1, 1), battle_cell(3, 1),
         battle_cell(2, 1), battle_cell(3, 1), 14, 7, false, 4},
        {"battle_throughshot", MOVEMENT_STEP, ATTACK_SHOOT, battle_cell(0, 1), battle_cell(5, 1),
         battle_cell(5, 1), battle_cell(0, 1), 15, 1, true, 4},
        {"battle_split_shot", MOVEMENT_STEP, ATTACK_SHOOT, battle_cell(1, 1), battle_cell(5, 1),
         battle_cell(5, 1), battle_cell(1, 1), 15, 4, true, 4, true},
        {"battle_breach_cut", MOVEMENT_STEP, ATTACK_STRIKE, battle_cell(2, 1), battle_cell(3, 1),
         battle_cell(3, 1), battle_cell(3, 1), 15, 0, true, 4, true},
    };
    for(auto const& test : CASES) {
        Game world = d.state();
        PartyMember& member = world.party.members[0];
        member.movement_class = test.movement;
        member.attack_class = test.attack;
        member.level = test.level ? test.level : (uint8_t)(2 * test.rank);
        member.ap = 4;
        member.perk_ranks = test.target == test.victim ? (uint8_t)(test.rank << 3) : test.rank;
        d.set_state(world);

        BattleState state{};
        state.field = BATTLEFIELD_GROVE;
        state.turn = BATTLE_PARTY;
        state.units[0] = BattleUnit{test.actor, 15, battle_party_id(0), BATTLE_PARTY};
        state.units[1] = BattleUnit{test.victim, 10, BATTLE_TEST_FIGHTER, BATTLE_ENEMY};
        state.units[2] = BattleUnit{battle_cell(7, 3), 10, BATTLE_TEST_FIGHTER, BATTLE_ENEMY};
        state.unit_count = 3;
        if(test.attack == ATTACK_SHOOT)
            state.units[2].cell = battle_cell(7, 1);
        if(test.signature && test.attack == ATTACK_SHOOT) {
            state.units[state.unit_count++] =
                BattleUnit{battle_cell(5, 0), 10, BATTLE_TEST_FIGHTER, BATTLE_ENEMY};
            state.units[state.unit_count++] =
                BattleUnit{battle_cell(5, 2), 10, BATTLE_TEST_FIGHTER, BATTLE_ENEMY};
        }
        if(test.attack == ATTACK_FLANK) {
            state.units[state.unit_count++] =
                BattleUnit{battle_cell(3, 0), 10, BATTLE_TEST_FIGHTER, BATTLE_PARTY};
        }
        if(test.attack == ATTACK_REACH) {
            state.units[state.unit_count++] =
                BattleUnit{battle_cell(2, 1), 10, BATTLE_TEST_FIGHTER, BATTLE_PARTY};
        }
        if(test.attack == ATTACK_BLAST) {
            state.units[state.unit_count++] =
                BattleUnit{battle_cell(5, 2), 1, BATTLE_TEST_FIGHTER, BATTLE_ENEMY};
            state.units[state.unit_count++] =
                BattleUnit{battle_cell(6, 1), 10, BATTLE_TEST_FIGHTER, BATTLE_ENEMY};
            state.units[state.unit_count++] =
                BattleUnit{battle_cell(4, 2), 10, BATTLE_TEST_FIGHTER, BATTLE_ENEMY};
        }
        d.set_battle(state);
        for(unsigned i = 1; i < BATTLE_COLUMNS; ++i)
            d.tap(BTN_LEFT);
        for(unsigned i = 1; i < BATTLE_ROWS; ++i)
            d.tap(BTN_UP);
        GroveCell cursor{0, 0};
        grove_move_cursor(d, cursor, {battle_column(test.actor), battle_row(test.actor)});
        d.tap(BTN_A);
        grove_move_cursor(d, cursor, {battle_column(test.target), battle_row(test.target)});
        if(test.target == test.victim && member.level >= BATTLE_STARTER_LEVEL) {
            d.tap(BTN_A);
            require(!d.battle_state().activations && d.state().party.members[0].ap == 4,
                    "opening the attack offer consumed the action");
            if(test.signature)
                d.tap(BTN_UP); // Wrap from Attack to the third row.
            else if(test.special)
                d.tap(BTN_DOWN);
        }

        {
            // Observe the player's animation through the final presentation before the AI thinks.
            PauseTiming timing(d);
            d.advance(BTN_A, 320000);
            uint64_t capture_after = test.target == test.victim ? 4800000 : 1200000;
            d.advance(0, capture_after);
            d.capture(out / (std::string(test.name) + ".pgm"));
            d.advance(0, 20680000 - capture_after);
            uint64_t worst = 0;
            for(size_t i = 1; i < timing.presentations.size(); ++i) {
                uint64_t interval =
                    timing.presentations[i].cycle - timing.presentations[i - 1].cycle;
                worst = std::max(worst, interval);
            }
            require(worst != 0, "battle action did not present consecutive frames");
            uint64_t span = timing.presentations.back().cycle - timing.presentations.front().cycle;
            std::cout << "{\"workload\":\"" << test.name
                      << "\",\"display_presentations\":" << timing.presentations.size()
                      << ",\"average_frame_cycles\":" << span / (timing.presentations.size() - 1)
                      << ",\"worst_frame_cycles\":" << worst << ",\"budget_cycles\":320000}"
                      << std::endl;
        }
        auto resolved = d.battle_state();
        require(resolved.activations == 1 && resolved.turn == BATTLE_ENEMY &&
                    resolved.units[0].cell == test.final_cell &&
                    resolved.units[0].hp == test.actor_hp &&
                    resolved.units[1].hp == test.victim_hp &&
                    d.state().party.members[0].ap == (test.signature ? 1 : test.special ? 2 : 4),
                (std::string(test.name) + " did not resolve from player input").c_str());
        if(test.attack == ATTACK_REACH)
            require(resolved.units[1].cell == battle_cell(4, 1) && resolved.units[3].hp == 10,
                    "Driving Thrust did not pass through its ally and push the target");
        if(test.attack == ATTACK_BLAST)
            require(!resolved.units[3].hp && resolved.units[4].hp == 8 && resolved.units[5].hp == 9,
                    "Detonation did not apply orthogonal and diagonal splash damage");
        if(test.signature && test.attack == ATTACK_SHOOT)
            require(resolved.units[2].hp == 5 && resolved.units[3].hp == 4 &&
                        resolved.units[4].hp == 4,
                    "Split Shot did not apply full lateral hits and retain Throughshot");
        else if(test.attack == ATTACK_SHOOT)
            require(resolved.units[2].hp == 1, "Throughshot did not hit beyond its selected target");
        d.wait_for_battle(1);
    }
    std::cout << "{\"result\":\"PASS\",\"scenario\":\"new combat classes through battle input\"}\n";
}

static void play_grove_battle(Device& d, bool verify_transition = false,
                              fs::path const* level_up_capture = nullptr,
                              fs::path const* ordinary_capture = nullptr) {
    uint16_t gold_before = d.state().gold;
    if(level_up_capture) {
        Game near_level = d.state();
        near_level.party.members[0].experience = 8;
        d.set_state(near_level);
    }
    d.wait_for_battle();
    BattleState fixture;
    battle_fixture_begin(fixture, &BATTLE_TEST_GROVE);
    d.set_battle(fixture);
    // Replacing BattleState does not reset the screen's cursor after menu fixtures.
    // Clamp it to a known corner before driving the victory plan.
    for(unsigned i = 1; i < BATTLE_COLUMNS; ++i)
        d.tap(BTN_LEFT);
    for(unsigned i = 1; i < BATTLE_ROWS; ++i)
        d.tap(BTN_UP);
    GroveCell cursor{0, 0};
    for(auto const& action : GROVE_VICTORY_PLAN) {
        grove_move_cursor(d, cursor, action.hero);
        d.tap(BTN_A); // Select the hero.
        grove_move_cursor(d, cursor, action.target);
        unsigned previous_activations = d.battle_state().activations;
        d.tap(BTN_A); // Confirm the destination.
        // A move leaves the cursor on the hero's new cell, and a strike returns it to the hero.
        cursor = action.strike ? action.hero : action.target;
        d.wait_for_battle(previous_activations);
    }
    require(d.battle_state().ended && d.battle_state().result == BATTLE_VICTORY,
            "grove battle did not reach victory");
    // Each results page waits for its own fresh A.
    d.advance(0, 1600000);
    uint16_t gold_after = gold_before <= UINT16_MAX - 6 ? gold_before + 6 : UINT16_MAX;
    require(d.state().gold == gold_after, "Grove victory did not credit six gold");
    if(ordinary_capture)
        d.capture(*ordinary_capture);
    if(level_up_capture) {
        d.capture(level_up_capture->parent_path() / "victory-rewards.pgm");
        d.advance(BTN_A, 3200000);
        require(d.battle_address() != 0 && d.prompt() == DIALOGUE_CLOSED &&
                    d.state().gold == gold_after,
                "held A skipped the level-up notice or repeated the reward");
        d.advance(0);
        auto rewarded = d.state().party.members[0];
        require(rewarded.level == 2 && party_unspent_perks(rewarded) == 1,
                "battle victory did not earn a level and leave the perk choice pending");
        d.capture(*level_up_capture);
    }
    if(verify_transition) {
        FadeTrace trace(d);
        auto frozen = d.state();
        d.tap(BTN_A);
        d.advance(0, 4000000);
        trace.verify("battle_exit");
        auto resumed = d.state();
        require(resumed.player.x == frozen.player.x && resumed.player.y == frozen.player.y &&
                    resumed.area == frozen.area, "battle exit moved the player");
    } else {
        d.tap(BTN_A);
        d.advance(0, 4000000);
    }
    Game resumed = d.state();
    require(d.prompt() == DIALOGUE_CLOSED && resumed.gold == gold_after &&
                resumed.flags.get_flag(GROVE_WOLF_DEFEATED) && resumed.remember_timer > 0,
            "Grove Wolf victory did not persist before returning to exploration");
    for(auto const& npc : resumed.npcs.actors)
        require(npc.id != GROVE_WOLF, "defeated Grove Wolf remained active");
}

static void verify_remembers(Device& d, fs::path const& out) {
    Game world = d.state();
    world.remember_timer = 1;
    world.flags.set_flag((FLAG)(FLAG_ENEMY_FIRST + 2));
    d.set_state(world);
    d.advance(0, 30000000);
    unsigned serial = d.serial();
    require(d.state().flags.get_flag(GROVE_WOLF_DEFEATED), "respawn cleared before the cutscene");
    d.advance(0, 8000000);
    require(d.serial() == serial, "The World Remembers advanced exploration");
    d.capture(out / "world-remembers.pgm");
    d.advance(0, 25000000);
    Game resumed = d.state();
    require(d.serial() != serial && !resumed.remember_timer &&
                !resumed.flags.get_flag(GROVE_WOLF_DEFEATED) &&
                resumed.flags.get_flag((FLAG)(FLAG_ENEMY_FIRST + 2)),
            "The World Remembers did not selectively clear defeat flags");
    for(auto const& npc : resumed.npcs.actors)
        require(npc.id != GROVE_WOLF, "Grove Wolf respawned in view");
}

#ifndef RPG_DEVICE_TEST_LIBRARY
// Interpreter audio RAM, as the runtime getters in runtime/lib/audio_state.c read it: the music
// status, the playing song's 24-bit start pointer, the music attenuation, and the saved
// attenuation and queued song of a song change's fade.
constexpr unsigned AUDIO_MUSIC_STATUS = 0xa4e;
constexpr unsigned AUDIO_MUSIC_START = 0xa50;
constexpr unsigned AUDIO_MUSIC_VOLUME = 0xa71;
constexpr unsigned AUDIO_FADE_VOLUME = 0xa73;
constexpr unsigned AUDIO_FADE_SONG = 0xa74;

static unsigned audio_pointer(Device const& d, unsigned address) {
    auto const& ram = d.ab->core_state.cpu.data;
    return ram[address] | (unsigned)ram[address + 1] << 8 | (unsigned)ram[address + 2] << 16;
}

// Waits out a song change, which fades the old song away before the new one starts, and returns
// the start pointer of the song that plays afterwards.
static unsigned settled_song(Device& d) {
    d.advance(0, 24000000);
    auto const& ram = d.ab->core_state.cpu.data;
    require(ram[AUDIO_MUSIC_STATUS] != 0 && ram[AUDIO_FADE_VOLUME] == 0,
            "song change did not finish");
    return audio_pointer(d, AUDIO_MUSIC_START);
}

#include "battle_recordings.inc"

// Holds a direction until a walk crosses into the area.
static void walk_to_area(Device& d, uint8_t held, uint8_t area, char const* message) {
    for(unsigned frames = 0; d.state().area != area; ++frames) {
        require(frames < 2 * TILE, message);
        d.frame(held);
    }
}

// The cave plays its own song, and the outdoors keeps Morning in the Meadow.
static void verify_area_music(Device& d) {
    Game original = d.state();
    unsigned outdoor = settled_song(d);

    // The cave mouth is a passage: walking north onto it enters the cave.
    Game at_mouth = original;
    at_mouth.player = {(uint16_t)(positions::CaveMouth.x * TILE),
                       (uint16_t)((positions::CaveMouth.y + 1) * TILE), NORTH, 0};
    d.set_state(at_mouth);
    d.advance(0, 1600000);
    walk_to_area(d, BTN_UP, AREA_CAVE, "walking onto the cave mouth did not teleport");
    unsigned cave = settled_song(d);
    require(cave != outdoor, "the cave kept the outdoor song");
    summary("cave_music", d, 30);

    walk_to_area(d, BTN_DOWN, AREA_OUTDOOR, "walking onto the cave exit did not teleport");
    require(settled_song(d) == outdoor, "leaving the cave did not bring the outdoor song back");

    // Leaving while the cave's song is still queued must replace the queue, not let the cave's
    // song start outdoors once the fade ends. The slowest fade, at attenuation 255, takes 1.02
    // seconds and outlasts the two screen fades of the walk out.
    auto& ram = d.ab->core_state.cpu.data;
    uint8_t volume = ram[AUDIO_MUSIC_VOLUME];
    ram[AUDIO_MUSIC_VOLUME] = 255;
    d.set_state(at_mouth);
    d.advance(0, 1600000);
    walk_to_area(d, BTN_UP, AREA_CAVE, "walking onto the cave mouth did not teleport");
    Game at_exit = d.state();
    at_exit.player = {
        (uint16_t)(positions::CaveExit.x * TILE),
        (uint16_t)(positions::CaveExit.y * TILE - FOOT_CENTER_Y - 1), SOUTH, 0};
    d.set_state(at_exit);
    walk_to_area(d, BTN_DOWN, AREA_OUTDOOR, "walking onto the cave exit did not teleport");
    require(ram[AUDIO_FADE_VOLUME] != 0 && audio_pointer(d, AUDIO_FADE_SONG) == outdoor,
            "leaving during a song change did not queue the outdoor song");
    require(settled_song(d) == outdoor, "the cave song started outdoors after a quick exit");
    ram[AUDIO_MUSIC_VOLUME] = volume;

    d.set_state(original);
    d.advance(0, 1600000);
}

// The one-shot effect stream's status byte in the interpreter's audio RAM, beside the music's:
// nonzero while an effect plays. The platform's sfx_ambient and chained effects read the same fact
// through avm_audio_status.
constexpr unsigned AUDIO_SFX_STATUS = 0xa58;

// How the effect stream behaved over a stretch of emulated time.
struct SfxWatch {
    std::vector<uint64_t> runs; // The cycles of each separate run of an effect playing.
    uint64_t gap = 0;           // The longest silence between two runs.
};

// Runs the device with buttons held, and stops after `cycles` or, when `wanted` runs have played,
// as soon as the last of them ends.
static SfxWatch watch_sfx(Device& d, uint8_t held, uint64_t cycles, unsigned wanted = 0) {
    auto& cpu = d.ab->core_state.cpu;
    SfxWatch watch;
    d.buttons = held;
    uint64_t end = cpu.cycle_count + cycles, last = cpu.cycle_count, quiet_since = last;
    bool was = cpu.data[AUDIO_SFX_STATUS] != 0;
    while(cpu.cycle_count < end) {
        d.step();
        bool now = cpu.data[AUDIO_SFX_STATUS] != 0;
        uint64_t elapsed = cpu.cycle_count - last;
        last = cpu.cycle_count;
        if(now && !was) {
            if(!watch.runs.empty())
                watch.gap = std::max(watch.gap, cpu.cycle_count - quiet_since);
            watch.runs.push_back(0);
        }
        if(now)
            watch.runs.back() += elapsed;
        if(was && !now) {
            quiet_since = cpu.cycle_count;
            if(wanted && watch.runs.size() >= wanted)
                break;
        }
        was = now;
    }
    return watch;
}

// Lets an effect that is already playing finish, so a watch starts from silence.
static void wait_for_quiet_sfx(Device& d) {
    for(unsigned i = 0; d.ab->core_state.cpu.data[AUDIO_SFX_STATUS]; ++i) {
        require(i < 200, "an effect never ended");
        d.advance(0, 160000);
    }
}

constexpr uint64_t CYCLES_PER_MS = 16000;

// Menus tick as the cursor moves through them, and answer a cancel with a longer note.
static void verify_menu_sounds(Device& d) {
    wait_for_quiet_sfx(d);
    d.advance(0, 800000);

    // Opening the pause menu ticks once, and every move through its list ticks once.
    SfxWatch open = watch_sfx(d, BTN_B, 4800000, 1);
    require(open.runs.size() == 1 && open.runs[0] > 10 * CYCLES_PER_MS &&
                open.runs[0] < 70 * CYCLES_PER_MS,
            "opening the pause menu did not tick");
    d.advance(0, 1600000);
    for(unsigned i = 0; i < 4; ++i) {
        SfxWatch step = watch_sfx(d, BTN_DOWN, 4800000, 1);
        require(step.runs.size() == 1 && step.runs[0] > 10 * CYCLES_PER_MS &&
                    step.runs[0] < 70 * CYCLES_PER_MS,
                "moving the pause menu cursor did not tick once");
        d.advance(0, 1600000);
    }
    // Closing the menu is a cancel, the longer falling chirp.
    SfxWatch close = watch_sfx(d, BTN_B, 6400000, 1);
    require(close.runs.size() == 1 && close.runs[0] > 75 * CYCLES_PER_MS,
            "closing the pause menu did not play the cancel sound");
    d.advance(0, 3200000);
}

// A gust that reaches the player is heard as wind, which is long and soft, and no effect plays when
// the SFX option is zero, because a muted effect would still take the music's voices.
static void verify_ambient_sounds(Device& d) {
    Game spawn = d.state();
    require(spawn.area == AREA_OUTDOOR, "the wind test starts outdoors");

    // Listen in front of a house door, where paving and walls leave the gust little grass to bend.
    d.frame();
    Game street = d.state();
    street.player = {(uint16_t)(positions::ElderDoor.x * TILE),
                     (uint16_t)((positions::ElderDoor.y + 1) * TILE), NORTH, 0};
    d.set_state(street);
    d.advance(0, 1600000);
    street = d.state();
    uint8_t px = (uint8_t)((street.player.x + FOOT_CENTER_X) >> TILE_SHIFT);
    uint8_t py = (uint8_t)((street.player.y + FOOT_CENTER_Y) >> TILE_SHIFT);

    // A moment when the gust line reaches the player, with no other sound due near it, so that
    // the wind is the only effect that the soundscape starts.
    auto quiet_near = [](uint16_t ticks) {
        for(int offset = -8; offset <= 64; ++offset) {
            uint16_t moment = (uint16_t)(ticks + offset);
            if(sound_due(moment))
                return false;
        }
        return true;
    };
    uint16_t gust = (uint16_t)(street.ticks + 32);
    for(;; ++gust)
        if(wind_passes(gust, px, py) && quiet_near(gust))
            break;

    for(bool muted : {false, true}) {
        wait_for_quiet_sfx(d);
        d.frame();
        Game g = d.state();
        g.ticks = (uint16_t)(gust - 4);
        g.options.sfx = muted ? 0 : spawn.options.sfx;
        g.player.walking = 0;
        d.set_state(g);
        SfxWatch wind = watch_sfx(d, 0, 24000000, 1);
        if(muted) {
            require(wind.runs.empty(), "a muted SFX option still played an effect");
        } else {
            require(wind.runs.size() == 1 && wind.runs[0] > 600 * CYCLES_PER_MS,
                    "the gust that reached the player was not heard as wind");
        }
        d.advance(0, 1600000);
    }
    d.frame();
    d.set_state(spawn);
    d.advance(0, 1600000);
}

// A lever's click is followed by the grind of the gate it opens, as two effects with a short gap,
// and throwing it back plays the pair again.
static void verify_mechanism_sounds(Device& d) {
    Game at_mouth = d.state();
    at_mouth.player = {(uint16_t)(positions::CaveMouth.x * TILE),
                       (uint16_t)((positions::CaveMouth.y + 1) * TILE), NORTH, 0};
    d.set_state(at_mouth);
    d.advance(0, 1600000);
    walk_to_area(d, BTN_UP, AREA_CAVE, "walking onto the cave mouth did not teleport");

    d.frame();
    Game at_lever = d.state();
    at_lever.player = {(uint16_t)(positions::CaveLever.x * TILE),
                       (uint16_t)((positions::CaveLever.y + 1) * TILE), NORTH, 0};
    d.set_state(at_lever);
    d.advance(0, 1600000);
    require(d.state().can_interact, "the cave lever did not become interactive");

    for(bool on : {true, false}) {
        wait_for_quiet_sfx(d);
        d.advance(0, 800000);
        SfxWatch thrown = watch_sfx(d, BTN_A, 32000000, 2);
        d.advance(0, 1600000);
        require(d.state().get_flag(FLAG_CAVE_GATE) == on, "the lever did not toggle its gate");
        require(thrown.runs.size() == 2, "the lever and its gate were not two effects");
        // The click is short, the grind is long, and the second starts when the first ends.
        require(thrown.runs[0] < 250 * CYCLES_PER_MS && thrown.runs[1] > 400 * CYCLES_PER_MS,
                "the lever's click and the gate's grind had the wrong lengths");
        require(thrown.gap < 60 * CYCLES_PER_MS, "the gate's grind did not follow the click");
    }
}

// Menus tick, a gust is heard as wind, and a lever and its gate are heard in turn. The cave comes
// last, because it leaves the player there.
static void verify_sounds(Device& d) {
    verify_menu_sounds(d);
    verify_ambient_sounds(d);
    verify_mechanism_sounds(d);
}

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
    d.advance(BTN_LEFT, 800000);
    // Let the battle entry finish while the longer audio fade is still running.
    d.advance(0, 4600000);
    require(d.battle_address() && d.ab->core_state.cpu.data[0xa73],
            "battle did not accept contact while music was fading");
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

static void advance_to_dialogue_options(Device& d) {
    for(unsigned page = 0; page < 16; ++page) {
        d.wait_for_prompt();
        auto prompt = d.prompt();
        if(prompt == DIALOGUE_OPTIONS)
            return;
        require(prompt == DIALOGUE_MORE || prompt == DIALOGUE_DONE,
                "expected a text page before the options");
        d.tap(BTN_A);
    }
    throw std::runtime_error("dialogue options did not appear");
}

static void verify_bram_meeting(Device& d, fs::path const& out) {
    constexpr auto yard = positions::BramYard;
    constexpr auto waiting = positions::BramLantern;
    auto bram = [&] {
        Game current = d.state();
        for(auto const& actor : current.npcs.actors)
            if(actor.id == NPC_BRAM)
                return actor;
        return Npc{};
    };

    d.frame();
    Game fixture = d.state();
    fixture.player = {(uint16_t)((yard.x + 1) * TILE), (uint16_t)(yard.y * TILE), WEST, 0};
    fixture.npcs = {};
    fixture.npcs.entering = 1;
    fixture.flags.set_flag(FLAG_LANTERN_STARTED);
    fixture.flags.set_flag(FLAG_LANTERN_DIAGNOSED);
    fixture.flags.set_flag(FLAG_LANTERN_RECORD_FOUND);
    d.set_state(fixture);
    d.frame();
    require(bram().id == NPC_BRAM && bram().assigned_site == yard,
            "Bram was missing from his yard");

    d.tap(BTN_A);
    d.wait_for_typing();
    require(d.prompt() == DIALOGUE_OPTIONS, "Bram did not offer his dialogue topics");
    d.tap(BTN_A);
    d.dismiss_dialogue();
    Npc walking = bram();
    require(d.state().get_flag(FLAG_BRAM_LANTERN_MEETING) && walking.id == NPC_BRAM &&
                walking.behavior == NPC_TRAVEL && walking.actor.x < yard.x * TILE,
            "Bram did not walk after his invitation");
    d.capture(out / "bram-leaves-yard.pgm");

    uint8_t lane = (uint8_t)(positions::VillageLantern.x + 3);
    uint8_t square_row = positions::VillageLantern.y;
    d.hold(BTN_DOWN, TILE);
    d.hold(BTN_LEFT, (yard.x + 1 - lane) * TILE);
    d.hold(BTN_UP, (yard.y + 1 - square_row) * TILE);
    d.hold(0, 16);
    Npc arrived = bram();
    require(arrived.id == NPC_BRAM && arrived.behavior == NPC_STAND &&
                arrived.actor.x == waiting.x * TILE && arrived.actor.y == waiting.y * TILE,
            "Bram was not waiting beside the lantern");
    d.capture(out / "bram-at-lantern.pgm");
    std::cout << "{\"result\":\"PASS\",\"scenario\":\"Bram walks to the lantern\"}\n";
}

static void verify_quest_level_up(Device& d, fs::path const& out) {
    Snapshot fixture = d.state();
    fixture.player = {(uint16_t)((positions::VillageLantern.x - 1) * TILE),
                      (uint16_t)(positions::VillageLantern.y * TILE), EAST, 0};
    fixture.npcs.entering = 1;
    fixture.flags.set_flag(FLAG_LANTERN_STARTED);
    fixture.flags.set_flag(FLAG_LANTERN_DIAGNOSED);
    fixture.flags.set_flag(FLAG_LANTERN_RECORD_FOUND);
    fixture.party.members[0].level = 1;
    fixture.party.members[0].experience = 4;
    fixture.party.members[0].perk_ranks = 0;
    d.set_state(fixture);
    d.frame();

    d.capture(out / "lantern-unlit.pgm", true);
    d.tap(BTN_A);
    advance_to_dialogue_options(d);
    d.capture(out / "lantern-modern-options.pgm");
    d.tap(BTN_B);
    d.frame();
    require(!d.state().get_flag(FLAG_LANTERN_RESTORED) &&
                !d.state().get_flag(FLAG_LANTERN_MODERN) &&
                d.state().party.members[0].experience == 4,
            "canceling the repair changed quest state");

    d.tap(BTN_A);
    advance_to_dialogue_options(d);
    d.tap(BTN_A);
    // Advance the final text without waiting for exploration, which the notice pauses.
    auto prompt = DIALOGUE_MORE;
    for(unsigned page = 0; page < 16 && prompt == DIALOGUE_MORE; ++page) {
        d.wait_for_prompt();
        prompt = d.prompt();
        require(prompt == DIALOGUE_MORE || prompt == DIALOGUE_DONE,
                "expected Bram's resolution text");
        d.tap(BTN_A);
    }
    require(prompt == DIALOGUE_DONE, "Bram's resolution text did not finish");
    d.advance(0, 3200000);
    Snapshot rewarded = d.state();
    require(rewarded.get_flag(FLAG_LANTERN_RESTORED) && rewarded.get_flag(FLAG_LANTERN_MODERN) &&
                rewarded.gold == fixture.gold + 12 && rewarded.party.members[0].level == 2 &&
                party_unspent_perks(rewarded.party.members[0]) == 1,
            "Lantern Keeper did not earn a level and a perk choice");
    unsigned serial = d.serial();
    d.advance(0, 3200000);
    require(d.serial() == serial, "quest completion panel did not wait for dismissal");
    d.capture(out / "lantern-quest-complete.pgm");
    d.tap(BTN_A);
    d.advance(0, 3200000);
    require(d.serial() == serial, "quest level-up notice did not wait for dismissal");
    d.capture(out / "quest-level-up.pgm");
    d.tap(BTN_A);
    d.frame();
    require(d.serial() != serial && party_unspent_perks(d.state().party.members[0]) == 1,
            "dismissing the quest notice did not resume exploration with a pending perk");
    d.capture(out / "lantern-modern.pgm", true);
}

static void verify_venlor_quests(Device& d, fs::path const& out) {
    auto place = [&](TilePos site, int8_t dx, int8_t dy, DIRECTION facing) {
        Game fixture = d.state();
        fixture.area = site.area;
        if(site.area == AREA_CAVE)
            fixture.dimensions = {CAVE_WIDTH, CAVE_HEIGHT};
        else if(site.area == AREA_INTERIORS)
            fixture.dimensions = {INTERIORS_WIDTH, INTERIORS_HEIGHT};
        else
            fixture.dimensions = {OUTDOOR_WIDTH, OUTDOOR_HEIGHT};
        fixture.player = {(uint16_t)((site.x + dx) * TILE), (uint16_t)((site.y + dy) * TILE),
                          facing, 0};
        fixture.previous_player_tile = {};
        fixture.npcs = {};
        fixture.npcs.entering = 1;
        d.set_state(fixture);
        d.frame();
    };
    auto inspect = [&](char const* filename, bool resume_exploration = true) {
        d.capture(out / filename, true);
        d.tap(BTN_A);
        d.dismiss_dialogue(resume_exploration);
    };
    auto surveyors_here = [&] {
        auto state = d.state();
        return std::count_if(std::begin(state.npcs.actors), std::end(state.npcs.actors),
                             [](Npc const& n) { return n.id == NPC_SURVEYOR; });
    };

    place(positions::WorkshopRecord, 0, 1, NORTH);
    inspect("workshop-record.pgm");
    require(d.state().get_flag(FLAG_LANTERN_RECORD_FOUND) && !d.state().get_flag(FLAG_LANTERN_STARTED),
            "reading the workshop record did not preserve early evidence");
    place(positions::SurveyorNotes, 0, 1, NORTH);
    inspect("surveyor-notes.pgm", false);
    d.advance(0, 3200000);
    require(d.state().get_flag(FLAG_SURVEY_STARTED) && d.state().get_flag(FLAG_SURVEY_NOTES_READ),
            "the survey notes did not start the search");
    unsigned serial = d.serial();
    d.advance(0, 3200000);
    require(d.serial() == serial && d.state().selected_quest == QUEST_NONE,
            "the quest start panel did not wait for a tracking choice");
    d.capture(out / "surveyor-quest-start.pgm");
    d.tap(BTN_A);
    d.frame();
    require(d.state().selected_quest == QUEST_MISSING_SURVEYOR,
            "Track did not select the new quest");
    d.capture(out / "tracked-quest-arrow.pgm", true);

    place(positions::MineLanternRelay, 0, 1, NORTH);
    Game gate_open = d.state();
    gate_open.flags.set_flag(FLAG_CAVE_GATE);
    d.set_state(gate_open);
    d.frame();
    inspect("mine-lantern-relay.pgm");
    require(d.state().get_flag(FLAG_LANTERN_RELAY_FOUND) && !d.state().get_flag(FLAG_CAVE_BRIDGE),
            "the relay depended on the bridge");

    Game crossing = d.state();
    crossing.flags.set_flag(FLAG_CAVE_BRIDGE);
    // Keep this scenario below the level-up threshold at each separate reward.
    crossing.party.members[0].level = 2;
    crossing.party.members[0].experience = 0;
    d.set_state(crossing);
    place(positions::SurveyorTrapped, 0, 1, NORTH);
    require(surveyors_here() == 1, "trapped surveyor did not appear");
    d.capture(out / "surveyor-trapped.pgm", true);
    for(uint8_t response : {BTN_B, BTN_A}) {
        d.tap(BTN_A);
        d.wait_for_prompt();
        while(d.prompt() == DIALOGUE_MORE) {
            d.tap(BTN_A);
            d.wait_for_prompt();
        }
        require(d.prompt() == DIALOGUE_CHOICE, "surveyor did not offer to leave");
        d.tap(response);
        if(response == BTN_B) {
            d.frame();
            require(d.state().get_flag(FLAG_SURVEYOR_FOUND) &&
                        !d.state().get_flag(FLAG_SURVEYOR_RESCUED) &&
                        d.state().gold == crossing.gold && surveyors_here() == 1,
                    "declining the rescue changed its outcome");
        } else {
            d.dismiss_dialogue(false);
            d.advance(0, 3200000);
            d.capture(out / "surveyor-quest-complete.pgm");
            d.tap(BTN_A);
            d.frame();
        }
    }
    require(d.state().get_flag(FLAG_SURVEYOR_RESCUED) && surveyors_here() == 0 &&
                d.state().gold == crossing.gold + 15 &&
                d.state().party.members[0].experience == 8 &&
                d.state().selected_quest == QUEST_NONE && !d.state().get_flag(FLAG_CAVE_CHEST),
            "rescue did not retire the surveyor and award exactly 8 XP and 15 G");
    place(positions::CaveLadder, 0, 1, NORTH);
    walk_to_area(d, BTN_UP, AREA_OUTDOOR, "the rescued surveyor's ladder did not lead outside");
    walk_to_area(d, BTN_UP, AREA_CAVE, "the hatch did not reopen the ladder route");
    place(positions::InnExit, 0, -1, SOUTH);
    walk_to_area(d, BTN_DOWN, AREA_OUTDOOR, "the inn did not return to Millcross");
    d.hold(BTN_RIGHT, TILE);
    require(surveyors_here() == 1, "the returned surveyor did not appear by the inn");
    d.capture(out / "surveyor-returned.pgm", true);
    d.tap(BTN_A);
    advance_to_dialogue_options(d);
    d.tap(BTN_DOWN);
    d.tap(BTN_A);
    d.dismiss_dialogue();
    require(d.state().gold == crossing.gold + 15, "post-rescue conversation reissued a reward");

    // Walk around Well Square from the inn. Normal movement admits actors incrementally.
    d.hold(BTN_DOWN, 3 * TILE);
    d.hold(BTN_RIGHT, (positions::VillageLantern.x - positions::SurveyorReturned.x) * TILE);
    Game diagnosis = d.state();
    diagnosis.flags.set_flag(FLAG_LANTERN_DIAGNOSED);
    d.set_state(diagnosis);
    d.tap(BTN_A);
    advance_to_dialogue_options(d);
    d.capture(out / "lantern-ancient-options.pgm");
    d.tap(BTN_DOWN);
    d.tap(BTN_A);
    d.dismiss_dialogue(false);
    d.advance(0, 3200000);
    d.tap(BTN_B); // Close the start panel. This interaction also completed the quest.
    d.advance(0, 3200000);
    serial = d.serial();
    d.tap(BTN_B);
    require(d.serial() == serial && d.state().selected_quest == QUEST_NONE,
            "closing Quest Start did not leave Quest Complete open and tracking clear");
    d.tap(BTN_A);
    d.frame();
    require(d.state().get_flag(FLAG_LANTERN_RESTORED) && !d.state().get_flag(FLAG_LANTERN_MODERN) &&
                d.state().gold == crossing.gold + 27 &&
                d.state().party.members[0].experience == 14,
            "ancient restoration did not preserve the repair method or award 6 XP and 12 G");
    d.capture(out / "lantern-ancient.pgm", true);
    d.tap(BTN_B);
    for(unsigned row = 0; row < PAUSE_JOURNAL - PAUSE_RESUME; ++row)
        d.tap(BTN_DOWN);
    d.tap(BTN_A);
    d.advance(0, 3200000);
    d.capture(out / "journal-both-completed.pgm");
    d.tap(BTN_DOWN);
    d.tap(BTN_A);
    d.advance(0, 3200000);
    d.capture(out / "journal-surveyor-completed.pgm");
    d.tap(BTN_B);
    d.tap(BTN_B);
    d.advance(0, 3200000);
}

static void verify_millcross_views(Device& d, fs::path const& out) {
    Game origin = d.state();
    auto place = [&](TilePos site, int8_t dx, int8_t dy, DIRECTION facing) {
        Game fixture = d.state();
        fixture.player = {(uint16_t)((site.x + dx) * TILE), (uint16_t)((site.y + dy) * TILE),
                          facing, 0};
        fixture.npcs.entering = 1;
        d.set_state(fixture);
        d.frame();
    };
    d.capture(out / "millcross-arrival.pgm");
    place(positions::WellSign, -1, -1, NORTH);
    summary("millcross_well", d, 30);
    d.capture(out / "millcross-well.pgm");
    place(positions::OrchardSign, 0, -1, WEST);
    summary("millcross_orchard", d, 30);
    d.capture(out / "millcross-orchard.pgm");
    place(positions::BramYard, 0, 1, NORTH);
    summary("millcross_smithy", d, 30);
    d.capture(out / "millcross-smithy.pgm");
    place(positions::CaveMouth, 0, 1, NORTH);
    summary("hollow_mine_mouth", d, 12);
    d.capture(out / "hollow-mine-mouth.pgm");
    walk_to_area(d, BTN_UP, AREA_CAVE, "Millcross trail did not lead into the mine");
    summary("hollow_mine_camp", d, 30);
    d.capture(out / "hollow-mine-camp.pgm");
    walk_to_area(d, BTN_DOWN, AREA_OUTDOOR, "Hollow Mine camp could not return to the trail");
    d.set_state(origin);
    d.frame();
}

static void verify_equipment(Device& d, fs::path const& out) {
    Game fixture = d.state();
    for(uint8_t id = 0; id < ITEM_DEFINED_COUNT; ++id)
        require(!fixture.get_flag(equipment_flag(id)), "new game starts with owned equipment");
    fixture.flags.set_flag(equipment_flag(ITEM_FIRSTSTRIKE_SWORD));
    fixture.party.members[1] = {18, 4, MOVEMENT_STEP, ATTACK_SHOOT, NPC_ROWAN, 1, 0, 0, {}};
    fixture.party.count = 2;
    d.set_state(fixture);

    d.tap(BTN_B);
    d.advance(0, 1600000);
    d.tap(BTN_DOWN);
    d.tap(BTN_DOWN);
    d.tap(BTN_A); // Party
    d.advance(0, 8000000);
    d.tap(BTN_UP);
    d.tap(BTN_A); // Equipment
    d.tap(BTN_RIGHT); // Rowan
    d.tap(BTN_A); // Weapon picker
    d.tap(BTN_DOWN); // Firststrike Sword
    d.tap(BTN_A); // Details
    d.capture(out / "equipment-details.pgm");
    d.tap(BTN_A); // Equip
    Game equipped = d.state();
    require(equipment_get(equipped.party.members[1], SLOT_WEAPON) == ITEM_FIRSTSTRIKE_SWORD,
            "Party did not equip Firststrike Sword");
    d.tap(BTN_B);
    d.tap(BTN_B);
    d.tap(BTN_LEFT); // Traveler
    d.tap(BTN_A);
    d.tap(BTN_DOWN);
    d.capture(out / "equipment-picker-worn.pgm");
    d.tap(BTN_A);
    d.tap(BTN_A); // Transfer
    Game transferred = d.state();
    require(equipment_get(transferred.party.members[0], SLOT_WEAPON) == ITEM_FIRSTSTRIKE_SWORD &&
                equipment_get(transferred.party.members[1], SLOT_WEAPON) == ITEM_NONE,
            "Party did not transfer Firststrike Sword to the traveler");
    d.tap(BTN_B);
    d.tap(BTN_UP); // Unequip
    d.tap(BTN_A);
    Game unequipped = d.state();
    require(equipment_get(unequipped.party.members[0], SLOT_WEAPON) == ITEM_NONE &&
                unequipped.get_flag(equipment_flag(ITEM_FIRSTSTRIKE_SWORD)),
            "Party unequip did not preserve ownership");
    d.tap(BTN_DOWN);
    d.tap(BTN_A);
    d.tap(BTN_A); // Equip for the save/restart check.
    d.tap(BTN_B);
    d.tap(BTN_B);
    d.tap(BTN_B);
    d.tap(BTN_B);
    d.advance(0, 8000000);
}

static void verify_supplies(Device& d, fs::path const& out) {
    Game original = d.state();
    require(original.supplies.quantity[SUPPLY_POTION] == 2 &&
                original.supplies.quantity[SUPPLY_ELIXIR] == 1 &&
                original.supplies.quantity[SUPPLY_REVIVE] == 1 &&
                original.supplies.quantity[SUPPLY_ETHER] == 1,
            "new game supplies have incorrect quantities");
    Game injured = original;
    injured.party.members[0].hp = 1;
    d.set_state(injured);
    d.tap(BTN_B);
    unsigned serial = d.serial();
    for(unsigned i = 0; i < 3; ++i)
        d.tap(BTN_DOWN);
    d.tap(BTN_A);
    d.advance(0, 3200000);
    d.capture(out / "supplies-list.pgm");
    d.tap(BTN_A);
    d.capture(out / "supplies-details.pgm");
    d.tap(BTN_A);
    d.capture(out / "supplies-target.pgm");
    d.advance(BTN_A, 3200000);
    require(d.state().party.members[0].hp == 9 &&
                d.state().supplies.quantity[SUPPLY_POTION] == 1,
            "Potion target or held-A handling is incorrect");
    d.advance(0);
    d.tap(BTN_DOWN);
    d.tap(BTN_A);
    d.tap(BTN_A);
    d.capture(out / "supplies-confirm.pgm");
    d.tap(BTN_B);
    require(d.state().party.members[0].hp == 9 &&
                d.state().supplies.quantity[SUPPLY_ELIXIR] == 1,
            "cancelled Elixir changed the party or inventory");
    d.tap(BTN_A);
    d.tap(BTN_A);
    require(d.state().party.members[0].hp == 13 &&
                !d.state().supplies.quantity[SUPPLY_ELIXIR],
            "confirmed Elixir did not heal the party");
    require(d.serial() == serial, "Supplies advanced exploration");
    d.tap(BTN_B);
    d.frame();
    require(d.serial() != serial, "Supplies did not resume exploration");
    original.supplies = d.state().supplies;
    d.set_state(original);
}

static void verify_shop(Device& d, fs::path const& out) {
    Game original = d.state();
    Game street = original;
    street.gold = 20;
    street.player = {(uint16_t)(positions::ShopDoor.x * TILE),
                     (uint16_t)((positions::ShopDoor.y + 1) * TILE), NORTH, 0};
    d.set_state(street);
    d.advance(0, 1600000);
    d.tap(BTN_A);
    d.advance(0, 4800000);
    require(d.state().area == positions::ShopExit.area, "Shop door did not enter the shop");
    d.frame();
    Game beside_ada = d.state();
    beside_ada.player = {(uint16_t)((positions::AdaShop.x - 1) * TILE),
                         (uint16_t)(positions::AdaShop.y * TILE), EAST, 0};
    d.set_state(beside_ada);
    d.frame();
    d.tap(BTN_A);
    d.wait_for_typing();
    require(d.prompt() == DIALOGUE_OPTIONS, "Ada did not offer dialogue options");

    unsigned serial = d.serial();
    d.advance(0);
    d.advance(BTN_A, 4800000); // Browse supplies and keep A held through the handoff.
    require(d.panel_addr, "missing dialogue panel QA address");
    require(d.serial() == serial && !d.ab->core_state.cpu.data[d.panel_addr] &&
                d.state().gold == street.gold,
            "Ada's dialogue stayed open behind the shop or opening the shop changed gameplay");
    d.capture(out / "shop-list.pgm");
    d.advance(0);
    d.tap(BTN_A);
    d.capture(out / "shop-details.pgm");
    d.tap(BTN_A);
    d.capture(out / "shop-confirm.pgm");
    d.tap(BTN_B);
    auto cancelled = d.state();
    require(cancelled.gold == street.gold &&
                !memcmp(&cancelled.supplies, &street.supplies, sizeof(Supplies)),
            "Cancelled shop confirmation changed the wallet or inventory");
    d.advance(BTN_A, 3200000);
    require(d.state().gold == street.gold &&
                d.state().supplies.quantity[SUPPLY_POTION] == street.supplies.quantity[SUPPLY_POTION],
            "Holding A on confirmation entry purchased a Potion");
    d.advance(0);
    d.advance(BTN_A, 3200000);
    require(d.state().gold == street.gold - 12 &&
                d.state().supplies.quantity[SUPPLY_POTION] ==
                    street.supplies.quantity[SUPPLY_POTION] + 1,
            "Shop confirmation did not purchase exactly one Potion");
    d.capture(out / "shop-purchased.pgm");
    d.advance(0);
    d.tap(BTN_A);
    d.tap(BTN_A);
    d.tap(BTN_A);
    require(d.state().gold == street.gold - 12 &&
                d.state().supplies.quantity[SUPPLY_POTION] ==
                    street.supplies.quantity[SUPPLY_POTION] + 1,
            "Unaffordable purchase changed the wallet or inventory");
    d.capture(out / "shop-insufficient.pgm");
    d.tap(BTN_B);
    d.tap(BTN_B);
    d.tap(BTN_B);
    d.frame();
    require(d.serial() != serial && d.prompt() == DIALOGUE_CLOSED &&
                !d.ab->core_state.cpu.data[d.panel_addr],
            "Closing Ada's shop did not return directly to exploration");
    for(unsigned frames = 0; d.state().area == positions::ShopExit.area; ++frames) {
        require(frames < 3 * TILE, "Walking onto the Shop exit did not return to the village");
        d.frame(BTN_DOWN);
    }
    original.gold = d.state().gold;
    original.supplies = d.state().supplies;
    d.set_state(original);
    d.advance(0, 1600000);
}

static void verify_smoke(Device& d, fs::path const& package, fs::path const& out) {
    d.advance(0, 3200000);
    require(d.serial() == 0 && d.music_enabled(), "smoke boot did not reach the title");
    d.capture(out / "title.pgm");
    d.tap(BTN_A);
    d.frame();
    auto initial = d.state();
    require(d.serial() != 0 && initial.ticks != 0 && initial.area == AREA_OUTDOOR,
            "smoke new game did not reach exploration");
    d.hold(BTN_DOWN, 4);
    require(d.state().player.y > initial.player.y, "smoke movement failed");
    d.capture(out / "exploration.pgm");
    verify_supplies(d, out);
    verify_quest_level_up(d, out);
    verify_perk_choice(d, out);
    verify_equipment(d, out);
    d.tap(BTN_B);
    unsigned serial = d.serial();
    d.advance(0, 800000);
    require(d.serial() == serial, "smoke menu did not pause exploration");
    d.tap(BTN_DOWN); // Save, immediately below Resume.
    d.tap(BTN_A);
    d.capture(out / "pause-top.pgm");
    d.tap(BTN_UP); // Save to Resume.
    d.tap(BTN_UP); // Wrap to Load and reveal the final row.
    d.capture(out / "pause-scrolled.pgm");
    d.tap(BTN_DOWN); // Wrap back to Resume.
    auto saved = d.state();
    auto sector = d.sector();
    require((sector[0] | (uint16_t)sector[1] << 8) == sizeof(Game) &&
                sector[2] == SAVE_VERSION && !memcmp(sector.data() + 2, &saved, sizeof(Game)),
            "smoke save did not persist the live game");

    Device restarted(package, d.game_addr, d.marker_addr, d.serial_addr, d.dialogue_addr);
    restarted.phase_addr = d.phase_addr;
    restarted.import_sector(sector);
    restarted.advance(0, 3200000);
    restarted.tap(BTN_A);
    restarted.frame();
    auto loaded = restarted.state();
    require(restarted.serial() != 0 && loaded.area == saved.area && loaded.gold == saved.gold &&
                loaded.player.x == saved.player.x && loaded.player.y == saved.player.y,
            "smoke restart did not restore the save");
    require(loaded.supplies.quantity[SUPPLY_POTION] == 1 &&
                loaded.supplies.quantity[SUPPLY_ELIXIR] == 0,
            "smoke restart did not restore used supplies");
    PartyMember const& member = loaded.party.members[0];
    require(loaded.get_flag(equipment_flag(ITEM_FIRSTSTRIKE_SWORD)) &&
                equipment_get(member, SLOT_WEAPON) == ITEM_FIRSTSTRIKE_SWORD &&
                equipment_get(loaded.party.members[1], SLOT_WEAPON) == ITEM_NONE,
            "smoke restart did not restore equipment ownership and slots");
    require(party_movement_perk_rank(member) == 1 && member.hp == 16 &&
                party_max_hp(member) == 20 && party_unspent_perks(member) == 0,
            "smoke restart did not restore the learned perk and current HP");
    std::cout << "{\"result\":\"PASS\",\"scenario\":\"boot, input, display, perk confirmation and "
                 "save/restart\"}\n";
}

// Journal frames: the opening slide, steady windows, and the latency from a press to its redraw.
// A steady window's worst interval is its worst frame: presentations are paced to the frame rate, so
// an interval longer than one frame is a frame that took longer than the budget.
static void report_steady(char const* name, PauseTiming const& timing) {
    uint64_t worst = 0;
    for(size_t i = 1; i < timing.presentations.size(); ++i)
        worst = std::max(worst, timing.presentations[i].cycle - timing.presentations[i - 1].cycle);
    std::cout << "{\"workload\":\"" << name << "\",\"display_presentations\":"
              << timing.presentations.size() << ",\"worst_frame_cycles\":" << worst
              << ",\"budget_cycles\":320000}\n";
}

// Render cycles of redraws caused by real presses, from the QA render marks the Journal sets. Each
// press-and-release pair is one redraw, and the marks do not advance the frame serial.
static void report_render_cost(char const* name, Device& d, unsigned redraws, uint8_t press) {
    unsigned previous = d.journal_redraws, count = 0;
    uint64_t sum = 0, worst = 0;
    for(unsigned i = 0; i < redraws; ++i) {
        d.advance(press, 320000);
        d.advance(0, 320000);
        if(d.journal_redraws != previous) {
            previous = d.journal_redraws;
            sum += d.journal_render;
            worst = std::max(worst, d.journal_render);
            ++count;
        }
    }
    std::cout << "{\"workload\":\"" << name << "\",\"redraws\":" << count
              << ",\"average_render_cycles\":" << (count ? sum / count : 0)
              << ",\"worst_render_cycles\":" << worst << ",\"budget_cycles\":320000}\n";
}

static void report_latency(char const* name, PauseTiming const& timing, uint64_t press) {
    require(!timing.presentations.empty(), "journal press did not redraw");
    std::cout << "{\"workload\":\"" << name << "\",\"press_to_display_cycles\":"
              << timing.presentations.front().cycle - press << ",\"budget_cycles\":320000}\n";
}

static void verify_journal_timing(Device& d, fs::path const& out) {
    (void)out;
    Snapshot before = d.state();
    Game started = before;
    started.flags.set_flag(FLAG_LANTERN_STARTED);
    d.set_state(started);
    d.tap(BTN_B);
    d.advance(0, 1600000);
    for(unsigned row = 0; row < PAUSE_JOURNAL - PAUSE_RESUME; ++row)
        d.tap(BTN_DOWN);
    {
        PauseTiming opening(d);
        d.tap(BTN_A);
        d.advance(0, 3200000);
        opening.verify("journal_open");
    }
    {
        PauseTiming steady(d);
        d.advance(0, 8000000);
        report_steady("journal_list_steady", steady);
    }
    {
        PauseTiming change(d);
        uint64_t press = d.ab->core_state.cpu.cycle_count;
        d.advance(BTN_A, 320000);
        d.advance(0, 4000000);
        report_latency("journal_open_detail", change, press);
    }
    {
        PauseTiming steady(d);
        d.advance(0, 8000000);
        report_steady("journal_detail_steady", steady);
    }
    report_render_cost("journal_detail_render", d, 20, BTN_A);
    {
        PauseTiming change(d);
        uint64_t press = d.ab->core_state.cpu.cycle_count;
        d.advance(BTN_A, 320000);
        d.advance(0, 4000000);
        report_latency("journal_track", change, press);
    }
    d.tap(BTN_B); // Detail returns to the list.
    report_render_cost("journal_list_render", d, 20, BTN_DOWN);
    d.tap(BTN_B);
    d.advance(0, 8000000);
    d.set_state(before);
}

int main(int argc, char** argv) try {
    require(argc >= 9 && argc <= 13,
            "device_tests package game_address marker_address serial_address "
            "output_directory "
            "normal|fxc|proof|smoke|smoke-functional|millcross|pause|battle|battle-classes|battle-recordings|perk-ranks|bram-meeting|"
            "shop|sounds|"
            "functional|"
            "sounds-functional "
            "dialogue_address phase_address "
            "[battle_pointer_address [battle_ready_address [panel_address [workspace_address]]]]");
    // The journal mode reports Journal frame costs, which may exceed the budget before they are fixed.
    check_timing = std::string(argv[6]) != "functional" && std::string(argv[6]) != "smoke-functional" &&
                   std::string(argv[6]) != "sounds-functional" && std::string(argv[6]) != "journal";
    if(!check_timing)
        std::cout << "{\"timing_checks_enforced\":false,\"mode\":\"" << argv[6] << "\"}\n";
    fs::path package = argv[1], out = argv[5];
    fs::create_directories(out);
    unsigned g = std::stoul(argv[2]), m = std::stoul(argv[3]), s = std::stoul(argv[4]);
    unsigned q = std::stoul(argv[7]);
    Device d(package, g, m, s, q, std::string(argv[6]) == "proof");
    d.phase_addr = std::stoul(argv[8]);
    if(argc >= 10)
        d.battle_pointer_addr = std::stoul(argv[9]);
    if(argc >= 11)
        d.battle_ready_addr = std::stoul(argv[10]);
    if(argc >= 12)
        d.panel_addr = std::stoul(argv[11]);
    if(argc == 13)
        workspace_address_arg = std::stoul(argv[12]);
    d.workspace_addr = workspace_address_arg;
    if(d.proof) {
        d.frame();
        summary("m0_proof", d, 30);
        d.capture(out / "proof-visible.pgm");
        return 0;
    }
    if(std::string(argv[6]) == "idle-long") {
        d.advance(0, 3200000);
        d.tap(BTN_A);
        summary("idle_long", d, 550);
        return 0;
    }

    if(std::string(argv[6]) == "smoke" || std::string(argv[6]) == "smoke-functional") {
        verify_smoke(d, package, out);
        return 0;
    }
    if(std::string(argv[6]) == "battle-classes" || std::string(argv[6]) == "battle-recordings" ||
       std::string(argv[6]) == "perk-ranks" ||
       std::string(argv[6]) == "bram-meeting") {
        d.advance(0, 3200000);
        d.tap(BTN_A);
        d.advance(0, 3200000);
        if(std::string(argv[6]) == "battle-recordings")
            record_battle_review(d, out);
        else if(std::string(argv[6]) == "perk-ranks")
            verify_consecutive_perk_ranks(d, out);
        else if(std::string(argv[6]) == "bram-meeting")
            verify_bram_meeting(d, out);
        else
            verify_battle_classes(d, out);
        return 0;
    }

    if(std::string(argv[6]) == "shop") {
        d.advance(0, 3200000);
        d.tap(BTN_A);
        d.advance(0, 3200000);
        verify_shop(d, out);
        start_grove_contact(d);
        auto victory_capture = out / "shop-grove-victory.pgm";
        play_grove_battle(d, true, nullptr, &victory_capture);

        auto earned = d.state();
        d.tap(BTN_B);
        d.tap(BTN_DOWN); // Save
        d.tap(BTN_A);
        auto sector = d.sector();
        Device restarted(package, g, m, s, q);
        restarted.phase_addr = d.phase_addr;
        restarted.import_sector(sector);
        restarted.advance(0, 3200000);
        restarted.tap(BTN_A);
        restarted.frame();
        auto loaded = restarted.state();
        require(loaded.gold == earned.gold &&
                    !memcmp(&loaded.supplies, &earned.supplies, sizeof(Supplies)),
                "Restart did not restore battle gold and purchased supplies");
        std::cout << "{\"result\":\"PASS\",\"scenario\":\"shop, Grove victory and restart\"}\n";
        return 0;
    }

    d.advance(0, 3200000);
    require(d.serial() == 0, "title unexpectedly advanced exploration");
    require(d.music_enabled(), "the title has no music");
    unsigned title_song = audio_pointer(d, AUDIO_MUSIC_START);
    d.capture(out / "title.pgm");
    {
        // The title never stands still: the flag alone changes it within a second.
        d.ab->advance(0);
        auto before = d.ab->peripherals.display.filtered_pixels;
        d.advance(0, 16000000);
        d.ab->advance(0);
        require(before != d.ab->peripherals.display.filtered_pixels, "title is not animated");
        require(d.serial() == 0, "animated title advanced exploration");
    }
    d.tap(BTN_A);
    require(d.serial() != 0 && d.state().ticks != 0, "new game did not start exploration");
    // The title song fades out and the area's song takes over; the fade lowers the music
    // attenuation while it runs, so the audio checks wait for it.
    require(settled_song(d) != title_song, "starting a game kept the title song");
    if(std::string(argv[6]) == "sounds" || std::string(argv[6]) == "sounds-functional") {
        verify_sounds(d);
        std::cout << "{\"result\":\"PASS\",\"scenario\":\"menu, ambient and mechanism sounds\"}\n";
        std::cout << "{\"worst_exploration_cycles\":" << worst_exploration_cycles
                  << ",\"within_frame_budget\":"
                  << (worst_exploration_cycles <= 320000 ? "true" : "false") << "}\n";
        return 0;
    }
    if(std::string(argv[6]) == "millcross") {
        verify_millcross_views(d, out);
        verify_venlor_quests(d, out);
        std::cout << "{\"result\":\"PASS\",\"scenario\":\"Millcross views, ancient lantern repair and surveyor rescue\"}\n";
        std::cout << "{\"worst_exploration_cycles\":" << worst_exploration_cycles
                  << ",\"within_frame_budget\":true}\n";
        return 0;
    }
    verify_options(d, out);
    verify_pause_party(d, out);
    Game before_perk = d.state();
    verify_perk_choice(d, out);
    d.set_state(before_perk);
    if(std::string(argv[6]) == "pause") {
        std::cout << "{\"result\":\"PASS\",\"scenario\":\"Audio and pause redraw\"}\n";
        return 0;
    }
    if(std::string(argv[6]) == "journal") {
        verify_journal_timing(d, out);
        return 0;
    }
    if(std::string(argv[6]) == "enemy-chase") {
        Game world = d.state();
        world.player = {(uint16_t)(positions::CaveMouth.x * TILE),
                        (uint16_t)((positions::CaveMouth.y + 1) * TILE), NORTH, 0};
        d.set_state(world);
        d.advance(0, 1600000);
        walk_to_area(d, BTN_UP, AREA_CAVE, "chase fixture could not enter Hollow Mine");

        world = d.state();
        world.npcs.entering = 1;
        world.player = {(uint16_t)(positions::MineScout.x * TILE + 40),
                        (uint16_t)(positions::MineScout.y * TILE), WEST, 0};
        d.set_state(world);
        d.frame();

        auto scout = [&] {
            Game current = d.state();
            for(auto const& npc : current.npcs.actors)
                if(npc.id == (NPC_ID)(NPC_ENEMY_FIRST + 1))
                    return npc;
            require(false, "Mine Scout did not spawn via its chunk");
            return Npc{};
        };
        Npc before = scout();
        require(before.behavior == NPC_CHASE, "Mine Scout did not detect the player");
        summary("hollow_mine_chase", d, 20);
        Npc after = scout();
        require(after.actor.x == before.actor.x + 20 && after.actor.y == before.actor.y,
                "Mine Scout did not move one pixel per frame");
        d.wait_for_battle();
        auto battle = d.battle_state();
        require((BATTLEFIELDS_CAVE & battlefield_bit(battle.field)) &&
                    battle.unit_count == d.state().party.count + 1,
                "Scout contact did not deploy the lone wolf on a cave board");
        std::cout << "{\"result\":\"PASS\",\"scenario\":\"Mine Scout pursuit and contact\"}\n";
        return 0;
    }
    if(std::string(argv[6]) == "game-over") {
        Game injured = d.state();
        injured.party.members[0].hp = 1;
        d.set_state(injured);
        start_grove_contact(d);
        unsigned serial = d.serial();
        auto save = d.sector();
        d.tap(BTN_B);
        d.advance(0, 1600000);
        d.tap(BTN_UP); // Pass and lose the last HP.
        d.tap(BTN_A);
        d.advance(0, 4800000);
        d.advance(BTN_A, 8000000);
        d.wait_for_battle(0);
        auto state = d.battle_state();
        require(state.ended && state.result == BATTLE_DEFEAT && !d.state().party.members[0].hp,
                "losing the last HP did not produce defeat");
        verify_game_over_title(d, out, serial, save);
        std::cout << "{\"result\":\"PASS\",\"scenario\":\"Game Over without a save\"}\n";
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
        start_grove_contact(d);
        verify_battle_menu(d, out);
        verify_battle_resign(d, out);
        std::cout << "{\"result\":\"PASS\",\"scenario\":\"battle pause, Audio, team passes and "
                     "Resign\"}\n";
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
    require(d.cached_tile(positions::WellSign.x, positions::WellSign.y) == TILE_SIGN,
            "sign atlas index changed");
    unsigned actors = 0;
    for(auto const& n : d.state().npcs.actors)
        actors += n.id != NPC_NONE;
    require(actors == 3, "the well residents and recruit did not activate");
    verify_millcross_views(d, out);
    verify_shop(d, out);
    verify_party_recruitment(d, out);
    verify_actor_layers(d, out);
    summary("idle_grove", d, 30);
    d.capture(out / "grove.pgm");

    auto before_wolf = d.state();
    place_grove_contact(d);
    {
        FadeTrace trace(d);
        press_with_music_fade(d);
        trace.verify("battle_entry");
    }
    {
        Game current = d.state();
        BattleState deployed = d.battle_state();
        require(deployed.unit_count == current.party.count + 1,
                "encounter did not deploy the saved party plus its enemies");
        for(uint8_t i = 0; i < current.party.count; ++i)
            require(deployed.units[i].id == battle_party_id(i) &&
                        deployed.units[i].hp == current.party.members[i].hp,
                    "battle did not preserve saved party HP and identity");
    }
    d.capture(out / "grove-battle.pgm");
    verify_battle_menu(d, out);
    auto level_up_capture = out / "victory-level-up.pgm";
    play_grove_battle(d, true, &level_up_capture);
    verify_remembers(d, out);
    Game before_resign = d.state();
    start_grove_contact(d);
    verify_battle_resign(d, out);
    before_resign.player = before_wolf.player;
    before_resign.npcs = before_wolf.npcs;
    d.set_state(before_resign);
    d.frame();
    verify_house_fades(d);
    verify_area_music(d);

    // A prepared lantern repair opens dialogue, and B can leave without committing it.
    Game before_social = d.state();
    verify_quest_level_up(d, out);
    d.set_state(before_social);
    d.frame();
    set_test_options(d);

    {
        PauseTiming timing(d);
        d.advance(0, 800000);
        d.tap(BTN_B);
        d.advance(0, 3200000);
        timing.verify("pause_open");
    }
    auto menu_state = d.state();
    unsigned menu_serial = d.serial();
    d.advance(0, 3200000);
    auto after_wait = d.state();
    require(d.serial() == menu_serial && !memcmp(&menu_state, &after_wait, sizeof(Game)),
            "menu did not pause exploration");
    d.verify_music();
    d.capture(out / "menu.pgm");
    require(pause_row_visible(d, 6),
            "overworld menu did not show Map or hid an invalid save incorrectly");

    // Up wraps to Audio. Map owns its loop and returns directly to gameplay.
    d.tap(BTN_UP);
    d.tap(BTN_UP); // Map
    d.tap(BTN_A);
    d.advance(0, 8000000);
    auto after_map = d.state();
    require(d.serial() == menu_serial && !memcmp(&menu_state, &after_map, sizeof(Game)),
            "world map advanced or changed gameplay");
    d.verify_music();
    d.tap(BTN_B);
    d.advance(0, 8000000);
    require(d.serial() != menu_serial, "Map did not resume exploration");
    d.tap(BTN_B);
    d.advance(0, 1600000);
    // Start Lantern Keeper for this menu, so the Journal lists it and its detail view has text to draw.
    Game before_quest = d.state();
    Game quest = before_quest;
    quest.flags.set_flag(FLAG_LANTERN_STARTED);
    d.set_state(quest);
    menu_state = d.state();
    menu_serial = d.serial();
    d.tap(BTN_UP);
    d.tap(BTN_UP);
    d.tap(BTN_UP); // Journal
    d.tap(BTN_A);
    d.tap(BTN_LEFT);
    d.tap(BTN_RIGHT);
    d.advance(0, 8000000);
    d.capture(out / "journal.pgm");
    auto after_journal = d.state();
    require(d.serial() == menu_serial && !memcmp(&menu_state, &after_journal, sizeof(Game)),
            "Journal advanced simulation");
    require(d.ab->core_state.cpu.timer1.ocrNa == 127, "Audio changed the audio rate");
    // A opens the quest's detail view, where the summary is wrapped and the goal is drawn.
    d.tap(BTN_A);
    d.advance(0, 8000000);
    d.capture(out / "journal-detail.pgm");
    auto detail = d.state();
    require(d.serial() == menu_serial && !memcmp(&menu_state, &detail, sizeof(Game)),
            "Journal detail advanced simulation");
    d.tap(BTN_B); // The detail view returns to the list.
    d.advance(0, 8000000);
    d.set_state(before_quest); // Later device checks keep the state they had before this menu.
    d.tap(BTN_B); // Back to gameplay, then reopen the pause menu for Party.
    d.advance(0, 8000000);
    require(d.serial() != menu_serial, "Journal did not resume exploration");
    d.tap(BTN_B);
    d.advance(0, 1600000);
    menu_serial = d.serial();
    d.tap(BTN_UP);
    d.tap(BTN_UP);
    d.tap(BTN_UP);
    d.tap(BTN_UP);
    d.tap(BTN_UP); // Party
    d.tap(BTN_A);
    d.advance(0, 8000000);
    d.capture(out / "party.pgm");
    require(d.serial() == menu_serial, "Party advanced simulation");
    d.tap(BTN_B);
    d.advance(0, 8000000);
    require(d.serial() != menu_serial, "Party did not resume exploration");
    d.tap(BTN_B);
    d.advance(0, 1600000);
    menu_serial = d.serial();
    d.tap(BTN_DOWN); // Save
    d.tap(BTN_A);
    d.tap(BTN_UP); // Resume
    d.tap(BTN_UP); // Wrap to the newly visible Load row and scroll.
    require(pause_row_visible(d, 6), "saved outdoor menu did not render its final row");
    d.capture(out / "menu-saved.pgm");
    auto saved = d.state();
    auto sector = d.sector();
    require((sector[0] | (uint16_t)sector[1] << 8) == sizeof(Game) && sector[2] == SAVE_VERSION &&
                !memcmp(sector.data() + 2, &saved, sizeof(Game)),
            "physical save did not contain the live Game object");
    // Closing with B reverses the slide and restores the world. Reopen and resume with A.
    {
        PauseTiming timing(d);
        d.tap(BTN_B);
        d.frame();
        timing.verify("pause_close");
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
    changed.supplies.quantity[SUPPLY_POTION] = 0;
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
    verify_audio_options(d, 94, 77);
    d.tap(BTN_B);
    d.frame();
    d.tap(BTN_B);
    d.advance(0, 1600000);
    require(pause_row_visible(d, 6),
            "interior menu did not show Load while hiding Map");
    d.tap(BTN_UP); // Load
    {
        PauseTiming timing(d);
        d.tap(BTN_A);
        d.frame();
        timing.verify("pause_load");
    }
    auto menu_loaded = d.state();
    require(!memcmp(&menu_loaded.supplies, &saved.supplies, sizeof(Supplies)),
            "physical load did not restore supplies");
    require(menu_loaded.gold == saved.gold && menu_loaded.area == saved.area &&
                menu_loaded.player.x == saved.player.x && menu_loaded.player.y == saved.player.y &&
                menu_loaded.view.cache.origin_tx == saved.view.cache.origin_tx &&
                menu_loaded.view.cache.origin_ty == saved.view.cache.origin_ty,
            "pause Load did not restore the saved world");
    require(menu_loaded.options.music == 11 && menu_loaded.options.sfx == 9,
            "Load overwrote options edited during this session");
    verify_audio_options(d, 94, 77);
    d.capture(out / "menu-loaded.pgm");

    Device restarted(package, g, m, s, q);
    restarted.phase_addr = d.phase_addr;
    restarted.import_sector(sector);
    restarted.advance(0, 3200000);
    require(restarted.state().gold == 0, "startup validation did not restore new-game defaults");
    require(restarted.state().options.music == 12 && restarted.state().options.sfx == 10,
            "startup did not load saved options");
    verify_audio_options(restarted, 102, 85);
    require(restarted.serial() == 0, "restart did not show title");
    require(restarted.music_enabled(), "the continue title has no music");
    unsigned continue_title_song = audio_pointer(restarted, AUDIO_MUSIC_START);
    restarted.tap(BTN_A);
    restarted.frame();
    auto loaded = restarted.state();
    require(restarted.serial() != 0 && loaded.area == saved.area &&
                loaded.player.x == saved.player.x && loaded.player.y == saved.player.y &&
                loaded.gold == saved.gold,
            "fresh emulator save/load mismatch");
    restarted.verify_music();
    require(settled_song(restarted) != continue_title_song,
            "continuing a game kept the title song");
    restarted.capture(out / "restarted.pgm");
    restarted.hold(BTN_DOWN, 20);
    require(restarted.state().player.y > saved.player.y, "16x16 pixel movement failed");
    summary("pixel_scroll", restarted, 30, BTN_DOWN);
    summary("dense_collision", restarted, 30, BTN_LEFT);
    restarted.capture(out / "forest.pgm");

    // B continues a saved game from the title just as A does.
    Device continued(package, g, m, s, q);
    continued.import_sector(sector);
    continued.advance(0, 3200000);
    continued.tap(BTN_B);
    auto resumed = continued.state();
    require(continued.serial() != 0 && resumed.area == saved.area &&
                resumed.player.x == saved.player.x && resumed.player.y == saved.player.y &&
                resumed.gold == saved.gold,
            "B on the title did not continue the saved game");
    require(resumed.options.music == 12 && resumed.options.sfx == 10,
            "Continue reset startup options");
    settled_song(continued);
    verify_audio_options(continued, 102, 85);
    continued.tap(BTN_B);
    continued.advance(0, 1600000);
    Game changed_continue = continued.state();
    changed_continue.gold = saved.gold + 123;
    continued.set_state(changed_continue);
    continued.tap(BTN_UP); // Wrap to Load on the scrolled viewport.
    continued.tap(BTN_A);
    continued.frame();
    require(continued.serial() != 0 && continued.state().gold == saved.gold,
            "continuing a game lost the save flag checked at startup");

    sector[2] ^= 0x40; // Invalid Game layout version; there is no checksum.
    Device corrupt(package, g, m, s, q);
    corrupt.import_sector(sector);
    corrupt.advance(0, 3200000);
    verify_audio_options(corrupt, 128, 128);
    corrupt.tap(BTN_A); // Startup validation must already have hidden the invalid Load entry.
    corrupt.tap(BTN_B);
    corrupt.advance(0, 1600000);
    corrupt.tap(BTN_UP); // With Load hidden, the last action is Audio.
    unsigned corrupt_serial = corrupt.serial();
    corrupt.tap(BTN_A);
    corrupt.advance(0, 3200000);
    require(corrupt.serial() == corrupt_serial, "startup accepted an invalid save payload");
    corrupt.tap(BTN_B);
    corrupt.frame();

    // With no valid save, B starts a new game as A does.
    Device invalid_continue(package, g, m, s, q);
    invalid_continue.import_sector(sector);
    invalid_continue.advance(0, 3200000);
    invalid_continue.tap(BTN_B);
    require(invalid_continue.serial() != 0 && invalid_continue.state().gold == 0,
            "B on the title did not start a new game over a corrupt save");
    require(invalid_continue.music_enabled(), "new game after a corrupt save has no music");
    std::cout << "{\"result\":\"PASS\",\"scenario\":\"16x16 world, encounters, menus, save/restart and "
                 "scrolling with the animated pause menu\"}\n";
    std::cout << "{\"worst_exploration_cycles\":" << worst_exploration_cycles
              << ",\"within_frame_budget\":"
              << (worst_exploration_cycles <= 320000 ? "true" : "false") << "}\n";
    return 0;
} catch(std::exception const& error) {

    std::cerr << "FAIL " << error.what() << '\n';
    return 1;
}
#endif
