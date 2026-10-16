#include "rpg.h"
#include "world_assets.h"

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

bool dialogue_show(Game&, char const PROGMEM* words) {
    ++shown_dialogues;
    last_dialogue = words;
    return accept_dialogue;
}

bool dialogue_ask(Game&, char const PROGMEM*) {
    ++asked_choices;
    return accept_choice;
}

static void hold(Game& g, uint8_t button, unsigned frames) {
    while(frames--)
        game_update(g, button);
    game_update(g, 0);
}

static void movement() {
    Game g, a, b;
    unsigned i;

    game_new(g);
    game_update(g, BTN_RIGHT);
    CHECK(g.player.x == 34);
    CHECK(g.player.x % 8 != 0);
    game_update(g, BTN_LEFT);
    CHECK(g.player.x == 32);
    CHECK(g.player.facing == WEST);
    game_update(g, BTN_UP | BTN_DOWN | BTN_RIGHT);
    CHECK(g.player.x == 34);
    game_update(g, BTN_UP | BTN_RIGHT);
    CHECK(g.player.y == 38);
    CHECK(g.player.x == 34);
    game_update(g, BTN_UP | BTN_DOWN | BTN_LEFT | BTN_RIGHT);
    CHECK(!g.player.walking);

    g.player.x = 8;
    g.player.y = 16;
    hold(g, BTN_LEFT, 500);
    CHECK(g.player.x == 7);

    hold(g, BTN_UP, 500);
    CHECK(g.player.y == 5);

    world_enter(g, OUTDOOR, 232, 24, EAST);
    hold(g, BTN_RIGHT, 1000);
    CHECK(g.player.x == 241);

    world_enter(g, INTERIOR, 64, 40, EAST);
    hold(g, BTN_RIGHT, 80);
    CHECK(g.player.x == 65);
    game_update(g, BTN_UP);
    CHECK(g.player.y == 38); /* slide away from a corner */

    world_enter(g, OUTDOOR, 96, 56, SOUTH);
    hold(g, BTN_DOWN, 16);
    CHECK(g.player.y == 88);
    CHECK(g.player.x == 96);

    world_enter(g, INTERIOR, 48, 48, NORTH);
    hold(g, BTN_UP, 40);
    CHECK(g.player.y == 37);
    CHECK(!actors_overlap(g.player.x, g.player.y, 48, 32));
    CHECK(!actor_move(g, &g.player, NORTH, 200, -1));

    game_new(a);
    game_new(b);

    /* Rowan must stop at the authored post, then can walk around its side. */
    CHECK(!actor_move(a, &a.npcs[0].actor, SOUTH, 1, 0));
    CHECK(actor_move(a, &a.npcs[0].actor, EAST, 1, 0));

    game_new(a);

    for(i = 0; i < 2000; ++i) {
        uint8_t input = (uint8_t)((i % 5 == 0) ? BTN_LEFT : BTN_DOWN);
        game_update(a, input);
        game_update(b, input);
        CHECK(memcmp(&a, &b, sizeof(a)) == 0);
    }
}

static void camera_and_portals() {
    Game g;
    Camera c;

    game_new(g);
    c = game_camera(g);
    CHECK(c.x == 0 && c.y == 12);
    g.player.x = 101;
    g.player.y = 91;
    c = game_camera(g);
    CHECK(c.x == 41 && c.y == 63);
    g.player.x = 240;
    g.player.y = 144;
    c = game_camera(g);
    CHECK(c.x == 128 && c.y == 96);

    world_enter(g, INTERIOR, 16, 56, NORTH);
    c = game_camera(g);
    CHECK(c.x == 0 && c.y == 16);
    CHECK(terrain_clear(g.area, g.player.x, g.player.y));
    CHECK(!portals_update(g));
    g.player.y = 58;
    CHECK(portals_update(g));
    CHECK(g.area == OUTDOOR && g.player.x == 96 && g.player.y == 48 && g.player.facing == SOUTH);
    CHECK(!portals_update(g));
    hold(g, BTN_UP, 4);
    CHECK(g.area == INTERIOR);
    CHECK(g.player.x == 16 && g.player.y == 56);
    CHECK(!portals_update(g));
}

static void npcs() {
    Game a, b;
    unsigned i, j, k;
    bool subpixel = false, walk = false;

    game_new(a);
    game_new(b);
#ifdef RPG_STRESS
    CHECK(a.npc_count == 12);
#else
    CHECK(a.npc_count == 3);
#endif

    for(i = 0; i < 10000; ++i) {
        game_update(a, 0);
        game_update(b, 0);
        CHECK(memcmp(&a, &b, sizeof(a)) == 0);
        for(j = 0; j < a.npc_count; ++j) {
            Npc const* n = &a.npcs[j];
            NpcDef const PROGMEM* d = &NPC_DEFS[n->id - 1];
            CHECK(terrain_clear(a.area, n->actor.x, n->actor.y));
            CHECK(n->actor.x >= d->min_x && n->actor.x <= d->max_x && n->actor.y >= d->min_y &&
                  n->actor.y <= d->max_y);
            CHECK(!actors_overlap(a.player.x, a.player.y, n->actor.x, n->actor.y));
            for(k = j + 1; k < a.npc_count; ++k)
                CHECK(
                    !actors_overlap(n->actor.x, n->actor.y, a.npcs[k].actor.x, a.npcs[k].actor.y));
            if(n->actor.x % 8 || n->actor.y % 8)
                subpixel = true;
            if(n->actor.walking)
                walk = true;
            CHECK(n->actor.x < 88 || n->actor.x > 112 ||
                  n->actor.y > 64); /* required doorway stays free */
        }
    }
    CHECK(walk && subpixel);

    world_enter(a, INTERIOR, 16, 56, NORTH);
    CHECK(a.npc_count == 1);
    CHECK(a.npcs[0].id == 1);

    world_enter(a, OUTDOOR, 32, 40, EAST);

    world_enter(b, OUTDOOR, 32, 40, EAST);
    CHECK(memcmp(a.npcs, b.npcs, a.npc_count * sizeof(Npc)) == 0);

    world_enter(a, OUTDOOR, 48, 80, EAST);
    CHECK(!actors_overlap(a.player.x, a.player.y, a.npcs[0].actor.x, a.npcs[0].actor.y));
}

static void dialogue_consequences() {
    Game g;

    game_new(g);
    world_enter(g, INTERIOR, 48, 40, NORTH);
    CHECK(interaction_target(g) == 1);
    Npc frozen = g.npcs[0];
    uint16_t ticks = g.ticks, rng = g.rng;
    dialogue_talk(g, interaction_target(g));
    CHECK(g.flags & FLAG_HELPED);
    CHECK(g.gold == 5);
    CHECK(g.ticks == ticks && g.rng == rng);
    CHECK(memcmp(&frozen, &g.npcs[0], sizeof(frozen)) == 0);
    CHECK(shown_dialogues == 2 && asked_choices == 1);

    dialogue_talk(g, 1);
    CHECK(strstr(last_dialogue, "remember") != NULL);
    CHECK(g.gold == 5 && asked_choices == 1);

    g.flags = 0;
    g.gold = 0;
    accept_choice = false;
    dialogue_talk(g, 1);
    CHECK(!g.flags && !g.gold);
    CHECK(strstr(last_dialogue, "another day") != NULL);

    accept_dialogue = false;
    unsigned choices = asked_choices;
    dialogue_talk(g, 1);
    CHECK(asked_choices == choices);
    CHECK(!g.flags && !g.gold);
    accept_dialogue = true;
    accept_choice = true;

    Game before = g;
    dialogue_talk(g, 2);
    CHECK(memcmp(&g, &before, sizeof(g)) == 0);
    CHECK(strstr(last_dialogue, "rest") != NULL);
}

static void persistence() {
    Game g, loaded, before;
    uint8_t data[SAVE_BYTES];
    unsigned i;

    game_new(g);
    g.player.x = 35;
    g.flags = FLAG_HELPED;
    g.gold = 5;
    g.rng = 1234;

    save_encode(g, data);
    game_new(loaded);
    CHECK(save_decode(loaded, data));
    CHECK(loaded.player.x == 35 && loaded.area == OUTDOOR && loaded.flags == FLAG_HELPED &&
          loaded.gold == 5 && loaded.rng == 1234);

    before = loaded;
    for(i = 0; i < SAVE_BYTES; ++i) {
        data[i] ^= 0x40;
        CHECK(!save_decode(loaded, data));
        CHECK(memcmp(&loaded, &before, sizeof(loaded)) == 0);
        data[i] ^= 0x40;
    }
    data[2] = 99;
    data[22] = (uint8_t)save_checksum(data);
    data[23] = (uint8_t)(save_checksum(data) >> 8);
    CHECK(!save_decode(loaded, data));

    save_encode(g, data);
    data[6] = 255;
    data[7] = 255;
    data[22] = (uint8_t)save_checksum(data);
    data[23] = (uint8_t)(save_checksum(data) >> 8);
    CHECK(!save_decode(loaded, data));

    memset(data, 0, sizeof(data));
    CHECK(!save_decode(loaded, data));

    world_enter(g, INTERIOR, 48, 32, NORTH);

    save_encode(g, data);
    CHECK(!save_decode(loaded, data));

    world_enter(g, OUTDOOR, 48, 80, EAST);

    save_encode(g, data);
    CHECK(save_decode(loaded, data));
    for(i = 0; i < loaded.npc_count; ++i)
        CHECK(!actors_overlap(loaded.player.x, loaded.player.y, loaded.npcs[i].actor.x,
                              loaded.npcs[i].actor.y));
}

static void complete_demo() {
    Game g, restarted;
    uint8_t data[SAVE_BYTES];

    game_new(g);
    hold(g, BTN_RIGHT, 30);
    CHECK(g.area == INTERIOR);
    CHECK(g.player.x == 16);
    hold(g, BTN_RIGHT, 16);
    hold(g, BTN_UP, 8);
    CHECK(interaction_target(g) == 1);
    dialogue_talk(g, interaction_target(g));
    CHECK(g.flags & FLAG_HELPED);
    hold(g, BTN_LEFT, 16);
    hold(g, BTN_DOWN, 9);
    CHECK(g.area == OUTDOOR);

    save_encode(g, data);
    game_new(restarted);
    CHECK(save_decode(restarted, data));
    CHECK(restarted.area == OUTDOOR && restarted.player.x == g.player.x &&
          restarted.player.y == g.player.y);
    CHECK(restarted.flags & FLAG_HELPED);
    CHECK(restarted.gold == 5);

    puts("PASS M1 rules: new -> walk -> enter -> choice -> exit -> save -> restart/load");
}

int main() {
    movement();
    camera_and_portals();
    npcs();
    dialogue_consequences();
    persistence();
    complete_demo();
    printf("PASS %u checks; host Game=%u bytes, NPC=%u bytes (guest report is authoritative)\n",
           checks, (unsigned)sizeof(Game), (unsigned)sizeof(Npc));
    return 0;
}
