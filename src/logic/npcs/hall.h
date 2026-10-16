#ifndef LOGIC_NPCS_HALL_H
#define LOGIC_NPCS_HALL_H

#include "common.h"

namespace npc_logic {

inline constexpr NpcDefinition PROGMEM HALL_WANDERER{NPC_WANDER, EAST, 2, 10, 8, 4, 0, 1, 0};

template <TilePos HOME> struct Hall : Defaults {
    static NpcDefinition const PROGMEM* definition() {
        if constexpr(HOME == positions::HallNpc2)
            return &HALL_WANDERER;
        else
            return &STANDING;
    }

    static TilePos home(uint8_t) {
        return HOME;
    }
};

} // namespace npc_logic

#endif
