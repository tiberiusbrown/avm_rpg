#include "npc_logic.h"
#include "npc_definitions.h"
#include "map_positions.h"
#include "quest.h"

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

// Silas offers Find Iona until it is accepted. Declining leaves it unstarted, so he offers it again.
static void wanderer_dialogue() {
    NPC_ID id = NPC_WANDERER;
    switch(quest_status(QUEST_FIND_IONA)) {
        case QUEST_NOT_STARTED:
            dialogue_show(id, F("I walk this path every night. Mind the roots near the old oak."));
            if(dialogue_ask(id, F("Have you seen Iona? She's been wandering around the grove. Could you check on her?")))
                quest_start(QUEST_FIND_IONA);
            else
                dialogue_show(id, F("Another time, then. I'll still be on this path."));
            break;
        case QUEST_ACTIVE:
            dialogue_show(id, F("Let me know when you've found Iona."));
            break;
        case QUEST_COMPLETED:
            dialogue_show(id, F("Good to hear Iona's safe. Thank you."));
            break;
        default:
            dialogue_show(id, F("I walk this path every night. Mind the roots near the old oak."));
            break;
    }
}

// Iona's own movement follows her conversations whatever the quest says. The quest completes only
// through its conversation event, which is emitted after the movement flags are updated.
static void villager_dialogue() {
    NPC_ID id = NPC_VILLAGER;
    if(quest_status(QUEST_FIND_IONA) == QUEST_ACTIVE)
        dialogue_show(id, F("Silas is worried about me? Tell him I'm fine. I just needed some time away from the village."));
    else
        dialogue_show(id,
            F("Evening, traveler. The lanterns stay lit all night in Nocturne Grove. I hope you decide to stay here. It's a quiet place, and the trees are kind to those who respect them."));
    // Talking sends the villager to the other end of the clearing, named by map markers.
    if(game.get_flag(FLAG_VILLAGER_MOVED))
        game.toggle_flag(FLAG_VILLAGER_EAST);
    else
        game.set_flag(FLAG_VILLAGER_EAST);
    game.set_flag(FLAG_VILLAGER_MOVED);
    quest_emit(NpcTalked<NPC_VILLAGER>{});
}

void npc_dialogue() {
    NPC_ID id = pending_talk;
    pending_talk = NPC_NONE;
    switch(id) {
        case NPC_ROWAN:
            dialogue_show(id, F("I'm Rowan. You could use a bow on the road. I'll join you!"));
            party_add(party_new_member(NPC_ROWAN, MOVEMENT_STANDARD, ATTACK_SHOT));
            break;
        case NPC_VILLAGER:
            villager_dialogue();
            break;
        case NPC_WANDERER:
            wanderer_dialogue();
            break;
        default:
            break;
    }
}
