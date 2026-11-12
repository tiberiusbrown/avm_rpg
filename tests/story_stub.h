#ifndef STORY_STUB_H
#define STORY_STUB_H

#include "battle.h"

// Host records of story calls. Dialogue stubs and the battle stub append to one ordered log, so a
// test can check that a handler's dialogue and battle calls happen in the right sequence.
enum STORY_EVENT_KIND : uint8_t { STORY_EVENT_DIALOGUE, STORY_EVENT_BATTLE };

struct StoryEvent {
    STORY_EVENT_KIND kind;
    char const* text;
    BattleEncounter const* encounter;
};

inline constexpr unsigned STORY_LOG_CAPACITY = 32;

// Clears the log and sets the result that the next battle_show calls return.
void story_stub_reset(BATTLE_RESULT result);
void story_stub_set_result(BATTLE_RESULT result);
void story_stub_record_dialogue(char const* text);

unsigned story_stub_count();
StoryEvent const& story_stub_event(unsigned index);
unsigned story_stub_battles();

#endif
