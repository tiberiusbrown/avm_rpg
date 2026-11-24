#include "rpg.h"
#include "chunk.h"
#include "encounter.h"
#include "journal.h"
#include "logic/enemy_catalog.h"
#include "logic/npc_logic.h"
#include "map_positions.h"
#include "quest.h"
#include "quest_arrow.h"
#include "story_stub.h"
#include "tile_ids.h"

#include <initializer_list>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static_assert(NPC_SURVEYOR == 8 && NPC_ENEMY_FIRST == 9);
static_assert(FLAG_ITEM_FIRST == 22 && FLAG_ENEMY_FIRST == 56 && FLAG_BRAM_LANTERN_MEETING == 72 &&
              FLAG_COUNT == 73);

static unsigned checks;
#define CHECK(value)                                                                               \
    do {                                                                                           \
        ++checks;                                                                                  \
        if(!(value)) {                                                                             \
            fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #value);                          \
            exit(1);                                                                               \
        }                                                                                          \
    } while(0)

static bool accept = true;
static char const* words;
static NPC_ID speaker;

bool dialogue_show(char const PROGMEM* text) {
    words = text;
    return accept;
}

bool dialogue_show(NPC_ID id, char const PROGMEM* text) {
    speaker = id;
    return dialogue_show(text);
}

bool dialogue_ask(char const PROGMEM* text) {
    return dialogue_show(text);
}

bool dialogue_ask(NPC_ID, char const PROGMEM* text) {
    return dialogue_ask(text);
}

static void stand(TilePos cell, DIRECTION direction) {
    world_enter(cell.area, cell.x * TILE, cell.y * TILE, direction);
    CHECK(game.player.x == cell.x * TILE && game.player.y == cell.y * TILE);
}

static void touch(TilePos cell, DIRECTION facing = NORTH) {
    constexpr int8_t X[] = {0, 1, 0, -1};
    constexpr int8_t Y[] = {-1, 0, 1, 0};
    stand({cell.area, (uint8_t)(cell.x - X[facing]), (uint8_t)(cell.y - Y[facing])}, facing);
    game_update(0, 0);
    CHECK(game.can_interact);
    game_update(0, BTN_A);
}

static void talk(NPC_ID id, uint8_t choice = 0, bool finish = true) {
    accept = finish;
    story_stub_set_choice(choice);
    npc_talk(id);
    update_visible_world(0, false);
    accept = true;
}

static void reload() {
    Game saved = game;
    game_new();
    game = saved;
    CHECK(save_valid() && save_restore());
    update_visible_world(0, false);
    CHECK(!memcmp(&saved.flags, &game.flags, sizeof(game.flags)));
    CHECK(saved.gold == game.gold && !memcmp(&saved.party, &game.party, sizeof(game.party)));
}

static void markers(QUEST_ID id, TilePos expected) {
    TilePos out;
    CHECK(quest_objective(id) && quest_marker(id, expected.area, out) && out == expected);
}

static void lantern(bool modern, bool start_at_inn) {
    constexpr auto id = QUEST_LANTERN_KEEPER;
    if(start_at_inn) {
        talk(NPC_MARA, 255);
        talk(NPC_MARA, 1, false);
        CHECK(quest_status(id) == QUEST_NOT_STARTED);
        talk(NPC_MARA, 1);
        CHECK(quest_status(id) == QUEST_ACTIVE && quest_selected() == QUEST_NONE);
        QuestNotice notice;
        CHECK(quest_take_notice(notice) && notice.started == id &&
              !strcmp(notice.objective, "Ask Bram"));
        CHECK(!quest_take_notice(notice));
        markers(id, positions::BramYard);
        talk(NPC_BRAM, 255);
        CHECK(!game.get_flag(FLAG_LANTERN_DIAGNOSED));
        talk(NPC_BRAM);
        markers(id, positions::SmithyDoor);
        markers(id, positions::WorkshopRecord);
        touch(positions::WorkshopRecord);
    } else {
        if(!modern) {
            touch(positions::CaveLever);
            touch(positions::MineLanternRelay);
            CHECK(game.get_flag(FLAG_LANTERN_RELAY_FOUND));
        }
        touch(positions::WorkshopRecord);
        talk(NPC_BRAM);
        CHECK(quest_status(id) == QUEST_NOT_STARTED);
        story_stub_set_choice(255);
        touch(positions::VillageLantern, EAST);
        CHECK(quest_status(id) == QUEST_ACTIVE);
    }
    if(!modern && start_at_inn) {
        if(!game.get_flag(FLAG_CAVE_GATE)) {
            markers(id, positions::CaveLever);
            touch(positions::CaveLever);
        }
        markers(id, positions::MineLanternRelay);
        touch(positions::MineLanternRelay);
    }
    markers(id, positions::VillageLantern);
    reload();

    uint16_t gold = game.gold;
    auto before = game.party.members[0];
    CHECK(cached_tile(positions::VillageLantern) == TILE_VILLAGE_LANTERN_DARK ||
          game.area != AREA_OUTDOOR);
    story_stub_set_choice(255);
    touch(positions::VillageLantern, EAST);
    CHECK(quest_status(id) == QUEST_ACTIVE);
    story_stub_set_choice(modern ? 1 : 2);
    touch(positions::VillageLantern, EAST);
    CHECK(!game.get_flag(FLAG_LANTERN_RESTORED) && !game.get_flag(FLAG_LANTERN_MODERN));
    CHECK(game.gold == gold && game.party.members[0].experience == before.experience);

    story_stub_set_choice(modern ? 0 : 1);
    touch(positions::VillageLantern, EAST);
    CHECK(quest_status(id) == QUEST_COMPLETED && !quest_objective(id));
    TilePos marker;
    CHECK(!quest_marker(id, AREA_OUTDOOR, marker));
    CHECK(game.get_flag(FLAG_LANTERN_MODERN) == modern && game.gold == gold + 12);
    CHECK(game.get_flag(FLAG_LANTERN_RELAY_FOUND) == !modern);
    unsigned xp = game.party.members[0].experience +
                  (game.party.members[0].level - before.level) * 10;
    CHECK(xp == before.experience + 6);
    reload();
    CHECK(cached_tile(positions::VillageLantern) == (modern ? TILE_VILLAGE_LANTERN_MODERN
                                                          : TILE_VILLAGE_LANTERN_ANCIENT));
    auto rewarded = game.party.members[0];
    touch(positions::VillageLantern, EAST);
    talk(NPC_MARA, 1);
    CHECK(speaker == NPC_MARA && words && game.gold == gold + 12);
    talk(NPC_BRAM);
    CHECK(speaker == NPC_BRAM && words && game.get_flag(FLAG_LANTERN_MODERN) == modern);
    CHECK(game.party.members[0].experience == rewarded.experience &&
          game.party.members[0].level == rewarded.level);
}

static void bram_meeting() {
    game_new();
    constexpr auto yard = positions::BramYard;
    constexpr auto waiting = positions::BramLantern;
    stand({yard.area, (uint8_t)(yard.x + 1), yard.y}, WEST);
    CHECK(npc_find(NPC_BRAM) && npc_location(NPC_BRAM) == yard);
    touch(positions::WorkshopRecord);
    stand({yard.area, (uint8_t)(yard.x + 1), yard.y}, WEST);
    talk(NPC_BRAM);
    CHECK(game.get_flag(FLAG_LANTERN_DIAGNOSED) && npc_location(NPC_BRAM) == yard);
    talk(NPC_BRAM, 0, false);
    CHECK(npc_location(NPC_BRAM) == yard && !game.get_flag(FLAG_BRAM_LANTERN_MEETING));
    talk(NPC_BRAM);
    CHECK(words && strstr(words, "Meet me at the well lantern"));
    CHECK(game.get_flag(FLAG_BRAM_LANTERN_MEETING) && npc_location(NPC_BRAM) == waiting);
    CHECK(save_valid());

    uint16_t start_x = npc_find(NPC_BRAM)->actor.x;
    game_update(0);
    const Npc* bram = npc_find(NPC_BRAM);
    CHECK(bram && bram->assigned_site == waiting && bram->behavior == NPC_TRAVEL);
    CHECK(bram->actor.x == start_x - 1 && bram->actor.walking);

    // Walk around the south side of the smithy and bakery to Well Square.
    auto walk = [](uint8_t button, unsigned pixels) {
        for(unsigned frame = 0; frame < pixels; ++frame)
            game_update(button);
    };
    uint8_t lane = (uint8_t)(positions::VillageLantern.x + 3);
    uint8_t square_row = positions::VillageLantern.y;
    walk(BTN_DOWN, TILE);
    walk(BTN_LEFT, (yard.x + 1 - lane) * TILE);
    walk(BTN_UP, (yard.y + 1 - square_row) * TILE);
    CHECK(game.player.x == lane * TILE && game.player.y == square_row * TILE);
    bram = npc_find(NPC_BRAM);
    CHECK(bram && bram->behavior == NPC_STAND);
    CHECK(bram->actor.x == waiting.x * TILE && bram->actor.y == waiting.y * TILE);
    walk(0, 60);
    bram = npc_find(NPC_BRAM);
    CHECK(bram && bram->behavior == NPC_STAND && bram->actor.x == waiting.x * TILE &&
          bram->actor.y == waiting.y * TILE);
    reload();
    CHECK(npc_find(NPC_BRAM) && npc_find(NPC_BRAM)->assigned_site == waiting);

    story_stub_set_choice(0);
    touch(positions::VillageLantern, EAST);
    CHECK(quest_status(QUEST_LANTERN_KEEPER) == QUEST_COMPLETED);
    CHECK(!game.get_flag(FLAG_BRAM_LANTERN_MEETING) && npc_location(NPC_BRAM) == yard);
}

static void open_bridge() {
    if(!game.get_flag(FLAG_CAVE_GATE))
        touch(positions::CaveLever);
    touch(positions::CaveLever);
    CHECK(!game.get_flag(FLAG_CAVE_GATE));
    touch(positions::CaveLever);
    touch(positions::CaveVaultLever);
    CHECK(game.get_flag(FLAG_CAVE_NOOK));
    stand(positions::CavePlate, NORTH);
    game_update(0, 0);
    CHECK(game.get_flag(FLAG_CAVE_BRIDGE));
    stand({AREA_CAVE, positions::CaveBridge_1.x, (uint8_t)(positions::CaveBridge_1.y + 1)}, NORTH);
    game_update(0, 0);
    CHECK(cached_tile(positions::CaveBridge_1) == CAVE_TILE_BRIDGE_VERTICAL);
}

enum SurveyLead { NELL, NOTES, FREE_EXPLORATION };

static void survey(SurveyLead lead) {
    constexpr auto id = QUEST_MISSING_SURVEYOR;
    CHECK(npc_location(NPC_SURVEYOR) == positions::SurveyorTrapped);
    if(lead == NELL) {
        talk(NPC_NELL, 0, false);
        CHECK(quest_status(id) == QUEST_NOT_STARTED);
        talk(NPC_NELL);
        CHECK(!game.get_flag(FLAG_SURVEY_NOTES_READ));
    } else if(lead == NOTES) {
        touch(positions::SurveyTrailMark);
        CHECK(game.get_flag(FLAG_SURVEY_TRAIL_FOUND) && quest_status(id) == QUEST_NOT_STARTED);
        touch(positions::SurveyorNotes);
        CHECK(game.get_flag(FLAG_SURVEY_NOTES_READ));
    }
    if(lead != FREE_EXPLORATION) {
        CHECK(quest_status(id) == QUEST_ACTIVE);
        markers(id, positions::CaveMouth);
    }
    open_bridge();
    CHECK(quest_status(id) == (lead == FREE_EXPLORATION ? QUEST_NOT_STARTED : QUEST_ACTIVE));
    if(lead != FREE_EXPLORATION)
        markers(id, positions::SurveyorTrapped);

    uint16_t gold = game.gold;
    auto before = game.party.members[0];
    accept = false;
    touch(positions::SurveyorTrapped);
    CHECK(game.get_flag(FLAG_SURVEYOR_FOUND) && !game.get_flag(FLAG_SURVEYOR_RESCUED));
    CHECK(quest_status(id) == QUEST_ACTIVE && game.gold == gold);
    CHECK(npc_find(NPC_SURVEYOR) && npc_find(NPC_SURVEYOR)->assigned_site == positions::SurveyorTrapped);
    reload();
    CHECK(npc_find(NPC_SURVEYOR));
    accept = true;
    touch(positions::SurveyorTrapped);
    CHECK(quest_status(id) == QUEST_COMPLETED && !quest_objective(id) && !npc_find(NPC_SURVEYOR));
    CHECK(npc_location(NPC_SURVEYOR) == positions::SurveyorReturned && game.gold == gold + 15);
    CHECK(game.party.members[0].experience + (game.party.members[0].level - before.level) * 10 ==
          before.experience + 8);
    CHECK(!game.get_flag(enemy_defeated_flag_at(1)) && !game.get_flag(FLAG_CAVE_CHEST));
    reload();
    CHECK(!npc_find(NPC_SURVEYOR));
    touch(positions::SurveyorReturned, WEST);
    CHECK(npc_find(NPC_SURVEYOR) && npc_find(NPC_SURVEYOR)->assigned_site == positions::SurveyorReturned);
    talk(NPC_SURVEYOR, 1);
    CHECK(game.gold == gold + 15 && game.get_flag(FLAG_CAVE_BRIDGE));
    talk(NPC_NELL);
    CHECK(speaker == NPC_NELL && words && game.gold == gold + 15);
}

struct FiveWide {
    uint16_t operator()(char const*, uint8_t from, uint8_t to) const {
        return (uint16_t)((to - from) * 5);
    }
};

static void journal_text() {
    for(uint8_t id = 1; id < QUEST_ID_LIMIT; ++id) {
        auto summary = quest_summary((QUEST_ID)id);
        uint8_t starts[JOURNAL_SUMMARY_LINES + 1];
        uint8_t count = journal_wrap(summary, FiveWide{}, starts, JOURNAL_SUMMARY_LINES);
        CHECK(count && starts[count] == strlen(summary));
    }
}

static void journal_completed() {
    CHECK(QUEST_ID_LIMIT == 3 && quest_journal_count() == 2 && quest_selected() == QUEST_NONE);
    CHECK(quest_journal_entry(0) == QUEST_LANTERN_KEEPER &&
          quest_journal_entry(1) == QUEST_MISSING_SURVEYOR && quest_journal_entry(2) == QUEST_NONE);
    journal_text();
    TilePos out;
    CHECK(!quest_map_marker(AREA_OUTDOOR, out));

    // The World Remembers can revive the mine wolf while preserving all authored progress.
    game.set_flag(enemy_defeated_flag_at(1));
    game.remember_timer = 1;
    CHECK(remember_tick());
    remember_reset();
    CHECK(!game.get_flag(enemy_defeated_flag_at(1)) && game.get_flag(FLAG_CAVE_BRIDGE));
    CHECK(quest_status(QUEST_LANTERN_KEEPER) == QUEST_COMPLETED &&
          quest_status(QUEST_MISSING_SURVEYOR) == QUEST_COMPLETED);
}

static void mine_without_combat() {
    game_new();
    story_stub_reset(BATTLE_DEFEAT);
    stand({AREA_OUTDOOR, positions::CaveMouth.x, (uint8_t)(positions::CaveMouth.y + 1)}, NORTH);
    auto walk = [](uint8_t direction, unsigned pixels) {
        for(unsigned i = 0; i < pixels; ++i) {
            game_update(direction);
            if(!encounter_run()) {
                fprintf(stderr, "Caught at %u, %u, direction %u\n", game.player.x, game.player.y, direction);
                CHECK(false);
            }
        }
    };

    for(unsigned frame = 0; frame < 2 * TILE && game.area != AREA_CAVE; ++frame)
        walk(BTN_UP, 1);
    CHECK(game.area == AREA_CAVE);
    walk(BTN_UP, game.player.y - positions::CaveSpawn.y * TILE);
    CHECK(game.player.y == positions::CaveSpawn.y * TILE);

    // Hug the east edge of the two-lane rail approach so the pursuing wolf stays
    // in the other lane. Its defeat flag must stay clear across the whole route.
    constexpr unsigned RAIL_MARGIN = TILE + FOOT_LEFT;
    constexpr unsigned DRAW_WOLF = 26;
    walk(BTN_UP, DRAW_WOLF);
    walk(0, 4);
    walk(BTN_RIGHT, RAIL_MARGIN);
    walk(BTN_UP, (positions::CaveSpawn.y - positions::CaveLever.y) * TILE - DRAW_WOLF);
    walk(BTN_RIGHT, (positions::CaveLever.x - 1 - positions::CaveSpawn.x) * TILE - RAIL_MARGIN);
    CHECK(game.player.x == (positions::CaveLever.x - 1) * TILE &&
          game.player.y == positions::CaveLever.y * TILE);
    game_update(0, BTN_A);
    CHECK(game.get_flag(FLAG_CAVE_GATE));

    walk(BTN_DOWN, TILE);
    walk(BTN_RIGHT, (positions::CaveGate.x - positions::CaveLever.x + 1) * TILE);
    walk(BTN_UP, (positions::CaveLever.y + 1 - positions::MineLanternRelay.y) * TILE);
    walk(BTN_LEFT, (positions::CaveGate.x - positions::MineLanternRelay.x - 1) * TILE);
    game_update(0, BTN_A);
    CHECK(game.get_flag(FLAG_LANTERN_RELAY_FOUND));
    CHECK(!game.get_flag(FLAG_CAVE_BRIDGE) && !game.get_flag(enemy_defeated_flag_at(1)));
    auto move_to = [&](uint8_t direction, uint16_t current, uint8_t tile) {
        uint16_t goal = tile * TILE;
        bool decreasing = direction == BTN_UP || direction == BTN_LEFT;
        CHECK(decreasing ? current >= goal : current <= goal);
        walk(direction, decreasing ? current - goal : goal - current);
        CHECK((direction == BTN_UP || direction == BTN_DOWN ? game.player.y : game.player.x) == goal);
    };
    auto east = [&](uint8_t tile) { move_to(BTN_RIGHT, game.player.x, tile); };
    auto west = [&](uint8_t tile) { move_to(BTN_LEFT, game.player.x, tile); };
    auto north = [&](uint8_t tile) { move_to(BTN_UP, game.player.y, tile); };
    auto south = [&](uint8_t tile) { move_to(BTN_DOWN, game.player.y, tile); };
    east(positions::CaveVaultLever.x);
    walk(BTN_DOWN, 1);
    game_update(0, BTN_A);
    CHECK(game.get_flag(FLAG_CAVE_NOOK));
    north(positions::MineLanternRelay.y);
    west(positions::CaveGate.x);
    south(positions::CavePlate.y);
    west(positions::CaveLever.x - 1);
    north(positions::CaveLever.y);
    west(positions::CaveLever.x - 3);
    south(positions::CavePlate.y);
    west(positions::CavePlate.x);
    CHECK(game.get_flag(FLAG_CAVE_BRIDGE));
    east(positions::CaveVent.x);
    // The grate can close while the player is approaching. Wait through at most
    // one vent cycle, then cross the bridge from the south.
    uint16_t landing = (positions::CaveBridge_0.y - 1) * TILE;
    for(unsigned i = 0; i < 256 + 10 * TILE && game.player.y > landing; ++i)
        walk(BTN_UP, 1);
    CHECK(game.player.y == landing);
    east(positions::SurveyorTrapped.x);
    game_update(BTN_UP);
    CHECK(encounter_run());
    CHECK(npc_find(NPC_SURVEYOR));
    game_update(0, BTN_A);
    CHECK(game.get_flag(FLAG_SURVEYOR_RESCUED) && !game.get_flag(FLAG_LANTERN_STARTED));
    CHECK(!game.get_flag(enemy_defeated_flag_at(1)) && story_stub_battles() == 0);
}

static void invalid_saves() {
    for(FLAG invalid : {FLAG_LANTERN_MODERN, FLAG_LANTERN_RESTORED, FLAG_SURVEYOR_FOUND,
                        FLAG_SURVEYOR_RESCUED, FLAG_SURVEY_NOTES_READ, FLAG_BRAM_LANTERN_MEETING}) {
        game_new();
        game.set_flag(invalid);
        CHECK(!save_valid());
    }
    game_new();
    game.set_flag(FLAG_SURVEY_STARTED);
    game.set_flag(FLAG_SURVEYOR_FOUND);
    game.set_flag(FLAG_SURVEYOR_RESCUED);
    CHECK(!save_valid());
    game.set_flag(FLAG_CAVE_BRIDGE);
    CHECK(save_valid());

    game_new();
    game.set_flag(FLAG_LANTERN_STARTED);
    game.set_flag(FLAG_LANTERN_DIAGNOSED);
    game.set_flag(FLAG_LANTERN_RECORD_FOUND);
    game.set_flag(FLAG_LANTERN_RESTORED);
    CHECK(!save_valid());
    game.set_flag(FLAG_LANTERN_MODERN);
    CHECK(save_valid());
    --game.save_version;
    CHECK(!save_valid());
}

static void quest_arrow_placement() {
    // With the player centered, the ray passes from the right edge through the top-right corner.
    auto right = quest_arrow_at_edge(56, 24, 570, -254, 2);
    auto corner = quest_arrow_at_edge(56, 24, 570, -260, 2);
    auto top = quest_arrow_at_edge(56, 24, 570, -265, 2);
    CHECK(right.x == 117 && right.y > corner.y);
    CHECK(corner.x == 117 && corner.y == 2);
    CHECK(top.y == 2 && top.x == corner.x - 1);

    // Switching from east to northeast changes the art while the ray still moves along the top.
    auto east = quest_arrow_at_edge(56, 24, 1000, -499, 2);
    auto northeast = quest_arrow_at_edge(56, 24, 1000, -500, 2);
    CHECK(east.frame == 4 && northeast.frame == 2);
    CHECK(east.y == 2 && northeast.y == 2 && east.x == northeast.x);

    // Camera clamping can bring the player to the inset boundary. The marker still fits.
    auto left = quest_arrow_at_edge(0, 40, -300, 0, 5);
    CHECK(left.x == 5 && left.y == 44 && left.frame == 3);
    auto south = quest_arrow_at_edge(92, 0, 0, 300, 5);
    CHECK(south.x == 96 && south.y == 50 && south.frame == 6);
    auto parallel = quest_arrow_at_edge(56, 0, 4000, 0, 5);
    auto slightly_up = quest_arrow_at_edge(56, 0, 4000, -1, 5);
    CHECK(parallel.x == slightly_up.x && parallel.y == slightly_up.y);

    // The word divide's scaled estimate rounds one pixel too far for this long bearing.
    auto far = quest_arrow_at_edge(56, 24, 2447, -3860, 2);
    CHECK(far.x == 76 && far.y == 2);
}

int main() {
    quest_arrow_placement();
    game_new();
    CHECK(!quest_start(QUEST_NONE) && !quest_select((QUEST_ID)200));
    CHECK(quest_journal_count() == 0);
    lantern(true, true);
    CHECK(quest_status(QUEST_MISSING_SURVEYOR) == QUEST_NOT_STARTED);
    survey(NOTES);
    journal_completed();
    touch(positions::CaveChest);
    CHECK(game.gold == 52);
    reload();
    touch(positions::CaveChest);
    CHECK(game.gold == 52);

    game_new();
    survey(FREE_EXPLORATION);
    CHECK(quest_status(QUEST_LANTERN_KEEPER) == QUEST_NOT_STARTED);
    lantern(false, true);
    journal_completed();

    game_new();
    lantern(false, false);
    CHECK(quest_status(QUEST_MISSING_SURVEYOR) == QUEST_NOT_STARTED);
    survey(NELL);
    journal_completed();

    game_new();
    talk(NPC_ROWAN);
    CHECK(game.party.count == 2);
    game.gold = UINT16_MAX - 1;
    quest_start(QUEST_LANTERN_KEEPER);
    game.set_flag(FLAG_LANTERN_DIAGNOSED);
    game.set_flag(FLAG_LANTERN_RECORD_FOUND);
    story_stub_set_choice(0);
    touch(positions::VillageLantern, EAST);
    CHECK(game.gold == UINT16_MAX && game.party.members[0].experience == 6 &&
          game.party.members[1].experience == 6);
    QuestNotice notice;
    CHECK(quest_take_notice(notice) && notice.started == QUEST_LANTERN_KEEPER &&
          !strcmp(notice.objective, "Ask Bram"));
    CHECK(quest_take_notice(notice) && notice.started == QUEST_NONE && notice.experience == 6 &&
          notice.gold == 1);
    CHECK(!quest_take_notice(notice));
    bram_meeting();
    invalid_saves();
    mine_without_combat();
    printf("Millcross quest checks passed (%u)\n", checks);
}
