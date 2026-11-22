#include "rpg.h"
#include "chunk.h"
#include "npc.h"
#include "logic/npc_logic.h"
#include "map_positions.h"
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

// The NPC logic library needs the dialogue overlay; these tests never open it.
bool dialogue_show(char const PROGMEM*) {
    return true;
}

bool dialogue_show(NPC_ID id, char const PROGMEM* words) {
    (void)id;
    return dialogue_show(words);
}

// The test overlay declines questions.
bool dialogue_ask(char const PROGMEM*) {
    return false;
}

bool dialogue_ask(NPC_ID id, char const PROGMEM* question) {
    (void)id;
    return dialogue_ask(question);
}

static bool residence_site(TilePos site) {
    if(site == positions::TessaOrchard)
        game.set_flag(FLAG_LANTERN_RECORD_FOUND);
    else
        game.clear_flag(FLAG_LANTERN_RECORD_FOUND);
    return npc_location(NPC_TESSA) == site;
}

static void place_player(uint8_t tx, uint8_t ty) {
    game.player.x = (uint16_t)tx * TILE;
    game.player.y = (uint16_t)ty * TILE;
    game.player.walking = 0;
}

static void test_defaults_and_view_admission() {
    game_new();
    CHECK(npc_location(NPC_TESSA) == positions::TessaWell);
    CHECK(npc_location(NPC_ORRIN) == positions::OrrinLane);
    CHECK(npc_find(NPC_TESSA) && npc_find(NPC_ORRIN));

    // Off-cache actors retire; their logical sites remain derived from quest state.
    place_player(positions::TessaOrchard.x - 1, positions::TessaOrchard.y);
    update_visible_world(0, false);
    CHECK(!npc_find(NPC_TESSA));
    CHECK(npc_location(NPC_TESSA) == positions::TessaWell);
}

static void test_static_npc_stays_in_its_chunk() {
    game_new();
    CHECK(npc_location(NPC_ORRIN) == positions::OrrinLane);
    CHECK(residence_site(positions::TessaWell));
    CHECK(npc_location(NPC_ORRIN) == positions::OrrinLane);
}

static void test_destination_admission_and_travel() {
    // Entering the area casts the villager at its new residence, which the player can see.
    npc_reset();
    // The player stands beside the destination, which is then in view.
    place_player(positions::TessaOrchard.x - 1, positions::TessaOrchard.y);
    CHECK(residence_site(positions::TessaOrchard));
    update_visible_world(0, false);
    Npc* villager = npc_find(NPC_TESSA);
    CHECK(villager);
    CHECK(villager->actor.x == positions::TessaOrchard.x * TILE &&
          villager->actor.y == positions::TessaOrchard.y * TILE);

    // An active dynamic NPC walks toward a new destination; it does not teleport.
    uint16_t x = villager->actor.x, y = villager->actor.y;
    CHECK(residence_site(positions::TessaWell));
    update_visible_world(0, false);
    villager = npc_find(NPC_TESSA);
    CHECK(villager && villager->behavior == NPC_TRAVEL);
    CHECK(villager->actor.x == x && villager->actor.y == y);
}

static void test_save_round_trip_and_rejection() {
    CHECK(residence_site(positions::TessaOrchard));
    TilePos before = npc_location(NPC_TESSA);
    Game saved = game;

    game_new();
    game = saved;
    // Transient records are untrusted and must be discarded before restoration.
    game.npcs.actors[0].id = (NPC_ID)250;
    game.npcs.actors[0].actor.x = 65535;
    CHECK(save_restore());
    CHECK(!npc_find((NPC_ID)250));
    CHECK(npc_location(NPC_TESSA) == before);

    // Invalid destinations are rejected before any loaded NPC state is used.
    game.save_version = SAVE_VERSION - 1;
    CHECK(!save_restore());
}

static void test_destination_collision_and_recruitment() {
    game_new();
    CHECK(residence_site(positions::TessaOrchard));
    place_player(positions::TessaOrchard.x, positions::TessaOrchard.y);
    update_visible_world(0, false);
    CHECK(!npc_find(NPC_TESSA));
    CHECK(npc_location(NPC_TESSA) == positions::TessaOrchard);

    // Even entry permission cannot override an occupied destination footprint.
    npc_reset();
    update_visible_world(0, false);
    CHECK(!npc_find(NPC_TESSA));
    place_player(positions::TessaOrchard.x - 1, positions::TessaOrchard.y);
    update_visible_world(0, false);
    CHECK(!npc_find(NPC_TESSA)); // A newly clear but visible site still waits.

    // Bring the site into the cache margin while it remains outside the guard.
    game.player.x = (positions::TessaOrchard.x - 6) * TILE + 8;
    game.player.y = positions::TessaOrchard.y * TILE;
    update_visible_world(0, false);
    CHECK(npc_find(NPC_TESSA));
    CHECK(npc_find(NPC_TESSA)->assigned_site == positions::TessaOrchard);
    CHECK(party_add({18, 4, MOVEMENT_STEP, ATTACK_STRIKE, NPC_TESSA, 1, 0}));
    CHECK(!npc_find(NPC_TESSA));
    Game saved = game;
    game_new();
    game = saved;
    CHECK(save_restore());
    update_visible_world(0, false);
    CHECK(party_contains(NPC_TESSA) && !npc_find(NPC_TESSA));
}

static unsigned moved_pixels(uint16_t x, uint16_t y, Npc const& n) {
    int dx = (int)n.actor.x - (int)x, dy = (int)n.actor.y - (int)y;
    return (unsigned)(abs(dx) + abs(dy));
}

// A walking NPC that leaves view keeps its slot and walks on. It never reappears at home.
static void test_leaving_view_is_continuous() {
    game_new();
    // Five tiles east of the wanderer's home. Its leash reaches one tile west of home.
    place_player(positions::OrrinLane.x + 5, positions::OrrinLane.y + 1);
    update_visible_world(0, false);
    CHECK(npc_move_to(NPC_ORRIN, positions::OrrinLane.x * TILE - TILE,
                      positions::OrrinLane.y * TILE));
    Npc* wanderer = npc_find(NPC_ORRIN);
    CHECK(wanderer);
    bool left_view = false;
    for(unsigned frame = 0; frame < 120; ++frame) {
        uint16_t x = wanderer->actor.x, y = wanderer->actor.y;
        game_update(0);
        CHECK(npc_find(NPC_ORRIN) == wanderer);
        CHECK(moved_pixels(x, y, *wanderer) <= 1);
        left_view |= !npc_visible(wanderer->actor.x, wanderer->actor.y);
    }
    CHECK(left_view);
}

static uint16_t home_x() {
    return (uint16_t)(positions::TessaWell.x * TILE);
}

static uint16_t home_y() {
    return (uint16_t)(positions::TessaWell.y * TILE);
}

static void test_travel_finishes_outside_cache() {
    game_new();
    Npc* villager = npc_find(NPC_TESSA);
    CHECK(villager);
    CHECK(residence_site(positions::TessaOrchard));

    npc_stage_begin();
    npc_stage_end();

    // Being blocked while cached must not complete the journey.
    memset(cache.tiles, TILE_TREE, sizeof(cache.tiles));
    note_tile_edit();
    for(unsigned frame = 0; frame < 100; ++frame) {
        game.ticks = frame;
        npc_update();
    }
    CHECK(villager->behavior == NPC_TRAVEL);
    CHECK(villager->actor.x == home_x() && villager->actor.y == home_y());

    // Off-cache completion releases the slot; the destination is still quest-derived.
    place_player(positions::TestField.x, positions::TestField.y);
    update_visible_world(0, true);
    CHECK(!npc_find(NPC_TESSA));
    CHECK(npc_location(NPC_TESSA) == positions::TessaOrchard);

    // Entry reconstructs the destination through the ordinary casting stage.
    npc_reset();
    place_player(positions::TessaOrchard.x - 1, positions::TessaOrchard.y);
    update_visible_world(0, false);
    villager = npc_find(NPC_TESSA);
    CHECK(villager && villager->assigned_site == positions::TessaOrchard);
    CHECK(villager->actor.x == positions::TessaOrchard.x * TILE);
    CHECK(residence_site(positions::TessaWell));
    update_visible_world(0, false);
    CHECK(npc_find(NPC_TESSA)->behavior == NPC_TRAVEL);
}

// An NPC outside the terrain cache is not simulated, and it resumes from the same place.
static void test_npc_outside_cache_is_frozen() {
    game_new();
    Npc* villager = npc_find(NPC_TESSA);
    CHECK(npc_move_to(NPC_TESSA, home_x(), home_y() + TILE));
    place_player(positions::TessaOrchard.x,
                 positions::TessaOrchard.y); // The well is outside the cache.
    update_visible_world(0, false);
    CHECK(npc_move_to(NPC_TESSA, home_x(), home_y() + TILE));
    for(unsigned frame = 0; frame < 20; ++frame)
        game_update(0);
    CHECK(villager->actor.x == home_x() && villager->actor.y == home_y());
    place_player(positions::PlayerStart.x,
                 positions::PlayerStart.y); // Back at the start, where the home is cached again.
    for(unsigned frame = 0; frame < 200 && villager->behavior != NPC_STAND; ++frame) {
        uint16_t x = villager->actor.x, y = villager->actor.y;
        game_update(0);
        CHECK(npc_find(NPC_TESSA) == villager);
        CHECK(moved_pixels(x, y, *villager) <= 1);
    }
    CHECK(villager->actor.x == home_x() && villager->actor.y == home_y() + TILE);
}

// NPCs step on every other frame, and their walk pose holds between steps.
static void test_npc_steps_every_other_frame() {
    game_new();
    Npc* villager = npc_find(NPC_TESSA);
    CHECK(npc_move_to(NPC_TESSA, home_x(), home_y() + TILE)); // Sixteen pixels south of home.
    unsigned frames = 0;
    bool moved_last = false;
    for(; frames < 200 && villager->actor.y != home_y() + TILE; ++frames) {
        uint16_t x = villager->actor.x, y = villager->actor.y;
        game_update(0);
        bool moved = villager->actor.x != x || villager->actor.y != y;
        CHECK(moved_pixels(x, y, *villager) <= 1 && !(moved && moved_last));
        CHECK(villager->actor.walking); // The pose holds between steps.
        moved_last = moved;
    }
    CHECK(villager->actor.y == home_y() + TILE);
    CHECK(frames >= 30 && frames <= 34); // About two frames per pixel.
}

int main() {
    game_new();
    test_defaults_and_view_admission();
    test_static_npc_stays_in_its_chunk();
    test_destination_admission_and_travel();
    test_save_round_trip_and_rejection();
    test_destination_collision_and_recruitment();
    test_leaving_view_is_continuous();
    test_travel_finishes_outside_cache();
    test_npc_outside_cache_is_frozen();
    test_npc_steps_every_other_frame();
    printf("npc residence checks passed: %u\n", checks);
    return 0;
}
