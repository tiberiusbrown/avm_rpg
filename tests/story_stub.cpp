#include "story_stub.h"

#include <stdio.h>
#include <stdlib.h>

static StoryEvent events[STORY_LOG_CAPACITY];
static unsigned event_count;
static unsigned battle_count;
static unsigned join_count;
static NPC_ID last_join = NPC_NONE;
static BATTLE_RESULT next_result = BATTLE_VICTORY;

static SfxRecord sounds[SFX_LOG_CAPACITY];
static unsigned sound_count;

void fade_world_out() {}

void fade_world_in() {}

void dialogue_close() {}

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
    join_count = 0;
    last_join = NPC_NONE;
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

void party_joined(NPC_ID id) {
    ++join_count;
    last_join = id;
}

unsigned story_stub_joins() {
    return join_count;
}

NPC_ID story_stub_last_join() {
    return last_join;
}

// The requests are kept up to the log's capacity, and later ones only count against the total.
static void record_sound(SfxRecord record) {
    if(sound_count < SFX_LOG_CAPACITY)
        sounds[sound_count] = record;
    ++sound_count;
}

void sfx_play(SFX effect, SFX then) {
    record_sound({effect, then, false});
}

void sfx_ambient(SFX effect) {
    record_sound({effect, SFX_NONE, true});
}

void sfx_stub_reset() {
    sound_count = 0;
}

unsigned sfx_stub_count() {
    return sound_count;
}

SfxRecord const& sfx_stub_record(unsigned index) {
    return sounds[index];
}
