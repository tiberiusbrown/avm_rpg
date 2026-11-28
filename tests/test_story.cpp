#include "rpg.h"
#include "chunk.h"
#include "encounter.h"
#include "logic/enemy_catalog.h"
#include "logic/handlers.h"
#include "logic/npc_logic.h"
#include "logic/story.h"
#include "map_positions.h"
#include "music_catalog.h"
#include "npc.h"
#include "pause_menu.h"
#include "quest.h"
#include "sfx.h"
#include "story_stub.h"
#include "world_assets.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Where the Light Ends is held back for a later scene.
static_assert(area_music(AREA_OUTDOOR) != MUSIC_CREEPY &&
              area_music(AREA_INTERIORS) != MUSIC_CREEPY && MUSIC_DEFAULT != MUSIC_CREEPY);

static unsigned checks;
#define CHECK(value)                                                                               \
    do {                                                                                           \
        ++checks;                                                                                  \
        if(!(value)) {                                                                             \
            fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #value);                        \
            exit(1);                                                                               \
        }                                                                                          \
    } while(0)

// The spoken lines of the story so far, with their speakers.
struct Line {
    NPC_ID speaker;
    char const* words;
};

static Line lines[16];
static unsigned line_count;
static bool answer_yes;

static bool record(NPC_ID speaker, char const* words) {
    CHECK(line_count < sizeof(lines) / sizeof(*lines));
    lines[line_count++] = {speaker, words};
    return true;
}

bool dialogue_show(char const PROGMEM* words) {
    return record(NPC_NONE, words);
}

bool dialogue_show(NPC_ID id, char const PROGMEM* words) {
    return record(id, words);
}

bool dialogue_ask(char const PROGMEM* question) {
    record(NPC_NONE, question);
    return answer_yes;
}

bool dialogue_ask(NPC_ID id, char const PROGMEM* question) {
    record(id, question);
    return answer_yes;
}

static bool said(unsigned index, NPC_ID speaker, char const* start) {
    return index < line_count && lines[index].speaker == speaker &&
           !strncmp(lines[index].words, start, strlen(start));
}

// Quest panels, as the exploration screen takes them after each frame.
static unsigned notices;
static QUEST_ID last_started, last_completed;
static uint8_t last_experience, last_gold;

static void frame() {
    game_update(0);
    QuestNotice notice;
    while(quest_take_notice(notice)) {
        ++notices;
        if(notice.started) {
            last_started = notice.quest;
        } else {
            last_completed = notice.quest;
            last_experience = notice.experience;
            last_gold = notice.gold;
        }
    }
    quest_take_level_ups();
}

static void quiet_frames(unsigned count) {
    line_count = 0;
    while(count--)
        frame();
}

static void stand(TilePos cell, DIRECTION facing = SOUTH) {
    world_enter(cell.area, (uint16_t)(cell.x * TILE), (uint16_t)(cell.y * TILE), facing);
}

static void talk(NPC_ID id) {
    line_count = 0;
    npc_talk(id);
    frame();
}

static TilePos tile(uint8_t area, uint8_t x, uint8_t y) {
    return {area, x, y};
}

// Loads the live game as a save would be loaded, and prepares its scene. Every slot comes back as
// it was saved; only the terrain under it is due for a new check.
static void reload() {
    NpcPool saved = game.npcs;
    CHECK(save_restore());
    for(unsigned i = 0; i < NPC_CAPACITY; ++i) {
        Npc expected = saved.actors[i];
        expected.policy_pending &= (uint8_t)~NPC_FLAG_CLEAR;
        CHECK(!memcmp(&game.npcs.actors[i], &expected, sizeof(Npc)));
    }
    update_visible_world(0, false);
}

static void way_home() {
    story_stub_reset(BATTLE_VICTORY);
    game_new();
    line_count = 0;
    // The story opens beside the nap spot with only the Alden line, and the story menus locked.
    CHECK(game.area == AREA_OUTDOOR && game.player.facing == SOUTH);
    CHECK(game.player_is_in_region(positions::NapAreaRegion));
    CHECK(story_locked_menu_items() ==
          (1u << PAUSE_PARTY | 1u << PAUSE_SUPPLIES | 1u << PAUSE_JOURNAL));
    frame();
    CHECK(line_count == 1 && said(0, NPC_ALDEN, "Wow, I sure slept"));
    quiet_frames(5);
    CHECK(!line_count && quest_status(QUEST_GO_HOME) == QUEST_NOT_STARTED);

    // Wandering away starts Go Home, and its panel comes before the Journal hint.
    stand(tile(AREA_OUTDOOR, 12, 15));
    line_count = 0;
    frame();
    CHECK(line_count == 1 && said(0, NPC_ALDEN, "Now which way was home"));
    CHECK(quest_status(QUEST_GO_HOME) == QUEST_ACTIVE && quest_selected() == QUEST_GO_HOME);
    CHECK(!game.get_flag(FLAG_MENU_JOURNAL) && notices == 1);
    CHECK(last_started == QUEST_GO_HOME);
    line_count = 0;
    frame();
    CHECK(line_count == 1 && said(0, NPC_NONE, "Press B to pause"));
    CHECK(game.get_flag(FLAG_MENU_JOURNAL));
    CHECK(story_locked_menu_items() == (1u << PAUSE_PARTY | 1u << PAUSE_SUPPLIES));
    CHECK(!strcmp(quest_objective(QUEST_GO_HOME), "Leave the clearing"));

    // Nothing is said while the boulders are still out of view.
    stand(tile(AREA_OUTDOOR, positions::GateMouth.x - 5, positions::GateMouth.y));
    quiet_frames(3);
    CHECK(!line_count && !game.get_flag(FLAG_BOULDERS_NOTICED));

    // The blocked way out is noticed once.
    stand(positions::GateMouth);
    line_count = 0;
    frame();
    CHECK(line_count == 2 && said(0, NPC_ALDEN, "Hey! Those boulders"));
    CHECK(said(1, NPC_NONE, "Press A while facing"));
    CHECK(!strcmp(quest_objective(QUEST_GO_HOME), "Push the boulders aside"));
    quiet_frames(5);
    CHECK(!line_count);

    stand(tile(AREA_OUTDOOR, 30, 14));
    frame();
    CHECK(game.get_flag(FLAG_LEFT_CLEARING));
    CHECK(!strcmp(quest_objective(QUEST_GO_HOME), "Go to your home"));
    CHECK(save_valid());

    // Stepping inside completes the quest with a little experience, and Eliza is waiting.
    uint8_t experience = game.party.members[0].experience;
    stand(positions::AldenExit);
    game.player.y = (uint16_t)(game.player.y - TILE);
    line_count = 0;
    frame();
    CHECK(quest_status(QUEST_GO_HOME) == QUEST_COMPLETED);
    CHECK(last_completed == QUEST_GO_HOME);
    CHECK(last_experience == 3 && game.party.members[0].experience == experience + 3);
    frame();
    CHECK(line_count == 1 && said(0, NPC_ELIZA, "Alden! I've been looking"));
    CHECK(npc_find(NPC_ELIZA) && npc_location(NPC_ELIZA) == positions::ElizaAtAlden);
    CHECK(save_valid());
}

static void giant_rat() {
    // Refusing leaves the quest unstarted; she asks again.
    answer_yes = false;
    talk(NPC_ELIZA);
    CHECK(line_count == 2 && said(0, NPC_ELIZA, "Alden... you have to help me!"));
    CHECK(said(1, NPC_ELIZA, "You won't help me?"));
    CHECK(quest_status(QUEST_GIANT_RAT) == QUEST_NOT_STARTED && !npc_find(NPC_GIANT_RAT));
    answer_yes = true;
    talk(NPC_ELIZA);
    CHECK(line_count == 2 && said(1, NPC_ELIZA, "Thank you so much!"));
    CHECK(quest_status(QUEST_GIANT_RAT) == QUEST_ACTIVE && quest_selected() == QUEST_GIANT_RAT);
    CHECK(last_started == QUEST_GIANT_RAT);
    CHECK(!strcmp(quest_objective(QUEST_GIANT_RAT), "Go to Eliza's home"));
    talk(NPC_ELIZA);
    CHECK(line_count == 1 && said(0, NPC_ELIZA, "I'll just wait here"));

    // The rat holds still in the bedroom until Alden comes close and has chosen how to fight.
    stand(tile(AREA_INTERIORS, 26, 5), EAST);
    CHECK(npc_find(NPC_GIANT_RAT));
    quiet_frames(100);
    CHECK(!line_count && !story_class_choice_due());
    Npc* rat = npc_find(NPC_GIANT_RAT);
    CHECK(rat->actor.x == positions::RatNest.x * TILE &&
          rat->actor.y == positions::RatNest.y * TILE);
    stand(tile(AREA_INTERIORS, 29, 5), EAST);
    line_count = 0;
    frame();
    CHECK(line_count == 1 && said(0, NPC_ALDEN, "There's the giant rat"));
    CHECK(story_class_choice_due() && story_stub_battles() == 0);

    story_choose_classes(MOVEMENT_LEAP, ATTACK_SHOOT);
    CHECK(!story_class_choice_due());
    CHECK(game.party.members[0].movement_class == MOVEMENT_LEAP &&
          game.party.members[0].attack_class == ATTACK_SHOOT);
    line_count = 0;
    frame();
    CHECK(line_count == 1 && said(0, NPC_NONE, "Press B to access the Party menu"));
    CHECK(story_locked_menu_items() == 1u << PAUSE_SUPPLIES);

    // A save taken during the chase resumes it from where the rat stood.
    for(unsigned i = 0; i < 100 && npc_find(NPC_GIANT_RAT)->behavior != NPC_CHASE; ++i)
        frame();
    CHECK(npc_find(NPC_GIANT_RAT)->behavior == NPC_CHASE);
    Game chase = game;
    game_new();
    game = chase;
    reload();
    CHECK(npc_find(NPC_GIANT_RAT) && npc_find(NPC_GIANT_RAT)->behavior == NPC_CHASE);

    // The rat now chases, and its battle ends the hunt.
    for(unsigned i = 0; i < 200 && !story_stub_battles(); ++i) {
        frame();
        CHECK(encounter_run());
    }
    CHECK(story_stub_battles() == 1 && game.get_flag(FLAG_RAT_DEFEATED));
    CHECK(!npc_find(NPC_GIANT_RAT));
    CHECK(!strcmp(quest_objective(QUEST_GIANT_RAT), "Go back to Eliza"));
    CHECK(save_valid());

    uint16_t gold = game.gold;
    stand({positions::ElizaAtAlden.area, positions::ElizaAtAlden.x,
           (uint8_t)(positions::ElizaAtAlden.y + 1)},
          NORTH);
    talk(NPC_ELIZA);
    CHECK(line_count == 2 && said(0, NPC_ALDEN, "The rat is gone") &&
          said(1, NPC_ELIZA, "Oh, thank goodness"));
    CHECK(quest_status(QUEST_GIANT_RAT) == QUEST_COMPLETED);
    CHECK(last_completed == QUEST_GIANT_RAT);
    CHECK(last_experience == 5 && last_gold == 10 && game.gold == gold + 10);
    CHECK(npc_location(NPC_ELIZA) == positions::ElizaGarden);
    talk(NPC_ELIZA);
    CHECK(!line_count);
    CHECK(save_valid() && save_restore());
}

// The table-driven edge of an object matches its mask pixel by pixel, for every line of the
// square and every way an edge can overhang it.
static void object_edges() {
    for(uint32_t step = 0; step <= 0xFFFF / 7; ++step) {
        uint16_t mask = step == 0xFFFF / 7 ? 0xFFFF : (uint16_t)(step * 7);
        for(uint8_t across = 0; across < TILE; ++across)
            for(int8_t along = -7; along < TILE; ++along)
                for(bool column : {false, true}) {
                    uint8_t expected = 0;
                    for(uint8_t i = 0; i < 8; ++i) {
                        int at = along + i;
                        if(at < 0 || at >= TILE)
                            continue;
                        uint8_t x = column ? across : (uint8_t)at,
                                y = column ? (uint8_t)at : across;
                        if(collision_pixel_solid(mask, x, y))
                            expected |= (uint8_t)(1u << i);
                    }
                    if(object_edge_bits(mask, across, along, column) != expected)
                        CHECK(!"object edge differs from its pixels");
                }
    }
    ++checks;
}

static constexpr NPC_ID BOULDERS[] = {NPC_BOULDER_A, NPC_BOULDER_B, NPC_BOULDER_C, NPC_BOULDER_D};
static constexpr TilePos HOMES[] = {positions::BoulderA, positions::BoulderB, positions::BoulderC,
                                    positions::BoulderD};

static bool at_tile(Npc const* n, uint8_t x, uint8_t y) {
    return n && n->actor.x == x * TILE && n->actor.y == y * TILE;
}

static unsigned boulders_present() {
    unsigned count = 0;
    for(NPC_ID id : BOULDERS)
        count += npc_find(id) != nullptr;
    return count;
}

// Moves the player without a scene change, so casting and retirement follow the camera.
static void shift_player(uint8_t tx) {
    game.player.x = (uint16_t)(tx * TILE);
    update_visible_world(0, false);
}

static void boulders() {
    game_new();
    uint8_t const row = positions::GateMouth.y;
    stand(tile(AREA_OUTDOOR, 22, row), EAST);
    CHECK(boulders_present() == 4);
    for(unsigned i = 0; i < 4; ++i)
        CHECK(at_tile(npc_find(BOULDERS[i]), HOMES[i].x, HOMES[i].y));

    // A boulder rolls one tile, holding both tiles while it slides. A blocked push is refused.
    sfx_stub_reset();
    game.player.facing = EAST;
    story_push_boulder(NPC_BOULDER_B);
    CHECK(sfx_stub_count() == 1 && sfx_stub_record(0).effect == SFX_ERROR);
    CHECK(npc_push(NPC_BOULDER_B, WEST, positions::BoulderGateRegion));
    CHECK(!npc_object_clear(25 * TILE, row * TILE) && !npc_object_clear(24 * TILE, row * TILE));
    CHECK(!npc_push(NPC_BOULDER_B, WEST, positions::BoulderGateRegion));
    for(unsigned i = 0; i < TILE + 2; ++i)
        frame();
    CHECK(at_tile(npc_find(NPC_BOULDER_B), 24, row));
    CHECK(npc_find(NPC_BOULDER_B)->behavior == NPC_STAND);

    // The chunk of its site holds a boulder, whatever bounds a push allows.
    CHECK(positions::GateMouth.x / CHUNK_WIDTH != HOMES[1].x / CHUNK_WIDTH);
    constexpr TileRegion WIDE{{AREA_OUTDOOR, 16, 8}, 16, 12};
    CHECK(!npc_push(NPC_BOULDER_B, WEST, WIDE));

    // While the cache reaches their chunk, every boulder keeps its slot and place, in view or not.
    shift_player(19);
    CHECK(cache.origin_tx + CACHE_WIDTH - 1 == 24);
    CHECK(boulders_present() == 4 && at_tile(npc_find(NPC_BOULDER_B), 24, row));
    CHECK(at_tile(npc_find(NPC_BOULDER_A), HOMES[0].x, HOMES[0].y));

    // Once it leaves the chunk, they retire together and take no slots.
    shift_player(17);
    CHECK(!boulders_present() && !game.npcs.actors[0].id);

    // Walking back casts each at its site, never in view: the opening arrangement again.
    bool present[4] = {};
    for(uint16_t x = 17 * TILE; x <= 22 * TILE; ++x) {
        game.player.x = x;
        update_visible_world(0, false);
        for(unsigned i = 0; i < 4; ++i)
            if(Npc* n = npc_find(BOULDERS[i]); n && !present[i]) {
                present[i] = true;
                CHECK(!npc_visible(n->actor.x, n->actor.y));
            }
    }
    CHECK(boulders_present() == 4);
    for(unsigned i = 0; i < 4; ++i)
        CHECK(at_tile(npc_find(BOULDERS[i]), HOMES[i].x, HOMES[i].y));

    // A save holds the pool. A load finds each boulder where it was, a sliding one still on its
    // way, and leaves the player standing where another began.
    stand(tile(AREA_OUTDOOR, 22, row), EAST);
    CHECK(npc_push(NPC_BOULDER_B, WEST, positions::BoulderGateRegion));
    for(unsigned i = 0; i < TILE + 2; ++i)
        frame();
    shift_player(HOMES[1].x);
    CHECK(npc_push(NPC_BOULDER_D, WEST, positions::BoulderGateRegion));
    for(unsigned i = 0; i < TILE / 2; ++i)
        frame();
    Game saved = game;
    CHECK(npc_find(NPC_BOULDER_D)->behavior == NPC_TARGET);
    game_new();
    game = saved;
    reload();
    CHECK(game.player.x == HOMES[1].x * TILE && game.player.y == row * TILE);
    CHECK(boulders_present() == 4 && at_tile(npc_find(NPC_BOULDER_B), 24, row));
    CHECK(npc_find(NPC_BOULDER_D)->actor.x == HOMES[3].x * TILE - TILE / 2);
    for(unsigned i = 0; i < TILE; ++i)
        frame();
    CHECK(at_tile(npc_find(NPC_BOULDER_D), HOMES[3].x - 1, HOMES[3].y));
    CHECK(npc_find(NPC_BOULDER_D)->behavior == NPC_STAND);
    CHECK(at_tile(npc_find(NPC_BOULDER_A), HOMES[0].x, HOMES[0].y));
    CHECK(at_tile(npc_find(NPC_BOULDER_C), HOMES[2].x, HOMES[2].y));
}

// A game past the opening lines, so that only the village answers.
static void settled_game() {
    game_new();
    game.set_flag(FLAG_INTRO_SHOWN);
    game.set_flag(FLAG_HOME_STARTED);
    game.set_flag(FLAG_MENU_JOURNAL);
}

// Activating a door enters its house, and walking onto the exit returns to the door.
static void houses() {
    settled_game();
    for(auto const& link : handler_impl::HOUSE_LINKS) {
        stand({link.door.area, link.door.x, (uint8_t)(link.door.y + 1)}, NORTH);
        frame();
        CHECK(game.area == AREA_OUTDOOR && game.can_interact);
        sfx_stub_reset();
        game_update(0, BTN_A);
        CHECK(game.area == AREA_INTERIORS);
        CHECK(sfx_stub_count() && sfx_stub_record(0).effect == SFX_DOOR_OPEN);
        CHECK(game.player.x == link.exit.x * TILE && game.player.y == (link.exit.y - 1) * TILE);
        CHECK(game.player_is_in_region(link.rooms) &&
              player_terrain_clear(game.player.x, game.player.y));
        for(unsigned i = 0; i < 2 * TILE && game.area == AREA_INTERIORS; ++i)
            game_update(BTN_DOWN);
        CHECK(game.area == AREA_OUTDOOR && game.player.x == link.door.x * TILE &&
              game.player.y == (link.door.y + 1) * TILE && game.player.facing == SOUTH);
    }
}

// Signs name their buildings, and the other doors only answer.
static void signs() {
    constexpr struct {
        TilePos cell;
        char const* words;
    } MESSAGES[] = {
        {positions::AldenSign, "Alden's home."},
        {positions::ElizaSign, "Eliza's home."},
        {positions::BramSign, "Bram's home."},
        {positions::ShopSign, "Shop."},
        {positions::InnSign, "Inn."},
        {positions::NorthHouseSign, "A house."},
        {positions::SouthHouseSign, "A house."},
        {positions::BarnSign, "A storehouse."},
        {positions::BramDoor, "The door is locked."},
        {positions::NorthHouseDoor, "No one answers."},
        {positions::SouthHouseDoor, "No one answers."},
        {positions::BarnDoor, "No one answers."},
    };

    settled_game();
    for(auto const& message : MESSAGES) {
        stand({message.cell.area, message.cell.x, (uint8_t)(message.cell.y + 1)}, NORTH);
        quiet_frames(1);
        CHECK(game.can_interact);
        line_count = 0;
        game_update(0, BTN_A);
        CHECK(line_count == 1 && said(0, NPC_NONE, message.words));
        CHECK(game.area == AREA_OUTDOOR);
    }
}

int main() {
    object_edges();
    way_home();
    giant_rat();
    boulders();
    houses();
    signs();
    printf("PASS story: %u checks\n", checks);
}
