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

inline constexpr uint8_t NPC_ENEMY_FIRST = NPC_ID_LIMIT;
static_assert(NPC_ENEMY_FIRST > NPC_ROWAN, "enemy NPC identity range");

#endif
