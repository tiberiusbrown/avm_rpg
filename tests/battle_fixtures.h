#ifndef BATTLE_FIXTURES_H
#define BATTLE_FIXTURES_H

#include "battle.h"

// These sources are available to synthetic battle states in fixture builds.
inline constexpr ENEMY_ID BATTLE_TEST_FIGHTER = (ENEMY_ID)(ENEMY_COUNT + 0);
inline constexpr ENEMY_ID BATTLE_TEST_ROGUE = (ENEMY_ID)(ENEMY_COUNT + 1);
inline constexpr ENEMY_ID BATTLE_TEST_ARCHER = (ENEMY_ID)(ENEMY_COUNT + 2);
inline constexpr ENEMY_ID BATTLE_TEST_SPEARMAN = (ENEMY_ID)(ENEMY_COUNT + 3);
inline constexpr uint8_t BATTLE_TEST_SOURCE_COUNT = ENEMY_COUNT + 4;

struct BattleFixture {
    uint8_t field, unit_count;
    BattleUnit units[BATTLE_UNIT_CAPACITY];
};

inline constexpr BattleFixture PROGMEM BATTLE_TEST_GROVE{
    BATTLEFIELD_GROVE,
    4,
    {{battle_cell(2, 1), 10, BATTLE_TEST_ROGUE, BATTLE_PARTY},
     {battle_cell(1, 2), 9, BATTLE_TEST_ARCHER, BATTLE_PARTY},
     {battle_cell(6, 1), 9, ENEMY_WOLF, BATTLE_ENEMY},
     {battle_cell(5, 2), 12, ENEMY_BRUTE, BATTLE_ENEMY}},
};

inline void battle_fixture_begin(BattleState& state, BattleFixture const PROGMEM* fixture) {
    state = {};
    state.field = fixture->field;
    state.unit_count = fixture->unit_count;
    for(uint8_t i = 0; i < BATTLE_UNIT_CAPACITY; ++i) {
        state.units[i].cell = fixture->units[i].cell;
        state.units[i].team = fixture->units[i].team;
        state.units[i].hp = fixture->units[i].hp;
        state.units[i].id = fixture->units[i].id;
    }
    state.result = BATTLE_DEFEAT;
}

#endif
