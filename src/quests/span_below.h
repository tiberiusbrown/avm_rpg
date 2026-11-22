#ifndef QUEST_SPAN_BELOW_H
#define QUEST_SPAN_BELOW_H

#include "map_positions.h"
#include "quest.h"

template <> struct Quest<QUEST_SPAN_BELOW> {
    static QUEST_STATUS status() {
        if(game.get_flag(FLAG_CAVE_BRIDGE))
            return QUEST_COMPLETED;
        return game.get_flag(FLAG_BRIDGE_SURVEY) ? QUEST_ACTIVE : QUEST_NOT_STARTED;
    }

    static auto title() { return F("The Span Below"); }

    static auto summary() {
        if(status() == QUEST_COMPLETED)
            return F("The west plate extended the bridge. The far landing leads to a ladder and the old supply cache.");
        return F("Hollow Mine's bridge release is in the west nook. The chalk arrows point through the east gate first.");
    }

    static auto objective() {
        return status() == QUEST_ACTIVE ? F("Extend the mine bridge") : nullptr;
    }

    static bool marker(uint8_t area, TilePos& out) {
        if(area != positions::CaveMouth.area)
            return false;
        out.area = area;
        out.x = positions::CaveMouth.x;
        out.y = positions::CaveMouth.y;
        return true;
    }

    static void start() { game.set_flag(FLAG_BRIDGE_SURVEY); }

};

#endif
