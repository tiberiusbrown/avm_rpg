#include "npc_logic.h"

#include "handlers.h"
#include "npc_content.h"
// Test fixtures add characters of their own.
#ifdef RPG_NPC_CONTENT_HEADER
#include RPG_NPC_CONTENT_HEADER
#endif

using EssentialContent = NpcContentCatalog<std::make_index_sequence<NPC_ID_LIMIT - 1>>;

// A rat that is cast again after the traveler has chosen how to fight resumes the chase.
void npc_on_activate(Npc& npc) {
    if(npc.id == NPC_GIANT_RAT && game.get_flag(FLAG_MENU_PARTY))
        npc_start_pursuit(npc.id);
}

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
    EssentialContent::talk(id);
}
