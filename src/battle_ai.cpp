#include "battle_ai.h"

// Scores are ordered by band. Each band is wider than any distance a move can score inside it.
static constexpr int16_t SCORE_KILL = 3000, SCORE_ATTACK = 2000, SCORE_MOVE = 1000;

// Manhattan distance from a cell to the nearest living party member.
static int16_t distance_to_party(BattleState const& state, uint8_t cell) {
    int16_t x = cell % BATTLE_COLUMNS, y = cell / BATTLE_COLUMNS;
    int16_t nearest = 255;
    for(uint8_t i = 0; i < state.unit_count; ++i) {
        BattleUnit const& unit = state.units[i];
        if(!unit.hp || unit.team != BATTLE_PARTY)
            continue;
        int16_t dx = x - unit.x, dy = y - unit.y;
        if(dx < 0)
            dx = -dx;
        if(dy < 0)
            dy = -dy;
        if(dx + dy < nearest)
            nearest = dx + dy;
    }
    return nearest;
}

static int16_t action_score(BattleState const& state, BattleAction action) {
    uint8_t target = battle_unit_at(state, action.destination);
    if(target != BATTLE_NO_UNIT)
        return state.units[target].hp <= BATTLE_ATTACK_DAMAGE ? SCORE_KILL : SCORE_ATTACK;
    return (int16_t)(SCORE_MOVE - distance_to_party(state, action.destination));
}

BattleAction battle_enemy_action(BattleState const& state) {
    BattleAction best{BATTLE_NO_UNIT, 0};
    int16_t best_score = -1;
    for(uint8_t i = 0; i < state.unit_count; ++i) {
        BattleUnit const& unit = state.units[i];
        if(!unit.hp || unit.team != BATTLE_ENEMY)
            continue;
        uint8_t cell = (uint8_t)(unit.y * BATTLE_COLUMNS + unit.x);
        for(DIRECTION dir = NORTH; dir <= WEST; dir = (DIRECTION)(dir + 1)) {
            uint8_t neighbor = 0;
            if(!battle_neighbor(cell, dir, neighbor))
                continue;
            BattleAction action{i, neighbor};
            if(!battle_legal(state, action))
                continue;
            // A strictly greater score keeps the first candidate in fixed order on a tie.
            int16_t score = action_score(state, action);
            if(score > best_score) {
                best = action;
                best_score = score;
            }
        }
    }
    return best;
}
