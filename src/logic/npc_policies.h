#ifndef NPC_POLICIES_H
#define NPC_POLICIES_H

#include "npc_policy.h"

template <>
struct NpcPolicy<NPC_VILLAGER>
    : AtOneOf<positions::VillagerHome,
              When<positions::VillagerEast,
                   [] {
                       return game.get_flag(FLAG_VILLAGER_MOVED) &&
                              game.get_flag(FLAG_VILLAGER_EAST);
                   }>,
              When<positions::VillagerWest, [] {
                  return game.get_flag(FLAG_VILLAGER_MOVED) && !game.get_flag(FLAG_VILLAGER_EAST);
              }>> {};

template <> struct NpcPolicy<NPC_WANDERER> : AtOneOf<positions::WandererHome> {};

template <> struct NpcPolicy<NPC_ROWAN> : AtOneOf<positions::RowanHome> {};

#endif
