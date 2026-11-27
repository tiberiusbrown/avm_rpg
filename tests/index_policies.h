#pragma once

#include "fixtures/absent_cast.h"
#include "fixtures/cast.h"

template <>
struct NpcPolicy<NPC_VILLAGER>
    : AtOneOf<sites::ORCHARD, When<sites::WELL, [] { return game.get_flag(TEST_FLAG_B); }>> {};

template <>
struct NpcPolicy<NPC_WANDERER>
    : AtOneOf<sites::LANE, When<sites::ORCHARD, [] { return game.get_flag(TEST_FLAG_B); }>> {};

template <> struct NpcPolicy<NPC_ARCHER> : AtOneOf<NO_LOCATION> {};

template <> struct NpcPolicy<NPC_INNKEEPER> : AtOneOf<NO_LOCATION> {};

template <> struct NpcPolicy<NPC_SMITH> : AtOneOf<NO_LOCATION> {};

template <> struct NpcPolicy<NPC_WATCHER> : AtOneOf<NO_LOCATION> {};

template <> struct NpcPolicy<NPC_MERCHANT> : AtOneOf<NO_LOCATION> {};

template <> struct NpcPolicy<NPC_SURVEY> : AtOneOf<NO_LOCATION> {};
