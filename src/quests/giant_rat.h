#ifndef QUEST_GIANT_RAT_H
#define QUEST_GIANT_RAT_H

#include "map_positions.h"
#include "npc.h"
#include "quest.h"

// Eliza asks the traveler to clear a giant rat out of her bedroom. The rat's defeat is its enemy
// flag; the quest completes when the traveler tells Eliza.
template <> struct Quest<QUEST_GIANT_RAT> {
    static QUEST_STATUS status() {
        if(game.get_flag(FLAG_RAT_REPORTED))
            return QUEST_COMPLETED;
        return game.get_flag(FLAG_RAT_ACCEPTED) ? QUEST_ACTIVE : QUEST_NOT_STARTED;
    }

    static auto title() {
        return F("Eliza's Giant Rat");
    }

    static auto summary() {
        if(game.get_flag(FLAG_RAT_REPORTED))
            return F("I got rid of the giant rat in Eliza's home.");
        if(game.get_flag(FLAG_RAT_DEFEATED))
            return F("The giant rat is gone. Eliza is waiting for me at my home.");
        return F("There's a giant rat inside Eliza's home. She asked me to get rid of it.");
    }

    static char const PROGMEM* objective() {
        if(status() != QUEST_ACTIVE)
            return nullptr;
        return game.get_flag(FLAG_RAT_DEFEATED) ? F("Go back to Eliza") : F("Go to Eliza's home");
    }

    // Outdoors the marker names a house. Indoors it follows the rat, and then Eliza.
    static bool marker(uint8_t area, TilePos& out) {
        bool report = game.get_flag(FLAG_RAT_DEFEATED);
        if(area == positions::AldenDoor.area)
            return report ? quest_fixed_marker<positions::AldenDoor>(area, out)
                          : quest_fixed_marker<positions::ElizaDoor>(area, out);
        if(!(report ? quest_fixed_marker<positions::ElizaAtAlden>(area, out)
                    : quest_fixed_marker<positions::RatNest>(area, out)))
            return false;
        NPC_ID id = report ? NPC_ELIZA : (NPC_ID)NPC_ENEMY_FIRST;
        if(game.area == area)
            if(Npc const* actor = npc_find(id)) {
                out.x = (uint8_t)((actor->actor.x + FOOT_CENTER_X) >> TILE_SHIFT);
                out.y = (uint8_t)((actor->actor.y + FOOT_CENTER_Y) >> TILE_SHIFT);
            }
        return true;
    }

    static void start() {
        game.set_flag(FLAG_RAT_ACCEPTED);
    }

    // Each step follows the one before it.
    static bool consistent() {
        auto follows = [](FLAG later, FLAG earlier) {
            return !game.get_flag(later) || game.get_flag(earlier);
        };
        return follows(FLAG_ELIZA_GREETED, FLAG_HOME_REACHED) &&
               follows(FLAG_RAT_ACCEPTED, FLAG_ELIZA_GREETED) &&
               follows(FLAG_RAT_SIGHTED, FLAG_RAT_ACCEPTED) &&
               follows(FLAG_CLASSES_CHOSEN, FLAG_RAT_SIGHTED) &&
               follows(FLAG_MENU_PARTY, FLAG_CLASSES_CHOSEN) &&
               follows(FLAG_RAT_DEFEATED, FLAG_RAT_ACCEPTED) &&
               follows(FLAG_RAT_REPORTED, FLAG_RAT_DEFEATED);
    }
};

#endif
