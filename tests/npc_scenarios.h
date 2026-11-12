#include "npc.h"
#include "chunk.h"
#include "map_positions.h"
#include "tile_ids.h"
#include "actor_draw_order.h"
#include <string.h>

static unsigned npc_test_dialogues, npc_test_choices;

// Scenario positions are pixel offsets from the map point TestField, the origin of the cleared
// window that these tests paint.
static inline uint16_t scene_x(unsigned dx) {
    return (uint16_t)(positions::TestField.x * TILE + dx);
}

static inline uint16_t scene_y(unsigned dy) {
    return (uint16_t)(positions::TestField.y * TILE + dy);
}

static inline TilePos scene_tile(unsigned dx, unsigned dy) {
    return TilePos{AREA_OUTDOOR, (uint8_t)(positions::TestField.x + dx),
                   (uint8_t)(positions::TestField.y + dy)};
}

__attribute__((noinline)) static void npc_test_draw_order() {
    game_new();
    npc_reset();
    game.camera = {scene_x(0), scene_y(0)};
    game.player = {scene_x(48), scene_y(0), SOUTH, 0};
    uint8_t order[NPC_CAPACITY + 1];
    CHECK(actor_draw_order(order) == 1 && order[0] == PLAYER_DRAW_SLOT);
    for(uint8_t i = 0; i < NPC_CAPACITY; ++i) {
        game.npcs.actors[i].id = NPC_VILLAGER;
        game.npcs.actors[i].actor = {scene_x(16), scene_y(63 - i * 8), SOUTH, 0};
    }
    CHECK(actor_draw_order(order) == NPC_CAPACITY + 1);
    CHECK(order[0] == PLAYER_DRAW_SLOT);
    for(uint8_t i = 0; i < NPC_CAPACITY; ++i) {
        CHECK(order[i + 1] == NPC_CAPACITY - 1 - i);
        CHECK(game.npcs.actors[i].actor.y == scene_y(63 - i * 8));
        game.npcs.actors[i].actor.y = scene_y(32);
    }
    game.player.y = scene_y(32);
    CHECK(actor_draw_order(order) == NPC_CAPACITY + 1);
    for(uint8_t i = 0; i <= NPC_CAPACITY; ++i)
        CHECK(order[i] == i); // Equal-Y ties keep slot order, with the player last.
    game.player.y = scene_y(63);
    CHECK(actor_draw_order(order) == NPC_CAPACITY + 1 && order[NPC_CAPACITY] == PLAYER_DRAW_SLOT);

    npc_reset();
    game.camera = {scene_x(32), scene_y(16)};
    game.player = {scene_x(48), scene_y(32), SOUTH, 0};
    for(uint8_t i = 0; i < 5; ++i)
        game.npcs.actors[i].id = NPC_VILLAGER;
    game.npcs.actors[0].actor = {scene_x(17), scene_y(17), SOUTH, 0}; // One visible column.
    game.npcs.actors[1].actor = {scene_x(16), scene_y(32), SOUTH, 0}; // Touches the left edge.
    game.npcs.actors[2].actor = {scene_x(32), scene_y(0), SOUTH, 0};
    game.npcs.actors[3].actor = {scene_x(32), scene_y(80), SOUTH, 0};
    game.npcs.actors[4].actor = {scene_x(160), scene_y(32), SOUTH, 0};
    CHECK(actor_draw_order(order) == 2 && order[0] == 0 && order[1] == PLAYER_DRAW_SLOT);
}

__attribute__((noinline)) static void npc_test_foot_geometry() {
    CHECK(FOOT_WIDTH == 8 && FOOT_HEIGHT == 8);
    CHECK(FOOT_LEFT == 4 && FOOT_RIGHT == 11 && FOOT_TOP == 8 && FOOT_BOTTOM == 15);
    CHECK(!npc_overlap(0, 0, 8, 0) && npc_overlap(0, 0, 7, 7));
    CHECK(!npc_overlap(0, 0, 0, 8));
    game_new();
    npc_reset();
    game.camera = {scene_x(0), scene_y(0)};
    game.cache.origin_tx = positions::TestField.x;
    game.cache.origin_ty = positions::TestField.y;
    memset(game.cache.tiles, TILE_TREE, sizeof(game.cache.tiles));
    set_tile(scene_tile(3, 3), TILE_GRASS_06);
    CHECK(terrain_clear(scene_x(44), scene_y(40)) && terrain_clear(scene_x(52), scene_y(48)));
    CHECK(!terrain_clear(scene_x(43), scene_y(40)) && !terrain_clear(scene_x(53), scene_y(40)));
    CHECK(!terrain_clear(scene_x(44), scene_y(39)) && !terrain_clear(scene_x(44), scene_y(49)));
    game.player = {scene_x(44), scene_y(40), SOUTH, 0};
    CHECK((game.player_is_on_tile(scene_tile(3, 3))));
}

static void npc_test_valid() {
    for(auto const& n : game.npcs.actors) {
        if(!n.id)
            continue;
        CHECK(npc_definition(n.id));
        CHECK(npc_definition(n.id)->sprite_row < ACTOR_ROW_COUNT);
        CHECK(actor_frame(n.actor, npc_definition(n.id)->sprite_row, game.ticks) <
              ACTOR_ROW_COUNT * 12);
    }
}

static void npc_test_push_step(DIRECTION dir) {
    npc_begin_frame();
    npc_update();
    npc_player_begin(dir);
    actor_move(&game.player, dir, 2);
    ++game.ticks;
}

__attribute__((noinline)) static void npc_test_corridor(bool can_detour) {
    game_new();
    npc_reset();
    game.camera = {scene_x(0), scene_y(0)};
    game.cache.origin_tx = positions::TestField.x;
    game.cache.origin_ty = positions::TestField.y;
    memset(game.cache.tiles, TILE_TREE, sizeof(game.cache.tiles));
    for(uint8_t x = 0; x < CACHE_WIDTH; ++x)
        set_tile(scene_tile(x, 3), TILE_GRASS_06);
    uint16_t y = scene_y(can_detour ? 48 : 44);
    game.player = {scene_x(20), y, EAST, 0};
    auto& blocker = game.npcs.actors[0];
    blocker = {};
    blocker.id = NPC_VILLAGER;
    blocker.actor = {scene_x(48), y, WEST, 0};
    bool blocked = false;
    for(unsigned i = 0; i < 40; ++i) {
        uint16_t old = game.player.x;
        npc_test_push_step(EAST);
        CHECK(terrain_clear(game.player.x, game.player.y));
        CHECK(game.player.x >= old && game.player.x <= old + 2);
        blocked |= old == game.player.x;
    }
    CHECK(blocked && !(blocker.state & NPC_PASS_PLAYER));
    CHECK(blocker.actor.x == scene_x(48) && blocker.actor.y == y);
    if(can_detour) {
        CHECK(game.player.x == scene_x(40) && (blocker.state & NPC_CAN_DETOUR));
        for(uint8_t i = 0; i < 4; ++i)
            npc_test_push_step(NORTH);
        for(uint8_t i = 0; i < 12; ++i)
            npc_test_push_step(EAST);
        for(uint8_t i = 0; i < 4; ++i)
            npc_test_push_step(SOUTH);
        CHECK(game.player.x == scene_x(64) && game.player.y == scene_y(48));
    } else {
        CHECK(game.player.x > scene_x(60)); // No 8-pixel lane fits: temporary passage still works.
    }
}

__attribute__((noinline)) static void npc_test_yield() {
    game_new();
    npc_reset();
    game.camera = {scene_x(0), scene_y(0)};
    game.cache.origin_tx = positions::TestField.x;
    game.cache.origin_ty = positions::TestField.y;
    memset(game.cache.tiles, TILE_TREE, sizeof(game.cache.tiles));
    for(uint8_t x = 0; x < CACHE_WIDTH; ++x)
        set_tile(scene_tile(x, 3), TILE_GRASS_06);
    set_tile(scene_tile(3, 2), TILE_GRASS_06);
    game.player = {scene_x(40), scene_y(44), EAST, 0};
    auto& blocker = game.npcs.actors[0];
    blocker = {};
    blocker.id = NPC_VILLAGER;
    blocker.actor = {scene_x(48), scene_y(44), WEST, 0};
    for(uint8_t i = 0; i < NPC_PUSH_FRAMES; ++i)
        npc_test_push_step(EAST);
    CHECK((blocker.state & NPC_YIELDING) && !(blocker.state & NPC_PASS_PLAYER));
    CHECK(blocker.actor.y == scene_y(44) && game.player.x == scene_x(40) &&
          (blocker.stuck & 15) == 8);
    // A yield steps every other frame, so its eight pixels take sixteen frames.
    bool moved_last = false;
    for(uint8_t i = 0; i < 16; ++i) {
        uint16_t old_y = blocker.actor.y;
        npc_test_push_step(EAST);
        bool moved = blocker.actor.y != old_y;
        CHECK(blocker.actor.y <= old_y && old_y - blocker.actor.y <= 1 && !(moved && moved_last));
        CHECK(terrain_clear(blocker.actor.x, blocker.actor.y));
        moved_last = moved;
    }
    CHECK(blocker.actor.y == scene_y(36) && !(blocker.state & (NPC_YIELDING | NPC_PASS_PLAYER)));
    CHECK(game.player.x > scene_x(40));
}

static void run_npc_scenarios() {
    npc_test_draw_order();
    npc_test_foot_geometry();
    game_new();
    CHECK(npc_find(NPC_VILLAGER) && npc_find(NPC_WANDERER));
    CHECK(npc_definition(NPC_VILLAGER)->sprite_row == 1);
    CHECK(npc_definition(NPC_WANDERER)->sprite_row == 2);
    CHECK(!npc_definition((NPC_ID)255));
    CHECK(!npc_move_to((NPC_ID)255, scene_x(0), scene_y(0)));
    CHECK(!npc_follow_route(NPC_VILLAGER, 255));
    Npc* wanderer = npc_find(NPC_WANDERER);
    bool walked = false, paused = false;
    uint8_t facings = 0;
    for(unsigned i = 0; i < 400; ++i) {
        game_update(0);
        CHECK(npc_find(NPC_WANDERER) == wanderer);
        // The leash is sixteen pixels west and east, and twelve north and south, of home.
        CHECK(wanderer->actor.x + 16 >= positions::WandererHome.x * TILE &&
              wanderer->actor.x <= positions::WandererHome.x * TILE + 16);
        CHECK(wanderer->actor.y + 12 >= positions::WandererHome.y * TILE &&
              wanderer->actor.y <= positions::WandererHome.y * TILE + 12);
        CHECK(terrain_clear(wanderer->actor.x, wanderer->actor.y));
        walked |= wanderer->actor.walking != 0;
        paused |= wanderer->actor.walking == 0;
        facings |= (uint8_t)(1u << wanderer->actor.facing);
        npc_test_valid();
    }
    CHECK(walked && paused && (facings & (facings - 1)));
    CHECK(npc_move_to<positions::VillagerHome>(NPC_WANDERER));
    CHECK(wanderer->target.x == positions::VillagerHome.x * TILE &&
          wanderer->target.y == positions::VillagerHome.y * TILE);
    CHECK(!npc_move_to(NPC_WANDERER, (uint16_t)(game.dimensions.width_tiles * TILE), scene_y(10)));

    game_new();
    Npc* villager = npc_find(NPC_VILLAGER);
    // Walks run until arrival or failure, since NPCs step every other frame.
    uint16_t const home_x = positions::VillagerHome.x * TILE, home_y = positions::VillagerHome.y * TILE;
    CHECK(npc_move_to(NPC_VILLAGER, home_x, home_y + TILE)); // One tile south, on the street.
    for(unsigned i = 0; i < 200 && villager->behavior != NPC_STAND; ++i)
        game_update(0);
    CHECK(villager->actor.y == home_y + TILE && villager->behavior == NPC_STAND);
    CHECK(npc_move_to(NPC_VILLAGER, home_x, home_y));
    for(unsigned i = 0; i < 200 && villager->behavior != NPC_STAND; ++i)
        game_update(0);
    CHECK(villager->actor.y == home_y && villager->behavior == NPC_STAND);
    CHECK(npc_move_to(NPC_VILLAGER, positions::GroveSign.x * TILE,
                      positions::GroveSign.y * TILE)); // The sign blocks this target.
    for(unsigned i = 0; i < 400 && !(villager->state & NPC_TARGET_FAILED); ++i)
        game_update(0);
    CHECK(villager->state & NPC_TARGET_FAILED);
    CHECK(terrain_clear(villager->actor.x, villager->actor.y));

    game_new();
    unsigned before = npc_test_dialogues;
    game_update(BTN_RIGHT, BTN_A);
    CHECK(npc_test_dialogues == before + 1 && !frame_pressed);
    CHECK(game.player.x == positions::PlayerStart.x * TILE && game.ticks == 0);
    world_enter(AREA_OUTDOOR, positions::VillagerEast.x * TILE, positions::VillagerEast.y * TILE,
                SOUTH);
    CHECK(!npc_find(NPC_VILLAGER)); // Its home lies outside the cache from here.
    world_enter(AREA_OUTDOOR, positions::PlayerStart.x * TILE, positions::PlayerStart.y * TILE,
                NORTH);
    CHECK(npc_find(NPC_VILLAGER));
    // With no clear tile left, nothing walks out and no NPC is removed.
    memset(game.cache.tiles, TILE_TREE, sizeof(game.cache.tiles));
    Npc* stuck = npc_find(NPC_VILLAGER);
    uint16_t sx = stuck->actor.x, sy = stuck->actor.y;
    npc_update();
    CHECK(npc_find(NPC_VILLAGER) == stuck && stuck->actor.x == sx && stuck->actor.y == sy);

    npc_test_corridor(true);
    npc_test_corridor(false);
    npc_test_yield();
}
