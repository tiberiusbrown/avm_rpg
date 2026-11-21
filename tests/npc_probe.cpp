#include "battle.h"
#include "rpg.h"
#include "sfx.h"
#include <avm.h>

volatile uint8_t world_phase;
volatile uint16_t probe_checks, probe_failure;

void fade_world_out() {}

void fade_world_in() {}

void dialogue_close() {}

void party_joined(NPC_ID) {}

// The probes make no sound.
void sfx_play(SFX, SFX) {}

void sfx_ambient(SFX) {}

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

bool dialogue_show(NPC_ID id, char const PROGMEM* words) {
    (void)id;
    return dialogue_show(words);
}

bool dialogue_ask(char const PROGMEM*) {
    ++npc_test_choices;
    return true;
}

bool dialogue_ask(NPC_ID id, char const PROGMEM* question) {
    (void)id;
    return dialogue_ask(question);
}

// The probes do not run the battle screen. Every battle is a win.
BATTLE_RESULT battle_show(BattleEncounter const PROGMEM*) {
    return BATTLE_VICTORY;
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
