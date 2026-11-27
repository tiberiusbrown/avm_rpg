#include "rpg.h"
#include "chunk.h"
#include "npc.h"
#include "logic/npc_logic.h"
#include "cast.h"
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
    if(site == sites::ORCHARD)
        game.set_flag(TEST_FLAG_A);
    else
        game.clear_flag(TEST_FLAG_A);
    return npc_location(NPC_VILLAGER) == site;
}

static void place_player(uint8_t tx, uint8_t ty) {
    game.player.x = (uint16_t)tx * TILE;
    game.player.y = (uint16_t)ty * TILE;
    game.player.walking = 0;
}

static void test_defaults_and_view_admission() {
    game_new();
    CHECK(npc_location(NPC_VILLAGER) == sites::WELL);
    CHECK(npc_location(NPC_WANDERER) == sites::LANE);
    CHECK(npc_find(NPC_VILLAGER) && npc_find(NPC_WANDERER));

    // Off-cache actors retire; their logical sites remain derived from quest state.
    place_player(sites::ORCHARD.x - 1, sites::ORCHARD.y);
    update_visible_world(0, false);
    CHECK(!npc_find(NPC_VILLAGER));
    CHECK(npc_location(NPC_VILLAGER) == sites::WELL);
}

static void test_static_npc_stays_in_its_chunk() {
    game_new();
    CHECK(npc_location(NPC_WANDERER) == sites::LANE);
    CHECK(residence_site(sites::WELL));
    CHECK(npc_location(NPC_WANDERER) == sites::LANE);
}

static void test_destination_admission_and_travel() {
    // Entering the area casts the villager at its new residence, which the player can see.
    npc_reset();
    // The player stands beside the destination, which is then in view.
    place_player(sites::ORCHARD.x - 1, sites::ORCHARD.y);
    CHECK(residence_site(sites::ORCHARD));
    update_visible_world(0, false);
    Npc* villager = npc_find(NPC_VILLAGER);
    CHECK(villager);
    CHECK(villager->actor.x == sites::ORCHARD.x * TILE &&
          villager->actor.y == sites::ORCHARD.y * TILE);

    // An active dynamic NPC walks toward a new destination; it does not teleport.
    uint16_t x = villager->actor.x, y = villager->actor.y;
    CHECK(residence_site(sites::WELL));
    update_visible_world(0, false);
    villager = npc_find(NPC_VILLAGER);
    CHECK(villager && villager->behavior == NPC_TRAVEL);
    CHECK(villager->actor.x == x && villager->actor.y == y);
}

static void test_save_round_trip_and_rejection() {
    CHECK(residence_site(sites::ORCHARD));
    TilePos before = npc_location(NPC_VILLAGER);
    Game saved = game;

    game_new();
    game = saved;
    CHECK(save_restore());
    CHECK(npc_location(NPC_VILLAGER) == before);

    // Invalid destinations are rejected before any loaded NPC state is used.
    game.save_version = SAVE_VERSION - 1;
    CHECK(!save_restore());
}

// A loaded pool is the saved one. What content decides for an actor is taken from the content
// again, and a pool that the update code cannot run rejects the save.
static void test_saved_pool() {
    game_new();
    uint8_t count = 0;
    while(count < NPC_CAPACITY && game.npcs.actors[count].id)
        ++count;
    CHECK(count >= 1 && count < NPC_CAPACITY);
    Npc& first = game.npcs.actors[0];
    first.actor.x += 3;
    first.actor.facing = WEST;
    first.timer = 17;
    Game saved = game;

    game_new();
    game = saved;
    uint8_t const content = first.policy_pending & (uint8_t)~NPC_FLAG_CLEAR;
    first.policy_pending ^= NPC_FLAG_FIXED_SITE | NPC_FLAG_STATIC | NPC_FLAG_IMMOVABLE;
    CHECK(save_restore());
    CHECK(first.id == saved.npcs.actors[0].id && first.actor.x == saved.npcs.actors[0].actor.x);
    CHECK(first.actor.facing == WEST && first.timer == 17 && first.policy_pending == content);
    for(uint8_t i = 1; i < NPC_CAPACITY; ++i)
        CHECK(game.npcs.actors[i].id == saved.npcs.actors[i].id);

    auto rejects = [&](auto damage) {
        game = saved;
        damage();
        Game damaged = game;
        CHECK(!save_valid() && !save_restore());
        CHECK(!memcmp(&game, &damaged, sizeof(Game)));
    };
    rejects([] { game.npcs.actors[0].id = (NPC_ID)250; });
    rejects([] { game.npcs.actors[0].id = NPC_ALDEN; }); // A party member has no map actor.
    rejects([count] { game.npcs.actors[count] = game.npcs.actors[0]; });
    rejects([count] {
        game.npcs.actors[count] = game.npcs.actors[count - 1];
        game.npcs.actors[count - 1] = {};
    });
    rejects([] { game.npcs.actors[0].behavior = (NPC_BEHAVIOR)(NPC_RETURN + 1); });
    rejects([] { game.npcs.actors[0].behavior = NPC_CHASE; }); // Only an enemy has rules.
    rejects([] { game.npcs.actors[0].behavior = NPC_RETURN; });
    rejects([] { game.npcs.actors[0].policy_pending |= NPC_FLAG_PURSUIT; });
    rejects([] { game.npcs.actors[0].actor.facing = STILL; });
    rejects([] {
        game.npcs.actors[0].state |= NPC_YIELDING;
        game.npcs.actors[0].stuck = 4 << 4 | 1;
    });
    rejects([] { game.npcs.actors[0].actor.x = 65535; });
    rejects([] { game.npcs.actors[0].actor.y = 65535; });
    rejects([] { game.npcs.entering = 2; });
    rejects([] { game.npcs.push_direction = (DIRECTION)(STILL + 1); });
}

static void test_destination_collision_and_recruitment() {
    game_new();
    CHECK(residence_site(sites::ORCHARD));
    place_player(sites::ORCHARD.x, sites::ORCHARD.y);
    update_visible_world(0, false);
    CHECK(!npc_find(NPC_VILLAGER));
    CHECK(npc_location(NPC_VILLAGER) == sites::ORCHARD);

    // Even entry permission cannot override an occupied destination footprint.
    npc_reset();
    update_visible_world(0, false);
    CHECK(!npc_find(NPC_VILLAGER));
    place_player(sites::ORCHARD.x - 1, sites::ORCHARD.y);
    update_visible_world(0, false);
    CHECK(!npc_find(NPC_VILLAGER)); // A newly clear but visible site still waits.

    // Bring the site into the cache margin while it remains outside the guard.
    game.player.x = (sites::ORCHARD.x - 6) * TILE + 8;
    game.player.y = sites::ORCHARD.y * TILE;
    update_visible_world(0, false);
    CHECK(npc_find(NPC_VILLAGER));
    CHECK(npc_find(NPC_VILLAGER)->assigned_site == sites::ORCHARD);
    CHECK(party_add({12, 4, MOVEMENT_STEP, ATTACK_STRIKE, NPC_VILLAGER, 1, 0}));
    CHECK(!npc_find(NPC_VILLAGER));
    Game saved = game;
    game_new();
    game = saved;
    CHECK(save_restore());
    update_visible_world(0, false);
    CHECK(party_contains(NPC_VILLAGER) && !npc_find(NPC_VILLAGER));
}

static unsigned moved_pixels(uint16_t x, uint16_t y, Npc const& n) {
    int dx = (int)n.actor.x - (int)x, dy = (int)n.actor.y - (int)y;
    return (unsigned)(abs(dx) + abs(dy));
}

// A walking NPC that leaves view keeps its slot and walks on. It never reappears at home.
static void test_leaving_view_is_continuous() {
    game_new();
    // Five tiles east of the wanderer's home. Its leash reaches one tile west of home.
    place_player(sites::LANE.x + 5, sites::LANE.y + 1);
    update_visible_world(0, false);
    CHECK(npc_move_to(NPC_WANDERER, sites::LANE.x * TILE - TILE, sites::LANE.y * TILE));
    Npc* wanderer = npc_find(NPC_WANDERER);
    CHECK(wanderer);
    bool left_view = false;
    for(unsigned frame = 0; frame < 120; ++frame) {
        uint16_t x = wanderer->actor.x, y = wanderer->actor.y;
        game_update(0);
        CHECK(npc_find(NPC_WANDERER) == wanderer);
        CHECK(moved_pixels(x, y, *wanderer) <= 1);
        left_view |= !npc_visible(wanderer->actor.x, wanderer->actor.y);
    }
    CHECK(left_view);
}

static uint16_t home_x() {
    return (uint16_t)(sites::WELL.x * TILE);
}

static uint16_t home_y() {
    return (uint16_t)(sites::WELL.y * TILE);
}

static void test_travel_finishes_outside_cache() {
    game_new();
    Npc* villager = npc_find(NPC_VILLAGER);
    CHECK(villager);
    CHECK(residence_site(sites::ORCHARD));

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
    place_player(sites::TEST_FIELD.x, sites::TEST_FIELD.y);
    update_visible_world(0, true);
    CHECK(!npc_find(NPC_VILLAGER));
    CHECK(npc_location(NPC_VILLAGER) == sites::ORCHARD);

    // Entry reconstructs the destination through the ordinary casting stage.
    npc_reset();
    place_player(sites::ORCHARD.x - 1, sites::ORCHARD.y);
    update_visible_world(0, false);
    villager = npc_find(NPC_VILLAGER);
    CHECK(villager && villager->assigned_site == sites::ORCHARD);
    CHECK(villager->actor.x == sites::ORCHARD.x * TILE);
    CHECK(residence_site(sites::WELL));
    update_visible_world(0, false);
    CHECK(npc_find(NPC_VILLAGER)->behavior == NPC_TRAVEL);
}

// An NPC outside the terrain cache is not simulated, and it resumes from the same place.
static void test_npc_outside_cache_is_frozen() {
    game_new();
    Npc* villager = npc_find(NPC_VILLAGER);
    CHECK(npc_move_to(NPC_VILLAGER, home_x(), home_y() + TILE));
    place_player(sites::ORCHARD.x,
                 sites::ORCHARD.y); // The well is outside the cache.
    update_visible_world(0, false);
    CHECK(npc_move_to(NPC_VILLAGER, home_x(), home_y() + TILE));
    for(unsigned frame = 0; frame < 20; ++frame)
        game_update(0);
    CHECK(villager->actor.x == home_x() && villager->actor.y == home_y());
    place_player(sites::PLAYER_START.x,
                 sites::PLAYER_START.y); // Back at the start, where the home is cached again.
    for(unsigned frame = 0; frame < 200 && villager->behavior != NPC_STAND; ++frame) {
        uint16_t x = villager->actor.x, y = villager->actor.y;
        game_update(0);
        CHECK(npc_find(NPC_VILLAGER) == villager);
        CHECK(moved_pixels(x, y, *villager) <= 1);
    }
    CHECK(villager->actor.x == home_x() && villager->actor.y == home_y() + TILE);
}

// NPCs step on every other frame, and their walk pose holds between steps.
static void test_npc_steps_every_other_frame() {
    game_new();
    Npc* villager = npc_find(NPC_VILLAGER);
    CHECK(npc_move_to(NPC_VILLAGER, home_x(), home_y() + TILE)); // Sixteen pixels south of home.
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
    test_saved_pool();
    test_destination_collision_and_recruitment();
    test_leaving_view_is_continuous();
    test_travel_finishes_outside_cache();
    test_npc_outside_cache_is_frozen();
    test_npc_steps_every_other_frame();
    printf("npc residence checks passed: %u\n", checks);
    return 0;
}
