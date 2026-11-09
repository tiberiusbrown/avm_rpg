#include "npc_logic.h"
#include "map_positions.h"

NpcDefinition const PROGMEM* npc_definition(NPC_ID id) {
    static constexpr NpcDefinition PROGMEM VILLAGER{NPC_STAND, SOUTH, 1, 32, 32,  0,
                                                    0,         3,     0, 1,  true};
    static constexpr NpcDefinition PROGMEM WANDERER{NPC_WANDER, WEST, 1, 48, 48,  16,
                                                    12,         2,    0, 2,  true};
    switch(id) {
        case NPC_VILLAGER:
            return &VILLAGER;
        case NPC_WANDERER:
            return &WANDERER;
        default:
            return nullptr;
    }
}

// The villager roams between residences; the wanderer always stays in its chunk.
bool npc_is_dynamic(NPC_ID id) {
    return id == NPC_VILLAGER;
}

// Default residence. Quest code moves dynamic NPCs with npc_set_destination.
TilePos npc_home(NPC_ID id) {
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

// Talking only records the request. Blocking dialogue runs later in npc_dialogue().
static NPC_ID pending_talk = NPC_NONE;

void npc_talk(NPC_ID id) {
    pending_talk = id;
}

void npc_dialogue() {
    NPC_ID id = pending_talk;
    pending_talk = NPC_NONE;
    switch(id) {
        case NPC_VILLAGER:
            dialogue_show(
                F("Evening, traveler. The lanterns stay lit all night in Nocturne Grove."));
            // Talking sends the villager to the other end of the clearing, named by map markers.
            npc_set_destination(NPC_VILLAGER,
                                game.npcs.residence[NPC_VILLAGER] == positions::VillagerEast
                                    ? positions::VillagerWest
                                    : positions::VillagerEast);
            break;
        case NPC_WANDERER:
            dialogue_show(F("I walk this path every night. Mind the roots near the old oak."));
            break;
        default:
            break;
    }
}
