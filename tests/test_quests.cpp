// Find Iona and the shipping quest runtime, driven through the NPC dialogue path. Dialogue answers
// are scripted, so each test chooses yes or no for every question it asks.
#include "rpg.h"
#include "chunk.h"
#include "journal.h"
#include "logic/npc_logic.h"
#include "map_positions.h"
#include "quest.h"
#include "story_stub.h"
#include "world_map.h"
#include <initializer_list>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(value)                                                                               \
    do {                                                                                           \
        ++checks;                                                                                  \
        if(!(value)) {                                                                             \
            fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #value);                        \
            exit(1);                                                                               \
        }                                                                                          \
    } while(0)

static unsigned shown, questions;
static char const* last_text;
static NPC_ID last_speaker = NPC_NONE;
static bool scripted[8];
static unsigned scripted_count, scripted_next;

// The next questions take these answers in order. Questions past the script answer no.
static void script(std::initializer_list<bool> answers) {
    scripted_count = scripted_next = 0;
    for(bool answer : answers)
        scripted[scripted_count++] = answer;
}

bool dialogue_show(char const PROGMEM* words) {
    ++shown;
    last_text = words;
    return true;
}

bool dialogue_show(NPC_ID id, char const PROGMEM* words) {
    last_speaker = id;
    return dialogue_show(words);
}

bool dialogue_ask(char const PROGMEM* question) {
    CHECK(question && strstr(question, "Iona"));
    // Unscripted questions answer no, so a test that forgets a script fails visibly.
    bool answer = scripted_next < scripted_count ? scripted[scripted_next] : false;
    ++scripted_next;
    ++questions;
    return answer;
}

bool dialogue_ask(NPC_ID id, char const PROGMEM* question) {
    (void)id;
    return dialogue_ask(question);
}

// One world frame, which runs the same dialogue and settle path as exploration.
static void talk(NPC_ID id) {
    npc_talk(id);
    update_visible_world(0, false);
}

static bool same_bytes(void const* a, void const* b, size_t size) {
    return memcmp(a, b, size) == 0;
}

static void reset_game() {
    game_new();
    shown = questions = 0;
    scripted_count = scripted_next = 0;
    last_speaker = NPC_NONE;
}

static void flags_match(Game const& expected) {
    CHECK(same_bytes(&expected.flags, &game.flags, sizeof(game.flags)));
}

static void offer_and_accept() {
    script({true});
    talk(NPC_WANDERER);
    CHECK(quest_status(QUEST_FIND_IONA) == QUEST_ACTIVE);
}

static void offer_declined_scenario() {
    reset_game();
    script({false});
    talk(NPC_WANDERER);
    CHECK(quest_status(QUEST_FIND_IONA) == QUEST_NOT_STARTED);
    CHECK(!game.get_flag(FLAG_QUEST_FIND_IONA_STARTED) && !game.get_flag(FLAG_QUEST_FIND_IONA_DONE));
    CHECK(quest_selected() == QUEST_NONE);
    CHECK(questions == 1 && strstr(last_text, "Another time"));
    // Silas asks again: a decline never remembers itself.
    script({true});
    talk(NPC_WANDERER);
    CHECK(questions == 2 && quest_status(QUEST_FIND_IONA) == QUEST_ACTIVE);
}

static void silas_conversations_scenario() {
    reset_game();
    offer_and_accept();
    CHECK(game.get_flag(FLAG_QUEST_FIND_IONA_STARTED) && !game.get_flag(FLAG_QUEST_FIND_IONA_DONE));
    CHECK(quest_selected() == QUEST_FIND_IONA);
    CHECK(questions == 1 && last_speaker == NPC_WANDERER);

    Game before = game;
    talk(NPC_WANDERER);
    CHECK(questions == 1 && strstr(last_text, "found Iona"));
    CHECK(quest_status(QUEST_FIND_IONA) == QUEST_ACTIVE && quest_selected() == QUEST_FIND_IONA);
    flags_match(before);
}

static void iona_before_acceptance_scenario() {
    reset_game();
    talk(NPC_VILLAGER);
    CHECK(strstr(last_text, "Nocturne Grove") && last_speaker == NPC_VILLAGER);
    CHECK(game.get_flag(FLAG_VILLAGER_MOVED) && game.get_flag(FLAG_VILLAGER_EAST));
    CHECK(quest_status(QUEST_FIND_IONA) == QUEST_NOT_STARTED);
    CHECK(!game.get_flag(FLAG_QUEST_FIND_IONA_DONE));
    CHECK(npc_location(NPC_VILLAGER) == positions::VillagerEast);

    // Accepting now places the objective at Iona's current logical site, not her starting one.
    offer_and_accept();
    TilePos marker;
    CHECK(quest_map_marker(AREA_OUTDOOR, marker) && marker == positions::VillagerEast);
    CHECK(quest_status(QUEST_FIND_IONA) == QUEST_ACTIVE);
}

static void iona_completion_scenario() {
    reset_game();
    offer_and_accept();
    TilePos marker;
    CHECK(quest_map_marker(AREA_OUTDOOR, marker) && marker == positions::VillagerHome);

    talk(NPC_VILLAGER);
    CHECK(strstr(last_text, "worried about me"));
    CHECK(quest_status(QUEST_FIND_IONA) == QUEST_COMPLETED);
    CHECK(game.get_flag(FLAG_QUEST_FIND_IONA_DONE) && game.get_flag(FLAG_QUEST_FIND_IONA_STARTED));
    // Completion clears tracking and the marker, and the quest stays in the Journal.
    CHECK(quest_selected() == QUEST_NONE);
    CHECK(!quest_map_marker(AREA_OUTDOOR, marker));
    CHECK(quest_listed(QUEST_FIND_IONA) && quest_journal_count() == 1);
    CHECK(!quest_objective(QUEST_FIND_IONA));
    // Iona's own movement is independent of the quest. Her first conversation sent her east.
    CHECK(npc_location(NPC_VILLAGER) == positions::VillagerEast);

    // Later conversations grant nothing and change no quest state; her movement still follows.
    Game done = game;
    talk(NPC_VILLAGER);
    CHECK(strstr(last_text, "Nocturne Grove"));
    CHECK(quest_status(QUEST_FIND_IONA) == QUEST_COMPLETED && quest_selected() == QUEST_NONE);
    CHECK(game.get_flag(FLAG_VILLAGER_MOVED) && !game.get_flag(FLAG_VILLAGER_EAST));
    CHECK(npc_location(NPC_VILLAGER) == positions::VillagerWest);
    CHECK(game.get_flag(FLAG_QUEST_FIND_IONA_DONE) == done.get_flag(FLAG_QUEST_FIND_IONA_DONE));
    CHECK(!quest_map_marker(AREA_OUTDOOR, marker));

    // Silas now acknowledges the result, and he does not offer the quest again.
    talk(NPC_WANDERER);
    CHECK(strstr(last_text, "Good to hear Iona's safe") && questions == 1);
}

static void unrelated_npc_scenario() {
    reset_game();
    offer_and_accept();
    // Talking to Rowan or Silas, and any other NPC, never completes Find Iona.
    script({true});
    talk(NPC_WANDERER);
    talk(NPC_WANDERER);
    CHECK(quest_status(QUEST_FIND_IONA) == QUEST_ACTIVE);
}

static void tracking_scenario() {
    reset_game();
    CHECK(!quest_select(QUEST_FIND_IONA));
    CHECK(quest_selected() == QUEST_NONE);
    quest_deselect();
    CHECK(quest_selected() == QUEST_NONE);

    offer_and_accept();
    CHECK(quest_selected() == QUEST_FIND_IONA);
    quest_deselect();
    CHECK(quest_selected() == QUEST_NONE);
    CHECK(quest_select(QUEST_FIND_IONA) && quest_selected() == QUEST_FIND_IONA);
    CHECK(quest_select(QUEST_FIND_IONA) && quest_selected() == QUEST_FIND_IONA);
    // Tracking does not change the quest's own state.
    CHECK(quest_status(QUEST_FIND_IONA) == QUEST_ACTIVE && !game.get_flag(FLAG_QUEST_FIND_IONA_DONE));

    // Invalid identities are never tracked.
    CHECK(!quest_select(QUEST_NONE));
    CHECK(!quest_select((QUEST_ID)QUEST_ID_LIMIT));
    CHECK(!quest_select((QUEST_ID)200));
    CHECK(quest_selected() == QUEST_FIND_IONA);

    // A started quest is not restarted, and a second start leaves tracking as it was.
    Game before = game;
    CHECK(!quest_start(QUEST_FIND_IONA));
    CHECK(quest_selected() == QUEST_FIND_IONA);
    flags_match(before);
}

static void marker_scenario() {
    reset_game();
    TilePos marker;
    CHECK(!quest_map_marker(AREA_OUTDOOR, marker));
    CHECK(!quest_marker(QUEST_FIND_IONA, AREA_OUTDOOR, marker));

    offer_and_accept();
    CHECK(quest_marker(QUEST_FIND_IONA, AREA_OUTDOOR, marker));
    CHECK(marker == npc_location(NPC_VILLAGER) && marker == positions::VillagerHome);
    // Another area has no marker, and the Journal has no map position to show there.
    CHECK(!quest_marker(QUEST_FIND_IONA, AREA_CAVE, marker));
    CHECK(!quest_marker(QUEST_FIND_IONA, AREA_INTERIORS, marker));

    // The marker follows Iona's logical site as it changes, without quest-specific coordinates. Her
    // policy moves her only after she has been spoken to, which the moved flag records.
    game.set_flag(FLAG_VILLAGER_MOVED);
    game.set_flag(FLAG_VILLAGER_EAST);
    CHECK(quest_map_marker(AREA_OUTDOOR, marker) && marker == positions::VillagerEast);
    game.toggle_flag(FLAG_VILLAGER_EAST);
    CHECK(quest_map_marker(AREA_OUTDOOR, marker) && marker == positions::VillagerWest);
    game.clear_flag(FLAG_VILLAGER_MOVED);
    game.clear_flag(FLAG_VILLAGER_EAST);
    CHECK(quest_map_marker(AREA_OUTDOOR, marker) && marker == positions::VillagerHome);

    // An untracked quest keeps its marker, but the map shows only the tracked one.
    quest_deselect();
    CHECK(!quest_map_marker(AREA_OUTDOOR, marker));
    CHECK(quest_marker(QUEST_FIND_IONA, AREA_OUTDOOR, marker));
}

static void journal_scenario() {
    reset_game();
    CHECK(quest_journal_count() == 0 && quest_journal_entry(0) == QUEST_NONE);
    CHECK(!quest_listed(QUEST_FIND_IONA));
    offer_and_accept();
    CHECK(quest_journal_count() == 1 && quest_journal_entry(0) == QUEST_FIND_IONA);
    CHECK(quest_journal_entry(1) == QUEST_NONE);
    CHECK(!strcmp(quest_title(QUEST_FIND_IONA), "Find Iona"));
    CHECK(!strcmp(quest_objective(QUEST_FIND_IONA), "Speak to Iona"));
    CHECK(!strcmp(quest_summary(QUEST_FIND_IONA),
                  "Silas asked me to check on Iona. She's somewhere in Nocturne Grove."));
    talk(NPC_VILLAGER);
    CHECK(quest_journal_count() == 1 && quest_journal_entry(0) == QUEST_FIND_IONA);
    CHECK(!strcmp(quest_summary(QUEST_FIND_IONA),
                  "I found Iona and learned that she was safe. Silas had nothing to worry about."));
}

static void invalid_identity_scenario() {
    reset_game();
    CHECK(quest_status(QUEST_NONE) == QUEST_NOT_STARTED);
    CHECK(quest_status((QUEST_ID)QUEST_ID_LIMIT) == QUEST_NOT_STARTED);
    CHECK(!quest_start(QUEST_NONE) && !quest_start((QUEST_ID)200));
    CHECK(!strcmp(quest_title(QUEST_NONE), ""));
    CHECK(!quest_objective((QUEST_ID)200));
    TilePos marker;
    CHECK(!quest_marker(QUEST_NONE, AREA_OUTDOOR, marker));
    CHECK(!quest_listed(QUEST_NONE));
    quest_deselect();
    CHECK(quest_selected() == QUEST_NONE);
}

static void query_side_effects_scenario() {
    reset_game();
    offer_and_accept();
    Game before = game;
    unsigned evaluations = quest_evaluations;
    TilePos marker;
    for(int i = 0; i < 50; ++i) {
        quest_status(QUEST_FIND_IONA);
        quest_title(QUEST_FIND_IONA);
        quest_summary(QUEST_FIND_IONA);
        quest_objective(QUEST_FIND_IONA);
        quest_marker(QUEST_FIND_IONA, AREA_OUTDOOR, marker);
        quest_map_marker(AREA_OUTDOOR, marker);
        quest_journal_count();
        quest_journal_entry(0);
        quest_saved_state_valid();
    }
    CHECK(!memcmp(&before, &game, sizeof(Game)));
    CHECK(quest_evaluations == evaluations);
}

static void normal_frames_scenario() {
    reset_game();
    offer_and_accept();
    unsigned evaluations = quest_evaluations;
    for(int i = 0; i < 20; ++i)
        update_visible_world(0, false);
    CHECK(quest_evaluations == evaluations);
}

// A monospaced stand-in measures each character as five pixels, so the layout is exact. Like the
// device's width callback, it holds no state.
struct FiveWide {
    uint16_t operator()(char const* text, uint8_t from, uint8_t to) const {
        (void)text;
        return (uint16_t)((to - from) * 5);
    }
};

static void journal_wrap_scenario() {
    char const* texts[] = {
        "Silas asked me to check on Iona. She's somewhere in Nocturne Grove.",
        "I found Iona and learned that she was safe. Silas had nothing to worry about.",
    };
    uint8_t starts[JOURNAL_SUMMARY_LINES + 1];
    for(char const* text : texts) {
        uint8_t count = journal_wrap(text, FiveWide{}, starts, JOURNAL_SUMMARY_LINES);
        CHECK(count > 0 && count <= JOURNAL_SUMMARY_LINES);
        // Joining the lines restores the text, and no line exceeds the width.
        char joined[160] = {};
        for(uint8_t k = 0; k < count; ++k) {
            uint8_t to = starts[k + 1];
            while(to > starts[k] && text[to - 1] == ' ')
                --to;
            CHECK(FiveWide{}(text, starts[k], to) <= JOURNAL_TEXT_WIDTH);
            if(k)
                strcat(joined, " ");
            strncat(joined, text + starts[k], to - starts[k]);
        }
        CHECK(!strcmp(joined, text));
    }
    // A word wider than the line is broken between characters, and no text is lost.
    char const* long_word = "Supercalifragilisticexpialidocious_extended_ending_here";
    uint8_t count = journal_wrap(long_word, FiveWide{}, starts, JOURNAL_SUMMARY_LINES);
    CHECK(count >= 2 && starts[count] == strlen(long_word));
    char rejoined[64] = {};
    for(uint8_t k = 0; k < count; ++k) {
        CHECK(FiveWide{}(long_word, starts[k], starts[k + 1]) <= JOURNAL_TEXT_WIDTH);
        strncat(rejoined, long_word + starts[k], starts[k + 1] - starts[k]);
    }
    CHECK(!strcmp(rejoined, long_word));
    // A summary too long for the space is truncated at the capacity.
    count = journal_wrap(long_word, FiveWide{}, starts, 1);
    CHECK(count == 1);
}

static void save_scenario() {
    reset_game();
    offer_and_accept();
    Game saved = game;
    CHECK(save_valid());

    // Save while active and tracked, then reload: status, tracking and the marker are preserved.
    talk(NPC_WANDERER);
    game = saved;
    CHECK(save_restore());
    update_visible_world(0, false);
    CHECK(quest_status(QUEST_FIND_IONA) == QUEST_ACTIVE && quest_selected() == QUEST_FIND_IONA);
    TilePos marker;
    CHECK(quest_map_marker(AREA_OUTDOOR, marker) && marker == positions::VillagerHome);

    // Completion survives reload, and loading replays no conversation or quest effect.
    talk(NPC_VILLAGER);
    Game done = game;
    game = done;
    CHECK(save_valid() && save_restore());
    shown = questions = 0;
    update_visible_world(0, false);
    CHECK(quest_status(QUEST_FIND_IONA) == QUEST_COMPLETED && quest_selected() == QUEST_NONE);
    CHECK(shown == 0 && questions == 0);
    flags_match(done);
    CHECK(!quest_map_marker(AREA_OUTDOOR, marker));

    // Before acceptance, a loaded game still offers the quest.
    reset_game();
    Game fresh = game;
    game = fresh;
    CHECK(save_restore() && quest_status(QUEST_FIND_IONA) == QUEST_NOT_STARTED);
    script({true});
    talk(NPC_WANDERER);
    CHECK(quest_status(QUEST_FIND_IONA) == QUEST_ACTIVE);
}

static void malformed_save_scenario() {
    reset_game();
    offer_and_accept();
    Game good = game;

    game = good;
    game.selected_quest = (QUEST_ID)200;
    CHECK(!save_valid());

    // Completion is impossible without the start fact.
    game = good;
    game.clear_flag(FLAG_QUEST_FIND_IONA_STARTED);
    game.set_flag(FLAG_QUEST_FIND_IONA_DONE);
    CHECK(!save_valid() && !save_restore());

    // A selection that names an inactive quest is dropped on restore, because it is not corrupt.
    game = good;
    game.clear_flag(FLAG_QUEST_FIND_IONA_STARTED);
    game.selected_quest = QUEST_FIND_IONA;
    CHECK(save_valid());
    CHECK(save_restore());
    CHECK(quest_selected() == QUEST_NONE);
    CHECK(quest_status(QUEST_FIND_IONA) == QUEST_NOT_STARTED);

    // A stale save version is rejected before any quest state is read.
    game = good;
    game.save_version = SAVE_VERSION - 1;
    CHECK(!save_valid());
    game = good;
}

static void reset_scenario() {
    reset_game();
    offer_and_accept();
    CHECK(quest_selected() == QUEST_FIND_IONA);
    // A new game clears tracking and the quest history with the rest of the state.
    game_new();
    CHECK(quest_selected() == QUEST_NONE && quest_status(QUEST_FIND_IONA) == QUEST_NOT_STARTED);
    CHECK(!game.get_flag(FLAG_QUEST_FIND_IONA_STARTED));
}

int main() {
    static_assert(QUEST_ID_LIMIT == 2, "the shipping catalog registers Find Iona only");
    offer_declined_scenario();
    silas_conversations_scenario();
    iona_before_acceptance_scenario();
    iona_completion_scenario();
    unrelated_npc_scenario();
    tracking_scenario();
    marker_scenario();
    journal_scenario();
    invalid_identity_scenario();
    query_side_effects_scenario();
    normal_frames_scenario();
    journal_wrap_scenario();
    save_scenario();
    malformed_save_scenario();
    reset_scenario();
    printf("quest checks passed: %u\n", checks);
    return 0;
}
