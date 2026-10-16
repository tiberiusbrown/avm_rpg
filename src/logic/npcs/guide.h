#ifndef LOGIC_NPCS_GUIDE_H
#define LOGIC_NPCS_GUIDE_H

#include "common.h"

namespace npc_logic {

struct Guide : Defaults {
    static NpcDefinition const PROGMEM* definition() {
        return &STANDING;
    }

    static TilePos home(uint8_t) {
        return positions::GuideHome;
    }

    static void talk() {
        dialogue_show(F("Iona: A facing a traveler starts a conversation. The hall to the east "
                        "tests crowded paths."));
    }
};

} // namespace npc_logic

#endif
