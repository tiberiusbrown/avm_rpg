// AVM timing probe for the battle search. It runs the search on representative positions and a few
// micro-kernels, and it stops at probe_mark between phases. avm-lldb reads `avm time` at each stop, so the
// difference between a phase's start and end marks is the emulated cost of that phase.
#include "battle.h"
#include "battle_ai.h"
#include <avm.h>

volatile uint8_t probe_phase;
volatile uint8_t probe_marks;
volatile int16_t probe_score;
volatile uint8_t probe_unit, probe_dest;
volatile uint16_t probe_nodes, probe_leaves, probe_checksum;
// Zero at run time. The compiler cannot prove it, so each kernel iteration keeps its real work.
volatile uint8_t probe_zero;

// The probe stops here at every phase boundary. avm-lldb sets a breakpoint on this function.
extern "C" RPG_NOINLINE void probe_mark() {
    probe_marks = (uint8_t)(probe_marks + 1);
}

static uint8_t cell(unsigned x, unsigned y) {
    return (uint8_t)(y * BATTLE_COLUMNS + x);
}

// Scenarios and kernel buffers are static, so the probe's own stack frames stay small.
static BattleState scenario;
static BattleAction legal[16];

static void grove(BATTLE_TEAM turn, uint8_t activations) {
    battle_begin(scenario, &BATTLE_GROVE);
    scenario.turn = turn;
    scenario.activations = activations;
}

// Runs one search between two marks. The results are stored before the end mark, so lldb reads them there.
static void measure_search(uint8_t phase, BattleState const& s) {
    probe_phase = phase;
    probe_mark();
    BattleSearchResult r = battle_search(s, BATTLE_AI_DEPTH);
    BattleSearchStats st = battle_search_stats();
    probe_score = r.score;
    probe_unit = r.action.unit_index;
    probe_dest = r.action.destination;
    probe_nodes = st.nodes;
    probe_leaves = st.leaves;
    probe_phase = (uint8_t)(phase + 1);
    probe_mark();
}

static constexpr unsigned KERNEL_LOOPS = 100;
// A RAM copy of the movement types, used only to compare its read cost with the program-memory table.
static uint8_t ram_movement[BATTLE_UNIT_TYPE_COUNT] = {0, 0, 0, 0};

// The opening position at a given depth, between two marks, so the depth increments show the cost per level.
static void measure_depth(uint8_t phase, BattleState const& s, uint8_t depth) {
    probe_phase = phase;
    probe_mark();
    BattleSearchResult r = battle_search(s, depth);
    BattleSearchStats st = battle_search_stats();
    probe_score = r.score;
    probe_nodes = st.nodes;
    probe_leaves = st.leaves;
    probe_phase = (uint8_t)(phase + 1);
    probe_mark();
}

// Each kernel runs KERNEL_LOOPS times between two marks. Every iteration copies the scenario and changes its
// activation count by probe_zero, which only the probe can read. The copy-only kernel gives the overhead
// that the other kernels include, so it can be subtracted.
static void run_kernels(BattleState const& base) {
    unsigned count = 0;
    BattleActionCursor cursor = battle_actions(base);
    BattleAction action{};
    while(battle_next_action(base, cursor, action))
        legal[count++] = action;

    uint16_t sum = 0;

    probe_phase = 20;
    probe_mark();
    for(unsigned i = 0; i < KERNEL_LOOPS; ++i) {
        BattleState t = base;
        t.activations = (uint8_t)(t.activations + probe_zero);
        sum = (uint16_t)(sum + t.turn);
    }
    probe_checksum = sum;
    probe_phase = 21;
    probe_mark();

    probe_phase = 22;
    probe_mark();
    for(unsigned i = 0; i < KERNEL_LOOPS; ++i) {
        BattleState t = base;
        t.activations = (uint8_t)(t.activations + probe_zero);
        sum = (uint16_t)(sum + (uint16_t)battle_score(t, BATTLE_ENEMY, 0));
    }
    probe_checksum = sum;
    probe_phase = 23;
    probe_mark();

    probe_phase = 24;
    probe_mark();
    for(unsigned i = 0; i < KERNEL_LOOPS; ++i) {
        BattleState t = base;
        t.activations = (uint8_t)(t.activations + probe_zero);
        BattleActionCursor c = battle_actions(t);
        BattleAction a{};
        while(battle_next_action(t, c, a))
            sum = (uint16_t)(sum + 1);
    }
    probe_checksum = sum;
    probe_phase = 25;
    probe_mark();

    probe_phase = 26;
    probe_mark();
    for(unsigned i = 0; i < KERNEL_LOOPS; ++i) {
        BattleState t = base;
        t.activations = (uint8_t)(t.activations + probe_zero);
        BattleActionMasks masks = battle_action_masks(t);
        sum = (uint16_t)(sum + masks.kind[BATTLE_ACTION_MOVE]);
    }
    probe_checksum = sum;
    probe_phase = 27;
    probe_mark();

    probe_phase = 28;
    probe_mark();
    for(unsigned i = 0; i < KERNEL_LOOPS; ++i) {
        BattleState t = base;
        t.activations = (uint8_t)(t.activations + probe_zero);
        battle_apply(t, legal[0]);
        sum = (uint16_t)(sum + t.activations);
    }
    probe_checksum = sum;
    probe_phase = 29;
    probe_mark();

    // Program-memory table reads against an identical RAM copy, with an index only the probe can change.
    probe_phase = 30;
    probe_mark();
    for(unsigned i = 0; i < KERNEL_LOOPS; ++i) {
        uint8_t index = (uint8_t)((probe_zero + i) & 3);
        sum = (uint16_t)(sum + BATTLE_UNIT_TYPES[index].movement_type);
    }
    probe_checksum = sum;
    probe_phase = 31;
    probe_mark();

    probe_phase = 32;
    probe_mark();
    for(unsigned i = 0; i < KERNEL_LOOPS; ++i) {
        uint8_t index = (uint8_t)((probe_zero + i) & 3);
        sum = (uint16_t)(sum + ram_movement[index]);
    }
    probe_checksum = sum;
    probe_phase = 33;
    probe_mark();
}

extern "C" void setup() {
    // The shared guest checks require the single game object, so the probe builds one like the other probes.
    game_new();
    grove(BATTLE_ENEMY, 0);
    measure_search(2, scenario);

    grove(BATTLE_ENEMY, 0);
    scenario.units[0].cell = cell(4, 1);
    measure_search(4, scenario);

    grove(BATTLE_ENEMY, 0);
    scenario.units[1].hp = 0;
    scenario.units[2].hp = 0;
    scenario.units[0].hp = 3;
    scenario.units[0].cell = cell(2, 1);
    scenario.units[3].hp = 2;
    scenario.units[3].cell = cell(3, 2);
    measure_search(6, scenario);

    grove(BATTLE_ENEMY, 19);
    measure_search(8, scenario);

    grove(BATTLE_ENEMY, 21);
    measure_search(10, scenario);

    grove(BATTLE_ENEMY, 0);
    measure_depth(40, scenario, 1);
    measure_depth(42, scenario, 2);
    measure_depth(44, scenario, 3);

    // The first enemy turn in play, after the party's opening move of HERO_A to (2,0).
    grove(BATTLE_ENEMY, 1);
    scenario.units[0].cell = cell(2, 0);
    measure_search(12, scenario);

    grove(BATTLE_ENEMY, 0);
    run_kernels(scenario);

    probe_phase = 34;
    probe_mark();
}

extern "C" void loop() {
    avm_idle();
}
