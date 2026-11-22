#ifndef QUEST_DRY_ROOTS_H
#define QUEST_DRY_ROOTS_H

#include "npc.h"
#include "quest.h"

template <> struct Quest<QUEST_DRY_ROOTS> {
    using Watches = Watch<NpcTalked<NPC_TESSA>>;

    static QUEST_STATUS status() {
        if(game.get_flag(FLAG_ORCHARD_SETTLED))
            return QUEST_COMPLETED;
        return game.get_flag(FLAG_ORCHARD_DISPUTE) ? QUEST_ACTIVE : QUEST_NOT_STARTED;
    }

    static auto title() { return F("Dry Roots"); }

    static bool ready() {
        return game.get_flag(FLAG_BRAM_ACCOUNT) && game.get_flag(FLAG_ORRIN_ACCOUNT);
    }

    static auto summary() {
        if(status() == QUEST_COMPLETED) {
            if(game.get_flag(FLAG_ORCHARD_PUBLIC))
                return F("Bram will explain the reverse flow at the well. Tessa is back in the orchard.");
            return F("Tessa will speak with Bram in private about the reverse flow.");
        }
        if(ready())
            return F("Bram closed the sluice after it ran backward. Orrin saw it too. Tessa needs to hear both accounts.");
        return F("Tessa's orchard has gone dry. Ask Bram and Orrin what happened at the sluice.");
    }

    static auto objective() {
        if(status() != QUEST_ACTIVE)
            return (char const PROGMEM*)nullptr;
        return ready() ? F("Report to Tessa") : F("Ask Bram and Orrin");
    }

    static bool marker(uint8_t area, TilePos& out) {
        NPC_ID target = NPC_TESSA;
        if(!game.get_flag(FLAG_BRAM_ACCOUNT))
            target = NPC_BRAM;
        else if(!game.get_flag(FLAG_ORRIN_ACCOUNT))
            target = NPC_ORRIN;
        out = npc_location(target);
        return out.area == area;
    }

    static void start() { game.set_flag(FLAG_ORCHARD_DISPUTE); }

    static bool consistent() {
        return (!game.get_flag(FLAG_ORCHARD_SETTLED) ||
                (game.get_flag(FLAG_ORCHARD_DISPUTE) && ready())) &&
               (!game.get_flag(FLAG_ORCHARD_PUBLIC) || game.get_flag(FLAG_ORCHARD_SETTLED));
    }

    // Tessa emits this event only after the player has confirmed how to reconcile the accounts.
    static void on(NpcTalked<NPC_TESSA>) {
        if(status() == QUEST_ACTIVE && ready()) {
            game.set_flag(FLAG_ORCHARD_SETTLED);
            quest_award_experience(4);
        }
    }
};

#endif
