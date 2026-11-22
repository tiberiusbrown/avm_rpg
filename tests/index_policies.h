#include "npc_policy.h"

template <>
struct NpcPolicy<NPC_TESSA>
    : AtOneOf<positions::TessaOrchard,
              When<positions::TessaWell, [] { return game.get_flag(FLAG_BRAM_ACCOUNT); }>> {};

template <>
struct NpcPolicy<NPC_ORRIN>
    : AtOneOf<positions::OrrinLane,
              When<positions::TessaOrchard, [] { return game.get_flag(FLAG_BRAM_ACCOUNT); }>> {};

template <> struct NpcPolicy<NPC_ROWAN> : AtOneOf<NO_LOCATION> {};

template <> struct NpcPolicy<NPC_MARA> : AtOneOf<NO_LOCATION> {};

template <> struct NpcPolicy<NPC_BRAM> : AtOneOf<NO_LOCATION> {};

template <> struct NpcPolicy<NPC_NELL> : AtOneOf<NO_LOCATION> {};

template <> struct NpcPolicy<NPC_ADA> : AtOneOf<NO_LOCATION> {};
