#include "rpg.h"
#include "chunk.h"
#include "world_assets.h"
#include "map_positions.h"
#include "npc.h"
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

static unsigned dialogues;
static char const* last_dialogue;

bool dialogue_show(char const PROGMEM* text) {
    ++dialogues;
    last_dialogue = text;
    story_stub_record_dialogue(text);
    CHECK(chunk_trace_count[STAGE_TILES] > 0);
    CHECK(cached_tile(positions::GroveSign) == TILE_SIGN);
    return true;
}

bool dialogue_ask(char const PROGMEM*) {
    return true;
}

static void animate() {
    for(uint8_t row = 0; row < ACTOR_ROW_COUNT; ++row) {
        for(DIRECTION dir = NORTH; dir <= WEST; dir = (DIRECTION)(dir + 1)) {
            Actor actor{0, 0, dir, 0};
            for(uint16_t ticks = 0; ticks < 64; ++ticks) {
                CHECK(actor_frame(actor, row, ticks) == row * 12 + dir * 3);
                actor.walking = 1;
                uint8_t expected[] = {0, 1, 0, 2};
                CHECK(actor_frame(actor, row, ticks) ==
                      row * 12 + dir * 3 + expected[(ticks / WALK_POSE_TICKS) & 3]);
                actor.walking = 0;
            }
        }
    }
}

static void movement() {
    game_new();
    uint16_t const start_x = positions::PlayerStart.x * TILE;
    uint16_t const start_y = positions::PlayerStart.y * TILE;
    CHECK(game.player.x == start_x && game.player.y == start_y && game.player.facing == NORTH);
    game_update(BTN_RIGHT);
    CHECK(game.player.x == start_x + PLAYER_PIXELS_PER_FRAME && game.player.walking);
    game_update(BTN_LEFT);
    CHECK(game.player.x == start_x && game.player.facing == WEST);
    game_update(BTN_UP | BTN_DOWN | BTN_RIGHT);
    CHECK(game.player.x == start_x + PLAYER_PIXELS_PER_FRAME);
    game_update(BTN_UP | BTN_DOWN | BTN_LEFT | BTN_RIGHT);
    CHECK(!game.player.walking);
    game_new();
    CHECK(actor_move(&game.player, NORTH, 8)); // Feet can approach the solid sign.
    CHECK(game.player.y == start_y - 8);
    CHECK(!actor_move(&game.player, NORTH, 1) && !game.player.walking);
    world_enter(AREA_OUTDOOR, start_x, start_y, WEST);
    npc_reset();
    // Walk west until the tree line stops the player, whatever the walking speed.
    for(unsigned i = 0; i < 1000; ++i)
        game_update(BTN_LEFT);
    CHECK(game.player.x < start_x && player_terrain_clear(game.player.x, game.player.y));
    CHECK(!game.dimensions.pixel_bounds(65535, 0));
    CHECK(!terrain_clear(0, 0) && !terrain_clear(65535, 65535));

    game_new();
    Game first = game;
    for(unsigned i = 0; i < 500; ++i)
        game_update(i % 40 < 20 ? BTN_RIGHT : BTN_LEFT);
    Game expected = game;
    game = first;
    for(unsigned i = 0; i < 500; ++i)
        game_update(i % 40 < 20 ? BTN_RIGHT : BTN_LEFT);
    CHECK(!memcmp(&game, &expected, sizeof(Game)));
}

// Each valid interaction shows the welcome, runs one battle, and then shows an outcome line.
static void sign() {
    game_new();
    CHECK(game.can_interact);
    story_stub_reset(BATTLE_VICTORY);
    unsigned before = dialogues;
    game_update(BTN_A); // Held A is not a fresh interaction.
    CHECK(dialogues == before && story_stub_count() == 0);
    uint16_t x = game.player.x, y = game.player.y, ticks = game.ticks, rng = game.rng;
    game_update(BTN_RIGHT, BTN_A);
    CHECK(dialogues == before + 2 && story_stub_battles() == 1);
    CHECK(strstr(story_stub_event(0).text, "Nocturne Grove"));
    CHECK(game.player.x == x && game.player.y == y && game.ticks == ticks && game.rng == rng);
    CHECK(!frame_pressed);
    game_update(BTN_A);
    CHECK(dialogues == before + 2 && story_stub_battles() == 1);
    game.player.facing = EAST;
    game_update(0, BTN_A);
    CHECK(dialogues == before + 2 && story_stub_battles() == 1);
    world_enter(AREA_OUTDOOR, (positions::GroveSign.x - 1) * TILE,
                (positions::GroveSign.y + 1) * TILE, NORTH);
    game_update(0, BTN_A);
    CHECK(dialogues == before + 2 && story_stub_battles() == 1); // Diagonals do not read the sign.
    world_enter(AREA_OUTDOOR, (positions::GroveSign.x + 1) * TILE,
                positions::GroveSign.y * TILE, WEST);
    game_update(0, BTN_A);
    CHECK(dialogues == before + 4 && story_stub_battles() == 2);
}

// The welcome, the battle, and the outcome line run in that order. The outcome picks the line,
// and every valid interaction starts a fresh battle without changing overworld state.
static void grove_battle() {
    game_new();
    story_stub_reset(BATTLE_VICTORY);
    uint8_t hp = game.hp;
    uint16_t gold = game.gold, x = game.player.x, y = game.player.y;

    // Neither a held A nor a sign that is not faced starts a battle.
    game_update(BTN_A);
    game.player.facing = EAST;
    game_update(0, BTN_A);
    CHECK(story_stub_count() == 0 && story_stub_battles() == 0);

    game.player.facing = NORTH;
    game_update(0, BTN_A);
    CHECK(story_stub_count() == 3 && story_stub_battles() == 1);
    StoryEvent const& welcome = story_stub_event(0);
    StoryEvent const& battle = story_stub_event(1);
    StoryEvent const& outcome = story_stub_event(2);
    CHECK(welcome.kind == STORY_EVENT_DIALOGUE && strstr(welcome.text, "Nocturne Grove"));
    CHECK(battle.kind == STORY_EVENT_BATTLE && battle.encounter == &BATTLE_GROVE);
    CHECK(outcome.kind == STORY_EVENT_DIALOGUE && strstr(outcome.text, "withdraw"));
    CHECK(game.player.x == x && game.player.y == y && game.area == AREA_OUTDOOR);
    CHECK(game.hp == hp && game.gold == gold);

    story_stub_set_result(BATTLE_DEFEAT);
    game_update(0, BTN_A);
    CHECK(story_stub_count() == 6 && story_stub_battles() == 2);
    CHECK(story_stub_event(3).kind == STORY_EVENT_DIALOGUE &&
          strstr(story_stub_event(3).text, "Nocturne Grove"));
    CHECK(story_stub_event(4).kind == STORY_EVENT_BATTLE &&
          story_stub_event(4).encounter == &BATTLE_GROVE);
    CHECK(story_stub_event(5).kind == STORY_EVENT_DIALOGUE &&
          strstr(story_stub_event(5).text, "drive you back"));
    CHECK(game.player.x == x && game.player.y == y && game.hp == hp);
}

static void checksum(uint8_t* record) {
    uint16_t value = save_checksum(record);
    record[SAVE_CRC_OFFSET] = (uint8_t)value;
    record[SAVE_CRC_OFFSET + 1] = (uint8_t)(value >> 8);
}

static void saves() {
    uint8_t record[SAVE_BYTES], bad[SAVE_BYTES];
    game_new();
    game.gold = 37;
    game.set_flag(FLAG_RESERVED);
    save_encode(record);
    game.gold = 0;
    CHECK(save_decode(record) && game.gold == 37 && game.get_flag(FLAG_RESERVED));
    for(unsigned i = 0; i < SAVE_BYTES; ++i) {
        memcpy(bad, record, sizeof(bad));
        bad[i] ^= 0x40;
        Game before = game;
        CHECK(!save_decode(bad));
        CHECK(!memcmp(&game, &before, sizeof(Game)));
    }
    for(uint8_t version = 1; version < SAVE_VERSION; ++version) {
        memcpy(bad, record, sizeof(bad));
        bad[2] = version;
        checksum(bad);
        CHECK(!save_decode(bad));
    }
    memcpy(bad, record, sizeof(bad));
    bad[6] = bad[7] = 255;
    checksum(bad);
    CHECK(!save_decode(bad));
    memcpy(bad, record, sizeof(bad));
    bad[6] = positions::GroveSign.x * TILE;
    bad[7] = 0;
    bad[8] = positions::GroveSign.y * TILE; // Syntactically valid, but inside the sign.
    bad[9] = 0;
    checksum(bad);
    CHECK(save_decode(bad));
    world_place(game.area, game.player.x, game.player.y, game.player.facing);
    update_visible_world(0, false);
    CHECK(player_terrain_clear(game.player.x, game.player.y));
    CHECK(game.player.x != positions::GroveSign.x * TILE ||
          game.player.y != positions::GroveSign.y * TILE);
    CHECK(!save_decode(record, 3));
}

int main() {
    animate();
    movement();
    sign();
    grove_battle();
    saves();
    printf("PASS 16x16 core: %u checks\n", checks);
}
