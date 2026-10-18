#include "npc.h"
#include "chunk.h"
#include "map_positions.h"
#include "tile_ids.h"
#include "actor_draw_order.h"
#include <string.h>

static unsigned npc_test_dialogues, npc_test_choices;

static void npc_test_draw_order() {
    game_new();
    npc_reset();
    game.camera = {0, 0};
    game.player = {48, 0, SOUTH, 0};
    uint8_t order[NPC_CAPACITY + 1];
    CHECK(actor_draw_order(order) == 1 && order[0] == PLAYER_DRAW_SLOT);
    for(uint8_t i = 0; i < NPC_CAPACITY; ++i) {
        game.npcs.actors[i].id = NPC_VILLAGER;
        game.npcs.actors[i].actor = {16, (uint16_t)(63 - i * 8), SOUTH, 0};
    }
    CHECK(actor_draw_order(order) == NPC_CAPACITY + 1);
    CHECK(order[0] == PLAYER_DRAW_SLOT);
    for(uint8_t i = 0; i < NPC_CAPACITY; ++i) {
        CHECK(order[i + 1] == NPC_CAPACITY - 1 - i);
        CHECK(game.npcs.actors[i].actor.y == 63 - i * 8);
        game.npcs.actors[i].actor.y = 32;
    }
    game.player.y = 32;
    CHECK(actor_draw_order(order) == NPC_CAPACITY + 1);
    for(uint8_t i = 0; i <= NPC_CAPACITY; ++i)
        CHECK(order[i] == i); // Equal-Y ties keep slot order, with the player last.
    game.player.y = 63;
    CHECK(actor_draw_order(order) == NPC_CAPACITY + 1 && order[NPC_CAPACITY] == PLAYER_DRAW_SLOT);

    npc_reset();
    game.camera = {32, 16};
    game.player = {48, 32, SOUTH, 0};
    for(uint8_t i = 0; i < 5; ++i)
        game.npcs.actors[i].id = NPC_VILLAGER;
    game.npcs.actors[0].actor = {17, 17, SOUTH, 0}; // One visible column.
    game.npcs.actors[1].actor = {16, 32, SOUTH, 0}; // Touches the left edge.
    game.npcs.actors[2].actor = {32, 0, SOUTH, 0};
    game.npcs.actors[3].actor = {32, 80, SOUTH, 0};
    game.npcs.actors[4].actor = {160, 32, SOUTH, 0};
    CHECK(actor_draw_order(order) == 2 && order[0] == 0 && order[1] == PLAYER_DRAW_SLOT);
}

static void npc_test_foot_geometry() {
    CHECK(FOOT_WIDTH == 8 && FOOT_HEIGHT == 8);
    CHECK(FOOT_LEFT == 4 && FOOT_RIGHT == 11 && FOOT_TOP == 8 && FOOT_BOTTOM == 15);
    CHECK(!npc_overlap(0, 0, 8, 0) && npc_overlap(0, 0, 7, 7));
    CHECK(!npc_overlap(0, 0, 0, 8));
    game_new();
    npc_reset();
    game.camera = {0, 0};
    game.cache.origin_tx = game.cache.origin_ty = 0;
    memset(game.cache.tiles, TILE_TREE, sizeof(game.cache.tiles));
    set_tile({AREA_OUTDOOR, 3, 3}, TILE_GROUND);
    CHECK(terrain_clear(44, 40) && terrain_clear(52, 48));
    CHECK(!terrain_clear(43, 40) && !terrain_clear(53, 40));
    CHECK(!terrain_clear(44, 39) && !terrain_clear(44, 49));
    game.player = {44, 40, SOUTH, 0};
    CHECK((game.player_is_on_tile({AREA_OUTDOOR, 3, 3})));
}

static void npc_test_valid() {
    for(auto const& n : game.npcs.actors) {
        if(!n.id)
            continue;
        CHECK(npc_definition(n.id));
        CHECK(npc_visible(n.actor.x, n.actor.y));
        CHECK(npc_definition(n.id)->sprite_row < 16);
        CHECK(actor_frame(n.actor, npc_definition(n.id)->sprite_row, game.ticks) < 192);
    }
    CHECK(!game.npcs.capacity_faults);
}

static void npc_test_push_step(DIRECTION dir) {
    npc_begin_frame();
    npc_update();
    npc_player_begin(dir);
    actor_move(&game.player, dir, 2);
    ++game.ticks;
}

static void npc_test_corridor(bool can_detour) {
    game_new();
    npc_reset();
    game.camera = {0, 0};
    game.cache.origin_tx = game.cache.origin_ty = 0;
    memset(game.cache.tiles, TILE_TREE, sizeof(game.cache.tiles));
    for(uint8_t x = 0; x < CACHE_WIDTH; ++x)
        set_tile({AREA_OUTDOOR, x, 3}, TILE_GROUND);
    uint16_t y = can_detour ? 48 : 44;
    game.player = {20, y, EAST, 0};
    auto& blocker = game.npcs.actors[0];
    blocker = {};
    blocker.id = NPC_VILLAGER;
    blocker.actor = {48, y, WEST, 0};
    bool blocked = false;
    for(unsigned i = 0; i < 40; ++i) {
        uint16_t old = game.player.x;
        npc_test_push_step(EAST);
        CHECK(terrain_clear(game.player.x, game.player.y));
        CHECK(game.player.x >= old && game.player.x <= old + 2);
        blocked |= old == game.player.x;
    }
    CHECK(blocked && !(blocker.state & NPC_PASS_PLAYER));
    CHECK(blocker.actor.x == 48 && blocker.actor.y == y);
    if(can_detour) {
        CHECK(game.player.x == 40 && (blocker.state & NPC_CAN_DETOUR));
        for(uint8_t i = 0; i < 4; ++i)
            npc_test_push_step(NORTH);
        for(uint8_t i = 0; i < 12; ++i)
            npc_test_push_step(EAST);
        for(uint8_t i = 0; i < 4; ++i)
            npc_test_push_step(SOUTH);
        CHECK(game.player.x == 64 && game.player.y == 48);
    } else {
        CHECK(game.player.x > 60); // No 8-pixel lane fits: temporary passage still works.
    }
}

static void npc_test_yield() {
    game_new();
    npc_reset();
    game.camera = {0, 0};
    game.cache.origin_tx = game.cache.origin_ty = 0;
    memset(game.cache.tiles, TILE_TREE, sizeof(game.cache.tiles));
    for(uint8_t x = 0; x < CACHE_WIDTH; ++x)
        set_tile({AREA_OUTDOOR, x, 3}, TILE_GROUND);
    set_tile({AREA_OUTDOOR, 3, 2}, TILE_GROUND);
    game.player = {40, 44, EAST, 0};
    auto& blocker = game.npcs.actors[0];
    blocker = {};
    blocker.id = NPC_VILLAGER;
    blocker.actor = {48, 44, WEST, 0};
    for(uint8_t i = 0; i < NPC_PUSH_FRAMES; ++i)
        npc_test_push_step(EAST);
    CHECK((blocker.state & NPC_YIELDING) && !(blocker.state & NPC_PASS_PLAYER));
    CHECK(blocker.actor.y == 44 && game.player.x == 40 && (blocker.stuck & 15) == 8);
    for(uint8_t i = 0; i < 8; ++i) {
        uint16_t old_y = blocker.actor.y;
        npc_test_push_step(EAST);
        CHECK(blocker.actor.y + 1 == old_y && blocker.actor.walking);
        CHECK(terrain_clear(blocker.actor.x, blocker.actor.y));
    }
    CHECK(blocker.actor.y == 36 && !(blocker.state & (NPC_YIELDING | NPC_PASS_PLAYER)));
    CHECK(game.player.x > 40);
}

static void run_npc_scenarios() {
    npc_test_draw_order();
    npc_test_foot_geometry();
    game_new();
    CHECK(npc_find(NPC_VILLAGER) && npc_find(NPC_WANDERER));
    CHECK(npc_definition(NPC_VILLAGER)->sprite_row == 1);
    CHECK(npc_definition(NPC_WANDERER)->sprite_row == 2);
    CHECK(!npc_definition((NPC_ID)255));
    CHECK(!npc_move_to((NPC_ID)255, 0, 0));
    CHECK(!npc_follow_route(NPC_VILLAGER, 255));
    Npc* wanderer = npc_find(NPC_WANDERER);
    bool walked = false, paused = false;
    uint8_t facings = 0;
    for(unsigned i = 0; i < 400; ++i) {
        game_update(0);
        CHECK(npc_find(NPC_WANDERER) == wanderer);
        CHECK(wanderer->actor.x >= 144 && wanderer->actor.x <= 176);
        CHECK(wanderer->actor.y >= 84 && wanderer->actor.y <= 108);
        CHECK(terrain_clear(wanderer->actor.x, wanderer->actor.y));
        walked |= wanderer->actor.walking != 0;
        paused |= wanderer->actor.walking == 0;
        facings |= (uint8_t)(1u << wanderer->actor.facing);
        npc_test_valid();
    }
    CHECK(walked && paused && (facings & (facings - 1)));
    CHECK(npc_move_to<positions::VillagerHome>(NPC_WANDERER));
    CHECK(wanderer->target.x == 80 && wanderer->target.y == 80);
    CHECK(!npc_move_to(NPC_WANDERER, 65535, 10));

    game_new();
    Npc* villager = npc_find(NPC_VILLAGER);
    CHECK(npc_move_to(NPC_VILLAGER, 80, 96));
    for(unsigned i = 0; i < 20; ++i)
        game_update(0);
    CHECK(villager->actor.y == 96 && villager->behavior == NPC_STAND);
    CHECK(npc_move_to(NPC_VILLAGER, 80, 64));
    for(unsigned i = 0; i < 40; ++i)
        game_update(0);
    CHECK(villager->actor.y == 64 && villager->behavior == NPC_STAND);
    CHECK(npc_move_to(NPC_VILLAGER, 112, 64)); // The sign blocks this target.
    for(unsigned i = 0; i < 90; ++i)
        game_update(0);
    CHECK(villager->state & NPC_TARGET_FAILED);
    CHECK(terrain_clear(villager->actor.x, villager->actor.y));

    game_new();
    unsigned before = npc_test_dialogues;
    game_update(BTN_RIGHT, BTN_A);
    CHECK(npc_test_dialogues == before + 1 && !frame_pressed);
    CHECK(game.player.x == 112 && game.ticks == 0);
    world_enter(AREA_OUTDOOR, 15 * TILE, 9 * TILE, SOUTH);
    CHECK(!npc_find(NPC_VILLAGER));
    world_enter(AREA_OUTDOOR, 7 * TILE, 5 * TILE, NORTH);
    CHECK(npc_find(NPC_VILLAGER));
    memset(game.cache.tiles, TILE_TREE, sizeof(game.cache.tiles));
    npc_repair();
    CHECK(!npc_find(NPC_VILLAGER) && !npc_find(NPC_WANDERER));

    npc_test_corridor(true);
    npc_test_corridor(false);
    npc_test_yield();
}
