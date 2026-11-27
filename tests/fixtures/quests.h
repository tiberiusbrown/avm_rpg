#pragma once

#include "quest.h"

// Engine tests run without the game's quests, whose state shares flags with the fixture enemies.
// These stand in for them and never start.
template <QUEST_ID ID> struct FixtureQuest {
    static QUEST_STATUS status() {
        return QUEST_NOT_STARTED;
    }

    static auto title() {
        return F("Fixture quest");
    }

    static auto summary() {
        return F("Never starts.");
    }
};

template <> struct Quest<QUEST_GO_HOME> : FixtureQuest<QUEST_GO_HOME> {};

template <> struct Quest<QUEST_GIANT_RAT> : FixtureQuest<QUEST_GIANT_RAT> {};
