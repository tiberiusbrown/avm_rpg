// AVM timing probe for the battle search. It runs the enemy's decision on representative positions,
// the complete search at each depth, and a few micro-kernels, and it stops at probe_mark between
// phases. avm-lldb reads `avm time` at each stop, so the difference between a phase's start and end
// marks is the emulated cost of that phase.
#include "battle.h"
#include "battle_ai.h"
#include <avm.h>

volatile uint8_t probe_phase;
volatile uint8_t probe_marks;
volatile int16_t probe_score;
volatile uint8_t probe_unit, probe_dest;
volatile uint16_t probe_nodes, probe_leaves, probe_checksum;
volatile uint8_t probe_completed, probe_exhausted;
// Zero at run time. The compiler cannot prove it, so each kernel iteration keeps its real work.
volatile uint8_t probe_zero;

// The probe stops here at every phase boundary. avm-lldb sets a breakpoint on this function.
extern "C" RPG_NOINLINE void probe_mark() {
    probe_marks = (uint8_t)(probe_marks + 1);
}

static uint8_t cell(unsigned x, unsigned y) {
    return (uint8_t)(y * BATTLE_COLUMNS + x);
}

static constexpr uint8_t ROGUE = 0, ARCHER = 1, WOLF = 2, BRUTE = 3;

// Scenarios and kernel buffers are static, so the probe's own stack frames stay small.
static BattleState scenario;
static uint8_t legal[BATTLE_ACTION_CAPACITY];

static void grove(BATTLE_TEAM turn, uint8_t activations) {
    battle_begin(scenario, &BATTLE_GROVE);
    scenario.turn = turn;
    scenario.activations = activations;
}

static void record(BattleAction action) {
    BattleSearchStats st = battle_search_stats();
    probe_unit = action.unit_index;
    probe_dest = action.destination;
    probe_nodes = st.nodes;
    probe_leaves = st.leaves;
    probe_completed = st.completed;
    probe_exhausted = st.exhausted;
}

// Runs the enemy's whole decision between two marks, as the battle screen calls it. The results are
// stored before the end mark, so lldb reads them there.
static void measure_decision(uint8_t phase, BattleState const& s) {
    probe_phase = phase;
    probe_mark();
    BattleAction action = battle_enemy_action(s);
    record(action);
    probe_score = 0;
    probe_phase = (uint8_t)(phase + 1);
    probe_mark();
}

// The complete search at one depth, between two marks, so the depth increments show the cost per
// level.
static void measure_depth(uint8_t phase, BattleState const& s, uint8_t depth) {
    probe_phase = phase;
    probe_mark();
    BattleSearchResult r = battle_search(s, depth);
    record(r.action);
    probe_score = r.score;
    probe_phase = (uint8_t)(phase + 1);
    probe_mark();
}

static constexpr unsigned KERNEL_LOOPS = 100;

// Each kernel runs KERNEL_LOOPS times between two marks. Every iteration copies the scenario and changes its
// activation count by probe_zero, which only the probe can read. The copy-only kernel gives the overhead
// that the other kernels include, so it can be subtracted.
static void run_kernels(BattleState const& base) {
    uint8_t attacks = 0;
    uint8_t count = battle_actions(base, legal, attacks);
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
        uint8_t a = 0;
        sum = (uint16_t)(sum + battle_actions(t, legal, a));
    }
    probe_checksum = sum;
    probe_phase = 25;
    probe_mark();

    probe_phase = 26;
    probe_mark();
    for(unsigned i = 0; i < KERNEL_LOOPS; ++i) {
        BattleState t = base;
        t.activations = (uint8_t)(t.activations + probe_zero);
        uint8_t a = 0;
        uint8_t n = battle_actions(t, legal, a);
        battle_order_actions(t, legal, n, a, true);
        sum = (uint16_t)(sum + legal[0]);
    }
    probe_checksum = sum;
    probe_phase = 27;
    probe_mark();

    count = battle_actions(base, legal, attacks);
    BattleAction first = battle_unpack(legal[count - 1]);
    probe_phase = 28;
    probe_mark();
    for(unsigned i = 0; i < KERNEL_LOOPS; ++i) {
        BattleState t = base;
        t.activations = (uint8_t)(t.activations + probe_zero);
        battle_apply(t, first);
        sum = (uint16_t)(sum + t.activations);
    }
    probe_checksum = sum;
    probe_phase = 29;
    probe_mark();
}

extern "C" void setup() {
    // The shared guest checks require the single game object, so the probe builds one like the other probes.
    game_new();

    // The opening position with the enemy to act before any party move.
    grove(BATTLE_ENEMY, 0);
    measure_decision(2, scenario);

    // The first enemy turn in play, after the archer's opening shot at the brute from range four.
    grove(BATTLE_PARTY, 0);
    battle_apply(scenario, BattleAction{ARCHER, cell(5, 2)});
    measure_decision(4, scenario);

    // Mid-battle adjacent combat: the rogue beside the brute, the wolf beside the rogue.
    grove(BATTLE_ENEMY, 6);
    scenario.units[ROGUE].cell = cell(4, 2);
    scenario.units[WOLF].cell = cell(4, 1);
    measure_decision(6, scenario);

    // Many ranged options: archers on both sides and an enemy spearman.
    grove(BATTLE_ENEMY, 4);
    scenario.units[ROGUE].type = BATTLE_TYPE_ARCHER;
    scenario.units[WOLF].type = BATTLE_TYPE_ARCHER;
    scenario.units[BRUTE].type = BATTLE_TYPE_SPEARMAN;
    scenario.units[ROGUE].cell = cell(1, 0);
    scenario.units[WOLF].cell = cell(6, 0);
    measure_decision(8, scenario);

    // Wolf sprint options: two wolves with open rows ahead.
    grove(BATTLE_ENEMY, 0);
    scenario.units[BRUTE].type = BATTLE_TYPE_WOLF;
    scenario.units[BRUTE].hp = 4;
    measure_decision(10, scenario);

    // A knockback is possible: the rogue has leapt beside the brute.
    grove(BATTLE_ENEMY, 2);
    scenario.units[ROGUE].cell = cell(4, 2);
    measure_decision(12, scenario);

    // Near exhaustion, and exhaustion active in adjacent combat.
    grove(BATTLE_ENEMY, 19);
    measure_decision(14, scenario);
    grove(BATTLE_ENEMY, 21);
    scenario.units[ROGUE].cell = cell(4, 2);
    scenario.units[WOLF].cell = cell(4, 1);
    measure_decision(16, scenario);

    // One survivor per side.
    grove(BATTLE_ENEMY, 8);
    scenario.units[ARCHER].hp = 0;
    scenario.units[WOLF].hp = 0;
    scenario.units[ROGUE].hp = 3;
    scenario.units[BRUTE].hp = 3;
    scenario.units[BRUTE].cell = cell(5, 1);
    measure_decision(18, scenario);

    // The complete search at each depth on the in-play position.
    grove(BATTLE_PARTY, 0);
    battle_apply(scenario, BattleAction{ARCHER, cell(5, 2)});
    measure_depth(40, scenario, 1);
    measure_depth(42, scenario, 2);
    measure_depth(44, scenario, 3);
    measure_depth(46, scenario, 4);

    grove(BATTLE_PARTY, 0);
    battle_apply(scenario, BattleAction{ARCHER, cell(5, 2)});
    run_kernels(scenario);

    probe_phase = 34;
    probe_mark();
}

extern "C" void loop() {
    avm_idle();
}
