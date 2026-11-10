#ifndef NPC_IDS_H
#define NPC_IDS_H

#include <stdint.h>

// Explicit identities are save/quest contracts, never slot or authoring indices.
enum NPC_ID : uint8_t { NPC_NONE = 0, NPC_VILLAGER = 1, NPC_WANDERER = 2 };
// One past the largest NPC_ID; residences are indexed by ID.
inline constexpr uint8_t NPC_ID_LIMIT = 3;

#endif
