#include "battle.h"

#ifdef RPG_BATTLE_FIXTURES
#include "../tests/battle_fixtures.h"
#endif

constexpr EnemyInfo PROGMEM ENEMY_INFO[] = {
    {9, MOVEMENT_CHARGE, ATTACK_REND, 17},
    {12, MOVEMENT_STEP, ATTACK_IMPACT, 25},
    {6, MOVEMENT_GLIDE, ATTACK_ARC, 18},
    {7, MOVEMENT_LEAP, ATTACK_FLANK, 19},
    {10, MOVEMENT_STEP, ATTACK_REACH, 23},
    {8, MOVEMENT_VAULT, ATTACK_BLAST, 29},
#ifdef RPG_BATTLE_FIXTURES
    {10, MOVEMENT_STEP, ATTACK_STRIKE, 2},
    {10, MOVEMENT_LEAP, ATTACK_STRIKE, 1},
    {9, MOVEMENT_STEP, ATTACK_SHOOT, 0},
    {10, MOVEMENT_STEP, ATTACK_REACH, 3},
#endif
};

constexpr auto ENEMY_INFO_COUNT = sizeof(ENEMY_INFO) / sizeof(*ENEMY_INFO);
#ifdef RPG_BATTLE_FIXTURES
static_assert(ENEMY_INFO_COUNT == BATTLE_TEST_SOURCE_COUNT);
#else
static_assert(ENEMY_INFO_COUNT == ENEMY_COUNT);
#endif
static_assert(ENEMY_INFO_COUNT <= BATTLE_PARTY_SOURCE);
static_assert([] {
    for(auto const& enemy : ENEMY_INFO)
        if(!enemy.max_hp || enemy.movement_class >= MOVEMENT_CLASS_COUNT ||
           enemy.attack_class >= ATTACK_CLASS_COUNT || enemy.sprite_row >= ACTOR_ROW_COUNT)
            return false;
    return true;
}());
