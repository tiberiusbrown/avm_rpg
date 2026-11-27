#pragma once

#include "fixtures/absent_cast.h"
#include "fixtures/cast.h"

// Exercise movement between residences in one area independently of the shipping quests.
template <>
struct NpcPolicy<NPC_VILLAGER>
    : AtOneOf<sites::WELL, When<sites::ORCHARD, [] { return game.get_flag(TEST_FLAG_A); }>> {};

template <> struct NpcPolicy<NPC_WANDERER> : AtOneOf<sites::LANE> {};

template <> struct NpcPolicy<NPC_ARCHER> : AtOneOf<sites::HOME> {};

template <> struct NpcPolicy<NPC_INNKEEPER> : AtOneOf<sites::INN> {};

template <> struct NpcPolicy<NPC_SMITH> : AtOneOf<sites::YARD> {};

template <> struct NpcPolicy<NPC_WATCHER> : AtOneOf<sites::LOOKOUT> {};

template <> struct NpcPolicy<NPC_MERCHANT> : AtOneOf<sites::SHOP> {};

template <> struct NpcPolicy<NPC_SURVEY> : AtOneOf<NO_LOCATION> {};
