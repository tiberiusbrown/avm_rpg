#include "npc_logic.h"
#include "npc_definitions.h"
#include "map_positions.h"

char const PROGMEM* npc_name(NPC_ID id) {
    switch(id) {
        case NPC_VILLAGER:
            return F("Iona");
        case NPC_WANDERER:
            return F("Silas");
        case NPC_ROWAN:
            return F("Rowan");
        default:
            return nullptr;
    }
}

NpcDefinition const PROGMEM* npc_definition(NPC_ID id) {
    switch(id) {
        case NPC_ROWAN:
            return &npc_definitions::ROWAN;
        case NPC_VILLAGER:
            return &npc_definitions::VILLAGER;
        case NPC_WANDERER:
            return &npc_definitions::WANDERER;
        default:
            return nullptr;
    }
}

void npc_on_activate(Npc&) {}

void npc_on_event(Npc&, NPC_EVENT) {}

void npc_on_retire(Npc const&, NPC_RETIRE_REASON) {}

static NPC_ID pending_talk = NPC_NONE;

void npc_logic_restore() {
    pending_talk = NPC_NONE;
}

// Talking only records the request. Blocking dialogue runs later in npc_dialogue().

void npc_talk(NPC_ID id) {
    pending_talk = id;
}

void npc_dialogue() {
    NPC_ID id = pending_talk;
    pending_talk = NPC_NONE;
    switch(id) {
        case NPC_ROWAN:
            dialogue_show(id, F("I'm Rowan. You could use a bow on the road. I'll join you!"));
            party_add({18, 18, MOVEMENT_FIGHTER, ATTACK_ARCHER, NPC_ROWAN});
            break;
        case NPC_VILLAGER:
            dialogue_show(id,
                F("Evening, traveler. The lanterns stay lit all night in Nocturne Grove. I hope you decide to stay here. It's a quiet place, and the trees are kind to those who respect them."));
            // Talking sends the villager to the other end of the clearing, named by map markers.
            if(game.get_flag(FLAG_VILLAGER_MOVED))
                game.toggle_flag(FLAG_VILLAGER_EAST);
            else
                game.set_flag(FLAG_VILLAGER_EAST);
            game.set_flag(FLAG_VILLAGER_MOVED);
            break;
        case NPC_WANDERER:
            dialogue_show(id, F("I walk this path every night. Mind the roots near the old oak."));
            break;
        default:
            break;
    }
}
