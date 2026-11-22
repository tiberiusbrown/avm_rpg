#include "npc_policy.h"

template <>
struct NpcPolicy<NPC_TESSA>
    : AtOneOf<positions::TessaWell,
              When<NO_LOCATION, [] { return game.get_flag(FLAG_LANTERN_DIAGNOSED); }>,
              When<TilePos{positions::CaveExit.area, positions::CaveExit.x,
                           (uint8_t)(positions::CaveExit.y - 1)},
                   [] { return game.get_flag(FLAG_LANTERN_RECORD_FOUND); }>> {};

template <> struct NpcPolicy<NPC_ORRIN> : AtOneOf<NO_LOCATION> {};

template <> struct NpcPolicy<NPC_ROWAN> : AtOneOf<NO_LOCATION> {};

template <> struct NpcPolicy<NPC_MARA> : AtOneOf<NO_LOCATION> {};

template <> struct NpcPolicy<NPC_BRAM> : AtOneOf<NO_LOCATION> {};

template <> struct NpcPolicy<NPC_NELL> : AtOneOf<NO_LOCATION> {};

template <> struct NpcPolicy<NPC_ADA> : AtOneOf<NO_LOCATION> {};

template <> struct NpcPolicy<NPC_SURVEYOR> : AtOneOf<NO_LOCATION> {};
