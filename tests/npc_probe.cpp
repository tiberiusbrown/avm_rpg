#include "rpg.h"
#include <avm.h>

volatile uint8_t world_phase;
volatile uint16_t probe_checks, probe_failure;

static void check(bool value, uint16_t line) {
    probe_checks = (uint16_t)(probe_checks + 1);
    if(!value && !probe_failure) {
        probe_failure = line;
        avm_debug_break();
    }
}

#define CHECK(value) check((value), __LINE__)
#include "npc_scenarios.h"

bool dialogue_show(char const PROGMEM*) {
    ++npc_test_dialogues;
    return true;
}

bool dialogue_ask(char const PROGMEM*) {
    ++npc_test_choices;
    return true;
}

extern "C" RPG_NOINLINE void probe_done() {
    avm_debug_break();
}

extern "C" void setup() {
    run_npc_scenarios();
    probe_done();
}

extern "C" void loop() {
    avm_idle();
}
