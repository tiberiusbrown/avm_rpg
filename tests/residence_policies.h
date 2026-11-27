#pragma once

#include "npc_policy.h"

// Exercise movement between residences in one area independently of the shipping quests.
template <> struct NpcPolicy<NPC_TESSA>
    : AtOneOf<positions::TessaWell,
              When<positions::TessaOrchard, [] { return game.get_flag(FLAG_LANTERN_RECORD_FOUND); }>> {};

template <> struct NpcPolicy<NPC_ORRIN> : AtOneOf<positions::OrrinLane> {};
template <> struct NpcPolicy<NPC_ROWAN> : AtOneOf<positions::RowanHome> {};
template <> struct NpcPolicy<NPC_MARA> : AtOneOf<positions::MaraInn> {};
template <> struct NpcPolicy<NPC_BRAM> : AtOneOf<positions::BramYard> {};
template <> struct NpcPolicy<NPC_NELL> : AtOneOf<positions::NellLookout> {};
template <> struct NpcPolicy<NPC_ADA> : AtOneOf<positions::AdaShop> {};
template <> struct NpcPolicy<NPC_SURVEYOR> : AtOneOf<NO_LOCATION> {};

template <> struct NpcPolicy<NPC_ALDEN> : AtOneOf<NO_LOCATION> {};
