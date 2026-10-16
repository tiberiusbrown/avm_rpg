#include "rpg.h"
#include "world_assets.h"
#include "map_positions.h"
#include "chunk.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if(!(x)) {                                                                                 \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x);                           \
            exit(1);                                                                               \
        }                                                                                          \
    } while(0)

static bool accept_dialogue = true, accept_choice = true;
static unsigned shown_dialogues, asked_choices;
static char const* last_dialogue;

bool dialogue_show(char const PROGMEM* words) {
    ++shown_dialogues;
    last_dialogue = words;
    return accept_dialogue;
}

bool dialogue_ask(char const PROGMEM*) {
    ++asked_choices;
    return accept_choice;
}

static void hold(uint8_t button, unsigned frames) {
    while(frames--)
        game_update(button);
    game_update(0);
}

static void movement() {
    Game a, b;
    unsigned i;

    game_new();
    game_update(BTN_RIGHT);
    CHECK(game.player.x == 34);
    CHECK(game.player.x % 8 != 0);
    game_update(BTN_LEFT);
    CHECK(game.player.x == 32);
    CHECK(game.player.facing == WEST);
    game_update(BTN_UP | BTN_DOWN | BTN_RIGHT);
    CHECK(game.player.x == 34);
    game_update(BTN_UP | BTN_RIGHT);
    CHECK(game.player.y == 38);
    CHECK(game.player.x == 34);
    game_update(BTN_UP | BTN_DOWN | BTN_LEFT | BTN_RIGHT);
    CHECK(!game.player.walking);

    game.player.x = 8;
    game.player.y = 16;
    hold(BTN_LEFT, 500);
    CHECK(game.player.x == 7);

    hold(BTN_UP, 500);
    CHECK(game.player.y == 5);

    world_enter(AREA_OUTDOOR, 232, 24, EAST);
    hold(BTN_RIGHT, 1000);
    CHECK(game.player.x == 241);

    world_enter(AREA_INTERIOR, 64, 40, EAST);
    hold(BTN_RIGHT, 80);
    CHECK(game.player.x == 65);
    game_update(BTN_UP);
    CHECK(game.player.y == 38); /* slide away from a corner */

    world_enter(AREA_OUTDOOR, 96, 56, SOUTH);
    hold(BTN_DOWN, 16);
    CHECK(game.player.y == 88);
    CHECK(game.player.x == 96);

    world_enter(AREA_INTERIOR, 48, 48, NORTH);
    hold(BTN_UP, 40);
    CHECK(game.player.y == 5);
    CHECK(!actor_move(&game.player, NORTH, 200));

    game_new();
    a = game;
    game_new();
    b = game;

    // Only host tests keep snapshots; every simulation step uses the active singleton.
    for(i = 0; i < 2000; ++i) {
        uint8_t input = (uint8_t)((i % 5 == 0) ? BTN_LEFT : BTN_DOWN);
        game = a;
        game_update(input);
        a = game;
        game = b;
        game_update(input);
        b = game;
        CHECK(memcmp(&a, &b, sizeof(a)) == 0);
    }
}

static void camera_and_portals() {
    Camera c;

    game_new();
    c = game_camera();
    CHECK(c.x == 0 && c.y == 12);
    game.player.x = 101;
    game.player.y = 91;
    c = game_camera();
    CHECK(c.x == 41 && c.y == 63);
    game.player.x = 240;
    game.player.y = 144;
    c = game_camera();
    CHECK(c.x == 128 && c.y == 96);

    world_enter(AREA_INTERIOR, 16, 56, NORTH);
    c = game_camera();
    CHECK(c.x == 0 && c.y == 16);
    CHECK(terrain_clear(game.player.x, game.player.y));
    game_update(BTN_DOWN);
    CHECK(game.area == AREA_INTERIOR && game.player.y == 58);
    game_update(0);
    CHECK(game.area == AREA_OUTDOOR && game.player.x == 96 && game.player.y == 48 &&
          game.player.facing == SOUTH);
    game_update(0);
    hold(BTN_UP, 4);
    CHECK(game.area == AREA_INTERIOR);
    CHECK(game.player.x == 16 && game.player.y == 56);
    game_update(0);
}

static void dialogue_consequences() {

    game_new();
    world_enter(AREA_INTERIOR, 48, 40, NORTH);
    CHECK(game.can_interact);
    uint16_t ticks = game.ticks, rng = game.rng;
    game_update(0, BTN_A);
    CHECK(game.get_flag(FLAG_HELPED));
    CHECK(game.gold == 5);
    CHECK(game.ticks == ticks && game.rng == rng);
    CHECK(shown_dialogues == 2 && asked_choices == 1);

    game_update(0, BTN_A);
    CHECK(strstr(last_dialogue, "remember") != NULL);
    CHECK(game.gold == 5 && asked_choices == 1);

    game.flags = {};
    game.gold = 0;
    accept_choice = false;
    game_update(0, BTN_A);
    CHECK(!game.get_flag(FLAG_HELPED) && !game.gold);
    CHECK(strstr(last_dialogue, "another day") != NULL);

    accept_dialogue = false;
    unsigned choices = asked_choices;
    game_update(0, BTN_A);
    CHECK(asked_choices == choices);
    CHECK(!game.get_flag(FLAG_HELPED) && !game.gold);
    accept_dialogue = true;
    accept_choice = true;

    Game before = game;
    game.player.facing = SOUTH;
    before = game;
    game_update(0, BTN_A);
    CHECK(game.gold == before.gold &&
          game.flags.get_flag(FLAG_HELPED) == before.flags.get_flag(FLAG_HELPED));
}

static void persistence() {
    Game before;
    uint8_t data[SAVE_BYTES];
    unsigned i;

    game_new();
    game.player.x = 35;
    game.flags = {};
    game.set_flag(FLAG_HELPED);
    game.gold = 5;
    game.rng = 1234;

    save_encode(data);
    game_new();
    CHECK(save_decode(data));
    CHECK(game.player.x == 35 && game.area == AREA_OUTDOOR && game.get_flag(FLAG_HELPED) &&
          game.gold == 5 && game.rng == 1234);

    before = game;
    for(i = 0; i < SAVE_BYTES; ++i) {
        data[i] ^= 0x40;
        CHECK(!save_decode(data));
        CHECK(memcmp(&game, &before, sizeof(game)) == 0);
        data[i] ^= 0x40;
    }
    data[2] = 99;
    data[SAVE_CRC_OFFSET] = (uint8_t)save_checksum(data);
    data[SAVE_CRC_OFFSET + 1] = (uint8_t)(save_checksum(data) >> 8);
    CHECK(!save_decode(data));

    save_encode(data);
    data[6] = 255;
    data[7] = 255;
    data[SAVE_CRC_OFFSET] = (uint8_t)save_checksum(data);
    data[SAVE_CRC_OFFSET + 1] = (uint8_t)(save_checksum(data) >> 8);
    CHECK(!save_decode(data));

    memset(data, 0, sizeof(data));
    CHECK(!save_decode(data));

    world_enter(AREA_INTERIOR, 48, 40, NORTH);
    game.player.y = 0;

    save_encode(data);
    CHECK(save_decode(data));
    CHECK(terrain_clear(game.player.x, game.player.y));
    CHECK(game.player.y != 0);

    // A blocked corner requires fallback to spawn and a freshly centered cache.
    game.player.x = game.player.y = 0;
    save_encode(data);
    CHECK(save_decode(data));
    Area const* area = world_area(AREA_INTERIOR);
    CHECK(game.player.x == area->spawn.x && game.player.y == area->spawn.y);
    CHECK(terrain_clear(game.player.x, game.player.y));

    world_enter(AREA_OUTDOOR, 48, 80, EAST);

    save_encode(data);
    CHECK(save_decode(data));
}

static void refresh_crc(uint8_t* bytes, uint16_t size) {
    uint16_t crc = save_checksum(bytes, size - 2);
    bytes[size - 2] = (uint8_t)crc;
    bytes[size - 1] = (uint8_t)(crc >> 8);
}

static void save_versions() {
    uint8_t data[SAVE_BYTES];
    game_new();
    game.set_flag(FLAG_HELPED);
    game.set_flag(FLAG_GATE_OPEN);
    game.set_flag(FLAG_BRIDGE_REPAIRED);
    game.set_flag(FLAG_WAYSTONE);
    game.set_flag(FLAG_GUARD_RELOCATED);
    game.set_flag(FLAG_COURIER_ARRIVED);
    game.set_flag(FLAG_ROWAN_AWAY);
    game.set_flag(FLAG_COURIER_DISPATCHED);
    save_encode(data);
    CHECK(save_decode(data));
    for(uint16_t bit = 0; bit < FLAG_COUNT; ++bit)
        CHECK(game.flags.get_flag(bit));
    Game before = game;
    for(uint16_t length = 0; length < SAVE_BYTES; ++length) {
        CHECK(!save_decode(data, length));
        CHECK(!memcmp(&before, &game, sizeof(game)));
    }
    // Undefined bits, future flag counts and nonzero padding are rejected.
    if(FLAG_COUNT % 8) {
        data[SAVE_HEADER_BYTES + WorldFlags::BYTE_COUNT - 1] |= (uint8_t)(1u << (FLAG_COUNT % 8));
        refresh_crc(data, SAVE_BYTES);
        CHECK(!save_decode(data));
    }
    save_encode(data);
    data[18] = FLAG_COUNT + 1;
    refresh_crc(data, SAVE_BYTES);
    CHECK(!save_decode(data));
    save_encode(data);
    data[SAVE_CRC_OFFSET - 1] = 1;
    refresh_crc(data, SAVE_BYTES);
    CHECK(!save_decode(data));
    // Earlier v3 flag counts zero-extend into the current bitset.
    save_encode(data);
    data[18] = 1;
    data[SAVE_HEADER_BYTES] = 1;
    refresh_crc(data, SAVE_BYTES);
    CHECK(save_decode(data));
    CHECK(game.get_flag(FLAG_HELPED) && !game.get_flag(FLAG_GATE_OPEN));
    uint16_t expected_rng = game.rng;
    // v1/v2 used a 16-bit mask at 16 and RNG at 18.
    for(uint8_t version = 1; version <= 2; ++version) {
        save_encode(data);
        data[2] = version;
        data[16] = version == 1 ? 1 : 31;
        data[17] = 0;
        data[18] = (uint8_t)game.rng;
        data[19] = (uint8_t)(game.rng >> 8);
        data[20] = data[21] = 0;
        refresh_crc(data, 24);
        CHECK(save_decode(data, 24));
        CHECK(game.get_flag(FLAG_HELPED));
        CHECK(game.get_flag(FLAG_WAYSTONE) == (version == 2));
        CHECK(game.get_flag(FLAG_BRIDGE_REPAIRED) == (version == 2));
        CHECK(game.rng == expected_rng);
    }
    // Structurally valid saves rebuild the cache, then repair blocked positions.
    game_new();
    game.set_flag(FLAG_GATE_OPEN);
    world_enter(AREA_OUTDOOR, 120, 16, EAST);
    save_encode(data);
    game_new(); // Loading an open gate must rebuild terrain from the saved flags.
    CHECK(save_decode(data));
    CHECK(game.player.x == 120 && game.player.y == 16);
    data[SAVE_HEADER_BYTES] = 0;
    refresh_crc(data, SAVE_BYTES);
    CHECK(save_decode(data));
    CHECK(!game.get_flag(FLAG_GATE_OPEN));
    CHECK(game.player.x != 120 || game.player.y != 16);
    CHECK(terrain_clear(game.player.x, game.player.y));
}

static void complete_demo() {
    uint8_t data[SAVE_BYTES];

    game_new();
    hold(BTN_RIGHT, 30);
    CHECK(game.area == AREA_INTERIOR);
    CHECK(game.player.x == 16);
    hold(BTN_RIGHT, 16);
    hold(BTN_UP, 8);
    CHECK(game.can_interact);
    game_update(0, BTN_A);
    CHECK(game.get_flag(FLAG_HELPED));
    hold(BTN_LEFT, 16);
    hold(BTN_DOWN, 9);
    CHECK(game.area == AREA_OUTDOOR);

    Game saved = game;
    save_encode(data);
    game_new();
    CHECK(save_decode(data));
    CHECK(game.area == AREA_OUTDOOR && game.player.x == saved.player.x &&
          game.player.y == saved.player.y);
    CHECK(game.get_flag(FLAG_HELPED));
    CHECK(game.gold == 5);

    puts("PASS M1 rules: new -> walk -> enter -> choice -> exit -> save -> restart/load");
}

int main() {
    movement();
    camera_and_portals();
    dialogue_consequences();
    persistence();
    save_versions();
    complete_demo();
    printf("PASS %u checks; host Game=%u bytes (guest report is authoritative)\n", checks,
           (unsigned)sizeof(Game));
    return 0;
}
