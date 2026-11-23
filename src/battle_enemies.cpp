#include "battle.h"

#ifdef RPG_BATTLE_FIXTURES
#include "../tests/battle_fixtures.h"
#endif

constexpr EnemyInfo PROGMEM ENEMY_INFO[] = {
    {9, MOVEMENT_CHARGE, ATTACK_REND, 17},
    {12, MOVEMENT_STEP, ATTACK_IMPACT, 25},
    {6, MOVEMENT_GLIDE, ATTACK_ARC, 18},
    {7, MOVEMENT_LEAP, ATTACK_FLANK, 19},
    {10, MOVEMENT_STEP, ATTACK_REACH, 23, 2, 1, 2},
    {8, MOVEMENT_VAULT, ATTACK_BLAST, 29, 1, 2, 3},
    {5, MOVEMENT_STEP, ATTACK_STRIKE, 16},
    {10, MOVEMENT_CHARGE, ATTACK_IMPACT, 20, 1},
    {7, MOVEMENT_LEAP, ATTACK_RECOIL, 21, 0, 1},
    {8, MOVEMENT_STEP, ATTACK_DRAIN, 22},
    {12, MOVEMENT_STEP, ATTACK_REND, 24, 1, 2},
    {8, MOVEMENT_LEAP, ATTACK_SHOOT, 26, 1, 1, 2},
    {11, MOVEMENT_GLIDE, ATTACK_DRAIN, 27, 3, 2, 2},
    {10, MOVEMENT_STEP, ATTACK_BLAST, 28, 1, 1, 2},
    {18, MOVEMENT_STEP, ATTACK_IMPACT, 30, 3, 4, 3},
    {14, MOVEMENT_VAULT, ATTACK_SHOOT, 31, 2, 4, 5},
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
           enemy.attack_class >= ATTACK_CLASS_COUNT || enemy.sprite_row >= ACTOR_ROW_COUNT ||
           enemy.movement_rank > PERK_TRACK_RANKS || enemy.attack_rank > PERK_TRACK_RANKS)
            return false;
    return true;
}());

char const PROGMEM* battle_enemy_name(uint8_t id) {
    static constexpr char const PROGMEM* const PROGMEM NAMES[] = {
        F("Wolf"), F("Brute"), F("Giant Bat"), F("Giant Spider"), F("Skeleton"), F("Floating Eye"),
        F("Giant Rat"), F("Giant Beetle"), F("Snake"), F("Slime"), F("Zombie"), F("Goblin"),
        F("Ghost"), F("Mushroom"), F("Golem"), F("Hooded Cultist"),
#ifdef RPG_BATTLE_FIXTURES
        F("Fighter"), F("Rogue"), F("Archer"), F("Spearman"),
#endif
    };
    static_assert(sizeof(NAMES) / sizeof(*NAMES) == ENEMY_INFO_COUNT);
    return NAMES[id];
}
