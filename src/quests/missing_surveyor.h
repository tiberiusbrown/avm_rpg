#ifndef QUEST_MISSING_SURVEYOR_H
#define QUEST_MISSING_SURVEYOR_H

#include "map_positions.h"
#include "quest.h"

template <> struct Quest<QUEST_MISSING_SURVEYOR> {
    static QUEST_STATUS status() {
        if(game.get_flag(FLAG_SURVEYOR_RESCUED))
            return QUEST_COMPLETED;
        return game.get_flag(FLAG_SURVEY_STARTED) ? QUEST_ACTIVE : QUEST_NOT_STARTED;
    }

    static auto title() { return F("The Missing Surveyor"); }

    static auto summary() {
        if(game.get_flag(FLAG_SURVEYOR_RESCUED))
            return F("The surveyor is back at the Lantern Inn. The bridge and west ridge hatch stay open.");
        if(game.get_flag(FLAG_SURVEYOR_FOUND))
            return F("The surveyor is waiting beyond the bridge. Show them the way up the maintenance ladder.");
        if(game.get_flag(FLAG_CAVE_BRIDGE))
            return F("Cross the restored mine bridge. Search the landing near the ladder.");
        if(!game.get_flag(FLAG_SURVEY_NOTES_READ) && !game.get_flag(FLAG_SURVEY_TRAIL_FOUND))
            return F("The surveyor went to Hollow Mine. Their notes at the inn may help you find a way through.");
        return F("Follow the rails to the east gate. The winch room opens the west nook, where a plate releases the bridge.");
    }

    static char const PROGMEM* objective() {
        if(status() != QUEST_ACTIVE)
            return nullptr;
        if(game.get_flag(FLAG_SURVEYOR_FOUND))
            return F("Help the surveyor out");
        if(game.get_flag(FLAG_CAVE_BRIDGE))
            return F("Search far landing");
        return F("Cross the mine");
    }

    static bool marker(uint8_t area, TilePos& out) {
        bool bridge = game.get_flag(FLAG_CAVE_BRIDGE);
        if(area == positions::SurveyorTrapped.area) {
            if(bridge)
                return quest_fixed_marker<positions::SurveyorTrapped>(area, out);
            if(game.get_flag(FLAG_CAVE_NOOK))
                return quest_fixed_marker<positions::CavePlate>(area, out);
            return game.get_flag(FLAG_CAVE_GATE)
                       ? quest_fixed_marker<positions::CaveVaultLever>(area, out)
                       : quest_fixed_marker<positions::CaveLever>(area, out);
        }
        if(bridge)
            return quest_fixed_marker<positions::CaveHole>(area, out);
        if(!game.get_flag(FLAG_SURVEY_NOTES_READ) && !game.get_flag(FLAG_SURVEY_TRAIL_FOUND) &&
           quest_fixed_marker<positions::SurveyorNotes>(area, out))
            return true;
        return quest_fixed_marker<positions::CaveMouth>(area, out);
    }

    static void start() { game.set_flag(FLAG_SURVEY_STARTED); }

    static bool consistent() {
        bool found = game.get_flag(FLAG_SURVEYOR_FOUND);
        return (!found || game.get_flag(FLAG_SURVEY_STARTED)) &&
               (!game.get_flag(FLAG_SURVEY_NOTES_READ) || game.get_flag(FLAG_SURVEY_STARTED)) &&
               (!game.get_flag(FLAG_SURVEYOR_RESCUED) ||
                (found && game.get_flag(FLAG_CAVE_BRIDGE)));
    }

    static bool rescue() {
        if(!game.get_flag(FLAG_SURVEYOR_FOUND) || !game.get_flag(FLAG_CAVE_BRIDGE) ||
           game.get_flag(FLAG_SURVEYOR_RESCUED))
            return false;
        game.set_flag(FLAG_SURVEYOR_RESCUED);
        quest_award_reward(8, 15);
        return true;
    }
};

#endif
