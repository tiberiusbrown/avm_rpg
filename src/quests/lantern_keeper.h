#ifndef QUEST_LANTERN_KEEPER_H
#define QUEST_LANTERN_KEEPER_H

#include "map_positions.h"
#include "quest.h"

template <> struct Quest<QUEST_LANTERN_KEEPER> {
    static QUEST_STATUS status() {
        if(game.get_flag(FLAG_LANTERN_RESTORED))
            return QUEST_COMPLETED;
        return game.get_flag(FLAG_LANTERN_STARTED) ? QUEST_ACTIVE : QUEST_NOT_STARTED;
    }

    static auto title() { return F("The Lantern Keeper"); }

    static bool ready() {
        return game.get_flag(FLAG_LANTERN_DIAGNOSED) && game.get_flag(FLAG_LANTERN_RECORD_FOUND);
    }

    static auto summary() {
        if(game.get_flag(FLAG_LANTERN_RESTORED))
            return game.get_flag(FLAG_LANTERN_MODERN)
                       ? F("Bram fitted a new regulator. The well lantern lights the square again.")
                       : F("The old mine relay is working again. Its light keeps time with the well.");
        if(!ready())
            return F("The well lantern has failed. Ask Bram in his yard and read the old record inside his smithy.");
        if(game.get_flag(FLAG_LANTERN_RELAY_FOUND))
            return F("Take the relay to the well lantern. Bram can fit it or make a new regulator.");
        return F("Bram can fit a new regulator at the well. The east winch room in Hollow Mine holds the old relay if you want to restore it.");
    }

    static char const PROGMEM* objective() {
        if(status() != QUEST_ACTIVE)
            return nullptr;
        if(!game.get_flag(FLAG_LANTERN_DIAGNOSED))
            return F("Ask Bram");
        if(!game.get_flag(FLAG_LANTERN_RECORD_FOUND))
            return F("Read smithy record");
        return game.get_flag(FLAG_LANTERN_RELAY_FOUND) ? F("Choose a repair")
                                                       : F("Repair or get relay");
    }

    static bool marker(uint8_t area, TilePos& out) {
        if(!game.get_flag(FLAG_LANTERN_DIAGNOSED))
            return quest_fixed_marker<positions::BramYard>(area, out);
        if(!game.get_flag(FLAG_LANTERN_RECORD_FOUND))
            return quest_fixed_marker<positions::WorkshopRecord>(area, out) ||
                   quest_fixed_marker<positions::SmithyDoor>(area, out);
        if(area == positions::MineLanternRelay.area && !game.get_flag(FLAG_LANTERN_RELAY_FOUND))
            return game.get_flag(FLAG_CAVE_GATE)
                       ? quest_fixed_marker<positions::MineLanternRelay>(area, out)
                       : quest_fixed_marker<positions::CaveLever>(area, out);
        return quest_fixed_marker<positions::VillageLantern>(area, out);
    }

    static void start() { game.set_flag(FLAG_LANTERN_STARTED); }

    static bool consistent() {
        if(!game.get_flag(FLAG_LANTERN_RESTORED))
            return !game.get_flag(FLAG_LANTERN_MODERN);
        return game.get_flag(FLAG_LANTERN_STARTED) && ready() &&
               (game.get_flag(FLAG_LANTERN_MODERN) || game.get_flag(FLAG_LANTERN_RELAY_FOUND));
    }

    static bool repair(bool modern) {
        if(!ready() || game.get_flag(FLAG_LANTERN_RESTORED) ||
           (!modern && !game.get_flag(FLAG_LANTERN_RELAY_FOUND)))
            return false;
        quest_start(QUEST_LANTERN_KEEPER);
        if(modern)
            game.set_flag(FLAG_LANTERN_MODERN);
        game.set_flag(FLAG_LANTERN_RESTORED);
        quest_award_reward(6, 12);
        return true;
    }
};

#endif
