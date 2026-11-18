#ifndef STORY_STUB_H
#define STORY_STUB_H

#include "battle.h"
#include "sfx.h"

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

// Host records of sound requests, in order. `ambient` marks a sfx_ambient request, which the
// platform drops while another effect plays; a test sees every request that the rules make.
struct SfxRecord {
    SFX effect, then;
    bool ambient;
};

inline constexpr unsigned SFX_LOG_CAPACITY = 64;

void sfx_stub_reset();
unsigned sfx_stub_count();
SfxRecord const& sfx_stub_record(unsigned index);

unsigned story_stub_count();
StoryEvent const& story_stub_event(unsigned index);
unsigned story_stub_battles();
unsigned story_stub_joins();
NPC_ID story_stub_last_join();

#endif
