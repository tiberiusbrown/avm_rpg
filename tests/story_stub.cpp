#include "story_stub.h"

#include <stdio.h>
#include <stdlib.h>

static StoryEvent events[STORY_LOG_CAPACITY];
static unsigned event_count;
static unsigned battle_count;
static BATTLE_RESULT next_result = BATTLE_VICTORY;

static void record(StoryEvent event) {
    if(event_count == STORY_LOG_CAPACITY) {
        fprintf(stderr, "story log full\n");
        exit(1);
    }
    events[event_count++] = event;
}

void story_stub_reset(BATTLE_RESULT result) {
    event_count = 0;
    battle_count = 0;
    next_result = result;
}

void story_stub_set_result(BATTLE_RESULT result) {
    next_result = result;
}

void story_stub_record_dialogue(char const* text) {
    record({STORY_EVENT_DIALOGUE, text, nullptr});
}

// Records the call and returns the configured result at once. It never runs a display loop.
BATTLE_RESULT battle_show(BattleEncounter const PROGMEM* encounter) {
    ++battle_count;
    record({STORY_EVENT_BATTLE, nullptr, encounter});
    return next_result;
}

unsigned story_stub_count() {
    return event_count;
}

StoryEvent const& story_stub_event(unsigned index) {
    return events[index];
}

unsigned story_stub_battles() {
    return battle_count;
}
