#ifndef LOGIC_NPCS_GUARD_H
#define LOGIC_NPCS_GUARD_H

#include "common.h"

namespace npc_logic {

struct Guard : Defaults {
    static inline constexpr NpcDefinition PROGMEM DEFINITION{NPC_STAND, SOUTH, 1, 12, 8,
                                                             0,         0,     4, 0};

    static NpcDefinition const PROGMEM* definition() {
        return &DEFINITION;
    }

    static uint8_t placement() {
        return game.get_flag(FLAG_GUARD_RELOCATED);
    }

    static TilePos home(uint8_t placement) {
        return placement ? positions::CastleGuardPost : positions::VillageGuardHome;
    }

    static void talk() {
        if(game.get_flag(FLAG_GUARD_RELOCATED))
            dialogue_show(
                F("Mara: My new post is in the house. I will leave when you are out of sight."));
        else if(dialogue_ask(F("Mara: Send me to the castle post inside the house?"))) {
            game.set_flag(FLAG_GUARD_RELOCATED);
            dialogue_show(
                F("Mara: Orders received. I will take up my new post when the road is quiet."));
        }
    }
};

} // namespace npc_logic

#endif
