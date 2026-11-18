#include "npc_policy.h"

template <>
struct NpcPolicy<NPC_VILLAGER>
    : AtOneOf<positions::VillagerEast,
              When<positions::VillagerHome, [] { return game.get_flag(FLAG_VILLAGER_MOVED); }>> {};

template <>
struct NpcPolicy<NPC_WANDERER>
    : AtOneOf<positions::WandererHome,
              When<positions::VillagerEast, [] { return game.get_flag(FLAG_VILLAGER_MOVED); }>> {};

template <> struct NpcPolicy<NPC_ROWAN> : AtOneOf<NO_LOCATION> {};
