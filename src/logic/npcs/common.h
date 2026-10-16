#ifndef LOGIC_NPCS_COMMON_H
#define LOGIC_NPCS_COMMON_H

#include "logic/npc_logic.h"
#include "map_positions.h"

namespace npc_logic {

inline constexpr NpcDefinition PROGMEM STANDING{NPC_STAND, SOUTH, 1, 12, 8, 0, 0, 3, 0};

// Unused defaults disappear under full LTO; no callback table or per-NPC RAM.
struct Defaults {
    static uint8_t placement() {
        return 0;
    }

    static void activate(Npc&) {}

    static void event(Npc&, NPC_EVENT) {}

    static void retire(Npc const&, NPC_RETIRE_REASON) {}

    static void restore() {}

    static void talk() {
        dialogue_show(F("Keep walking. We will make room, even in this narrow hall."));
    }
};

} // namespace npc_logic

#endif
