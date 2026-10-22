// Host-only observer: shipped AVR/guest code and peripheral behavior are unchanged.
#define RPG_DEVICE_TEST_LIBRARY
#include "device_tests.cpp"

#include <map>
#include <sstream>
#include <tuple>

struct TimingSample {
    uint64_t due = 0, entry = 0, write = 0;
    unsigned due_pc = 0, guest_pc = 0, mask_pc = 0;
    bool synthesis = false;
    std::string scenario, blocker;
};

struct TimingObserver {
    Device& device;
    std::map<std::string, unsigned> symbols;
    std::map<std::tuple<std::string, std::string, std::string, int64_t>, uint64_t> histogram;
    struct Blackout { uint64_t count = 0, total = 0, maximum = 0; };
    std::map<unsigned, Blackout> blackouts;
    std::ofstream outliers;
    std::string scenario = "startup";
    TimingSample pending;
    uint64_t next_due = 0, before_cycle = 0, last_write = 0, last_synthesis = 0;
    uint64_t mask_begin = 0, samples = 0, due_count = 0;
    unsigned before_pc = 0, mask_pc = 0;
    unsigned entry_pc = 0, endpoint_pc = 0;
    int interrupt_vector = -1;
    bool before_active = false, before_i = false, masked = false;
    int64_t minimum = 100000, maximum = 0;
    static constexpr uint64_t PERIOD = 1024;
    static constexpr uint64_t ENTRY_CYCLES = 7; // AVR response 4 + vector JMP 3.
    static constexpr uint64_t WRITE_CYCLES = 14; // Body entry to completed OCR4A STS.

    absim::atmega32u4_t& cpu() { return device.ab->core_state.cpu; }

    unsigned word(unsigned byte_address) {
        return cpu().prog[byte_address] | unsigned(cpu().prog[byte_address + 1]) << 8;
    }

    TimingObserver(Device& d, fs::path const& listing, fs::path const& prefix) : device(d) {
        std::ifstream input(listing);
        require(bool(input), "interpreter symbol listing unavailable");
        for(std::string line; std::getline(input, line);) {
            std::istringstream row(line);
            unsigned address;
            std::string scope, section, size, name;
            if(row >> std::hex >> address >> scope >> section >> size >> name)
                if(section == ".text")
                    symbols[name] = address / 2;
        }
        entry_pc = symbols.at("timer1_compa_isr");
        endpoint_pc = symbols.at("timer1_endpoint_write");
        auto address = endpoint_pc * 2;
        require((word(address) & 0xfe0f) == 0x9200 &&
                    word(address + 2) == absim::reg::addr::OCR4A,
                "listing does not match the package PWM store");
        outliers.open(prefix.string() + "-outliers.csv");
        require(bool(outliers), "cannot create timing outlier CSV");
        outliers << "scenario,path,due_cycle,entry_cycle,write_cycle,lateness_cycles,"
                    "blocker,mask_pc_bytes,due_pc_bytes,guest_pc\n";
        device.observer_context = this;
        device.cycle_observer = [](void* context, bool after) {
            static_cast<TimingObserver*>(context)->observe(after);
        };
    }

    void bin(std::string const& path, std::string const& metric, int64_t value) {
        ++histogram[{pending.scenario, path, metric, value}];
    }

    void complete() {
        require(pending.entry && pending.write, "incomplete Timer1 observation");
        require(pending.entry >= pending.due + ENTRY_CYCLES, "negative ISR entry delay");
        require(pending.write - pending.entry == WRITE_CYCLES,
                "PWM write latency changed in the synthesis ISR");
        int64_t delay = int64_t(pending.entry - pending.due - ENTRY_CYCLES);
        require(delay < int64_t(PERIOD), "Timer1 missed an output deadline");
        auto path = pending.synthesis ? "synthesis" : "midpoint";
        bin(path, "entry_lateness", delay);
        bin(path, "pwm_lateness", int64_t(pending.write - pending.due - ENTRY_CYCLES - WRITE_CYCLES));
        bin(path, "isr_body_cycles", int64_t(cpu().cycle_count - pending.entry));
        if(last_write)
            bin(path, "pwm_interval_error", int64_t(pending.write - last_write) - int64_t(PERIOD));
        if(pending.synthesis && last_synthesis)
            bin(path, "synthesis_interval_error", int64_t(pending.entry - last_synthesis) - int64_t(PERIOD));
        last_write = pending.write;
        if(pending.synthesis)
            last_synthesis = pending.entry;
        minimum = std::min(minimum, delay);
        maximum = std::max(maximum, delay);
        ++samples;
        if(delay >= 10) {
            outliers << pending.scenario << ',' << path << ',' << pending.due << ','
                     << pending.entry << ',' << pending.write << ',' << delay << ','
                     << pending.blocker << ',' << pending.mask_pc * 2 << ','
                     << pending.due_pc * 2 << ',' << pending.guest_pc << '\n';
        }
        pending = {};
    }

    void observe(bool after) {
        auto& c = cpu();
        if(!after) {
            before_cycle = c.cycle_count;
            before_pc = c.pc;
            before_active = c.active;
            before_i = (c.data[absim::reg::addr::SREG] & 128) != 0;
            if(!next_due && device.music_enabled()) {
                require(c.timer1.divider == 8 && c.timer1.top == 127,
                        "expected 15625 Hz Timer1 CTC output");
                require(!(c.data[absim::reg::addr::TIFR1] & 2), "observer attached with IRQ pending");
                uint64_t ticks = c.timer1.ocrNa > c.timer1.tcnt ?
                    c.timer1.ocrNa - c.timer1.tcnt :
                    c.timer1.top + 1 + c.timer1.ocrNa - c.timer1.tcnt;
                next_due = c.timer_sync.prev_update_cycle +
                    8 - c.timer_sync.prescaler_cycle % 8 + (ticks - 1) * 8;
            }
            return;
        }

        bool after_i = (c.data[absim::reg::addr::SREG] & 128) != 0;
        if(c.just_interrupted) {
            interrupt_vector = c.pc;
            if(pending.due && !pending.entry && interrupt_vector != 0x22)
                pending.blocker = interrupt_vector == 0x2a ? "timer0" : "other_isr";
        } else if(before_active && before_i && !after_i) {
            masked = true;
            mask_pc = before_pc;
            mask_begin = before_cycle;
        }

        if(next_due && next_due <= c.cycle_count) {
            require(!pending.due, "Timer1 compare coalesced with an unserviced interrupt");
            require(next_due >= before_cycle, "timer deadline tracking drifted");
            pending.due = next_due;
            pending.scenario = scenario;
            pending.due_pc = before_pc;
            pending.guest_pc = c.data[4] | unsigned(c.data[5]) << 8 | unsigned(c.data[6]) << 16;
            if(interrupt_vector != -1 && interrupt_vector != 0x22)
                pending.blocker = interrupt_vector == 0x2a ? "timer0" : "other_isr";
            else if(masked) {
                pending.blocker = "cli";
                pending.mask_pc = mask_pc;
            } else
                pending.blocker = before_active ? "instruction_boundary" : "sleep_wakeup";
            next_due += PERIOD;
            ++due_count;
        }
        if(masked && after_i) {
            uint64_t span = c.cycle_count - mask_begin;
            auto& block = blackouts[mask_pc];
            ++block.count;
            block.total += span;
            block.maximum = std::max(block.maximum, span);
            masked = false;
        }
        if(c.active && c.pc == entry_pc) {
            require(pending.due && !pending.entry, "unpaired Timer1 entry");
            pending.entry = c.cycle_count;
            pending.synthesis = true;
        }
        if(before_active && before_pc == endpoint_pc)
            pending.write = c.cycle_count;
        if(before_active && word(before_pc * 2) == 0x9518) { // RETI
            if(interrupt_vector == 0x22)
                complete();
            interrupt_vector = -1;
        }
    }

    void finish(fs::path const& prefix) {
        while(pending.due)
            device.step();
        device.cycle_observer = nullptr;
        require(samples == due_count && samples > 100, "timing sample accounting failed");
        std::ofstream hist(prefix.string() + "-histogram.csv");
        hist << "scenario,path,metric,delta_cycles,count\n";
        for(auto const& [key, count] : histogram) {
            auto const& [name, path, metric, delta] = key;
            hist << name << ',' << path << ',' << metric << ',' << delta << ',' << count << '\n';
        }
        std::ofstream blocks(prefix.string() + "-blackouts.csv");
        blocks << "cli_pc_bytes,count,total_cycles,max_cycles\n";
        for(auto const& [pc, block] : blackouts)
            blocks << pc * 2 << ',' << block.count << ',' << block.total << ',' << block.maximum << '\n';
        std::cout << prefix.filename().string() << ": " << samples << " IRQs, delay "
                  << minimum << ".." << maximum << " cycles\n";
    }
};

int main(int argc, char** argv) try {
    require(argc == 12, "audio_timing package listing game marker serial dialogue phase output seconds variant calibration");
    fs::path package = argv[1], listing = argv[2], out = argv[8];
    fs::create_directories(out);
    unsigned g = std::stoul(argv[3]), m = std::stoul(argv[4]), s = std::stoul(argv[5]);
    unsigned q = std::stoul(argv[6]), phase = std::stoul(argv[7]);
    unsigned seconds = std::stoul(argv[9]);
    std::string variant = argv[10];
    bool calibration = std::string(argv[11]) == "1";
    for(unsigned trial = 0; trial < (calibration ? 2u : 1u); ++trial) {
        Device d(package, g, m, s, q);
        d.phase_addr = phase;
        d.advance(0, 3200000);
        d.tap(BTN_A);
        require(d.music_enabled(), "RPG did not enable music");
        while(!d.ab->core_state.cpu.active ||
              !(d.ab->core_state.cpu.data[absim::reg::addr::SREG] & 128) ||
              (d.ab->core_state.cpu.data[absim::reg::addr::TIFR1] & 2))
            d.step();
        auto prefix = out / (calibration ? (trial ? "calibration-cli20" : "calibration-nop") : variant);
        if(calibration) {
            auto& c = d.ab->core_state.cpu;
            c.st(absim::reg::addr::TIMSK0, 0);
            c.st(absim::reg::addr::TCCR0B, 0);
            unsigned address = 0x200;
            auto emit = [&](unsigned instruction) {
                c.prog[address++] = uint8_t(instruction);
                c.prog[address++] = uint8_t(instruction >> 8);
            };
            if(trial)
                emit(0x94f8); // CLI
            for(unsigned i = 0; i < (trial ? 20u : 1u); ++i)
                emit(0x0000); // NOP
            if(trial) {
                emit(0x9478); // SEI
                emit(0x0000); // NOP (SEI defers interrupt acceptance one instruction)
            }
            emit(0xc000 | ((0x100 - (address / 2 + 1)) & 0x0fff));
            c.decode();
            c.pc = 0x100;
            c.prev_sreg |= 128;
            TimingObserver monitor(d, listing, prefix);
            monitor.scenario = trial ? "cli20" : "nop";
            d.advance(0, 4000000);
            monitor.finish(prefix);
            require(monitor.minimum == 0, "observer does not reproduce zero-delay baseline");
            require(trial ? (monitor.maximum >= 20 && monitor.maximum <= 25) : monitor.maximum <= 1,
                    "observer failed known instruction/CLI delay calibration");
        } else {
            TimingObserver monitor(d, listing, prefix);
            monitor.scenario = "idle_song";
            d.advance(0, uint64_t(seconds) * 16000000);
            if(variant == "normal" && seconds >= 8) {
                monitor.scenario = "dialogue";
                d.tap(BTN_A);
                require(d.prompt() != DIALOGUE_CLOSED, "timing dialogue scenario did not open");
                d.advance(0, 32000000);
                for(unsigned i = 0; i < 8 && d.prompt() != DIALOGUE_CLOSED; ++i)
                    d.tap(BTN_A);
                require(d.prompt() == DIALOGUE_CLOSED, "timing dialogue did not close");
                monitor.scenario = "menu_status";
                d.tap(BTN_B);
                d.advance(0, 32000000);
                d.tap(BTN_A);
                d.advance(0, 16000000);
                d.tap(BTN_B);
                d.tap(BTN_DOWN);
                monitor.scenario = "save";
                d.tap(BTN_A);
                d.advance(0, 8000000);
                auto paused_serial = d.serial();
                d.tap(BTN_B);
                require(d.serial() != paused_serial, "timing scenario did not leave the save menu");
                monitor.scenario = "walking";
                auto start = d.state().player;
                d.advance(BTN_DOWN, 48000000);
                require(d.state().player.y > start.y, "timing walking scenario did not move");
                d.advance(BTN_LEFT, 48000000);
                auto blocked = d.state().player;
                require(!blocked.walking, "timing collision setup did not reach a wall");
                monitor.scenario = "collision";
                d.advance(BTN_LEFT, 32000000);
                require(d.state().player.x == blocked.x && d.state().player.y == blocked.y,
                        "timing collision scenario did not remain blocked");
            }
            monitor.finish(prefix);
        }
    }
    return 0;
} catch(std::exception const& error) {
    std::cerr << "FAIL " << error.what() << '\n';
    return 1;
}
