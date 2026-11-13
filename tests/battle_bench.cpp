// Host benchmark for the battle search. For representative positions it reports the enemy's bounded
// deepening search (nodes, deepest completed iteration, whether the budget ran out, host time) and
// the complete fixed-depth search at each depth against the unpruned reference. It then reports the
// spread over random legal positions with the enemy to act. The guest cost is measured separately
// on the AVM emulator with tests/battle_probe.cpp.
#include "battle.h"
#include "battle_ai.h"
#include "battle_reference.h"
#include <chrono>
#include <stdio.h>
#include <stdlib.h>

static constexpr uint8_t ROGUE = 0, ARCHER = 1, WOLF = 2, BRUTE = 3;

static uint8_t cell(unsigned x, unsigned y) {
    return (uint8_t)(y * BATTLE_COLUMNS + x);
}

static BattleState fresh() {
    BattleState state;
    battle_begin(state, &BATTLE_GROVE);
    return state;
}

static void place(BattleState& state, uint8_t unit, unsigned x, unsigned y) {
    state.units[unit].cell = cell(x, y);
}

static uint32_t random_seed = 7;

static unsigned next_random(unsigned bound) {
    random_seed = random_seed * 1103515245u + 12345u;
    return (random_seed >> 16) % bound;
}

// The position after `plies` random legal activations, with the activation count set to `start`.
static BattleState random_position(unsigned plies, unsigned start) {
    BattleState s = fresh();
    s.activations = (uint8_t)start;
    for(unsigned ply = 0; ply < plies && !s.ended; ++ply) {
        battle_settle(s);
        if(s.ended)
            break;
        uint8_t actions[BATTLE_ACTION_CAPACITY];
        uint8_t attacks = 0;
        uint8_t count = battle_actions(s, actions, attacks);
        if(count == 0)
            break;
        battle_apply(s, battle_unpack(actions[next_random(count)]));
    }
    battle_settle(s);
    return s;
}

static void print_action(BattleAction action) {
    if(action.unit_index == BATTLE_NO_UNIT)
        printf("pass        ");
    else
        printf("unit %u (%u,%u)", action.unit_index, battle_column(action.destination),
               battle_row(action.destination));
}

// Times the enemy's bounded search, then lists the complete searches by depth.
static void report(char const* name, BattleState const& s) {
    using clock = std::chrono::steady_clock;
    constexpr unsigned repeats = 50;
    BattleSearchResult result{};
    auto begin = clock::now();
    for(unsigned i = 0; i < repeats; ++i)
        result = battle_think(s, BATTLE_AI_DEPTH, BATTLE_AI_NODE_BUDGET);
    auto end = clock::now();
    BattleSearchStats stats = battle_search_stats();
    double micros = std::chrono::duration<double, std::micro>(end - begin).count() / repeats;
    printf("%-28s %5u nodes  depth %u%s  %7.1f us  ", name, (unsigned)stats.nodes,
           (unsigned)stats.completed, stats.exhausted ? " (budget)" : "         ", micros);
    print_action(result.action);
    printf("  score %6d  iterations", result.score);
    for(uint8_t d = 0; d < BATTLE_AI_DEPTH; ++d)
        printf(" %u", (unsigned)stats.iteration_nodes[d]);
    printf("\n");
    for(uint8_t depth = 1; depth <= BATTLE_AI_DEPTH; ++depth) {
        battle_search(s, depth);
        BattleSearchStats full = battle_search_stats();
        uint64_t unpruned = 0;
        reference_search(s, depth, unpruned);
        printf("    complete depth %u: %5u nodes %5u leaves, unpruned %llu\n", (unsigned)depth,
               (unsigned)full.nodes, (unsigned)full.leaves, (unsigned long long)unpruned);
    }
}

int main() {
    printf("max depth %u, node budget %u, workspace %u bytes, state %u bytes\n",
           (unsigned)BATTLE_AI_DEPTH, (unsigned)BATTLE_AI_NODE_BUDGET,
           (unsigned)battle_search_workspace_bytes(), (unsigned)sizeof(BattleState));

    // The same positions as tests/battle_probe.cpp, with the enemy to act.
    BattleState initial = fresh();
    initial.turn = BATTLE_ENEMY;
    report("initial encounter", initial);

    // The first enemy turn in play: the archer has shot the brute from range four.
    BattleState in_play = fresh();
    battle_apply(in_play, BattleAction{ARCHER, cell(5, 2)});
    report("in-play first enemy turn", in_play);

    BattleState adjacent = fresh();
    adjacent.turn = BATTLE_ENEMY;
    place(adjacent, ROGUE, 4, 2);
    place(adjacent, WOLF, 4, 1);
    report("adjacent combat", adjacent);

    BattleState ranged = fresh();
    ranged.turn = BATTLE_ENEMY;
    ranged.units[ROGUE].type = BATTLE_TYPE_ARCHER;
    ranged.units[WOLF].type = BATTLE_TYPE_ARCHER;
    ranged.units[BRUTE].type = BATTLE_TYPE_SPEARMAN;
    place(ranged, ROGUE, 1, 0);
    place(ranged, WOLF, 6, 0);
    place(ranged, BRUTE, 5, 2);
    report("many ranged options", ranged);

    BattleState sprint = fresh();
    sprint.turn = BATTLE_ENEMY;
    sprint.units[BRUTE].type = BATTLE_TYPE_WOLF;
    sprint.units[BRUTE].hp = 4;
    report("two wolves sprinting", sprint);

    BattleState knock = fresh();
    knock.turn = BATTLE_ENEMY;
    place(knock, ROGUE, 4, 2);
    report("knockback available", knock);

    BattleState near_exhaustion = fresh();
    near_exhaustion.turn = BATTLE_ENEMY;
    near_exhaustion.activations = 19;
    report("near exhaustion (19)", near_exhaustion);

    BattleState exhausted = adjacent;
    exhausted.activations = 21;
    report("exhaustion active (21)", exhausted);

    BattleState survivors = fresh();
    survivors.turn = BATTLE_ENEMY;
    survivors.units[ARCHER].hp = 0;
    survivors.units[WOLF].hp = 0;
    survivors.units[ROGUE].hp = 3;
    survivors.units[BRUTE].hp = 3;
    place(survivors, BRUTE, 5, 1);
    report("one survivor per side", survivors);

    // Spread over random legal positions with the enemy to act.
    unsigned samples = 0, budget_hits = 0, max_nodes = 0, max_nodes_seed = 0;
    unsigned depth_counts[BATTLE_AI_DEPTH + 1] = {};
    unsigned long long total_nodes = 0, total_full = 0, max_full = 0;
    for(unsigned seed = 0; samples < 300 && seed < 3000; ++seed) {
        BattleState s = random_position(seed % 11, (seed * 5) % 26);
        if(s.ended || s.turn != BATTLE_ENEMY)
            continue;
        battle_think(s, BATTLE_AI_DEPTH, BATTLE_AI_NODE_BUDGET);
        BattleSearchStats stats = battle_search_stats();
        if(stats.nodes > max_nodes) {
            max_nodes = stats.nodes;
            max_nodes_seed = seed;
        }
        total_nodes += stats.nodes;
        budget_hits += stats.exhausted ? 1 : 0;
        ++depth_counts[stats.completed];
        battle_search(s, BATTLE_AI_DEPTH);
        unsigned full = battle_search_stats().nodes;
        total_full += full;
        if(full > max_full)
            max_full = full;
        ++samples;
    }
    if(samples == 0) {
        printf("no random positions with the enemy to act\n");
        return 1;
    }
    printf("random enemy-to-act positions: %u samples, bounded mean nodes %.1f, max %u (seed %u), "
           "budget reached %u; complete depth-%u search mean %.1f, max %llu\n",
           samples, (double)total_nodes / samples, max_nodes, max_nodes_seed, budget_hits,
           (unsigned)BATTLE_AI_DEPTH, (double)total_full / samples, max_full);
    printf("deepest completed iteration:");
    for(uint8_t d = 0; d <= BATTLE_AI_DEPTH; ++d)
        printf(" depth %u: %u", (unsigned)d, depth_counts[d]);
    printf("\n");
    return 0;
}
