#include "battle.h"
#include "chunk.h"
#include "map_positions.h"
#include "tile_ids.h"
#include <avm.h>

volatile uint8_t world_phase;
volatile uint16_t probe_checks, probe_failure;
static unsigned dialogues;

void fade_world_out() {}

void fade_world_in() {}

bool dialogue_show(char const PROGMEM*) {
    ++dialogues;
    return true;
}

bool dialogue_ask(char const PROGMEM*) {
    return true;
}

// The probes do not run the battle screen. Every battle is a win.
BATTLE_RESULT battle_show(BattleEncounter const PROGMEM*) {
    return BATTLE_VICTORY;
}

static void check(bool value, uint16_t line) {
    probe_checks = (uint16_t)(probe_checks + 1);
    if(!value && !probe_failure)
        probe_failure = line;
}

#define CHECK(value) check((value), __LINE__)
#include "world_scenarios.h"

extern "C" RPG_NOINLINE void probe_done() {
    avm_debug_break();
}

extern "C" void setup() {
    run_world_scenarios();
    game_new();
    game_update(0, BTN_A);
    // The sign shows its welcome, runs a battle (stubbed here), and then shows the outcome line.
    CHECK(dialogues == 2 && !frame_pressed);
    game.gold = 19;
    CHECK(save_restore() && game.gold == 19 && game.player.x == positions::PlayerStart.x * TILE);
    game.save_version = 3;
    CHECK(!save_restore());
    probe_done();
}

extern "C" void loop() {
    avm_idle();
}
