#pragma once

#include "enemy_spawn.h"
#ifndef RPG_DISABLE_ENEMY_SPAWNS
#include "handlers.h"
#endif

using AuthoredEnemies = EnemyCatalog<std::make_index_sequence<ENEMY_SPAWN_COUNT>,
#ifndef RPG_DISABLE_ENEMY_SPAWNS
    typename Join<
#define ENEMY_SPAWN(...) TypeList<EnemySpawn<__VA_ARGS__>>,
#include "enemy_spawns.inc"
#undef ENEMY_SPAWN
        TypeList<>>::type
#else
    TypeList<>
#endif
>;
