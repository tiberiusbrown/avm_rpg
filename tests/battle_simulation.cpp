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

static unsigned legal_actions(BattleState const& s, BattleAction* out) {
    uint8_t packed[BATTLE_ACTION_CAPACITY];
    uint8_t attacks = 0;
    uint8_t count = battle_actions(s, packed, attacks);
    for(uint8_t i = 0; i < count; ++i)
        out[i] = battle_unpack(packed[i]);
    return count;
}

// The attack gap from a cell to the nearest living unit of `team`, for a unit of `type`. Returns 99
// when none is alive.
static int nearest_gap(BattleState const& s, BATTLE_UNIT_TYPE type, uint8_t cell,
                       BATTLE_TEAM team) {
    int best = 99;
    for(uint8_t i = 0; i < s.unit_count; ++i) {
        BattleUnit const& unit = s.units[i];
        if(!unit.hp || unit.team != team)
            continue;
        int dx = abs((int)battle_column(cell) - (int)battle_column(unit.cell));
        int dy = abs((int)battle_row(cell) - (int)battle_row(unit.cell));
        int gap = battle_attack_gap(type, (uint8_t)dx, (uint8_t)dy);
        if(gap < best)
            best = gap;
    }
    return best;
}

// The party's choice under one policy. Every policy picks only legal actions.
static BattleAction party_choice(BattleState const& s, uint8_t policy) {
    BattleAction options[BATTLE_ACTION_CAPACITY];
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
                BATTLE_UNIT_TYPE type = s.units[a.unit_index].type;
                int before = nearest_gap(s, type, s.units[a.unit_index].cell, BATTLE_ENEMY);
                int after = nearest_gap(s, type, a.destination, BATTLE_ENEMY);
                score = after < before ? 2 : 1;
            }
        } else {
            // Defensive: fewest enemies able to strike the acting unit afterwards, then attacks.
            BattleState t = s;
            battle_apply(t, a);
            int threats = 0;
            for(uint8_t e = 0; e < t.unit_count; ++e)
                if(t.units[e].team == BATTLE_ENEMY && battle_can_attack(t, e, a.unit_index))
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

static unsigned team_hp(BattleState const& s, BATTLE_TEAM team) {
    unsigned total = 0;
    for(uint8_t i = 0; i < s.unit_count; ++i)
        if(s.units[i].team == team)
            total += s.units[i].hp;
    return total;
}

static unsigned total_hp(BattleState const& s) {
    return team_hp(s, BATTLE_PARTY) + team_hp(s, BATTLE_ENEMY);
}

// Counts per team, indexed by BATTLE_TEAM.
struct GameStats {
    unsigned activations;
    unsigned attacks[2], moves[2], engaging_moves[2], engagement_damage[2], move_price[2];
    unsigned sacrifices[2], sacrifice_wins[2], passes[2], damage_taken[2];
    unsigned enemy_retreats, max_retreat_streak;
    unsigned victory, defeat, timeout;
};

// Every completed activation must lower the total living HP: the battle's termination guarantee.
static void require_progress(unsigned before, BattleState const& s) {
    if(total_hp(s) >= before) {
        printf("activation %u lost no HP\n", (unsigned)s.activations);
        exit(1);
    }
}

// Plays one battle from the Grove encounter. A retreat is an enemy move that increases its attack
// gap to the nearest living party member, so an archer stepping out of contact counts as one. A
// sacrifice is a move by a unit with only the blood price left in HP; it falls unless its
// engagement wins the battle first, which counts as a sacrifice win.
static GameStats play(uint8_t policy) {
    GameStats g{};
    BattleState s;
    battle_begin(s, &BATTLE_GROVE);
    unsigned starting_hp = total_hp(s);
    unsigned streak = 0;
    for(unsigned guard = 0; guard < 400 && !s.ended; ++guard) {
        while(!s.ended && !battle_can_act(s)) {
            unsigned before = total_hp(s);
            ++g.passes[s.turn];
            battle_pass(s);
            require_progress(before, s);
        }
        if(s.ended)
            break;
        BATTLE_TEAM team = s.turn;
        BattleAction a = team == BATTLE_ENEMY ? battle_enemy_action(s) : party_choice(s, policy);
        if(!battle_legal(s, a)) {
            printf("illegal action\n");
            exit(1);
        }
        BATTLE_TEAM foe = team == BATTLE_PARTY ? BATTLE_ENEMY : BATTLE_PARTY;
        unsigned own_before = team_hp(s, team), foe_before = team_hp(s, foe), before = total_hp(s);
        bool last_hp = s.units[a.unit_index].hp == BATTLE_MOVE_COST;
        bool attack = battle_unit_at(s, a.destination) != BATTLE_NO_UNIT;
        if(attack) {
            ++g.attacks[team];
        } else {
            ++g.moves[team];
            if(team == BATTLE_ENEMY) {
                BATTLE_UNIT_TYPE type = s.units[a.unit_index].type;
                bool retreat = nearest_gap(s, type, a.destination, BATTLE_PARTY) >
                               nearest_gap(s, type, s.units[a.unit_index].cell, BATTLE_PARTY);
                g.enemy_retreats += retreat;
                streak = retreat ? streak + 1 : 0;
                if(streak > g.max_retreat_streak)
                    g.max_retreat_streak = streak;
            }
        }
        battle_apply(s, a);
        require_progress(before, s);
        g.damage_taken[foe] += foe_before - team_hp(s, foe);
        g.damage_taken[team] += own_before - team_hp(s, team);
        if(!attack) {
            unsigned dealt = foe_before - team_hp(s, foe);
            g.engaging_moves[team] += dealt != 0;
            g.engagement_damage[team] += dealt;
            g.move_price[team] += own_before - team_hp(s, team);
            if(last_hp) {
                ++g.sacrifices[team];
                if(s.ended && (s.result == BATTLE_VICTORY) == (team == BATTLE_PARTY))
                    ++g.sacrifice_wins[team];
            }
        }
    }
    g.activations = s.activations;
    if(g.activations > starting_hp) {
        printf("battle lasted %u activations, beyond its %u starting HP\n", g.activations,
               starting_hp);
        exit(1);
    }
    if(!s.ended)
        g.timeout = 1;
    else if(s.result == BATTLE_VICTORY)
        g.victory = 1;
    else
        g.defeat = 1;
    return g;
}

static void add(GameStats& sum, GameStats const& g) {
    sum.activations += g.activations;
    for(int t = 0; t < 2; ++t) {
        sum.attacks[t] += g.attacks[t];
        sum.moves[t] += g.moves[t];
        sum.engaging_moves[t] += g.engaging_moves[t];
        sum.engagement_damage[t] += g.engagement_damage[t];
        sum.move_price[t] += g.move_price[t];
        sum.sacrifices[t] += g.sacrifices[t];
        sum.sacrifice_wins[t] += g.sacrifice_wins[t];
        sum.passes[t] += g.passes[t];
        sum.damage_taken[t] += g.damage_taken[t];
    }
    sum.enemy_retreats += g.enemy_retreats;
    if(g.max_retreat_streak > sum.max_retreat_streak)
        sum.max_retreat_streak = g.max_retreat_streak;
    sum.victory += g.victory;
    sum.defeat += g.defeat;
    sum.timeout += g.timeout;
}

int main() {
    // Each policy plays the same battle. Deterministic policies play once, and the random policy
    // plays a fixed set of seeds. Pairs are party/enemy; rows are averages per game except the
    // win/loss/timeout totals and the longest retreat run.
    constexpr unsigned random_games = 64;
    printf("%-10s %5s %5s %7s %7s %7s %7s %7s %7s %7s %7s %7s %7s %4s %4s %4s %4s\n", "policy",
           "games", "acts", "attacks", "moves", "engMove", "engDmg", "price", "sacrif", "sacWin",
           "passes", "dmgTkn", "eRetr", "run", "win", "loss", "tout");
    unsigned total_timeouts = 0;
    for(uint8_t policy = 0; policy < POLICY_COUNT; ++policy) {
        unsigned games = policy == POLICY_RANDOM ? random_games : 1;
        GameStats sum{};
        for(unsigned game = 0; game < games; ++game) {
            random_seed = 1 + game;
            add(sum, play(policy));
        }
        total_timeouts += sum.timeout;
        double n = games;
        auto pair = [&](unsigned const* v) {
            char text[32];
            snprintf(text, sizeof(text), "%.1f/%.1f", v[0] / n, v[1] / n);
            printf(" %7s", text);
        };
        printf("%-10s %5u %5.1f", policy_name(policy), games, sum.activations / n);
        pair(sum.attacks);
        pair(sum.moves);
        pair(sum.engaging_moves);
        pair(sum.engagement_damage);
        pair(sum.move_price);
        pair(sum.sacrifices);
        pair(sum.sacrifice_wins);
        pair(sum.passes);
        pair(sum.damage_taken);
        printf(" %7.1f %4u %4u %4u %4u\n", sum.enemy_retreats / n, sum.max_retreat_streak,
               sum.victory, sum.defeat, sum.timeout);
    }
    return total_timeouts == 0 ? 0 : 1;
}
