#pragma once

#include "npc_policy.h"

// The game's characters have no place in a fixture cast.
template <> struct NpcPolicy<NPC_ELIZA> : AtOneOf<NO_LOCATION> {};

template <> struct NpcPolicy<NPC_BRAM> : AtOneOf<NO_LOCATION> {};

template <> struct NpcPolicy<NPC_ALDEN> : AtOneOf<NO_LOCATION> {};

template <> struct NpcPolicy<NPC_BOULDER_A> : AtOneOf<NO_LOCATION> {};

template <> struct NpcPolicy<NPC_BOULDER_B> : AtOneOf<NO_LOCATION> {};

template <> struct NpcPolicy<NPC_BOULDER_C> : AtOneOf<NO_LOCATION> {};

template <> struct NpcPolicy<NPC_BOULDER_D> : AtOneOf<NO_LOCATION> {};
