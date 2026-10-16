#include "npc_logic.h"
#include "npcs/guide.h"
#include "npcs/rowan.h"
#include "npcs/guard.h"
#include "npcs/courier.h"
#include "npcs/hall.h"

NpcDefinition const PROGMEM* npc_definition(NPC_ID id) {
    switch(id) {
#define NPC_LOGIC(ID, LOGIC)                                                                       \
    case ID:                                                                                       \
        return LOGIC::definition();
#include "npcs/roster.inc"
#undef NPC_LOGIC
        default:
            return nullptr;
    }
}

uint8_t npc_placement(NPC_ID id) {
    switch(id) {
#define NPC_LOGIC(ID, LOGIC)                                                                       \
    case ID:                                                                                       \
        return LOGIC::placement();
#include "npcs/roster.inc"
#undef NPC_LOGIC
        default:
            return NPC_PLACEMENT_ABSENT;
    }
}

TilePos npc_home(NPC_ID id, uint8_t placement) {
    if(placement == NPC_PLACEMENT_ABSENT)
        return {0, 0, 0};
    switch(id) {
#define NPC_LOGIC(ID, LOGIC)                                                                       \
    case ID:                                                                                       \
        return LOGIC::home(placement);
#include "npcs/roster.inc"
#undef NPC_LOGIC
        default:
            return {0, 0, 0};
    }
}

void npc_on_activate(Npc& npc) {
    switch(npc.id) {
#define NPC_LOGIC(ID, LOGIC)                                                                       \
    case ID:                                                                                       \
        return LOGIC::activate(npc);
#include "npcs/roster.inc"
#undef NPC_LOGIC
        default:
            return;
    }
}

void npc_on_event(Npc& npc, NPC_EVENT event) {
    switch(npc.id) {
#define NPC_LOGIC(ID, LOGIC)                                                                       \
    case ID:                                                                                       \
        return LOGIC::event(npc, event);
#include "npcs/roster.inc"
#undef NPC_LOGIC
        default:
            return;
    }
}

void npc_on_retire(Npc const& npc, NPC_RETIRE_REASON reason) {
    switch(npc.id) {
#define NPC_LOGIC(ID, LOGIC)                                                                       \
    case ID:                                                                                       \
        return LOGIC::retire(npc, reason);
#include "npcs/roster.inc"
#undef NPC_LOGIC
        default:
            return;
    }
}

void npc_logic_restore() {
#define NPC_LOGIC(ID, LOGIC) LOGIC::restore();
#include "npcs/roster.inc"
#undef NPC_LOGIC
}

void npc_talk(NPC_ID id) {
    switch(id) {
#define NPC_LOGIC(ID, LOGIC)                                                                       \
    case ID:                                                                                       \
        return LOGIC::talk();
#include "npcs/roster.inc"
#undef NPC_LOGIC
        default:
            return;
    }
}
