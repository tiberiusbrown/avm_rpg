#include "rpg.h"
#include "npc.h"
#include "chunk.h"
#include "logic/npc_logic.h"
#include "tile_ids.h"
#include "map_positions.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Detour tests. Blocked NPCs wait a brief pause, then walk around the obstacle. Tiles set with
// set_tile are rebuilt from the map every frame, so these tests place their obstacles in the cache
// after each refresh, as the game's own frame does.

static unsigned checks;
#define CHECK(value)                                                                               \
    do {                                                                                           \
        ++checks;                                                                                  \
        if(!(value)) {                                                                             \
            fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #value);                        \
            exit(1);                                                                               \
        }                                                                                          \
    } while(0)

// The overlay is not needed here; the NPC logic library only calls it.
bool dialogue_show(char const PROGMEM*) {
    return true;
}

static void cache_tile(uint8_t tx, uint8_t ty, uint8_t tile) {
    int dx = (int)tx - (int)game.cache.origin_tx, dy = (int)ty - (int)game.cache.origin_ty;
    if(dx >= 0 && dy >= 0 && dx < CACHE_WIDTH && dy < CACHE_HEIGHT)
        game.cache.tiles[dy * CACHE_WIDTH + dx] = tile;
}

static unsigned moved(uint16_t x0, uint16_t y0, Npc const* n) {
    int dx = (int)n->actor.x - (int)x0, dy = (int)n->actor.y - (int)y0;
    return (unsigned)((dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy));
}

// The same frame over open grass, so the only obstacle is the tree.
static void frame_in_field(uint8_t tx, uint8_t ty, uint8_t tile) {
    update_visible_world(0, false);
    memset(game.cache.tiles, TILE_GRASS_06, sizeof(game.cache.tiles));
    cache_tile(tx, ty, tile);
    npc_update();
    ++game.ticks;
}

// A frame with the given obstacle placed after the refresh, then the NPC step.
static void frame_with_tile(uint8_t tx, uint8_t ty, uint8_t tile) {
    update_visible_world(0, false);
    cache_tile(tx, ty, tile);
    npc_update();
    ++game.ticks;
}

// Scenario positions are pixel offsets from the residents' map homes.
static uint16_t villager_x(int dx) {
    return (uint16_t)(positions::VillagerHome.x * TILE + dx);
}

static uint16_t villager_y(int dy) {
    return (uint16_t)(positions::VillagerHome.y * TILE + dy);
}

static uint16_t wanderer_x(int dx) {
    return (uint16_t)(positions::WandererHome.x * TILE + dx);
}

static uint16_t wanderer_y(int dy) {
    return (uint16_t)(positions::WandererHome.y * TILE + dy);
}

// Stands the player two tiles west of the villager's home. The cache then reaches the tile that
// the villager's feet occupy at the far end of its walk.
static void stand_west_of_home() {
    game.player.x = (uint16_t)(positions::VillagerHome.x - 2) * TILE;
    game.player.y = (uint16_t)positions::VillagerHome.y * TILE;
    update_visible_world(0, false);
}

// A villager walks east past a tree. The tree is directly in its row, so it must detour south.
static void test_villager_detours_around_tree() {
    game_new();
    stand_west_of_home();
    Npc* v = npc_find(NPC_VILLAGER);
    CHECK(v && v->actor.x == positions::VillagerHome.x * TILE &&
          v->actor.y == positions::VillagerHome.y * TILE);
    CHECK(npc_move_to(NPC_VILLAGER, villager_x(40), villager_y(0)));
    unsigned blocked_at = 0, sidestep_at = 0, frames = 0;
    uint16_t max_y = 0;
    bool reached = false;
    for(; frames < 600 && !reached; ++frames) {
        uint16_t x0 = v->actor.x, y0 = v->actor.y;
        // The tree sits just east of home, in the villager's row, on open ground.
        frame_in_field(positions::VillagerHome.x + 1, positions::VillagerHome.y, TILE_TREE);
        CHECK(npc_find(NPC_VILLAGER) == v);
        CHECK(terrain_clear(v->actor.x, v->actor.y));
        CHECK(moved(x0, y0, v) <= 1);
        if(!blocked_at && v->actor.x == villager_x(4) && v->actor.y == villager_y(0))
            blocked_at = frames;
        if(!sidestep_at && v->actor.y != y0)
            sidestep_at = frames;
        if(v->actor.y > max_y)
            max_y = v->actor.y;
        reached = v->behavior == NPC_STAND && v->actor.x == villager_x(40) &&
                  v->actor.y == villager_y(0);
    }
    CHECK(reached);
    CHECK(max_y >= villager_y(8)); // It went around the tree on the south side, clear of its rows.
    // It waits a brief pause against the tree before it sidesteps.
    CHECK(blocked_at && sidestep_at > blocked_at);
    unsigned pause = sidestep_at - blocked_at;
    CHECK(pause + 2 >= NPC_DETOUR_FRAMES && pause <= NPC_DETOUR_FRAMES + 4);
}

// A target inside a tree can never be reached. The NPC detours a few times and then fails, so the
// budget bounds the loop instead of letting it run forever.
static void test_target_inside_obstacle_fails_within_budget() {
    game_new();
    stand_west_of_home();
    Npc* v = npc_find(NPC_VILLAGER);
    CHECK(v);
    // Its feet would lie inside the tree just north-east of home.
    CHECK(npc_move_to(NPC_VILLAGER, villager_x(40), villager_y(-16)));
    unsigned frames = 0;
    for(; frames < 2000 && !(v->state & NPC_TARGET_FAILED); ++frames) {
        uint16_t x0 = v->actor.x, y0 = v->actor.y;
        frame_with_tile(positions::VillagerHome.x + 2, positions::VillagerHome.y - 1, TILE_TREE);
        CHECK(npc_find(NPC_VILLAGER) == v);
        CHECK(terrain_clear(v->actor.x, v->actor.y));
        CHECK(moved(x0, y0, v) <= 1);
    }
    CHECK(v->state & NPC_TARGET_FAILED);
    CHECK(frames < 1500);
    CHECK(v->behavior == NPC_STAND);
}

// Solid all around, the villager has no way past. It holds its place and its target fails.
static void test_boxed_in_npc_gives_up_in_place() {
    game_new();
    Npc* v = npc_find(NPC_VILLAGER);
    CHECK(v);
    CHECK(npc_move_to(NPC_VILLAGER, villager_x(40), villager_y(0)));
    uint16_t sx = v->actor.x, sy = v->actor.y;
    unsigned frames = 0;
    for(; frames < 800 && !(v->state & NPC_TARGET_FAILED); ++frames) {
        update_visible_world(0, false);
        memset(game.cache.tiles, TILE_TREE, sizeof(game.cache.tiles));
        npc_update();
        ++game.ticks;
        CHECK(npc_find(NPC_VILLAGER) == v);
        CHECK(v->actor.x == sx && v->actor.y == sy);
    }
    CHECK(v->state & NPC_TARGET_FAILED);
}

// A wanderer walking west meets a tree inside its leash. It pauses, then goes under the tree and
// keeps to its leash. It never overlaps the tree and never leaves the leash.
static void test_wanderer_detours_under_tree_within_leash() {
    game_new();
    Npc* w = npc_find(NPC_WANDERER);
    CHECK(w && w->actor.x == positions::WandererHome.x * TILE &&
          w->actor.y == positions::WandererHome.y * TILE);
    w->state |= NPC_WALK_PHASE;
    w->timer = 400;
    w->actor.facing = WEST;
    uint16_t min_x = 65535;
    bool went_south = false;
    for(unsigned frame = 0; frame < 200; ++frame) {
        uint16_t x0 = w->actor.x, y0 = w->actor.y;
        // The tree sits one tile west of home, inside the leash.
        frame_with_tile(positions::WandererHome.x - 1, positions::WandererHome.y, TILE_TREE);
        CHECK(npc_find(NPC_WANDERER) == w);
        CHECK(terrain_clear(w->actor.x, w->actor.y));
        CHECK(moved(x0, y0, w) <= 1);
        if(w->actor.x < min_x)
            min_x = w->actor.x;
        if(w->actor.y >= wanderer_y(8))
            went_south = true;
    }
    CHECK(went_south);
    CHECK(min_x == wanderer_x(-16)); // The leash edge, reached on the far side of the tree.
}

int main() {
    test_villager_detours_around_tree();
    test_target_inside_obstacle_fails_within_budget();
    test_boxed_in_npc_gives_up_in_place();
    test_wanderer_detours_under_tree_within_leash();
    printf("npc detour checks passed: %u\n", checks);
    return 0;
}
