#include "chunk.h"
#include "map_positions.h"
#include "tile_ids.h"
#include <avm.h>

volatile uint8_t world_phase;
volatile uint16_t probe_checks, probe_failure;
static unsigned dialogues;

bool dialogue_show(char const PROGMEM*) {
    ++dialogues;
    return true;
}

bool dialogue_ask(char const PROGMEM*) {
    return true;
}

static void check(bool value, uint16_t line) {
    probe_checks = (uint16_t)(probe_checks + 1);
    if(!value && !probe_failure)
        probe_failure = line;
}

#define CHECK(value) check((value), __LINE__)
#include "world_scenarios.h"
static uint8_t record[SAVE_BYTES];

extern "C" RPG_NOINLINE void probe_done() {
    avm_debug_break();
}

extern "C" void setup() {
    run_world_scenarios();
    game_new();
    game_update(0, BTN_A);
    CHECK(dialogues == 1 && !frame_pressed);
    game.gold = 19;
    save_encode(record);
    CHECK(save_decode(record) && game.gold == 19 && game.player.x == 112);
    record[2] = 3;
    CHECK(!save_decode(record));
    probe_done();
}

extern "C" void loop() {
    avm_idle();
}
