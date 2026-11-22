#ifndef QUEST_TRACKS_IN_DUST_H
#define QUEST_TRACKS_IN_DUST_H

#include "map_positions.h"
#include "quest.h"

template <> struct Quest<QUEST_TRACKS_IN_DUST> {
    static QUEST_STATUS status() {
        if(game.get_flag(FLAG_MINE_LEDGER))
            return QUEST_COMPLETED;
        return game.get_flag(FLAG_MINE_WARNING) || game.get_flag(FLAG_MINE_WOLF_DRIVEN)
                   ? QUEST_ACTIVE
                   : QUEST_NOT_STARTED;
    }

    static auto title() { return F("Tracks in the Dust"); }

    static auto summary() {
        if(status() == QUEST_COMPLETED)
            return F("The mine tally records the same load twice. Both entries carry an unfamiliar mark.");
        if(game.get_flag(FLAG_MINE_WOLF_DRIVEN))
            return F("The wolf is gone for now. Search the tally board in the rail chamber.");
        return F("A lone wolf is prowling the rail chamber in Hollow Mine. Keep an eye out for the old tally board.");
    }

    static auto objective() {
        if(status() != QUEST_ACTIVE)
            return (char const PROGMEM*)nullptr;
        return game.get_flag(FLAG_MINE_WOLF_DRIVEN) ? F("Read the mine tally") : F("Drive off the wolf");
    }

    static bool marker(uint8_t area, TilePos& out) {
        out.area = area;
        if(area == positions::CaveMouth.area) {
            out.x = positions::CaveMouth.x;
            out.y = positions::CaveMouth.y;
        } else if(area == positions::MineLedger.area) {
            bool driven = game.get_flag(FLAG_MINE_WOLF_DRIVEN);
            out.x = driven ? positions::MineLedger.x : positions::MineScout.x;
            out.y = driven ? positions::MineLedger.y : positions::MineScout.y;
        } else {
            return false;
        }
        return true;
    }

    static void start() { game.set_flag(FLAG_MINE_WARNING); }

    static bool consistent() {
        return !game.get_flag(FLAG_MINE_LEDGER) || game.get_flag(FLAG_MINE_WOLF_DRIVEN);
    }

};

#endif
