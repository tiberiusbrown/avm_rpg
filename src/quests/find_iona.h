#ifndef QUEST_FIND_IONA_H
#define QUEST_FIND_IONA_H

#include "quest.h"
#include "npc.h"

// Silas asks the player to find Iona. Talking to her after that completes the quest. The objective
// marker follows Iona's policy-assigned logical site, so this quest stores no coordinates.
template <> struct Quest<QUEST_FIND_IONA> {
    using Watches = Watch<NpcTalked<NPC_VILLAGER>>;

    static QUEST_STATUS status() {
        if(game.get_flag(FLAG_QUEST_FIND_IONA_DONE))
            return QUEST_COMPLETED;
        if(game.get_flag(FLAG_QUEST_FIND_IONA_STARTED))
            return QUEST_ACTIVE;
        return QUEST_NOT_STARTED;
    }

    static char const PROGMEM* title() {
        return F("Find Iona");
    }

    static char const PROGMEM* summary() {
        if(status() == QUEST_COMPLETED)
            return F("I found Iona and learned that she was safe. Silas had nothing to worry about.");
        return F("Silas asked me to check on Iona. She's somewhere in Nocturne Grove.");
    }

    static char const PROGMEM* objective() {
        return status() == QUEST_ACTIVE ? F("Speak to Iona") : nullptr;
    }

    static bool marker(uint8_t area, TilePos& out) {
        out = npc_location(NPC_VILLAGER);
        return out.area == area;
    }

    static void start() {
        game.set_flag(FLAG_QUEST_FIND_IONA_STARTED);
    }

    static bool consistent() {
        return !game.get_flag(FLAG_QUEST_FIND_IONA_DONE) || game.get_flag(FLAG_QUEST_FIND_IONA_STARTED);
    }

    // The handler checks status, so a conversation before the offer or after completion does nothing.
    static void on(NpcTalked<NPC_VILLAGER>) {
        if(status() == QUEST_ACTIVE) {
            game.set_flag(FLAG_QUEST_FIND_IONA_DONE);
            quest_award_experience(2);
        }
    }
};

#endif
