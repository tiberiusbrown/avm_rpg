#pragma once

#include "fixtures/cast.h"
#include "npc_policy.h"

// A content-free catalog: 64 identities, two sites each, without portrait or sprite rows. The home
// sites fill two chunk rows with an empty row between them, which keeps the chunks that any window
// collects within the chunk plan.
template <NPC_ID ID>
struct NpcPolicy
    : AtOneOf<TilePos{sites::TEST_FIELD.area, (uint8_t)(1 + ID % 33),
                      (uint8_t)(16 + (ID / 33) * 8)},
              When<TilePos{sites::TEST_FIELD.area, (uint8_t)(sites::TEST_FIELD.x - 1 + ID % 6),
                           (uint8_t)(sites::TEST_FIELD.y - 1 + (ID / 6) % 4)},
                   [] { return game.get_flag(TEST_FLAG_A); }>> {};
