#include "npc_policy.h"

template <>
struct NpcPolicy<NPC_VILLAGER>
    : AtOneOf<positions::VillagerHome,
              When<NO_LOCATION, [] { return game.get_flag(FLAG_VILLAGER_MOVED); }>,
              When<TilePos{positions::CaveExit.area, positions::CaveExit.x,
                           (uint8_t)(positions::CaveExit.y - 1)},
                   [] { return game.get_flag(FLAG_VILLAGER_EAST); }>> {};

template <> struct NpcPolicy<NPC_WANDERER> : AtOneOf<NO_LOCATION> {};

template <> struct NpcPolicy<NPC_ROWAN> : AtOneOf<NO_LOCATION> {};
