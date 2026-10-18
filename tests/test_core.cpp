#include "rpg.h"
#include "chunk.h"
#include "world_assets.h"
#include "map_positions.h"
#include "npc.h"
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
    CHECK(chunk_trace_count[STAGE_TILES] > 0);
    CHECK(cached_tile(positions::GroveSign) == TILE_SIGN);
    return true;
}

bool dialogue_ask(char const PROGMEM*) {
    return true;
}

static void animate() {
    for(uint8_t row = 0; row < 16; ++row) {
        for(DIRECTION dir = NORTH; dir <= WEST; dir = (DIRECTION)(dir + 1)) {
            Actor actor{0, 0, dir, 0};
            for(uint16_t ticks = 0; ticks < 64; ++ticks) {
                CHECK(actor_frame(actor, row, ticks) == row * 12 + dir * 3);
                actor.walking = 1;
                uint8_t expected[] = {0, 1, 0, 2};
                CHECK(actor_frame(actor, row, ticks) ==
                      row * 12 + dir * 3 + expected[(ticks >> 2) & 3]);
                actor.walking = 0;
            }
        }
    }
}

static void movement() {
    game_new();
    CHECK(game.player.x == 112 && game.player.y == 80 && game.player.facing == NORTH);
    game_update(BTN_RIGHT);
    CHECK(game.player.x == 114 && game.player.walking);
    game_update(BTN_LEFT);
    CHECK(game.player.x == 112 && game.player.facing == WEST);
    game_update(BTN_UP | BTN_DOWN | BTN_RIGHT);
    CHECK(game.player.x == 114);
    game_update(BTN_UP | BTN_DOWN | BTN_LEFT | BTN_RIGHT);
    CHECK(!game.player.walking);
    game_new();
    CHECK(actor_move(&game.player, NORTH, 8)); // Feet can approach the solid sign.
    CHECK(game.player.y == 72);
    CHECK(!actor_move(&game.player, NORTH, 1) && !game.player.walking);
    world_enter(AREA_OUTDOOR, 2 * TILE, 7 * TILE, EAST);
    npc_reset();
    for(unsigned i = 0; i < 200; ++i)
        game_update(BTN_RIGHT);
    CHECK(game.player.x == 260 && terrain_clear(game.player.x, game.player.y));
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

static void sign() {
    game_new();
    CHECK(game.can_interact);
    unsigned before = dialogues;
    game_update(BTN_A); // Held A is not a fresh interaction.
    CHECK(dialogues == before);
    uint16_t x = game.player.x, y = game.player.y, ticks = game.ticks, rng = game.rng;
    game_update(BTN_RIGHT, BTN_A);
    CHECK(dialogues == before + 1 && strstr(last_dialogue, "Nocturne Grove"));
    CHECK(game.player.x == x && game.player.y == y && game.ticks == ticks && game.rng == rng);
    CHECK(!frame_pressed);
    game_update(BTN_A);
    CHECK(dialogues == before + 1);
    game.player.facing = EAST;
    game_update(0, BTN_A);
    CHECK(dialogues == before + 1);
    world_enter(AREA_OUTDOOR, 6 * TILE, 5 * TILE, NORTH);
    game_update(0, BTN_A);
    CHECK(dialogues == before + 1); // Diagonals do not read the sign.
    world_enter(AREA_OUTDOOR, 8 * TILE, 4 * TILE, WEST);
    game_update(0, BTN_A);
    CHECK(dialogues == before + 2);
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
    bad[6] = 7 * TILE;
    bad[7] = 0;
    bad[8] = 4 * TILE; // Syntactically valid, but inside the sign.
    bad[9] = 0;
    checksum(bad);
    CHECK(save_decode(bad));
    CHECK(terrain_clear(game.player.x, game.player.y));
    CHECK(game.player.x != 7 * TILE || game.player.y != 4 * TILE);
    CHECK(!save_decode(record, 3));
}

int main() {
    animate();
    movement();
    sign();
    saves();
    printf("PASS 16x16 core: %u checks\n", checks);
}
