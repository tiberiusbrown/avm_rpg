#pragma once

#include "cast.h"
#include "logic/enemy_spawn.h"

// A wandering, a chasing and a patrolling enemy for engine tests.
inline constexpr uint8_t FIXTURE_PATROL = 1;
template <> inline constexpr uint8_t PATROL_AREA<FIXTURE_PATROL> = AREA_INTERIORS;

inline constexpr BattleEncounter PROGMEM BATTLE_GROVE_WOLVES = battle_encounter(ENEMY_WOLF);
inline constexpr BattleEncounter PROGMEM BATTLE_MINE_SCOUTS =
    battle_encounter(BATTLEFIELDS_CAVE, ENEMY_WOLF);
inline constexpr BattleEncounter PROGMEM BATTLE_VAULT_GUARDS =
    battle_encounter(BATTLEFIELDS_DUNGEON, ENEMY_BRUTE, ENEMY_WOLF, ENEMY_BRUTE);

struct FixtureGuardHooks : EnemyHooks {
    static bool before_battle() {
        dialogue_show(F("The guardian turns toward you."));
        return true;
    }

    static void after_victory() {
        dialogue_show(F("The guardian falls still."));
    }
};

using FixtureEnemies = TypeList<
    EnemySpawn<sites::GROVE_WOLF, &BATTLE_GROVE_WOLVES, 17>,
    EnemySpawn<sites::SCOUT, &BATTLE_MINE_SCOUTS, 17, ENEMY_CHASE, SPEED_FAST, 0, RESPAWN_REMEMBER>,
    EnemySpawn<sites::GUARD, &BATTLE_VAULT_GUARDS, 25, ENEMY_ROUTE, SPEED_NORMAL, FIXTURE_PATROL,
               RESPAWN_NEVER, FixtureGuardHooks>>;
