#include "npc_logic.h"

#include "handlers.h"
#include "npc_content.h"

using EssentialContent = NpcContentCatalog<std::make_index_sequence<NPC_ID_LIMIT - 1>>;

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
    EssentialContent::talk(id);
}
