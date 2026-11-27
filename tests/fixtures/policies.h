#pragma once

#include "cast.h"
#include "absent_cast.h"

// The default fixture cast lives at fixed places.
template <> struct NpcPolicy<NPC_VILLAGER> : AtOneOf<sites::WELL> {};

template <> struct NpcPolicy<NPC_WANDERER> : AtOneOf<sites::LANE> {};

template <> struct NpcPolicy<NPC_ARCHER> : AtOneOf<sites::HOME> {};

template <> struct NpcPolicy<NPC_INNKEEPER> : AtOneOf<sites::INN> {};

template <> struct NpcPolicy<NPC_SMITH> : AtOneOf<sites::YARD> {};

template <> struct NpcPolicy<NPC_WATCHER> : AtOneOf<sites::LOOKOUT> {};

template <> struct NpcPolicy<NPC_MERCHANT> : AtOneOf<sites::SHOP> {};

template <> struct NpcPolicy<NPC_SURVEY> : AtOneOf<NO_LOCATION> {};
