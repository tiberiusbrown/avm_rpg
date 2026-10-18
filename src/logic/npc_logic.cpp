#include "npc_logic.h"
#include "map_positions.h"

NpcDefinition const PROGMEM* npc_definition(NPC_ID id) {
    static constexpr NpcDefinition PROGMEM VILLAGER{NPC_STAND, SOUTH, 1, 16, 16,   0,
                                                    0,         3,     0, 1,  false};
    static constexpr NpcDefinition PROGMEM WANDERER{NPC_WANDER, WEST, 1, 24, 24,   16,
                                                    12,         2,    0, 2,  false};
    switch(id) {
        case NPC_VILLAGER:
            return &VILLAGER;
        case NPC_WANDERER:
            return &WANDERER;
        default:
            return nullptr;
    }
}

uint8_t npc_placement(NPC_ID) {
    return 0;
}

TilePos npc_home(NPC_ID id, uint8_t placement) {
    if(placement == NPC_PLACEMENT_ABSENT)
        return {};
    switch(id) {
        case NPC_VILLAGER:
            return positions::VillagerHome;
        case NPC_WANDERER:
            return positions::WandererHome;
        default:
            return {};
    }
}

void npc_on_activate(Npc&) {}

void npc_on_event(Npc&, NPC_EVENT) {}

void npc_on_retire(Npc const&, NPC_RETIRE_REASON) {}

void npc_logic_restore() {}

void npc_talk(NPC_ID) {}
