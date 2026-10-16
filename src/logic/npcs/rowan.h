#ifndef LOGIC_NPCS_ROWAN_H
#define LOGIC_NPCS_ROWAN_H

#include "common.h"

namespace npc_logic {

struct Rowan : Defaults {
    static inline constexpr NpcDefinition PROGMEM DEFINITION{NPC_WANDER, EAST, 1, 12, 12,
                                                             12,         8,    2, 0};

    static NpcDefinition const PROGMEM* definition() {
        return &DEFINITION;
    }

    static uint8_t placement() {
        return game.get_flag(FLAG_ROWAN_AWAY) ? NPC_PLACEMENT_ABSENT : 0;
    }

    static TilePos home(uint8_t) {
        return positions::RowanHome;
    }

    static void talk() {
        if(game.get_flag(FLAG_ROWAN_AWAY))
            dialogue_show(F("Rowan: I will head home when the garden is quiet."));
        else if(dialogue_ask(F("Rowan: Shall I take a break at home?")))
            game.set_flag(FLAG_ROWAN_AWAY);
    }
};

} // namespace npc_logic

#endif
