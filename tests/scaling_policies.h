#include "npc_policy.h"

// A content-free catalog: 64 identities, two sites each, without portrait or sprite rows.
template <NPC_ID ID>
struct NpcPolicy
    : AtOneOf<
          TilePos{positions::TestField.area, (uint8_t)(16 + (ID % 16) * 8),
                  (uint8_t)(40 + (ID / 16) * 8)},
          When<TilePos{positions::TestField.area, (uint8_t)(positions::TestField.x - 1 + ID % 6),
                       (uint8_t)(positions::TestField.y - 1 + (ID / 6) % 4)},
               [] { return game.get_flag(FLAG_LANTERN_RECORD_FOUND); }>> {};
