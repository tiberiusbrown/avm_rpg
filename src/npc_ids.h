#ifndef NPC_IDS_H
#define NPC_IDS_H

#include <stdint.h>

// Explicit identities are save/quest contracts, never slot or authoring indices.
enum NPC_ID : uint8_t { NPC_NONE = 0, NPC_VILLAGER = 1, NPC_WANDERER = 2, NPC_ROWAN = 3 };
// One past the last essential identity. Party and quest dialogue use this range.
#ifndef RPG_NPC_ID_LIMIT
#define RPG_NPC_ID_LIMIT 4
#endif
inline constexpr uint8_t NPC_ID_LIMIT = RPG_NPC_ID_LIMIT;

// The policy fixture with synthetic essential identities has no authored enemies.
inline constexpr unsigned ENEMY_SPAWN_COUNT = 0
#ifndef RPG_DISABLE_ENEMY_SPAWNS
#define ENEMY_SPAWN(...) + 1
#include "logic/enemy_spawns.inc"
#undef ENEMY_SPAWN
#endif
    ;
inline constexpr uint8_t NPC_ENEMY_FIRST = NPC_ID_LIMIT;
inline constexpr unsigned NPC_ENEMY_END = NPC_ENEMY_FIRST + ENEMY_SPAWN_COUNT;
static_assert(NPC_ENEMY_FIRST > NPC_ROWAN && NPC_ENEMY_END <= 256, "enemy NPC identity range");

constexpr bool npc_is_enemy(NPC_ID id) {
    return ENEMY_SPAWN_COUNT && id >= NPC_ENEMY_FIRST && id < NPC_ENEMY_END;
}

#endif
