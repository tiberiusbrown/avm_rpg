#include "npc.h"
#include "chunk.h"
#include "map_positions.h"
#include "tile_ids.h"
#include <string.h>

static unsigned npc_test_dialogues, npc_test_choices;

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

static void run_npc_scenarios() {
    game_new();
    CHECK(npc_find(NPC_VILLAGER) && npc_find(NPC_WANDERER));
    CHECK(npc_definition(NPC_VILLAGER)->sprite_row == 1);
    CHECK(npc_definition(NPC_WANDERER)->sprite_row == 2);
    CHECK(!npc_definition((NPC_ID)255));
    CHECK(!npc_move_to((NPC_ID)255, 0, 0));
    CHECK(!npc_follow_route(NPC_VILLAGER, 255));
    CHECK(!npc_overlap(0, 0, 12, 0) && npc_overlap(0, 0, 11, 9));
    CHECK(!npc_overlap(0, 0, 0, 10));
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

    // A one-tile-wide corridor still permits progress past a 16x16 NPC.
    game_new();
    npc_reset();
    game.camera = {0, 0};
    game.cache.origin_tx = game.cache.origin_ty = 0;
    memset(game.cache.tiles, TILE_TREE, sizeof(game.cache.tiles));
    for(uint8_t x = 0; x < CACHE_WIDTH; ++x)
        set_tile({AREA_OUTDOOR, x, 3}, TILE_GROUND);
    game.player = {20, 48, EAST, 0};
    auto& blocker = game.npcs.actors[0];
    blocker = {};
    blocker.id = NPC_VILLAGER;
    blocker.actor = {48, 48, WEST, 0};
    bool blocked = false;
    for(unsigned i = 0; i < 40; ++i) {
        uint16_t old = game.player.x;
        npc_test_push_step(EAST);
        CHECK(terrain_clear(game.player.x, game.player.y));
        CHECK(game.player.x >= old && game.player.x <= old + 2);
        blocked |= old == game.player.x;
    }
    CHECK(blocked && game.player.x > 60 && !(blocker.state & NPC_PASS_PLAYER));
}
