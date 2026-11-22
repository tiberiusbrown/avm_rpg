#include "npc_policy.h"

template <>
struct NpcPolicy<NPC_TESSA>
    : AtOneOf<positions::TessaWell,
              When<NO_LOCATION, [] { return game.get_flag(FLAG_BRAM_ACCOUNT); }>,
              When<TilePos{positions::CaveExit.area, positions::CaveExit.x,
                           (uint8_t)(positions::CaveExit.y - 1)},
                   [] { return game.get_flag(FLAG_ORRIN_ACCOUNT); }>> {};

template <> struct NpcPolicy<NPC_ORRIN> : AtOneOf<NO_LOCATION> {};

template <> struct NpcPolicy<NPC_ROWAN> : AtOneOf<NO_LOCATION> {};

template <> struct NpcPolicy<NPC_MARA> : AtOneOf<NO_LOCATION> {};

template <> struct NpcPolicy<NPC_BRAM> : AtOneOf<NO_LOCATION> {};

template <> struct NpcPolicy<NPC_NELL> : AtOneOf<NO_LOCATION> {};

template <> struct NpcPolicy<NPC_ADA> : AtOneOf<NO_LOCATION> {};
