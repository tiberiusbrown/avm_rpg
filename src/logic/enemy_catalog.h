#pragma once

#include "enemy_spawn.h"
#ifndef RPG_DISABLE_ENEMY_SPAWNS
#include "handlers.h"
#endif

// Catalog order determines enemy IDs and saved defeat flags. Append new spawns at the end.
#ifndef RPG_DISABLE_ENEMY_SPAWNS
using EnemyEntries = typename Join<VillageEnemies, CaveEnemies, DungeonEnemies,
                                   OrchardEnemies, MineWestEnemies, VaultEastEnemies,
                                   MillcrossWildlife, MineCreatures, VaultCreatures>::type;
#else
using EnemyEntries = TypeList<>;
#endif

template <typename> struct CatalogFor;

template <typename... Entries> struct CatalogFor<TypeList<Entries...>> {
    using type = EnemyCatalog<std::index_sequence_for<Entries...>, TypeList<Entries...>>;
};

using AuthoredEnemies = CatalogFor<EnemyEntries>::type;

inline constexpr unsigned ENEMY_SPAWN_COUNT = AuthoredEnemies::count;
inline constexpr unsigned NPC_ENEMY_END = NPC_ENEMY_FIRST + ENEMY_SPAWN_COUNT;
static_assert(NPC_ENEMY_END <= 256, "enemy NPC identity range");
static_assert(ENEMY_SPAWN_COUNT <= ENEMY_DEFEAT_CAPACITY, "enemy defeat flags exceed saved capacity");

constexpr bool npc_is_enemy(NPC_ID id) {
    return ENEMY_SPAWN_COUNT && id >= NPC_ENEMY_FIRST && id < NPC_ENEMY_END;
}

constexpr FLAG enemy_defeated_flag(NPC_ID id) {
    return npc_is_enemy(id) ? enemy_defeated_flag_at(id - NPC_ENEMY_FIRST) : FLAG_COUNT;
}
