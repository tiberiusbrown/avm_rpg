// Runtime checks of the quest machinery against a 40-ID catalog: flag subscriptions, cascades,
// bounded settling, conversation delivery, tracking, markers, consistency and a long Journal. The
// synthetic quests live in quest_catalog_registry.h, and this file reads their probe arrays.
#include "rpg.h"
#include "chunk.h"
#include "logic/npc_logic.h"
#include "map_positions.h"
#include "quest.h"
#include "quest_catalog_registry.h"
#include "story_stub.h"
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

// Conversations are not under test here, so the dialogue stubs only make the NPC library link.
bool dialogue_show(char const PROGMEM*) {
    return true;
}

bool dialogue_show(NPC_ID id, char const PROGMEM* words) {
    (void)id;
    return dialogue_show(words);
}

bool dialogue_ask(char const PROGMEM*) {
    return false;
}

bool dialogue_ask(NPC_ID id, char const PROGMEM* question) {
    (void)id;
    return dialogue_ask(question);
}

static QUEST_ID Q(uint8_t id) {
    return (QUEST_ID)id;
}

static void fresh() {
    game_new();
    quest_reset();
    memset(test_status, 0, sizeof(test_status));
    memset(test_talks, 0, sizeof(test_talks));
    memset(test_evaluations, 0, sizeof(test_evaluations));
    memset(test_starts, 0, sizeof(test_starts));
    memset(test_completions, 0, sizeof(test_completions));
}

static void catalog_shape() {
    static_assert(QUEST_ID_LIMIT == 40, "this build registers forty quests");
    CHECK(quest_status(QUEST_NONE) == QUEST_NOT_STARTED);
    CHECK(quest_status(Q(QUEST_ID_LIMIT)) == QUEST_NOT_STARTED);
    CHECK(!quest_start(Q(QUEST_ID_LIMIT)) && !quest_select(Q(200)));
    CHECK(!quest_listed(QUEST_NONE) && !quest_listed(Q(200)));
}

static void flag_subscriptions() {
    fresh();
    unsigned before = quest_evaluations;
    // The watchers are inactive, so the change is seen but no quest progresses.
    game.set_flag(FLAG_CAVE_GATE);
    quest_settle();
    CHECK(test_evaluations[2] == 0 && test_evaluations[11] == 0);
    CHECK(quest_evaluations == before);

    // Settling cleared the notification, so an active quest sees nothing until the next change.
    test_status[2] = QUEST_ACTIVE;
    quest_settle();
    CHECK(quest_evaluations == before);

    // Setting a flag that is already set is no change, so nothing is notified.
    game.set_flag(FLAG_CAVE_GATE);
    quest_settle();
    CHECK(quest_evaluations == before);

    // A real change reaches each subscriber once, and coalesces with a second change in the frame.
    game.clear_flag(FLAG_CAVE_GATE);
    game.set_flag(FLAG_CAVE_GATE);
    test_status[11] = QUEST_ACTIVE;
    quest_settle();
    CHECK(test_evaluations[2] == 1 && test_evaluations[11] == 1);
    CHECK(test_evaluations[3] == 0);

    // A flag that no quest watches notifies nothing, and invalid flag values are ignored.
    unsigned evaluations = quest_evaluations;
    game.set_flag(FLAG_DUNGEON_CHEST);
    game.toggle_flag(FLAG_CAVE_CHEST);
    game.set_flag((FLAG)200);
    quest_settle();
    CHECK(quest_evaluations == evaluations);
}

static void cascade() {
    fresh();
    test_status[2] = test_status[3] = QUEST_ACTIVE;
    unsigned first = test_evaluations[2], second = test_evaluations[3];
    game.set_flag(FLAG_CAVE_GATE);
    quest_settle();
    // The gate's evaluation sets the nook, which schedules quest 3 in the next pass, deterministically.
    CHECK(test_evaluations[2] == first + 1 && test_evaluations[3] == second + 1);
    CHECK(game.get_flag(FLAG_CAVE_NOOK) && game.get_flag(FLAG_CAVE_BRIDGE));
    unsigned work = quest_evaluations;
    quest_settle();
    CHECK(quest_evaluations == work);
}

static void bounded_cycle() {
    fresh();
    test_status[10] = QUEST_ACTIVE;
    unsigned before = test_evaluations[10];
    game.set_flag(FLAG_DUNGEON_ORB);
    quest_settle();
    // Four passes per settle bound the frame. The last toggle still pends, so the work continues.
    CHECK(test_evaluations[10] - before == 4);
    quest_settle();
    CHECK(test_evaluations[10] - before == 8);
    // A reset discards the transient notification, which ends the cycle.
    quest_reset();
    quest_settle();
    CHECK(test_evaluations[10] - before == 8);
}

static void conversations() {
    fresh();
    quest_emit(NpcTalked<NPC_ORRIN>{});
    quest_emit(NpcTalked<NPC_ORRIN>{});
    CHECK(test_talks[4] == 2 && test_talks[5] == 0);
    // An NPC with no subscribers reaches no handler.
    quest_emit(NpcTalked<NPC_ROWAN>{});
    CHECK(test_talks[4] == 2 && test_talks[5] == 0);

    test_status[5] = QUEST_ACTIVE;
    CHECK(quest_select(Q(5)) && quest_selected() == Q(5));
    quest_emit(NpcTalked<NPC_TESSA>{});
    CHECK(test_talks[5] == 1 && test_completions[5] == 1 && test_status[5] == QUEST_COMPLETED);
    // Completion clears tracking, and the selection is not given to another quest.
    CHECK(quest_selected() == QUEST_NONE);
    // A repeated conversation delivers again but completes nothing new.
    quest_emit(NpcTalked<NPC_TESSA>{});
    CHECK(test_talks[5] == 2 && test_completions[5] == 1);
    // The shipping quest does not subscribe to the synthetic conversation.
    CHECK(quest_status(QUEST_LANTERN_KEEPER) == QUEST_NOT_STARTED);
}

static void starts() {
    fresh();
    // A quest without start() cannot be started, and it stays unchanged.
    CHECK(!quest_start(Q(7)));
    CHECK(quest_status(Q(7)) == QUEST_NOT_STARTED && quest_selected() == QUEST_NONE);
    CHECK(quest_start(Q(8)) && test_starts[8] == 1);
    CHECK(quest_status(Q(8)) == QUEST_ACTIVE && quest_selected() == Q(8));
    // Starting again is idempotent, and it does not run start() again.
    CHECK(!quest_start(Q(8)) && test_starts[8] == 1);
    // A later start keeps the tracked quest.
    CHECK(quest_start(QUEST_LANTERN_KEEPER) && quest_selected() == Q(8));
    CHECK(quest_select(QUEST_LANTERN_KEEPER) && quest_selected() == QUEST_LANTERN_KEEPER);
}

static void long_journal() {
    fresh();
    // Quests 2 to 39 take each status in turn, so listings and tracking cover every state.
    QUEST_ID expected[QUEST_ID_LIMIT];
    unsigned listed = 0;
    for(uint8_t id = 2; id < QUEST_ID_LIMIT; ++id) {
        test_status[id] = (QUEST_STATUS)(id % 4);
        if(test_status[id] != QUEST_NOT_STARTED)
            expected[listed++] = Q(id);
    }
    CHECK(quest_journal_count() == listed);
    for(unsigned i = 0; i < listed; ++i)
        CHECK(quest_journal_entry((uint8_t)i) == expected[i]);
    CHECK(quest_journal_entry((uint8_t)listed) == QUEST_NONE);
    // Lantern Keeper is not started, so it is hidden from the Journal.
    CHECK(!quest_listed(QUEST_LANTERN_KEEPER));

    // A tracked quest replaces the previous one, and only active quests can be tracked.
    CHECK(quest_select(Q(9)) && quest_selected() == Q(9));
    CHECK(!quest_select(Q(10)) && !quest_select(Q(11)));
    CHECK(quest_selected() == Q(9));
}

static void markers() {
    fresh();
    TilePos marker;
    CHECK(!quest_marker(Q(6), AREA_OUTDOOR, marker));
    test_status[6] = QUEST_ACTIVE;
    CHECK(quest_marker(Q(6), AREA_OUTDOOR, marker) && marker == positions::WellSign);
    // The quest answers only for its own area, and the map shows only the tracked quest.
    CHECK(!quest_marker(Q(6), AREA_CAVE, marker));
    CHECK(!quest_map_marker(AREA_OUTDOOR, marker));
    CHECK(quest_select(Q(6)) && quest_map_marker(AREA_OUTDOOR, marker) && marker == positions::WellSign);
    // A marker outside its map is refused, whatever the quest computed.
    test_status[9] = QUEST_ACTIVE;
    CHECK(!quest_marker(Q(9), AREA_OUTDOOR, marker));
    // Completion removes the marker and clears tracking after the settle.
    test_status[6] = QUEST_COMPLETED;
    quest_settle();
    CHECK(quest_selected() == QUEST_NONE && !quest_map_marker(AREA_OUTDOOR, marker));
}

static void consistency() {
    fresh();
    CHECK(quest_saved_state_valid());
    test_status[12] = QUEST_COMPLETED;
    CHECK(!quest_saved_state_valid());
    game.set_flag(FLAG_DUNGEON_CHEST);
    CHECK(quest_saved_state_valid());
    game.selected_quest = (QUEST_ID)QUEST_ID_LIMIT;
    CHECK(!quest_saved_state_valid());
    game.selected_quest = QUEST_NONE;

    // A tracked quest that completed without a settle is dropped by the restore.
    test_status[8] = QUEST_ACTIVE;
    CHECK(quest_select(Q(8)));
    test_status[8] = QUEST_COMPLETED;
    quest_restore();
    CHECK(quest_selected() == QUEST_NONE);
}

static void reset_discards_pending() {
    fresh();
    test_status[2] = QUEST_ACTIVE;
    game.set_flag(FLAG_CAVE_GATE);
    quest_reset();
    unsigned before = quest_evaluations;
    quest_settle();
    CHECK(quest_evaluations == before && test_evaluations[2] == 0);
}

static void quiet_frames() {
    fresh();
    test_status[2] = test_status[3] = QUEST_ACTIVE;
    unsigned before = quest_evaluations;
    for(int i = 0; i < 30; ++i)
        update_visible_world(0, false);
    // Ordinary frames with no flag change do no quest evaluation work.
    CHECK(quest_evaluations == before);
}

int main() {
    catalog_shape();
    flag_subscriptions();
    cascade();
    bounded_cycle();
    conversations();
    starts();
    long_journal();
    markers();
    consistency();
    reset_discards_pending();
    quiet_frames();
    printf("quest catalog checks passed: %u\n", checks);
    return 0;
}
