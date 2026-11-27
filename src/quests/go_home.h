#ifndef QUEST_GO_HOME_H
#define QUEST_GO_HOME_H

#include "map_positions.h"
#include "quest.h"

// The traveler wakes in the forest clearing and walks home through the boulder gate. The story
// (src/logic/story.cpp) records each step as a saved fact.
template <> struct Quest<QUEST_GO_HOME> {
    static QUEST_STATUS status() {
        if(game.get_flag(FLAG_HOME_REACHED))
            return QUEST_COMPLETED;
        return game.get_flag(FLAG_HOME_STARTED) ? QUEST_ACTIVE : QUEST_NOT_STARTED;
    }

    static auto title() {
        return F("Go Home");
    }

    static auto summary() {
        if(game.get_flag(FLAG_HOME_REACHED))
            return F("I made it home.");
        if(game.get_flag(FLAG_LEFT_CLEARING))
            return F("I'm out of the clearing. My home is in the village to the east.");
        if(game.get_flag(FLAG_BOULDERS_NOTICED))
            return F("Boulders rolled into the way out while I slept. I have to push them aside to "
                     "make an exit.");
        return F(
            "I dozed off in the forest clearing west of the village. The way out is to the east.");
    }

    static char const PROGMEM* objective() {
        if(status() != QUEST_ACTIVE)
            return nullptr;
        if(game.get_flag(FLAG_LEFT_CLEARING))
            return F("Go to your home");
        return game.get_flag(FLAG_BOULDERS_NOTICED) ? F("Push the boulders aside")
                                                    : F("Leave the clearing");
    }

    static bool marker(uint8_t area, TilePos& out) {
        if(game.get_flag(FLAG_LEFT_CLEARING))
            return quest_fixed_marker<positions::AldenDoor>(area, out);
        return quest_fixed_marker<positions::ClearingExit>(area, out);
    }

    static void start() {
        game.set_flag(FLAG_HOME_STARTED);
    }

    static bool consistent() {
        if(game.get_flag(FLAG_HOME_STARTED))
            return true;
        return !game.get_flag(FLAG_BOULDERS_NOTICED) && !game.get_flag(FLAG_LEFT_CLEARING) &&
               !game.get_flag(FLAG_HOME_REACHED) && !game.get_flag(FLAG_MENU_JOURNAL);
    }
};

#endif
