#ifndef NPC_IDS_H
#define NPC_IDS_H

#include <stdint.h>

// Explicit identities are save/quest contracts, never slot or authoring indices.
enum NPC_ID : uint8_t {
    NPC_NONE,
    NPC_ELIZA,
    NPC_BRAM,
    NPC_ALDEN, // The player: a named speaker that is never placed on the map.
    // Static objects: the boulders in the way out of the forest clearing.
    NPC_BOULDER_A,
    NPC_BOULDER_B,
    NPC_BOULDER_C,
    NPC_BOULDER_D
};
// One past the last essential identity. Party and quest dialogue use this range.
#ifndef RPG_NPC_ID_LIMIT
#define RPG_NPC_ID_LIMIT 8
#endif
inline constexpr uint8_t NPC_ID_LIMIT = RPG_NPC_ID_LIMIT;

inline constexpr uint8_t NPC_ENEMY_FIRST = NPC_ID_LIMIT;
static_assert(NPC_ENEMY_FIRST > NPC_BOULDER_D, "enemy NPC identity range");

#endif
