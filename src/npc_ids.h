#ifndef NPC_IDS_H
#define NPC_IDS_H

#include <stdint.h>

// Explicit identities are save/quest contracts, never slot or authoring indices.
enum NPC_ID : uint8_t {
    NPC_NONE,
    NPC_TESSA,
    NPC_ORRIN,
    NPC_ROWAN,
    NPC_MARA,
    NPC_BRAM,
    NPC_NELL,
    NPC_ADA,
    NPC_SURVEYOR,
    NPC_ALDEN // The player: a named speaker that is never placed on the map.
};
// One past the last essential identity. Party and quest dialogue use this range.
#ifndef RPG_NPC_ID_LIMIT
#define RPG_NPC_ID_LIMIT 10
#endif
inline constexpr uint8_t NPC_ID_LIMIT = RPG_NPC_ID_LIMIT;

inline constexpr uint8_t NPC_ENEMY_FIRST = NPC_ID_LIMIT;
static_assert(NPC_ENEMY_FIRST > NPC_ALDEN, "enemy NPC identity range");

#endif
