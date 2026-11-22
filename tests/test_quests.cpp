#include "rpg.h"
#include "chunk.h"
#include "encounter.h"
#include "journal.h"
#include "logic/enemy_catalog.h"
#include "logic/npc_logic.h"
#include "map_positions.h"
#include "quest.h"
#include "story_stub.h"
#include "tile_ids.h"

#include <initializer_list>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(value)                                                                               \
    do {                                                                                           \
        ++checks;                                                                                  \
        if(!(value)) {                                                                             \
            fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #value);                          \
            exit(1);                                                                               \
        }                                                                                          \
    } while(0)

static bool accept;
static char const* words;

bool dialogue_show(char const PROGMEM* text) {
    words = text;
    return accept;
}

bool dialogue_show(NPC_ID, char const PROGMEM* text) {
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

static void social(bool public_explanation) {
    talk(NPC_TESSA, 0, false);
    CHECK(quest_status(QUEST_DRY_ROOTS) == QUEST_NOT_STARTED);
    talk(NPC_TESSA);
    CHECK(quest_status(QUEST_DRY_ROOTS) == QUEST_ACTIVE);
    TilePos target;
    CHECK(quest_marker(QUEST_DRY_ROOTS, AREA_OUTDOOR, target) && target == positions::BramYard);

    talk(NPC_BRAM, 255);
    talk(NPC_ORRIN, 0, false);
    CHECK(!game.get_flag(FLAG_BRAM_ACCOUNT) && !game.get_flag(FLAG_ORRIN_ACCOUNT));
    talk(NPC_ORRIN);
    talk(NPC_TESSA);
    CHECK(quest_status(QUEST_DRY_ROOTS) == QUEST_ACTIVE);
    reload();
    talk(NPC_BRAM);
    CHECK(quest_marker(QUEST_DRY_ROOTS, AREA_OUTDOOR, target) && target == positions::TessaWell);
    talk(NPC_TESSA, 255);
    talk(NPC_TESSA, 0, false);
    CHECK(quest_status(QUEST_DRY_ROOTS) == QUEST_ACTIVE && !game.get_flag(FLAG_ORCHARD_PUBLIC));

    talk(NPC_TESSA, public_explanation ? 0 : 1);
    CHECK(quest_status(QUEST_DRY_ROOTS) == QUEST_COMPLETED && !quest_objective(QUEST_DRY_ROOTS));
    CHECK(game.get_flag(FLAG_ORCHARD_PUBLIC) == public_explanation);
    CHECK(npc_location(NPC_TESSA) == positions::TessaOrchard);
    auto xp = game.party.members[0].experience;
    reload();
    talk(NPC_BRAM);
    CHECK(strstr(words, public_explanation ? "square" : "heard me out"));
    talk(NPC_TESSA);
    CHECK(game.party.members[0].experience == xp);
}

static void bridge() {
    accept = false;
    touch(positions::CaveNotice);
    CHECK(quest_status(QUEST_SPAN_BELOW) == QUEST_NOT_STARTED);
    accept = true;
    touch(positions::CaveNotice);
    CHECK(quest_status(QUEST_SPAN_BELOW) == QUEST_ACTIVE);
    touch(positions::CaveLever);
    CHECK(game.get_flag(FLAG_CAVE_GATE));
    touch(positions::CaveLever);
    CHECK(!game.get_flag(FLAG_CAVE_GATE));
    touch(positions::CaveLever);
    reload();
    touch(positions::CaveVaultLever);
    CHECK(game.get_flag(FLAG_CAVE_NOOK));
    stand(positions::CavePlate, NORTH);
    game_update(0, 0);
    CHECK(quest_status(QUEST_SPAN_BELOW) == QUEST_COMPLETED && !quest_objective(QUEST_SPAN_BELOW));
    stand({AREA_CAVE, positions::CaveBridge_1.x, (uint8_t)(positions::CaveBridge_1.y + 1)}, NORTH);
    CHECK(cached_tile(positions::CaveBridge_1) == CAVE_TILE_BRIDGE_VERTICAL);
    reload();
    touch(positions::CaveChest);
    CHECK(equipment_owned(ITEM_TRAVELERS_BOOTS) && game.gold == 25);
    reload();
    touch(positions::CaveChest);
    CHECK(game.gold == 25 && strstr(words, "empty"));
}

static void tracks(bool read_warning) {
    touch(positions::MineLedger);
    CHECK(!game.get_flag(FLAG_MINE_LEDGER));
    if(read_warning) {
        touch(positions::MineWarning);
        CHECK(quest_status(QUEST_TRACKS_IN_DUST) == QUEST_ACTIVE);
    }
    stand({AREA_CAVE, (uint8_t)(positions::MineScout.x - 1), positions::MineScout.y}, EAST);
    auto const wolf = (NPC_ID)(NPC_ENEMY_FIRST + 1);
    CHECK(npc_find(wolf));

    story_stub_reset(BATTLE_DEFEAT);
    encounter_contact(wolf);
    CHECK(!encounter_run() && !game.get_flag(FLAG_MINE_WOLF_DRIVEN));
    story_stub_set_result(BATTLE_VICTORY);
    encounter_contact(wolf);
    CHECK(encounter_run() && game.get_flag(FLAG_MINE_WOLF_DRIVEN));
    CHECK(story_stub_battles() == 2 && quest_status(QUEST_TRACKS_IN_DUST) == QUEST_ACTIVE);
    reload();
    accept = false;
    touch(positions::MineLedger);
    CHECK(!game.get_flag(FLAG_MINE_LEDGER));
    accept = true;
    touch(positions::MineLedger);
    CHECK(quest_status(QUEST_TRACKS_IN_DUST) == QUEST_COMPLETED);
    auto xp = game.party.members[0].experience;
    reload();
    touch(positions::MineLedger);
    CHECK(game.party.members[0].experience == xp);
    talk(NPC_NELL);
    CHECK(strstr(words, "eastern trail"));
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

int main() {
    game_new();
    CHECK(!quest_start(QUEST_NONE) && !quest_select((QUEST_ID)200));
    CHECK(quest_journal_count() == 0);
    social(true);
    bridge();
    tracks(true);
    CHECK(quest_journal_count() == 3 && quest_selected() == QUEST_NONE);
    journal_text();

    game_new();
    tracks(false);
    bridge();
    social(false);
    CHECK(quest_journal_count() == 3 && quest_selected() == QUEST_NONE);
    journal_text();

    game_new();
    game.set_flag(FLAG_MINE_LEDGER);
    CHECK(!save_valid());
    game_new();
    game.set_flag(FLAG_ORCHARD_SETTLED);
    CHECK(!save_valid());
    game_new();
    --game.save_version;
    CHECK(!save_valid());
    printf("Millcross quest checks passed (%u)\n", checks);
}
