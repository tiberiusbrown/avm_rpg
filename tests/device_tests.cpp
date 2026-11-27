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
#include <bit>
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
    // Set when dialogue opened during the frame, which then waited on the player.
    bool frame_waited = false;
    // Set to leave the next frame out of the budget: it prepares a scene that a test injected.
    bool prepare_next = false;
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
        if(marker && prompt() != DIALOGUE_CLOSED)
            frame_waited = true;
        if(marker != previous_marker) {
            if(marker == 1) {
                frame_waited = prepare_next;
                prepare_next = false;
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
                bool const budgeted = !frame_waited && (proof || state().area == frame_area);
                if(check_timing && budgeted && last.total > 320000)
                    std::cerr << "Over-budget frame: total=" << last.total
                              << " render=" << last.render << " reload=" << last.reload
                              << " chunks=" << last.chunks << " actors/collision=" << last.collision
                              << " serial=" << serial() << " area=" << unsigned(frame_area)
                              << " player=" << state().player.x << ',' << state().player.y
                              << '\n';
                if(budgeted)
                    worst_exploration_cycles = std::max(worst_exploration_cycles, last.total);
                require(!check_timing || !budgeted || last.total <= 320000,
                        "exploration frame exceeded the 320000-cycle budget");
                if(trace)
                    trace.finish(state(), serial(), last, budgeted);
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

    BATTLE_QA_PHASE battle_phase() const {
        return (BATTLE_QA_PHASE)ab->core_state.cpu.data[battle_ready_addr];
    }

    void wait_for_battle(unsigned previous_activations = 256, bool formation = false) {
        require(battle_ready_addr, "missing battle QA readiness address");
        buttons = 0;
        uint64_t deadline = ab->core_state.cpu.cycle_count + 120000000;
        for(;;) {
            auto phase = battle_phase();
            if((formation ? phase == BATTLE_QA_FORMATION
                          : phase == BATTLE_QA_INPUT || phase == BATTLE_QA_RESULT) &&
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

    void accept_formation() {
        wait_for_battle(256, true);
        tap(BTN_B);
        tap(BTN_A);
        wait_for_battle();
    }

    void open_title() {
        advance(0, 3200000);
        tap(BTN_A);
        advance(0, 8000000);
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

// The saved Game of a sector's newest record. Each save appends a record, headed by its size, and
// a load takes the last one.
static uint8_t const* newest_save(std::vector<uint8_t> const& sector) {
    constexpr unsigned RECORD = 2 + sizeof(Game);
    auto holds = [&](unsigned at) {
        return at + RECORD <= 4094 && (sector[at] | (unsigned)sector[at + 1] << 8) == sizeof(Game);
    };
    require(holds(0), "the sector holds no save");
    unsigned at = 0;
    while(holds(at + RECORD))
        at += RECORD;
    return sector.data() + at + 2;
}

// Reads the dialogue that is open to its end, answering every page with A, and returns how many
// pages it read. Lines that follow one another keep the panel open, and are all read.
static unsigned read_dialogue(Device& d) {
    unsigned pages = 0;
    do {
        d.wait_for_prompt();
        require(d.prompt() == DIALOGUE_MORE || d.prompt() == DIALOGUE_DONE,
                "expected a text dialogue prompt");
        d.tap(BTN_A);
        d.advance(0, 1600000);
        require(++pages <= 16, "dialogue exceeded the page bound");
    } while(d.prompt() != DIALOGUE_CLOSED);
    return pages;
}

// Starts exploring from the title of a game without a save, which opens with a line.
static void start_new_game(Device& d) {
    d.tap(BTN_A);
    d.frame();
    require(d.prompt() != DIALOGUE_CLOSED, "a new game did not open with the traveler's line");
    read_dialogue(d);
    d.frame();
}

// Records every step of the story short of the rat's defeat, with every menu unlocked, so that a
// scenario tests its menus and mechanics without the story interrupting it. The facts stay ones
// that a save accepts: the Party menu follows the choice of classes, and so the rat quest.
static void skip_story(Device& d) {
    d.frame();
    Game g = d.state();
    for(FLAG flag : {FLAG_INTRO_SHOWN, FLAG_HOME_STARTED, FLAG_BOULDERS_NOTICED, FLAG_LEFT_CLEARING,
                     FLAG_HOME_REACHED, FLAG_ELIZA_GREETED, FLAG_RAT_ACCEPTED, FLAG_RAT_SIGHTED,
                     FLAG_CLASSES_CHOSEN, FLAG_MENU_JOURNAL, FLAG_MENU_PARTY, FLAG_MENU_SUPPLIES})
        g.flags.set_flag(flag);
    d.set_state(g);
    d.frame();
}

// Moves the player between two frames, as a scene change would find them. NPCs around the new view
// may be cast although they are in sight.
static void place_player(Device& d, uint16_t x, uint16_t y, DIRECTION facing) {
    d.frame();
    Game g = d.state();
    g.player = {x, y, facing, 0};
    g.npcs.entering = 1;
    d.set_state(g);
    d.prepare_next = true;
    d.frame();
}

static void place_player(Device& d, TilePos cell, DIRECTION facing) {
    place_player(d, (uint16_t)(cell.x * TILE), (uint16_t)(cell.y * TILE), facing);
}

// Stands the player under a door and walks in with A.
static void enter_house(Device& d, TilePos door) {
    place_player(d, {door.area, door.x, (uint8_t)(door.y + 1)}, NORTH);
    d.advance(0, 1600000);
    require(d.state().can_interact, "house door did not become interactive");
    d.tap(BTN_A);
    d.advance(0, 4800000);
    require(d.state().area == AREA_INTERIORS, "the door did not enter the house");
}

// Walks out of a house from the floor above its exit.
static void leave_house(Device& d, TilePos exit) {
    place_player(d, {exit.area, exit.x, (uint8_t)(exit.y - 1)}, SOUTH);
    for(unsigned frames = 0; d.state().area == exit.area; ++frames) {
        require(frames < 2 * TILE, "walking onto the house exit did not leave");
        d.frame(BTN_DOWN);
    }
    d.advance(0, 4800000);
}

static Npc find_actor(Device const& d, NPC_ID id) {
    Game g = d.state();
    for(auto const& actor : g.npcs.actors)
        if(actor.id == id)
            return actor;
    return Npc{};
}

// Stands the player just west of a character, which holds still, so that A reaches it.
static void face_npc(Device& d, NPC_ID id) {
    d.frame();
    Game g = d.state();
    for(auto& actor : g.npcs.actors)
        if(actor.id == id) {
            actor.state = NPC_STATE_NONE;
            actor.behavior = NPC_STAND;
            actor.timer = 200;
            g.player = {(uint16_t)(actor.actor.x - 12), actor.actor.y, EAST, 0};
            d.set_state(g);
            d.frame();
            return;
        }
    require(false, "the character to talk to is not present");
}

static void verify_actor_layers(Device& d, fs::path const& out) {
    Game original = d.state();
    Game fixture = original;
    // The fixture stands half a tile left of the spawn and one tile below it.
    uint16_t const spawn_x = positions::PlayerStart.x * TILE,
                   spawn_y = positions::PlayerStart.y * TILE;
    fixture.player = {(uint16_t)(spawn_x - 8), (uint16_t)(spawn_y + 8), SOUTH, 0};
    fixture.npcs = {};
    // Deliberately reverse the NPC slots relative to depth, with the player between.
    fixture.flags.set_flag(FLAG_RAT_REPORTED); // Eliza belongs in the village.
    fixture.npcs.actors[0].id = NPC_ELIZA;
    fixture.npcs.actors[0].assigned_site = positions::ElizaGarden;
    fixture.npcs.actors[0].actor = {(uint16_t)(spawn_x + 4), (uint16_t)(spawn_y + 12), SOUTH, 0};
    fixture.npcs.actors[1].id = NPC_BRAM;
    fixture.npcs.actors[1].assigned_site = positions::BramPost;
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
    compose(expected, rendered.npcs.actors[1].actor, NpcContent<NPC_BRAM>::definition.sprite_row);
    compose(expected, rendered.player, 0);
    compose(expected, rendered.npcs.actors[0].actor, NpcContent<NPC_ELIZA>::definition.sprite_row);
    compose(old_order, rendered.npcs.actors[0].actor, NpcContent<NPC_ELIZA>::definition.sprite_row);
    compose(old_order, rendered.npcs.actors[1].actor, NpcContent<NPC_BRAM>::definition.sprite_row);
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
    require(ram[0xa17] == 128, "Audio changed the default shared master gain");
}

static void verify_options(Device& d, fs::path const& out) {
    require(d.state().options.music == 15 && d.state().options.sfx == 15 &&
                !d.state().options.disabled,
            "new game did not initialize audio defaults");
    verify_audio_options(d, 255, 255);
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
    require(d.music_enabled(), "opening Audio toggled its Enable setting");
    d.tap(BTN_A);
    require(d.state().options.disabled && !d.music_enabled(),
            "disabling audio did not stop playback");
    verify_audio_options(d, 255, 255);
    d.capture(out / "options-disabled.pgm");
    d.tap(BTN_DOWN);
    d.tap(BTN_LEFT);
    verify_audio_options(d, 238, 255);
    require(!d.music_enabled(), "adjusting a slider re-enabled audio");
    d.tap(BTN_UP);
    d.tap(BTN_A);
    require(!d.state().options.disabled && d.music_enabled(),
            "enabling audio did not bring back playback");
    verify_audio_options(d, 238, 255);
    d.tap(BTN_DOWN);
    for(unsigned i = 0; i < 17; ++i)
        d.tap(BTN_LEFT);
    verify_audio_options(d, 0, 255);
    d.capture(out / "options-music-muted.pgm");
    d.advance(BTN_RIGHT, 32000000);
    d.advance(0, 800000);
    verify_audio_options(d, 255, 255);
    require(d.state().options.music == 15, "music did not reach 100 percent");
    d.tap(BTN_RIGHT);
    verify_audio_options(d, 255, 255);
    d.capture(out / "options-music-100.pgm");

    d.tap(BTN_DOWN);
    d.tap(BTN_LEFT);
    d.tap(BTN_LEFT);
    verify_audio_options(d, 255, 221);
    for(unsigned i = 0; i < 17; ++i)
        d.tap(BTN_LEFT);
    verify_audio_options(d, 255, 0);
    d.advance(BTN_RIGHT, 32000000);
    d.advance(0, 800000);
    verify_audio_options(d, 255, 255);
    require(d.state().options.sfx == 15, "SFX did not reach 100 percent");
    d.tap(BTN_RIGHT);
    verify_audio_options(d, 255, 255);

    d.tap(BTN_DOWN); // Wrap to Enable.
    d.tap(BTN_UP); // Wrap back to SFX.
    d.tap(BTN_LEFT);
    verify_audio_options(d, 255, 238);
    d.tap(BTN_RIGHT);
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
    d.tap(BTN_DOWN);
    for(unsigned i = 0; i < 3; ++i)
        d.tap(BTN_LEFT);
    d.tap(BTN_DOWN);
    for(unsigned i = 0; i < 5; ++i)
        d.tap(BTN_LEFT);
    verify_audio_options(d, 204, 170);
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
    milestone.party.members[0].hp = 8;
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
                party_unspent_perks(learned) == 0 && learned.hp == 8 && party_max_hp(learned) == 12,
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

static void start_rat_contact(Device& d);

static void grove_move_cursor(Device& d, GroveCell& cursor, GroveCell target);

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
    d.tap(BTN_DOWN);
    d.tap(BTN_LEFT);
    verify_audio_options(d, 238, 255);
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
    if(d.prompt() != DIALOGUE_CLOSED) {
        read_dialogue(d);
        d.frame();
    }
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
                    next.player.y == positions::PlayerStart.y * TILE && next.party.count == 1 &&
                    next.party.members[0].hp == PARTY_BASE_HP,
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
    place_player(
        d,
        {positions::AldenDoor.area, positions::AldenDoor.x, (uint8_t)(positions::AldenDoor.y + 1)},
        NORTH);
    d.advance(0, 1600000);
    require(d.state().can_interact, "house door did not become interactive");
    uint8_t door_tile = d.cached_tile(positions::AldenDoor.x, positions::AldenDoor.y);
    {
        FadeTrace trace(d);
        d.tap(BTN_A);
        d.advance(0, 4800000);
        trace.verify("house_entry");
    }
    Game inside = d.state();
    require(inside.area == positions::AldenExit.area &&
                inside.player.x == positions::AldenExit.x * TILE &&
                inside.player.y == (positions::AldenExit.y - 1) * TILE &&
                inside.player.facing == NORTH &&
                d.cached_tile(positions::AldenExit.x, positions::AldenExit.y) ==
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
    require(outside.area == AREA_OUTDOOR && outside.player.x == positions::AldenDoor.x * TILE &&
                outside.player.y == (positions::AldenDoor.y + 1) * TILE &&
                outside.player.facing == SOUTH && !outside.player.walking &&
                d.cached_tile(positions::AldenDoor.x, positions::AldenDoor.y) == door_tile,
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

// Starts a battle with the giant rat in Eliza's bedroom, whatever the story has reached.
static void place_rat_contact(Device& d) {
    d.frame();
    if(d.state().area != AREA_INTERIORS)
        enter_house(d, positions::ElizaDoor);
    d.frame();
    Game world = d.state();
    for(FLAG flag : {FLAG_RAT_ACCEPTED, FLAG_RAT_SIGHTED, FLAG_CLASSES_CHOSEN})
        world.flags.set_flag(flag);
    world.flags.clear_flag(FLAG_RAT_DEFEATED);
    world.player = {(uint16_t)(positions::RatNest.x * TILE - TILE - 2),
                    (uint16_t)(positions::RatNest.y * TILE), EAST, 0};
    world.npcs = {};
    world.npcs.entering = 1;
    d.set_state(world);
    d.frame();
    Game ready = d.state();
    auto* rat = std::find_if(ready.npcs.actors, ready.npcs.actors + NPC_CAPACITY,
                             [](Npc const& npc) { return npc.id == NPC_GIANT_RAT; });
    require(rat != ready.npcs.actors + NPC_CAPACITY, "the giant rat did not spawn via its chunk");
    // Hold the rat still beside the player while the frame and fade observers attach.
    rat->state = NPC_STATE_NONE;
    rat->behavior = NPC_STAND;
    rat->policy_pending &= (uint8_t)~NPC_FLAG_PURSUIT;
    rat->timer = 200;
    ready.player.x = (uint16_t)(rat->actor.x - FOOT_WIDTH);
    ready.player.y = rat->actor.y;
    d.set_state(ready);
}

static void start_rat_contact(Device& d) {
    place_rat_contact(d);
    d.tap(BTN_RIGHT);
    d.accept_formation();
}

#ifndef RPG_DEVICE_TEST_LIBRARY
static void verify_battle_formation(Device& d, fs::path const& out) {
    Game world = d.state();
    world.party.count = 2;
    world.party.members[1] = world.party.members[0];
    world.party.members[1].id = NPC_ELIZA;
    world.party.members[1].hp = 11;
    world.party.members[1].attack_class = ATTACK_SHOOT;
    d.set_state(world);
    place_rat_contact(d);
    d.tap(BTN_RIGHT);
    d.wait_for_battle(256, true);
    BattleState initial = d.battle_state();
    require(initial.unit_count == 3, "formation fixture did not deploy two allies and an enemy");
    auto const& field = BATTLEFIELDS[initial.field];
    uint8_t hero = initial.units[0].cell, ally = initial.units[1].cell;
    uint8_t enemy = initial.units[2].cell;
    uint8_t empty = BATTLE_CELLS, blocked = BATTLE_CELLS;
    for(uint8_t y = field.party_area.y; y < field.party_area.y + field.party_area.height; ++y)
        for(uint8_t x = field.party_area.x; x < field.party_area.x + field.party_area.width; ++x) {
            uint8_t cell = battle_cell(x, y);
            if((field.blocked[y] >> x) & 1)
                blocked = cell;
            else if(cell != hero && cell != ally && cell != enemy)
                empty = cell;
        }
    require(empty != BATTLE_CELLS, "formation fixture has no empty starting cell");
    GroveCell cursor{battle_column(hero), battle_row(hero)};
    auto move_to = [&](uint8_t cell) {
        grove_move_cursor(d, cursor, {battle_column(cell), battle_row(cell)});
    };
    auto capture_transfer = [&](char const* name) {
        d.advance(BTN_A, 1280000);
        d.capture(out / name);
        d.advance(0, 3200000);
    };
    auto verify_positions = [&](uint8_t first, uint8_t second) {
        BattleState state = d.battle_state();
        require(!state.ended && !state.activations && state.turn == BATTLE_PARTY,
                "formation input started or consumed a turn");
        require(state.units[0].cell == first && state.units[1].cell == second &&
                    state.units[2].cell == enemy,
                "formation placed a unit in the wrong cell");
        for(uint8_t i = 0; i < state.unit_count; ++i)
            require(state.units[i].hp == initial.units[i].hp &&
                        state.units[i].id == initial.units[i].id &&
                        state.units[i].ap == initial.units[i].ap,
                    "formation changed deployed unit statistics or identity");
        auto party = d.state().party;
        require(!memcmp(&world.party, &party, sizeof(Party)), "formation changed the saved party");
    };

    d.capture(out / "formation.pgm", true);
    d.tap(BTN_A);
    if(blocked != BATTLE_CELLS) {
        move_to(blocked);
        d.tap(BTN_A);
        verify_positions(hero, ally);
    }
    move_to(enemy);
    d.capture(out / "formation-enemy-info.pgm");
    d.tap(BTN_A);
    verify_positions(hero, ally);
    move_to(battle_cell(4, 1));
    d.tap(BTN_A);
    verify_positions(hero, ally);
    move_to(empty);
    capture_transfer("formation-moving.pgm");
    verify_positions(empty, ally);

    d.tap(BTN_A);
    move_to(ally);
    d.capture(out / "formation-swap.pgm");
    capture_transfer("formation-swapping.pgm");
    verify_positions(ally, empty);
    d.tap(BTN_A);
    d.tap(BTN_B);
    verify_positions(ally, empty);

    d.tap(BTN_B | BTN_A);
    require(d.battle_phase() == BATTLE_QA_FORMATION, "opening confirmation also accepted it");
    d.capture(out / "formation-confirm.pgm");
    d.tap(BTN_B);
    verify_positions(ally, empty);
    d.tap(BTN_B);
    d.tap(BTN_A);
    d.wait_for_battle();
    require(d.battle_phase() == BATTLE_QA_INPUT, "accepting formation did not open the first turn");
    verify_positions(ally, empty);
    d.capture(out / "formation-first-turn.pgm");
    std::cout << "{\"result\":\"PASS\",\"scenario\":\"battle formation through player input\"}\n";
}
#endif

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
    start_rat_contact(d);

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
        {"battle_before_starter", MOVEMENT_STEP, ATTACK_STRIKE, battle_cell(2, 1),
         battle_cell(3, 1), battle_cell(3, 1), battle_cell(2, 1), 8, 7, false, 0, false, 2},
        {"battle_starter_unlocked", MOVEMENT_STEP, ATTACK_STRIKE, battle_cell(2, 1),
         battle_cell(3, 1), battle_cell(3, 1), battle_cell(2, 1), 8, 4, true, 0, false, 3},
        {"battle_step", MOVEMENT_STEP, ATTACK_STRIKE, battle_cell(1, 1), battle_cell(2, 1),
         battle_cell(3, 1), battle_cell(2, 1), 8, 9, false},
        {"battle_charge", MOVEMENT_CHARGE, ATTACK_STRIKE, battle_cell(1, 1), battle_cell(3, 1),
         battle_cell(4, 1), battle_cell(3, 1), 7, 7, false},
        {"battle_vault", MOVEMENT_VAULT, ATTACK_STRIKE, battle_cell(1, 1), battle_cell(3, 1),
         battle_cell(2, 1), battle_cell(3, 1), 7, 7, false},
        {"battle_glide", MOVEMENT_GLIDE, ATTACK_STRIKE, battle_cell(1, 0), battle_cell(3, 2),
         battle_cell(3, 1), battle_cell(3, 2), 7, 8, false},
        {"battle_arc", MOVEMENT_STEP, ATTACK_ARC, battle_cell(3, 1), battle_cell(4, 2),
         battle_cell(4, 2), battle_cell(3, 1), 8, 3, true},
        {"battle_drain", MOVEMENT_STEP, ATTACK_DRAIN, battle_cell(2, 1), battle_cell(3, 1),
         battle_cell(3, 1), battle_cell(2, 1), 11, 3, true, 3},
        {"battle_reach", MOVEMENT_STEP, ATTACK_REACH, battle_cell(1, 1), battle_cell(3, 1),
         battle_cell(3, 1), battle_cell(1, 1), 8, 3, true, 3},
        {"battle_recoil", MOVEMENT_LEAP, ATTACK_RECOIL, battle_cell(2, 1), battle_cell(3, 1),
         battle_cell(3, 1), battle_cell(1, 1), 8, 2, true},
        {"battle_flank", MOVEMENT_STEP, ATTACK_FLANK, battle_cell(2, 1), battle_cell(3, 1),
         battle_cell(3, 1), battle_cell(2, 1), 8, 4, false},
        {"battle_blast", MOVEMENT_STEP, ATTACK_BLAST, battle_cell(2, 1), battle_cell(5, 1),
         battle_cell(5, 1), battle_cell(2, 1), 8, 4, true, 3},
        {"battle_trample", MOVEMENT_CHARGE, ATTACK_STRIKE, battle_cell(1, 1), battle_cell(3, 1),
         battle_cell(2, 1), battle_cell(3, 1), 7, 7, false, 4},
        {"battle_throughshot", MOVEMENT_STEP, ATTACK_SHOOT, battle_cell(0, 1), battle_cell(5, 1),
         battle_cell(5, 1), battle_cell(0, 1), 8, 1, true, 4},
        {"battle_split_shot", MOVEMENT_STEP, ATTACK_SHOOT, battle_cell(1, 1), battle_cell(5, 1),
         battle_cell(5, 1), battle_cell(1, 1), 8, 4, true, 4, true},
        {"battle_breach_cut", MOVEMENT_STEP, ATTACK_STRIKE, battle_cell(2, 1), battle_cell(3, 1),
         battle_cell(3, 1), battle_cell(3, 1), 8, 0, true, 4, true},
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
        state.units[0] = BattleUnit{test.actor, 8, battle_party_id(0), BATTLE_PARTY};
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
                resumed.flags.get_flag(FLAG_RAT_DEFEATED) && !resumed.remember_timer,
            "the victory did not persist before returning to exploration");
    for(auto const& npc : resumed.npcs.actors)
        require(npc.id != NPC_GIANT_RAT, "the defeated rat remained active");
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

// Houses play the village's song, which carries on as the player goes in and out.
static void verify_area_music(Device& d) {
    Game original = d.state();
    unsigned outdoor = settled_song(d);
    enter_house(d, positions::AldenDoor);
    require(settled_song(d) == outdoor, "entering a house changed the song");
    summary("indoor_music", d, 30);
    leave_house(d, positions::AldenExit);
    require(settled_song(d) == outdoor, "leaving a house changed the song");
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
    street.player = {(uint16_t)(positions::AldenDoor.x * TILE),
                     (uint16_t)((positions::AldenDoor.y + 1) * TILE), NORTH, 0};
    street.npcs.entering = 1;
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

// A boulder grinds as it rolls, and a push that cannot move it answers with a short tone.
static void verify_boulder_sounds(Device& d) {
    Game original = d.state();
    auto stand = [&](TilePos cell, DIRECTION facing) {
        place_player(d, cell, facing);
        d.advance(0, 1600000);
        wait_for_quiet_sfx(d);
        d.advance(0, 800000);
    };
    auto resting = [&](NPC_ID id, TilePos cell) {
        Npc boulder = find_actor(d, id);
        return boulder.id == id && boulder.actor.x == cell.x * TILE &&
               boulder.actor.y == cell.y * TILE;
    };
    constexpr TilePos a = positions::BoulderA, c = positions::BoulderC;

    // The boulder west of C blocks a push from its east side.
    stand({c.area, (uint8_t)(c.x + 1), c.y}, WEST);
    require(d.state().can_interact, "the boulder did not become interactive");
    SfxWatch blocked = watch_sfx(d, BTN_A, 16000000, 1);
    require(blocked.runs.size() == 1 && blocked.runs[0] < 300 * CYCLES_PER_MS,
            "a blocked push did not answer with a short tone");
    require(resting(NPC_BOULDER_C, c), "a blocked boulder moved");

    stand({a.area, a.x, (uint8_t)(a.y - 1)}, SOUTH);
    SfxWatch pushed = watch_sfx(d, BTN_A, 32000000, 1);
    require(pushed.runs.size() == 1 && pushed.runs[0] > 400 * CYCLES_PER_MS,
            "a rolling boulder was not heard grinding");
    d.advance(0, 1600000);
    require(resting(NPC_BOULDER_A, {a.area, a.x, (uint8_t)(a.y + 1)}),
            "the boulder did not roll one tile");
    d.set_state(original);
    d.advance(0, 1600000);
}

// Menus tick, a gust is heard as wind, and boulders grind or refuse.
static void verify_sounds(Device& d) {
    verify_menu_sounds(d);
    verify_ambient_sounds(d);
    verify_boulder_sounds(d);
}

static void press_with_music_fade(Device& d) {
    struct Trace {
        Device& device;
        std::vector<uint8_t> levels;
        uint8_t previous;
        void (*observer)(void*, bool);
        void* observer_context;
    } trace{d, {}, 255, d.cycle_observer, d.observer_context};

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
        require(ram[0xa17] == 128 && ram[0xa72] == 255, "music fade changed master or SFX gain");
    };
    d.advance(BTN_RIGHT, 800000);
    // Let the battle entry finish while the longer audio fade is still running.
    d.advance(0, 4600000);
    require(d.battle_address() && d.ab->core_state.cpu.data[0xa73],
            "battle did not accept contact while music was fading");
    auto display = d.ab->peripherals.display.ram;
    d.tap(BTN_LEFT);
    require(d.ab->peripherals.display.ram != display,
            "music fade blocked input or display updates");
    d.tap(BTN_RIGHT); // Restore the menu selection or battle cursor.
    d.advance(0, 16000000);
    d.cycle_observer = trace.observer;
    d.observer_context = trace.observer_context;
    require(trace.levels.size() > 40 && trace.levels.back() == 255 &&
                d.ab->core_state.cpu.data[0xa73] == 0,
            "song switch did not complete its fade and restore the configured volume");
    for(unsigned i = 1; i + 1 < trace.levels.size(); ++i)
        require(trace.levels[i] < trace.levels[i - 1], "music fade did not ramp down");
}

// Telling Eliza that the rat is gone completes her quest, and its experience earns a level.
static void verify_quest_level_up(Device& d, fs::path const& out) {
    Game original = d.state();
    enter_house(d, positions::AldenDoor);
    d.frame();
    Game fixture = d.state();
    for(FLAG flag : {FLAG_RAT_ACCEPTED, FLAG_RAT_SIGHTED, FLAG_CLASSES_CHOSEN, FLAG_RAT_DEFEATED})
        fixture.flags.set_flag(flag);
    fixture.party.members[0].level = 1;
    fixture.party.members[0].experience = 8;
    fixture.party.members[0].perk_ranks = 0;
    d.set_state(fixture);
    face_npc(d, NPC_ELIZA);
    d.tap(BTN_A);
    require(d.prompt() != DIALOGUE_CLOSED, "Eliza did not answer");
    read_dialogue(d);
    d.advance(0, 3200000);
    Snapshot rewarded = d.state();
    require(rewarded.get_flag(FLAG_RAT_REPORTED) && rewarded.gold == fixture.gold + 10 &&
                rewarded.party.members[0].level == 2 &&
                party_unspent_perks(rewarded.party.members[0]) == 1,
            "reporting the rat did not earn a level and a perk choice");
    unsigned serial = d.serial();
    d.advance(0, 3200000);
    require(d.serial() == serial, "quest completion panel did not wait for dismissal");
    d.capture(out / "rat-quest-complete.pgm");
    d.tap(BTN_A);
    d.advance(0, 3200000);
    require(d.serial() == serial, "quest level-up notice did not wait for dismissal");
    d.capture(out / "quest-level-up.pgm");
    d.tap(BTN_A);
    d.frame();
    require(d.serial() != serial && party_unspent_perks(d.state().party.members[0]) == 1,
            "dismissing the quest notice did not resume exploration with a pending perk");
    leave_house(d, positions::AldenExit);
    d.set_state(original);
    d.advance(0, 1600000);
}

static void verify_equipment(Device& d, fs::path const& out) {
    Game fixture = d.state();
    for(uint8_t id = 0; id < ITEM_DEFINED_COUNT; ++id)
        require(!fixture.get_flag(equipment_flag(id)), "new game starts with owned equipment");
    fixture.flags.set_flag(equipment_flag(ITEM_FIRSTSTRIKE_SWORD));
    fixture.party.members[1] = {12, 4, MOVEMENT_STEP, ATTACK_SHOOT, NPC_ELIZA, 1, 0, 0, {}};
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
    d.tap(BTN_RIGHT); // The second member
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
    require(d.state().party.members[0].hp == PARTY_BASE_HP &&
                !d.state().supplies.quantity[SUPPLY_ELIXIR],
            "confirmed Elixir did not heal the party");
    require(d.serial() == serial, "Supplies advanced exploration");
    d.tap(BTN_B);
    d.frame();
    require(d.serial() != serial, "Supplies did not resume exploration");
    original.supplies = d.state().supplies;
    d.set_state(original);
}

static void enter_exploration_with_fade(Device& d, uint8_t button) {
    auto serial = d.serial();
    FadeTrace trace(d);
    d.advance(button, (FADE_STEPS + 2) * 320000);
    require(d.serial() == serial, "exploration advanced during the title transition");
    d.frame();

    auto lit_pixels = [](FadeTrace::Frame const& frame) {
        return std::accumulate(frame.pixels.begin(), frame.pixels.end(), 0,
                               [](int total, uint8_t pixels) { return total + std::popcount(pixels); });
    };
    auto black = std::find_if(trace.frames.begin() + 1, trace.frames.end(),
                              [&](FadeTrace::Frame const& frame) { return !lit_pixels(frame); });
    require(black - trace.frames.begin() >= 3 && trace.frames.end() - black > 3,
            "the title transition did not present black between the scenes");
    require(lit_pixels(*(black - 3)) > lit_pixels(*(black - 1)) &&
                lit_pixels(*(black + 1)) < lit_pixels(*(black + 3)),
            "the title transition did not fade out and back in");
}

static void verify_smoke(Device& d, fs::path const& package, fs::path const& out) {
    d.advance(0, 3200000);
    require(d.serial() == 0 && !d.state().options.disabled && !d.music_enabled(),
            "audio started before the title");
    d.capture(out / "pre-title.pgm");
    d.tap(BTN_B);
    require(d.state().options.disabled && !d.music_enabled(), "B did not mute from pre-title");
    d.capture(out / "pre-title-muted.pgm");
    d.tap(BTN_B);
    require(!d.state().options.disabled && !d.music_enabled(),
            "choosing enabled audio started playback before the title");
    d.advance(BTN_A, 2500000);
    require(!d.music_enabled(), "music started while the logo was moving");
    d.capture(out / "pre-title-moving.pgm");
    d.advance(BTN_A, 4000000);
    require(!d.music_enabled(), "music started while the title scene was fading in");
    d.capture(out / "pre-title-fade-in.pgm");
    d.advance(BTN_A, 6300000);
    require(d.serial() == 0 && d.music_enabled(), "held A skipped the animated title");
    d.advance(0);
    d.capture(out / "title.pgm");
    enter_exploration_with_fade(d, BTN_A);
    require(d.prompt() != DIALOGUE_CLOSED, "a new game did not open with the traveler's line");
    read_dialogue(d);
    skip_story(d);
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
    restarted.open_title();
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
    require(party_movement_perk_rank(member) == 1 && member.hp == 8 && party_max_hp(member) == 12 &&
                party_unspent_perks(member) == 0,
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
    started.flags.set_flag(FLAG_HOME_STARTED);
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

// Waits for a panel that holds exploration still, such as a quest notice, captures it and closes
// it.
static void dismiss_notice(Device& d, fs::path const& capture) {
    d.advance(0, 4800000);
    unsigned serial = d.serial();
    d.advance(0, 1600000);
    require(d.serial() == serial && d.prompt() == DIALOGUE_CLOSED, "expected a waiting notice");
    d.capture(capture, true);
    d.tap(BTN_A);
    d.advance(0, 800000);
}

// Runs exploration, holding a button, until dialogue opens.
static void play_until_dialogue(Device& d, uint8_t held, char const* message) {
    for(unsigned frames = 0; d.prompt() == DIALOGUE_CLOSED; ++frames) {
        require(frames < 200, message);
        d.frame(held);
    }
}

// Captures the first page of the open dialogue, then reads it through.
static unsigned read_captured(Device& d, fs::path const& capture) {
    d.wait_for_prompt();
    d.capture(capture);
    return read_dialogue(d);
}

// The whole of the opening story on the device: waking in the clearing, the boulder gate, home,
// Eliza's rat, the choice of classes, the fight and the report, then a save and a restart.
// A save taken among the pushed boulders holds the pool: a restart brings each back where it lay,
// around the traveler.
static void verify_saved_boulders(Device& d, fs::path const& package, fs::path const& out) {
    d.frame();
    d.tap(BTN_B);
    d.advance(0, 1600000);
    d.tap(BTN_DOWN); // Save, immediately below Resume.
    d.tap(BTN_A);
    d.advance(0, 1600000);
    Game saved = d.state();
    auto sector = d.sector();
    require(!memcmp(newest_save(sector), &saved, sizeof(Game)), "the boulders were not saved");
    d.tap(BTN_B);
    d.advance(0, 1600000);

    Device restarted(package, d.game_addr, d.marker_addr, d.serial_addr, d.dialogue_addr);
    restarted.phase_addr = d.phase_addr;
    restarted.import_sector(sector);
    restarted.open_title();
    restarted.tap(BTN_A);
    restarted.frame();
    Game loaded = restarted.state();
    require(loaded.player.x == saved.player.x && loaded.player.y == saved.player.y,
            "a restart moved the traveler from among the boulders");
    for(NPC_ID id = NPC_BOULDER_A; id <= NPC_BOULDER_D; id = (NPC_ID)(id + 1)) {
        Npc before{};
        for(auto const& actor : saved.npcs.actors)
            if(actor.id == id)
                before = actor;
        Npc after = find_actor(restarted, id);
        require(before.id == id && after.id == id && after.actor.x == before.actor.x &&
                    after.actor.y == before.actor.y,
                "a restart did not restore a pushed boulder");
    }
    restarted.capture(out / "08-restarted-gate.pgm", true);
}

static void verify_story(Device& d, fs::path const& package, fs::path const& out) {
    d.open_title();
    d.tap(BTN_A);
    play_until_dialogue(d, 0, "the story did not open");
    require(read_captured(d, out / "01-wake.pgm") == 1, "waking took more than one line");
    d.frame();
    require(d.state().area == AREA_OUTDOOR && !d.state().flags.get_flag(FLAG_HOME_STARTED),
            "the story moved on while the player stood still");
    d.tap(BTN_B);
    d.advance(0, 1600000);
    require(pause_row_visible(d, 3) && !pause_row_visible(d, 4),
            "the opening menu did not hold only Resume, Save, Map and Audio");
    d.capture(out / "02-opening-menu.pgm", true);
    d.tap(BTN_B);
    d.frame();

    play_until_dialogue(d, BTN_RIGHT, "wandering did not start the way home");
    read_captured(d, out / "03-way-home.pgm");
    dismiss_notice(d, out / "04-go-home-notice.pgm");
    play_until_dialogue(d, 0, "the Journal hint did not appear");
    read_captured(d, out / "05-journal-hint.pgm");
    d.frame();
    require(d.state().flags.get_flag(FLAG_MENU_JOURNAL), "the Journal did not unlock");

    // Walking east toward the gate casts the boulders as they come within reach.
    place_player(d,
                 {AREA_OUTDOOR, (uint8_t)(positions::GateMouth.x - 8),
                  (uint8_t)(positions::GateMouth.y + 1)},
                 EAST);
    play_until_dialogue(d, BTN_RIGHT, "the blocked way out went unnoticed");
    require(read_captured(d, out / "06-boulders.pgm") >= 2, "the boulder lines were cut short");
    d.frame();
    summary("approach_gate_east", d, 56, BTN_RIGHT);
    constexpr NPC_ID BOULDERS[] = {NPC_BOULDER_A, NPC_BOULDER_B, NPC_BOULDER_C, NPC_BOULDER_D};
    for(NPC_ID id : BOULDERS)
        require(find_actor(d, id).id == id, "walking up to the gate did not cast every boulder");
    d.capture(out / "07-gate.pgm", true);

    // Four pushes open the gate. The content test proves this order, and that no order traps.
    struct Push {
        NPC_ID boulder;
        TilePos from;
        DIRECTION facing;
        TilePos to;
    };

    constexpr TilePos a = positions::BoulderA, b = positions::BoulderB, c = positions::BoulderC;
    constexpr Push PUSHES[] = {
        {NPC_BOULDER_B,
         {b.area, b.x, (uint8_t)(b.y + 1)},
         NORTH,
         {b.area, b.x, (uint8_t)(b.y - 1)}},
        {NPC_BOULDER_C, {c.area, (uint8_t)(c.x - 1), c.y}, EAST, {c.area, (uint8_t)(c.x + 1), c.y}},
        {NPC_BOULDER_C, c, EAST, {c.area, (uint8_t)(c.x + 2), c.y}},
        {NPC_BOULDER_A,
         {a.area, a.x, (uint8_t)(a.y + 1)},
         NORTH,
         {a.area, a.x, (uint8_t)(a.y - 1)}},
    };
    unsigned index = 0;
    for(Push const& push : PUSHES) {
        place_player(d, push.from, push.facing);
        d.frame();
        require(d.state().can_interact, "a boulder of the solution could not be reached");
        d.tap(BTN_A);
        d.advance(0, 8000000);
        Npc boulder = find_actor(d, push.boulder);
        require(boulder.id == push.boulder && boulder.actor.x == push.to.x * TILE &&
                    boulder.actor.y == push.to.y * TILE,
                "a boulder of the solution did not roll");
        d.capture(out / ("08-push-" + std::to_string(++index) + ".pgm"), true);
    }
    verify_saved_boulders(d, package, out);
    place_player(d, positions::ClearingExit, EAST);
    d.frame();
    require(d.state().flags.get_flag(FLAG_LEFT_CLEARING), "leaving the clearing went unnoticed");
    summary("village_gate", d, 30);
    d.capture(out / "09-past-the-gate.pgm", true);

    // Far from their chunk the boulders unload; walking back casts them at their sites again.
    place_player(d,
                 {AREA_OUTDOOR, (uint8_t)(positions::ClearingExit.x + 8),
                  (uint8_t)(positions::ClearingExit.y + 1)},
                 WEST);
    d.frame();
    for(NPC_ID id : BOULDERS)
        require(find_actor(d, id).id == NPC_NONE, "a boulder outlived its chunk");
    summary("approach_gate_west", d, 112, BTN_LEFT);
    for(NPC_ID id : BOULDERS)
        require(find_actor(d, id).id == id, "walking back did not cast every boulder");
    require(find_actor(d, NPC_BOULDER_A).actor.y == positions::BoulderA.y * TILE &&
                find_actor(d, NPC_BOULDER_C).actor.x == positions::BoulderC.x * TILE,
            "an unloaded boulder did not return to its site");

    enter_house(d, positions::AldenDoor);
    dismiss_notice(d, out / "10-home-reward.pgm");
    play_until_dialogue(d, 0, "Eliza did not greet the traveler");
    read_captured(d, out / "11-eliza-greets.pgm");
    d.frame();
    face_npc(d, NPC_ELIZA);
    d.tap(BTN_A);
    for(unsigned page = 0; page < 8; ++page) {
        d.wait_for_prompt();
        if(d.prompt() == DIALOGUE_CHOICE)
            break;
        d.tap(BTN_A);
    }
    require(d.prompt() == DIALOGUE_CHOICE, "Eliza did not ask for help");
    d.capture(out / "12-eliza-asks.pgm");
    d.tap(BTN_A);
    d.advance(0, 1600000);
    read_captured(d, out / "13-eliza-thanks.pgm");
    dismiss_notice(d, out / "14-rat-quest-notice.pgm");
    leave_house(d, positions::AldenExit);

    enter_house(d, positions::ElizaDoor);
    d.frame();
    d.capture(out / "15-eliza-home.pgm", true);
    place_player(
        d, {positions::RatNest.area, (uint8_t)(positions::RatNest.x - 2), positions::RatNest.y},
        EAST);
    play_until_dialogue(d, 0, "the rat went unnoticed");
    read_captured(d, out / "16-rat-sighted.pgm");

    // The class screen: choose Leap and Reach, reading each description.
    d.advance(0, 8000000);
    d.capture(out / "17-movement-list.pgm", true);
    d.tap(BTN_DOWN);
    d.tap(BTN_A);
    d.advance(0, 1600000);
    d.capture(out / "18-leap.pgm", true);
    d.tap(BTN_A);
    d.advance(0, 1600000);
    d.tap(BTN_DOWN);
    d.tap(BTN_DOWN);
    d.capture(out / "19-attack-list.pgm", true);
    d.tap(BTN_A);
    d.advance(0, 1600000);
    d.capture(out / "20-reach.pgm", true);
    d.tap(BTN_A);
    d.advance(0, 1600000);
    d.capture(out / "21-confirm.pgm", true);
    d.tap(BTN_A);
    d.advance(0, 4800000);
    play_until_dialogue(d, 0, "the Party hint did not appear");
    read_captured(d, out / "22-party-hint.pgm");
    PartyMember chosen = d.state().party.members[0];
    require(chosen.movement_class == MOVEMENT_LEAP && chosen.attack_class == ATTACK_REACH &&
                d.state().flags.get_flag(FLAG_MENU_PARTY),
            "the class choice did not take");

    // The rat now chases, and reaches the traveler.
    d.buttons = 0;
    d.wait_for_battle(256, true);
    d.capture(out / "23-formation.pgm", true);
    d.accept_formation();
    d.capture(out / "24-battle.pgm", true);
    BattleState fight = d.battle_state();
    require(fight.unit_count == 2 && fight.units[1].id == ENEMY_GIANT_RAT &&
                (BATTLEFIELDS_HOUSE & battlefield_bit(fight.field)),
            "the fight is not the lone rat indoors");
    // Finish it from a known position: one thrust fells a rat with 1 HP.
    fight.field = BATTLEFIELD_GROVE;
    fight.units[0].cell = battle_cell(1, 1);
    fight.units[1].cell = battle_cell(2, 1);
    fight.units[1].hp = 1;
    d.set_battle(fight);
    for(unsigned i = 1; i < BATTLE_COLUMNS; ++i)
        d.tap(BTN_LEFT);
    for(unsigned i = 1; i < BATTLE_ROWS; ++i)
        d.tap(BTN_UP);
    GroveCell cursor{0, 0};
    grove_move_cursor(d, cursor, {1, 1});
    d.tap(BTN_A);
    grove_move_cursor(d, cursor, {2, 1});
    d.tap(BTN_A);
    d.wait_for_battle(0);
    require(d.battle_state().ended && d.battle_state().result == BATTLE_VICTORY,
            "the thrust did not win the fight");
    d.advance(0, 1600000);
    d.capture(out / "25-victory.pgm", true);
    d.tap(BTN_A);
    d.advance(0, 4800000);
    d.frame();
    require(d.state().flags.get_flag(FLAG_RAT_DEFEATED) && d.state().area == AREA_INTERIORS &&
                d.state().gold == 3,
            "the victory did not return to Eliza's home");

    d.tap(BTN_B);
    d.advance(0, 1600000);
    d.tap(BTN_DOWN);
    d.tap(BTN_DOWN);
    d.tap(BTN_A); // Party
    d.advance(0, 8000000);
    d.capture(out / "26-party.pgm", true);
    d.tap(BTN_B);
    d.advance(0, 8000000);

    leave_house(d, positions::ElizaExit);
    enter_house(d, positions::AldenDoor);
    d.frame();
    face_npc(d, NPC_ELIZA);
    d.tap(BTN_A);
    require(read_captured(d, out / "27-report.pgm") >= 2, "the report was cut short");
    dismiss_notice(d, out / "28-rat-reward.pgm");
    dismiss_notice(d, out / "29-level-up.pgm");
    d.frame();
    Game finished = d.state();
    require(finished.flags.get_flag(FLAG_RAT_REPORTED) && finished.gold == 13 &&
                finished.party.members[0].level == 2,
            "the report did not pay its reward");

    d.tap(BTN_B);
    d.advance(0, 1600000);
    d.tap(BTN_DOWN);
    d.tap(BTN_DOWN);
    d.tap(BTN_DOWN);
    d.tap(BTN_A); // Journal
    d.advance(0, 8000000);
    d.capture(out / "30-journal.pgm", true);
    d.tap(BTN_A);
    d.advance(0, 8000000);
    d.capture(out / "31-journal-detail.pgm", true);
    d.tap(BTN_B);
    d.tap(BTN_B);
    d.advance(0, 8000000);

    leave_house(d, positions::AldenExit);
    place_player(d,
                 {positions::ElizaGarden.area, positions::ElizaGarden.x,
                  (uint8_t)(positions::ElizaGarden.y + 2)},
                 NORTH);
    d.frame();
    require(find_actor(d, NPC_ELIZA).id == NPC_ELIZA, "Eliza did not return to the village");
    summary("village_garden", d, 30);
    face_npc(d, NPC_ELIZA);
    d.tap(BTN_A);
    require(d.prompt() == DIALOGUE_CLOSED, "Eliza spoke again after her quest");
    d.capture(out / "32-village.pgm", true);
    place_player(
        d, {positions::BramPost.area, positions::BramPost.x, (uint8_t)(positions::BramPost.y - 2)},
        SOUTH);
    summary("village_bridge", d, 30);
    d.capture(out / "33-bram.pgm", true);

    d.tap(BTN_B);
    d.advance(0, 1600000);
    d.tap(BTN_DOWN); // Save
    d.tap(BTN_A);
    auto saved = d.state();
    auto sector = d.sector();
    require(!memcmp(newest_save(sector), &saved, sizeof(Game)), "the story was not saved");
    Device restarted(package, d.game_addr, d.marker_addr, d.serial_addr, d.dialogue_addr);
    restarted.phase_addr = d.phase_addr;
    restarted.import_sector(sector);
    restarted.open_title();
    restarted.tap(BTN_A);
    restarted.frame();
    Game loaded = restarted.state();
    require(restarted.prompt() == DIALOGUE_CLOSED && loaded.gold == saved.gold &&
                !memcmp(&loaded.flags, &saved.flags, sizeof(WorldFlags)) &&
                !memcmp(&loaded.party, &saved.party, sizeof(Party)),
            "a restart did not resume the story where it was saved");
    std::cout << "{\"result\":\"PASS\",\"scenario\":\"the opening story, saved and resumed\"}\n";
}

int main(int argc, char** argv) try {
    require(argc >= 9 && argc <= 13,
            "device_tests package game_address marker_address serial_address "
            "output_directory "
            "normal|fxc|proof|smoke|smoke-functional|story|pause|battle|battle-classes|battle-"
            "formation|battle-recordings|perk-ranks|"
            "sounds|"
            "functional|"
            "sounds-functional "
            "dialogue_address phase_address "
            "[battle_pointer_address [battle_ready_address [panel_address [workspace_address]]]]");
    // The journal mode reports Journal frame costs, which may exceed the budget before they are fixed.
    check_timing = check_timing && std::string(argv[6]) != "functional" &&
                   std::string(argv[6]) != "smoke-functional" &&
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
        d.open_title();
        d.tap(BTN_A);
        summary("idle_long", d, 550);
        return 0;
    }

    if(std::string(argv[6]) == "smoke" || std::string(argv[6]) == "smoke-functional") {
        verify_smoke(d, package, out);
        return 0;
    }
    if(std::string(argv[6]) == "story") {
        verify_story(d, package, out);
        return 0;
    }
    if(std::string(argv[6]) == "battle-classes" || std::string(argv[6]) == "battle-formation" ||
       std::string(argv[6]) == "battle-recordings" || std::string(argv[6]) == "perk-ranks") {
        d.open_title();
        start_new_game(d);
        skip_story(d);
        if(std::string(argv[6]) == "battle-recordings")
            record_battle_review(d, out);
        else if(std::string(argv[6]) == "perk-ranks")
            verify_consecutive_perk_ranks(d, out);
        else if(std::string(argv[6]) == "battle-formation")
            verify_battle_formation(d, out);
        else
            verify_battle_classes(d, out);
        return 0;
    }

    d.open_title();
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
    start_new_game(d);
    require(d.serial() != 0 && d.state().ticks != 0, "new game did not start exploration");
    // The title song fades out and the area's song takes over; the fade lowers the music
    // attenuation while it runs, so the audio checks wait for it.
    require(settled_song(d) != title_song, "starting a game kept the title song");
    skip_story(d);
    if(std::string(argv[6]) == "sounds" || std::string(argv[6]) == "sounds-functional") {
        verify_sounds(d);
        std::cout << "{\"result\":\"PASS\",\"scenario\":\"menu, ambient and boulder sounds\"}\n";
        std::cout << "{\"worst_exploration_cycles\":" << worst_exploration_cycles
                  << ",\"within_frame_budget\":"
                  << (worst_exploration_cycles <= 320000 ? "true" : "false") << "}\n";
        return 0;
    }
    verify_options(d, out);
    verify_pause_party(d, out);
    Game before_perk = d.state();
    verify_perk_choice(d, out);
    d.set_state(before_perk);
    if(std::string(argv[6]) == "pause") {
        open_audio(d);
        d.tap(BTN_A);
        d.tap(BTN_B);
        d.frame();
        d.tap(BTN_B);
        d.advance(0, 1600000);
        d.tap(BTN_DOWN); // Save
        d.tap(BTN_A);
        auto sector = d.sector();

        Device muted(package, g, m, s, q);
        muted.import_sector(sector);
        muted.advance(0, 3200000);
        require(muted.state().options.disabled && !muted.music_enabled(),
                "pre-title did not load the saved disabled setting");
        muted.tap(BTN_B);
        require(!muted.state().options.disabled && !muted.music_enabled(),
                "enabling audio on the pre-title did not stay silent");
        muted.tap(BTN_B);
        muted.open_title();
        require(muted.state().options.disabled && !muted.music_enabled(),
                "muting on the pre-title did not silence the title");
        muted.tap(BTN_A);
        muted.frame();
        require(!muted.music_enabled(), "Continue re-enabled muted audio");
        open_audio(muted, true);
        muted.tap(BTN_A);
        require(!muted.state().options.disabled && muted.music_enabled(),
                "re-enabling audio did not start the area's music");

        // The previous save format stored two plain bytes with levels of up to 30.
        size_t options = 2 + offsetof(Game, options);
        sector[options] = 30;
        sector[options + 1] = 20;
        Device legacy(package, g, m, s, q);
        legacy.import_sector(sector);
        legacy.open_title();
        require(!legacy.state().options.disabled && legacy.music_enabled(),
                "legacy save did not default to enabled audio");
        verify_audio_options(legacy, 255, 255);
        legacy.tap(BTN_A);
        legacy.frame();
        require(legacy.state().gold == before_perk.gold && legacy.state().options.music == 15 &&
                    legacy.state().options.sfx == 15,
                "legacy save did not retain progress and cap both audio levels");
        // Open Audio before the title-to-area fade finishes.
        for(uint8_t button : {BTN_B, BTN_UP, BTN_UP, BTN_A}) {
            legacy.advance(button, 640000);
            legacy.advance(0, 1600000);
        }
        auto const& ram = legacy.ab->core_state.cpu.data;
        require(ram[AUDIO_FADE_VOLUME] && ram[AUDIO_MUSIC_VOLUME] < 255,
                "audio toggle fixture did not reach the fade");
        unsigned queued = audio_pointer(legacy, AUDIO_FADE_SONG);
        legacy.tap(BTN_A);
        require(!legacy.music_enabled(), "disabling audio left the fading song playing");
        legacy.tap(BTN_A);
        require(legacy.music_enabled() && audio_pointer(legacy, AUDIO_MUSIC_START) == queued,
                "enabling audio did not start the queued song");
        verify_audio_options(legacy, 255, 255);

        std::cout << "{\"result\":\"PASS\",\"scenario\":\"Audio and pause redraw\"}\n";
        return 0;
    }
    if(std::string(argv[6]) == "journal") {
        verify_journal_timing(d, out);
        return 0;
    }
    if(std::string(argv[6]) == "enemy-chase") {
        enter_house(d, positions::ElizaDoor);
        d.frame();
        Game world = d.state();
        for(FLAG flag : {FLAG_RAT_ACCEPTED, FLAG_RAT_SIGHTED, FLAG_CLASSES_CHOSEN})
            world.flags.set_flag(flag);
        world.npcs = {};
        world.npcs.entering = 1;
        world.player = {(uint16_t)(positions::RatNest.x * TILE - 40),
                        (uint16_t)(positions::RatNest.y * TILE), EAST, 0};
        d.set_state(world);
        d.frame();

        auto rat = [&] {
            Npc found = find_actor(d, NPC_GIANT_RAT);
            require(found.id == NPC_GIANT_RAT, "the rat did not spawn via its chunk");
            return found;
        };
        Npc before = rat();
        require(before.behavior == NPC_CHASE, "the rat did not chase the player");
        summary("rat_chase", d, 20);
        Npc after = rat();
        require(after.actor.x < before.actor.x, "the chasing rat did not close in");
        d.accept_formation();
        auto battle = d.battle_state();
        require((BATTLEFIELDS_HOUSE & battlefield_bit(battle.field)) &&
                    battle.unit_count == d.state().party.count + 1,
                "rat contact did not deploy the lone rat on a house board");
        std::cout << "{\"result\":\"PASS\",\"scenario\":\"rat pursuit and contact\"}\n";
        return 0;
    }
    if(std::string(argv[6]) == "game-over") {
        Game injured = d.state();
        injured.party.members[0].hp = 1;
        d.set_state(injured);
        start_rat_contact(d);
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
        start_rat_contact(d);
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
    unsigned actors = 0;
    for(auto const& n : d.state().npcs.actors)
        actors += n.id != NPC_NONE;
    require(actors == 0, "the clearing has company");
    verify_actor_layers(d, out);
    summary("idle_clearing", d, 30);
    d.capture(out / "clearing.pgm");

    auto before_rat = d.state();
    place_rat_contact(d);
    {
        FadeTrace trace(d);
        press_with_music_fade(d);
        trace.verify("battle_entry");
    }
    d.accept_formation();
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
    d.capture(out / "rat-battle.pgm");
    verify_battle_menu(d, out);
    auto level_up_capture = out / "victory-level-up.pgm";
    play_grove_battle(d, true, &level_up_capture);
    start_rat_contact(d);
    verify_battle_resign(d, out);
    d.set_state(before_rat);
    d.frame();
    verify_house_fades(d);
    verify_area_music(d);

    // Eliza's quest completes and earns a level.
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
    // The Journal lists the finished way home, so its detail view has text to draw.
    Game before_quest = d.state();
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
    changed.player = {(uint16_t)(positions::AldenDoor.x * TILE),
                      (uint16_t)((positions::AldenDoor.y + 1) * TILE), NORTH, 0};
    d.set_state(changed);
    d.advance(0, 1600000);
    d.tap(BTN_A);
    d.advance(0, 4800000);
    require(d.state().area == AREA_INTERIORS, "load fixture did not enter the house");
    open_audio(d, true);
    d.tap(BTN_DOWN);
    d.tap(BTN_LEFT);
    d.tap(BTN_DOWN);
    d.tap(BTN_LEFT);
    verify_audio_options(d, 187, 153);
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
    verify_audio_options(d, 187, 153);
    d.capture(out / "menu-loaded.pgm");

    Device restarted(package, g, m, s, q);
    restarted.phase_addr = d.phase_addr;
    restarted.import_sector(sector);
    restarted.open_title();
    require(restarted.state().gold == 0, "startup validation did not restore new-game defaults");
    require(restarted.state().options.music == 12 && restarted.state().options.sfx == 10,
            "startup did not load saved options");
    verify_audio_options(restarted, 204, 170);
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
    continued.open_title();
    enter_exploration_with_fade(continued, BTN_B);
    auto resumed = continued.state();
    require(continued.serial() != 0 && resumed.area == saved.area &&
                resumed.player.x == saved.player.x && resumed.player.y == saved.player.y &&
                resumed.gold == saved.gold,
            "B on the title did not continue the saved game");
    require(resumed.options.music == 12 && resumed.options.sfx == 10,
            "Continue reset startup options");
    settled_song(continued);
    verify_audio_options(continued, 204, 170);
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
    corrupt.open_title();
    verify_audio_options(corrupt, 255, 255);
    // Startup validation must already have hidden the invalid Load entry.
    start_new_game(corrupt);
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
    invalid_continue.open_title();
    invalid_continue.tap(BTN_B);
    invalid_continue.frame();
    require(invalid_continue.prompt() != DIALOGUE_CLOSED,
            "a new game did not open with the traveler's line");
    read_dialogue(invalid_continue);
    invalid_continue.frame();
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
