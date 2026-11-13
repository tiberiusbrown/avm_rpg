// Deterministic host simulations of the Grove battle. The enemy plays the negamax search, and the party
// plays one of several simple policies. The counts show how the enemy behaves across whole battles, and
// they are used to tune the evaluation. Nothing here changes the battle rules.
#include "battle.h"
#include "battle_ai.h"
#include <stdio.h>
#include <stdlib.h>

enum PARTY_POLICY : uint8_t {
    POLICY_AGGRESSIVE,
    POLICY_DEFENSIVE,
    POLICY_GREEDY,
    POLICY_RANDOM,
    POLICY_SEARCH,
    POLICY_COUNT,
};

static char const* policy_name(uint8_t policy) {
    switch(policy) {
        case POLICY_AGGRESSIVE:
            return "aggressive";
        case POLICY_SEARCH:
            return "search";
        case POLICY_DEFENSIVE:
            return "defensive";
        case POLICY_GREEDY:
            return "greedy";
        default:
            return "random";
    }
}

// Random legal choices use a fixed linear congruential sequence, so every run is the same.
static uint32_t random_seed = 1;

static unsigned next_random(unsigned bound) {
    random_seed = random_seed * 1103515245u + 12345u;
    return (random_seed >> 16) % bound;
}

// Manhattan distance from a cell to the nearest living unit of `team`. Returns 99 when none is alive.
static int nearest_distance(BattleState const& s, uint8_t cell, BATTLE_TEAM team) {
    int best = 99;
    for(uint8_t i = 0; i < s.unit_count; ++i) {
        BattleUnit const& unit = s.units[i];
        if(!unit.hp || unit.team != team)
            continue;
        int dx = (int)battle_column(cell) - (int)battle_column(unit.cell);
        int dy = (int)battle_row(cell) - (int)battle_row(unit.cell);
        if(dx < 0)
            dx = -dx;
        if(dy < 0)
            dy = -dy;
        if(dx + dy < best)
            best = dx + dy;
    }
    return best;
}

static unsigned legal_actions(BattleState const& s, BattleAction* out) {
    unsigned count = 0;
    BattleActionCursor cursor = battle_actions(s);
    BattleAction action{};
    while(count < 16 && battle_next_action(s, cursor, action))
        out[count++] = action;
    return count;
}

// The party's choice under one policy. Every policy picks only legal actions.
static BattleAction party_choice(BattleState const& s, uint8_t policy) {
    BattleAction options[16];
    unsigned count = legal_actions(s, options);
    if(count == 0)
        return BattleAction{BATTLE_NO_UNIT, 0};
    if(policy == POLICY_GREEDY)
        return battle_greedy_action(s);
    if(policy == POLICY_SEARCH)
        return battle_enemy_action(s);
    if(policy == POLICY_RANDOM)
        return options[next_random(count)];

    BattleAction pick = options[0];
    int best = -1000;
    for(unsigned i = 0; i < count; ++i) {
        BattleAction a = options[i];
        int score = 0;
        if(policy == POLICY_AGGRESSIVE) {
            // Kills first, then attacks, then steps toward the nearest enemy, then anything else.
            BATTLE_ACTION_KIND kind = battle_action_kind(s, a);
            if(kind == BATTLE_ACTION_WIN)
                score = 5;
            else if(kind == BATTLE_ACTION_KILL)
                score = 4;
            else if(kind == BATTLE_ACTION_ATTACK)
                score = 3;
            else {
                int before = nearest_distance(s, s.units[a.unit_index].cell, BATTLE_ENEMY);
                int after = nearest_distance(s, a.destination, BATTLE_ENEMY);
                score = after < before ? 2 : 1;
            }
        } else {
            // Defensive: fewest enemies able to strike the acting unit afterwards, then attacks.
            BattleState t = s;
            battle_apply(t, a);
            int threats = 0;
            for(uint8_t e = 0; e < t.unit_count; ++e)
                if(t.units[e].team == BATTLE_ENEMY && battle_can_strike(t, e, a.unit_index))
                    ++threats;
            score = -4 * threats + (battle_unit_at(s, a.destination) != BATTLE_NO_UNIT ? 1 : 0);
        }
        if(score > best) {
            best = score;
            pick = a;
        }
    }
    return pick;
}

static unsigned party_hp(BattleState const& s) {
    unsigned total = 0;
    for(uint8_t i = 0; i < s.unit_count; ++i)
        if(s.units[i].team == BATTLE_PARTY)
            total += s.units[i].hp;
    return total;
}

static unsigned enemy_hp(BattleState const& s) {
    unsigned total = 0;
    for(uint8_t i = 0; i < s.unit_count; ++i)
        if(s.units[i].team == BATTLE_ENEMY)
            total += s.units[i].hp;
    return total;
}

struct GameStats {
    unsigned actions;
    unsigned enemy_attacks, enemy_moves, enemy_retreats;
    unsigned max_retreat_streak;
    unsigned party_damage, enemy_damage;
    unsigned victory, defeat, timeout;
    unsigned exhaustion;
};

// Plays one battle from the Grove encounter.
static GameStats play(uint8_t policy) {
    GameStats g{};
    BattleState s;
    battle_begin(s, &BATTLE_GROVE);
    unsigned streak = 0;
    for(unsigned guard = 0; guard < 400 && !s.ended; ++guard) {
        battle_settle(s);
        if(s.ended)
            break;
        if(s.activations > BATTLE_EXHAUSTION_START)
            g.exhaustion = 1;
        if(s.turn == BATTLE_ENEMY) {
            BattleAction a = battle_enemy_action(s);
            if(!battle_legal(s, a)) {
                printf("illegal enemy action\n");
                exit(1);
            }
            unsigned before = party_hp(s);
            bool attack = battle_unit_at(s, a.destination) != BATTLE_NO_UNIT;
            bool retreat = false;
            if(attack) {
                ++g.enemy_attacks;
            } else {
                ++g.enemy_moves;
                int from = nearest_distance(s, s.units[a.unit_index].cell, BATTLE_PARTY);
                int to = nearest_distance(s, a.destination, BATTLE_PARTY);
                retreat = to > from;
                if(retreat)
                    ++g.enemy_retreats;
            }
            battle_apply(s, a);
            g.party_damage += before - party_hp(s);
            streak = retreat ? streak + 1 : 0;
            if(streak > g.max_retreat_streak)
                g.max_retreat_streak = streak;
        } else {
            BattleAction a = party_choice(s, policy);
            unsigned before = enemy_hp(s);
            battle_apply(s, a);
            g.enemy_damage += before - enemy_hp(s);
        }
        ++g.actions;
    }
    if(!s.ended)
        g.timeout = 1;
    else if(s.result == BATTLE_VICTORY)
        g.victory = 1;
    else
        g.defeat = 1;
    return g;
}

int main() {
    // Each policy plays the same battle. Deterministic policies play once, and the random policy plays a
    // fixed set of seeds.
    constexpr unsigned random_games = 64;
    printf("%-10s %5s %6s %7s %7s %6s %6s %6s %7s %5s %5s %5s %6s\n", "policy", "games", "acts",
           "pDmg", "eDmg", "eAtk", "eMov", "eRet", "maxStrk", "win", "loss", "tout", "exhst");
    unsigned total_timeouts = 0;
    for(uint8_t policy = 0; policy < POLICY_COUNT; ++policy) {
        unsigned games = policy == POLICY_RANDOM ? random_games : 1;
        GameStats sum{};
        unsigned max_streak = 0;
        for(unsigned game = 0; game < games; ++game) {
            random_seed = 1 + game;
            GameStats g = play(policy);
            sum.actions += g.actions;
            sum.enemy_attacks += g.enemy_attacks;
            sum.enemy_moves += g.enemy_moves;
            sum.enemy_retreats += g.enemy_retreats;
            sum.party_damage += g.party_damage;
            sum.enemy_damage += g.enemy_damage;
            sum.victory += g.victory;
            sum.defeat += g.defeat;
            sum.timeout += g.timeout;
            sum.exhaustion += g.exhaustion;
            if(g.max_retreat_streak > max_streak)
                max_streak = g.max_retreat_streak;
        }
        total_timeouts += sum.timeout;
        printf("%-10s %5u %6.1f %7.1f %7.1f %6.1f %6.1f %6.1f %7u %5u %5u %5u %6u\n",
               policy_name(policy), games, (double)sum.actions / games,
               (double)sum.party_damage / games, (double)sum.enemy_damage / games,
               (double)sum.enemy_attacks / games, (double)sum.enemy_moves / games,
               (double)sum.enemy_retreats / games, max_streak, sum.victory, sum.defeat,
               sum.timeout, sum.exhaustion);
    }
    return total_timeouts == 0 ? 0 : 1;
}
