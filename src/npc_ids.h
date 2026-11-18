#ifndef NPC_IDS_H
#define NPC_IDS_H

#include <stdint.h>

// Explicit identities are save/quest contracts, never slot or authoring indices.
enum NPC_ID : uint8_t { NPC_NONE = 0, NPC_VILLAGER = 1, NPC_WANDERER = 2, NPC_ROWAN = 3 };
// One past the largest stable identity; independent of actor and portrait counts.
#ifndef RPG_NPC_ID_LIMIT
#define RPG_NPC_ID_LIMIT 4
#endif
inline constexpr uint8_t NPC_ID_LIMIT = RPG_NPC_ID_LIMIT;

#endif
