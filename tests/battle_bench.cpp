// Host benchmark for the battle search. For representative positions it reports the nodes and
// leaves the search visits, the unpruned count of the reference search, and the host time per
// search. It then reports the spread over random legal positions with the enemy to act. The guest
// cost is measured separately on the AVM emulator.
#include "battle.h"
#include "battle_ai.h"
#include "battle_reference.h"
#include <chrono>
#include <stdio.h>
#include <stdlib.h>

static constexpr uint8_t HERO_A = 0, HERO_B = 1, FOE_A = 2, FOE_B = 3;

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
        BattleAction options[16];
        unsigned count = 0;
        BattleActionCursor cursor = battle_actions(s);
        BattleAction action{};
        while(count < 16 && battle_next_action(s, cursor, action))
            options[count++] = action;
        if(count == 0)
            break;
        battle_apply(s, options[next_random(count)]);
    }
    battle_settle(s);
    return s;
}

// Times the search, then reports its counters beside the reference's unpruned count.
static void report(char const* name, BattleState const& s) {
    using clock = std::chrono::steady_clock;
    constexpr unsigned repeats = 50;
    BattleSearchResult result{};
    auto begin = clock::now();
    for(unsigned i = 0; i < repeats; ++i)
        result = battle_search(s, BATTLE_AI_DEPTH);
    auto end = clock::now();
    BattleSearchStats stats = battle_search_stats();
    uint64_t unpruned = 0;
    reference_search(s, BATTLE_AI_DEPTH, unpruned);
    double micros = std::chrono::duration<double, std::micro>(end - begin).count() / repeats;
    if(result.action.unit_index == BATTLE_NO_UNIT)
        printf("%-30s %6u %7u %10llu %10.1f   pass  score %6d\n", name, (unsigned)stats.nodes,
               (unsigned)stats.leaves, (unsigned long long)unpruned, micros, result.score);
    else
        printf("%-30s %6u %7u %10llu %10.1f   unit %u -> (%u,%u)  score %6d\n", name,
               (unsigned)stats.nodes, (unsigned)stats.leaves, (unsigned long long)unpruned, micros,
               result.action.unit_index, battle_column(result.action.destination),
               battle_row(result.action.destination), result.score);
}

int main() {
    printf("search depth %u, workspace %u bytes, static evaluation weights in battle_ai.cpp\n",
           (unsigned)BATTLE_AI_DEPTH, (unsigned)battle_search_workspace_bytes());
    printf("%-30s %6s %7s %10s %10s\n", "position (enemy to act)", "nodes", "leaves", "unpruned",
           "host us");

    BattleState initial = fresh();
    initial.turn = BATTLE_ENEMY;
    report("initial encounter", initial);

    BattleState adjacent = fresh();
    adjacent.turn = BATTLE_ENEMY;
    place(adjacent, HERO_A, 4, 1);
    report("adjacent combat", adjacent);

    BattleState survivors = fresh();
    survivors.turn = BATTLE_ENEMY;
    survivors.units[HERO_B].hp = 0;
    survivors.units[FOE_A].hp = 0;
    survivors.units[HERO_A].hp = 3;
    place(survivors, HERO_A, 2, 1);
    survivors.units[FOE_B].hp = 2;
    place(survivors, FOE_B, 3, 2);
    report("one survivor per side", survivors);

    BattleState near_exhaustion = fresh();
    near_exhaustion.turn = BATTLE_ENEMY;
    near_exhaustion.activations = 19;
    report("near exhaustion (19)", near_exhaustion);

    BattleState exhausted = adjacent;
    exhausted.activations = 20;
    report("adjacent, exhaustion next", exhausted);

    BattleState forced = fresh();
    forced.turn = BATTLE_ENEMY;
    forced.units[FOE_A].hp = 0;
    forced.units[FOE_B].hp = 0;
    report("forced pass (contrived)", forced);

    // Spread over random legal positions with the enemy to act.
    unsigned samples = 0;
    unsigned max_nodes = 0, max_nodes_seed = 0;
    unsigned long long total_nodes = 0, total_unpruned = 0, max_unpruned = 0;
    for(unsigned seed = 0; samples < 300 && seed < 3000; ++seed) {
        BattleState s = random_position(seed % 9, (seed * 5) % 26);
        if(s.ended || s.turn != BATTLE_ENEMY)
            continue;
        battle_search(s, BATTLE_AI_DEPTH);
        BattleSearchStats stats = battle_search_stats();
        uint64_t unpruned = 0;
        reference_search(s, BATTLE_AI_DEPTH, unpruned);
        if(stats.nodes > max_nodes) {
            max_nodes = stats.nodes;
            max_nodes_seed = seed;
        }
        if(unpruned > max_unpruned)
            max_unpruned = unpruned;
        total_nodes += stats.nodes;
        total_unpruned += unpruned;
        ++samples;
    }
    if(samples == 0) {
        printf("no random positions with the enemy to act\n");
        return 1;
    }
    printf("random enemy-to-act positions: %u samples, mean nodes %.1f (unpruned %.1f), "
           "max nodes %u (seed %u), max unpruned %llu\n",
           samples, (double)total_nodes / samples, (double)total_unpruned / samples, max_nodes,
           max_nodes_seed, max_unpruned);
    return 0;
}
