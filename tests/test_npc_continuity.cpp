#include "rpg.h"
#include "npc.h"
#include "chunk.h"
#include "logic/npc_logic.h"
#include "map_positions.h"
#include "tile_ids.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Property test: random play must never remove, jump, or visibly create an NPC. Every frame is
// checked against the invariants that npc.cpp is built to keep.

static unsigned checks, frames_run, moves, entries, escapes;

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

static void fail(char const* what, unsigned frame, unsigned id) {
    fprintf(stderr, "FAIL frame %u NPC %u: %s\n", frame, id, what);
    exit(1);
}

// The rectangles npc.cpp uses: what is in view, and the guard a cast must clear.
static bool in_view(uint16_t x, uint16_t y, Camera c) {
    return x < c.x + 128 && x + TILE > c.x && y < c.y + 64 && y + TILE > c.y;
}

static bool guarded(uint16_t x, uint16_t y, Camera c) {
    return x < c.x + 128 + TILE && x + 2 * TILE > c.x && y < c.y + 64 + TILE &&
           y + 2 * TILE > c.y;
}

struct Snapshot {
    bool present[NPC_ID_LIMIT];
    uint16_t x[NPC_ID_LIMIT], y[NPC_ID_LIMIT];
    bool clear[NPC_ID_LIMIT];
};

static Snapshot take() {
    Snapshot s{};
    for(uint8_t id = 1; id < NPC_ID_LIMIT; ++id)
        if(Npc* n = npc_find((NPC_ID)id)) {
            s.present[id] = true;
            s.x[id] = n->actor.x;
            s.y[id] = n->actor.y;
            s.clear[id] = terrain_clear(n->actor.x, n->actor.y);
        }
    return s;
}

static void check_frame(Snapshot const& before, Snapshot const& after, Camera cb, Camera ca,
                        unsigned frame) {
    bool camera_moved = cb.x != ca.x || cb.y != ca.y;
    for(uint8_t id = 1; id < NPC_ID_LIMIT; ++id) {
        if(before.present[id] && !after.present[id])
            fail("NPC removed while its scene lasts", frame, id);
        if(!before.present[id] && after.present[id]) {
            // A cast happens at the camera of the frame, before the player moves.
            if(guarded(after.x[id], after.y[id], cb))
                fail("NPC cast where it could be seen", frame, id);
            continue;
        }
        if(!before.present[id])
            continue;
        int dx = (int)after.x[id] - (int)before.x[id];
        int dy = (int)after.y[id] - (int)before.y[id];
        int jump = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
        if(jump > 1)
            fail("NPC moved more than one pixel in a frame", frame, id);
        bool moved = jump != 0;
        if(moved)
            ++moves;
        if(!before.clear[id] && moved)
            ++escapes;
        bool was_visible = in_view(before.x[id], before.y[id], cb);
        bool visible = in_view(after.x[id], after.y[id], ca);
        if(!was_visible && visible) {
            ++entries;
            // An NPC may come into view only by walking there or by the camera moving.
            if(!moved && !camera_moved)
                fail("NPC appeared in view without moving", frame, id);
        }
    }
}

static uint32_t rng_state = 1;
static uint32_t rng() {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

// Writes a tile into the terrain cache. The cache is rebuilt from the map every frame, so an edit
// reaches an NPC only when it is made after that frame's refresh and before the NPC step.
static void cache_tile(uint8_t tx, uint8_t ty, uint8_t tile) {
    int dx = (int)tx - (int)game.cache.origin_tx, dy = (int)ty - (int)game.cache.origin_ty;
    if(dx >= 0 && dy >= 0 && dx < CACHE_WIDTH && dy < CACHE_HEIGHT)
        game.cache.tiles[dy * CACHE_WIDTH + dx] = tile;
}

// The direction of held buttons, as game.cpp maps them: vertical input wins over horizontal.
static DIRECTION held_direction(uint8_t held) {
    bool vup = (held & BTN_UP) != 0, vdown = (held & BTN_DOWN) != 0;
    bool left = (held & BTN_LEFT) != 0, right = (held & BTN_RIGHT) != 0;
    if(vup != vdown)
        return vup ? NORTH : SOUTH;
    if(left != right)
        return right ? EAST : WEST;
    return STILL;
}

// One frame, as game_update runs it, with a terrain edit placed under a random NPC in between the
// refresh and the NPC step.
static void edited_frame(uint8_t held) {
    update_visible_world(0, false);
    Npc* n = npc_find((NPC_ID)(1 + rng() % 2));
    if(n) {
        int tx = (int)((n->actor.x + FOOT_CENTER_X) >> TILE_SHIFT) + (int)(rng() % 3) - 1;
        int ty = (int)((n->actor.y + FOOT_CENTER_Y) >> TILE_SHIFT) + (int)(rng() % 3) - 1;
        if(tx >= 0 && ty >= 0)
            cache_tile((uint8_t)tx, (uint8_t)ty, rng() % 2 ? TILE_TREE : TILE_GRASS_06);
    }
    npc_update();
    npc_player_begin(held_direction(held));
    actor_move(&game.player, held_direction(held), PLAYER_PIXELS_PER_FRAME);
    ++game.ticks;
}

static void run(uint32_t seed, unsigned frames) {
    rng_state = seed;
    game_new();
    TilePos const residences[] = {positions::VillagerWest, positions::VillagerEast,
                                  positions::VillagerHome};
    uint8_t const moves_for[] = {BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT, BTN_UP | BTN_RIGHT,
                                 BTN_DOWN | BTN_LEFT, 0};
    uint8_t held = 0;
    for(unsigned frame = 0; frame < frames; ++frame) {
        // Player input and world events are chosen before the frame, as in real play.
        if(rng() % 8 == 0)
            held = moves_for[rng() % (sizeof(moves_for) / sizeof(moves_for[0]))];
        if(rng() % 120 == 0)
            npc_set_destination(NPC_VILLAGER, residences[rng() % 3]);
        if(rng() % 160 == 0)
            npc_move_to(NPC_WANDERER, (uint16_t)(144 + rng() % 33), (uint16_t)(84 + rng() % 25));
        Snapshot before = take();
        Camera camera_before = game_camera();
        if(rng() % 25 == 0)
            edited_frame(held);
        else
            game_update(held);
        ++frames_run;
        Snapshot after = take();
        check_frame(before, after, camera_before, game_camera(), frame);
        ++checks;
    }
}

// A villager standing on (5,5) is embedded in a 2x2 block of solid tiles. Its nearest clear spot is
// twelve pixels west, so it must walk out over many steps, and no step may jump.
static void test_tile_change_under_npc_walks_out() {
    game_new();
    Npc* villager = npc_find(NPC_VILLAGER);
    CHECK(villager && villager->actor.x == 5 * TILE && villager->actor.y == 5 * TILE);
    uint16_t start_x = villager->actor.x, start_y = villager->actor.y;
    for(unsigned frame = 0; frame < 120; ++frame) {
        Snapshot before = take();
        Camera camera_before = game_camera();
        update_visible_world(0, false);
        for(uint8_t ty = 5; ty <= 6; ++ty)
            for(uint8_t tx = 5; tx <= 6; ++tx)
                cache_tile(tx, ty, TILE_TREE);
        npc_update();
        ++game.ticks;
        ++frames_run;
        Snapshot after = take();
        check_frame(before, after, camera_before, game_camera(), frame);
        CHECK(npc_find(NPC_VILLAGER) == villager);
    }
    CHECK(terrain_clear(villager->actor.x, villager->actor.y));
    CHECK(start_x - villager->actor.x >= 8 && start_x - villager->actor.x <= 16 &&
          villager->actor.y == start_y);
}

int main() {
    for(uint32_t seed = 1; seed <= 4; ++seed)
        run(seed * 0x9e3779b9u, 4000);
    test_tile_change_under_npc_walks_out();
    // The run must exercise real motion, real escapes, and real appearances to mean anything.
    CHECK(moves > 1000 && escapes > 0 && entries > 0);
    printf("npc continuity: %u frames, %u moves, %u escape steps, %u view entries, %u checks\n",
           frames_run, moves, escapes, entries, checks);
    return 0;
}
