#include "rpg.h"
#include "chunk.h"
#include "npc.h"
#include "tile_ids.h"
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

// Two dynamic NPCs with their own logic, so casting is exercised without the content NPCs.
// This file replaces the logic library entirely.
NpcDefinition const PROGMEM* npc_definition(NPC_ID id) {
    static constexpr NpcDefinition PROGMEM ONE{NPC_STAND, SOUTH, 1, 0, 0, 0, 0, 3, 0};
    static constexpr NpcDefinition PROGMEM TWO{NPC_STAND, SOUTH, 1, 0, 0, 0, 0, 2, 0};
    return id == NPC_TESSA ? &ONE : id == NPC_ORRIN ? &TWO : nullptr;
}

static unsigned activations;

void npc_on_activate(Npc&) {
    ++activations;
}

static unsigned target_events;

void npc_on_event(Npc&, NPC_EVENT event) {
    if(event == NPC_EVENT_TARGET_REACHED)
        ++target_events;
}
void npc_on_retire(Npc const&, NPC_RETIRE_REASON) {}
void npc_logic_restore() {}
void npc_talk(NPC_ID) {}
void npc_dialogue() {}

// The sign handler in the chunk dispatch references the overlay; these tests never open it.
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

static void place_player(uint8_t tx, uint8_t ty) {
    game.player.x = (uint16_t)tx * TILE;
    game.player.y = (uint16_t)ty * TILE;
    game.player.walking = 0;
}

// Entry casts a resident at its home when the home is cached. The villager's home lies outside
// the start cache, so it is not cast until the cache covers it.
static void test_entry_casts_residents() {
    game_new();
    Npc* wanderer = npc_find(NPC_ORRIN);
    CHECK(wanderer);
    CHECK(wanderer->actor.x == positions::OrrinLane.x * TILE &&
          wanderer->actor.y == positions::OrrinLane.y * TILE);
    CHECK(!npc_find(NPC_TESSA));
}

// After entry, a spot in view is never cast into, and a home beyond the cache stays empty. A cached
// spot out of view also casts once it is clear. The camera follows the player, and the map's border
// keeps it from clamping at a corner, so no interior spot is cached yet off screen. That case is
// left to the corner-free scenarios.
static void test_casts_wait_for_spots_out_of_view() {
    game_new();
    game.set_flag(FLAG_BRAM_ACCOUNT);
    place_player(positions::TessaWell.x - 1, positions::TessaWell.y);
    update_visible_world(0, false); // Refreshes the cache for this view before the edits below.
    npc_reset();
    memset(cache.tiles, TILE_TREE, sizeof(cache.tiles));
    note_tile_edit();
    npc_stage_begin();
    process_all_chunks<STAGE_NPCS>();
    npc_stage_end(); // Entry finds no clear spot.
    CHECK(!npc_find(NPC_TESSA) && !npc_find(NPC_ORRIN));
    memset(cache.tiles, TILE_GRASS_06, sizeof(cache.tiles));
    note_tile_edit();
    npc_stage_begin();
    process_all_chunks<STAGE_NPCS>();
    npc_stage_end(); // The villager's home is in view, so it waits. The wanderer's is not cached.
    CHECK(!npc_find(NPC_TESSA) && !npc_find(NPC_ORRIN));
}

// Real world refresh, NPC simulation, then player movement: admission must happen
// before the next draw, even at every fractional camera phase and chunk crossing.
static void test_four_direction_approaches() {
    for(unsigned dir = 0; dir < 4; ++dir)
        for(unsigned phase = 0; phase < TILE; ++phase) {
            game_new();
            for(unsigned tile = 0; tile < 256; ++tile)
                tile_collision_override((uint8_t)tile, 0);
            npc_reset();
            game.npcs.entering = 0;
            int x = positions::OrrinLane.x * TILE;
            int y = positions::OrrinLane.y * TILE;
            int dx = dir == EAST ? -1 : dir == WEST ? 1 : 0;
            int dy = dir == SOUTH ? -1 : dir == NORTH ? 1 : 0;
            int distance = (dx ? 160 : 112) + phase;
            game.player.x = (uint16_t)(x + dx * distance + (dx ? 0 : 8));
            game.player.y = (uint16_t)(y + dy * distance + (dy ? 0 : 8));
            bool activated = false, seen = false;
            for(int frame = 0; frame < distance - 32; ++frame) {
                bool before = npc_find(NPC_ORRIN) != nullptr;
                update_visible_world(0, false);
                Npc* n = npc_find(NPC_ORRIN);
                if(n && !before) {
                    CHECK(!activated);
                    activated = true;
                    Camera c = game_camera();
                    CHECK(!(x < c.x + 129 && x + TILE > (int)c.x - 1 &&
                            y < c.y + 65 && y + TILE > (int)c.y - 1));
                }
                if(activated)
                    CHECK(n);
                npc_update();
                npc_player_begin((DIRECTION)dir);
                actor_move(&game.player, (DIRECTION)dir, PLAYER_PIXELS_PER_FRAME);
                Camera c = game_camera();
                bool visible = x < c.x + 128 && x + TILE > c.x &&
                               y < c.y + 64 && y + TILE > c.y;
                if(visible) {
                    CHECK(n);
                    seen = true;
                }
                ++game.ticks;
            }
            CHECK(activated && seen);
            tile_collision_reset();
        }
}

static void test_exact_clearance_and_clipped_retirement() {
    for(unsigned edge = 0; edge < 4; ++edge)
        for(int offset = -2; offset <= 2; ++offset) {
            game_new();
            npc_reset();
            game.npcs.entering = 0;
            TilePos site = positions::OrrinLane;
            int x = site.x * TILE, y = site.y * TILE;
            int cx = x - 56, cy = y - 24;
            if(edge == 0)
                cx = x - 129 + offset;
            if(edge == 1)
                cx = x + TILE + 1 + offset;
            if(edge == 2)
                cy = y - 65 + offset;
            if(edge == 3)
                cy = y + TILE + 1 + offset;
            game.player.x = cx + 56;
            game.player.y = cy + 24;
            cache.origin_tx = site.x - 1;
            cache.origin_ty = site.y - 1;
            memset(cache.tiles, TILE_GRASS_06, sizeof(cache.tiles));
            note_tile_edit();
            npc_stage_begin();
            npc_offer_candidate(NPC_ORRIN, site);
            npc_stage_end();
            bool guarded = x < cx + 129 && x + TILE > cx - 1 &&
                           y < cy + 65 && y + TILE > cy - 1;
            CHECK((npc_find(NPC_ORRIN) != nullptr) == !guarded);
        }
    game_new();
    Npc* n = npc_find(NPC_ORRIN);
    CHECK(n);
    Camera c = game_camera();
    n->actor.x = c.x + 127;
    n->actor.y = c.y + 20;
    n->behavior = NPC_STAND;
    // Only the leftmost sprite column remains onscreen; its feet are uncached.
    cache.origin_tx = (n->actor.x + FOOT_RIGHT) / TILE + 1;
    npc_stage_begin();
    CHECK(npc_find(NPC_ORRIN));
    npc_stage_end();
    // Clamping must not wrap the protected rectangle at the top/left map edge.
    npc_reset();
    game.player.x = game.player.y = 0;
    CHECK(game_camera().x == 0 && game_camera().y == 0);
    camera = game_camera();
    CHECK(npc_visible(0, 0));
    CHECK(!npc_visible(128, 0));
    CHECK(!npc_visible(0, 64));
    game.player.x = game.dimensions.width_tiles * TILE - TILE;
    game.player.y = game.dimensions.height_tiles * TILE - TILE;
    c = game_camera();
    CHECK(c.x == game.dimensions.width_tiles * TILE - 128);
    CHECK(c.y == game.dimensions.height_tiles * TILE - 64);
}

// Frame 180's lifecycle case: feet cross the top cache boundary, but the
// logical home remains fully cached and offscreen. No retirement/recasting.
static void test_uncached_wanderer_retains_local_home() {
    game_new();
    TilePos home = positions::OrrinLane;
    game.player.x = home.x * TILE + 4 * TILE;
    game.player.y = home.y * TILE + 3 * TILE;
    update_visible_world(0, false);
    Npc* n = npc_find(NPC_ORRIN);
    CHECK(n);
    n->actor.x = home.x * TILE - TILE;
    n->actor.y = home.y * TILE - 12;
    n->behavior = NPC_WANDER;
    n->timer = 7;
    n->detour = 0x31;
    n->state = NPC_WALK_PHASE;
    Npc saved = *n;
    unsigned before = activations;
    for(unsigned frame = 0; frame < 4; ++frame) {
        update_visible_world(0, false);
        CHECK(npc_find(NPC_ORRIN) == n);
        npc_update();
        CHECK(memcmp(n, &saved, sizeof(saved)) == 0);
        CHECK(activations == before);
        ++game.ticks;
    }
    // Scripted travel is not retained by the home rule.
    n->behavior = NPC_TRAVEL;
    unsigned events_before = target_events;
    npc_stage_begin();
    CHECK(!npc_find(NPC_ORRIN));
    CHECK(target_events == events_before + 1);
    npc_stage_end();
    // Reenter, then move far enough that actor and home both leave the cache.
    game_new();
    game.player.x = home.x * TILE + 4 * TILE;
    game.player.y = home.y * TILE + 3 * TILE;
    update_visible_world(0, false);
    n = npc_find(NPC_ORRIN);
    CHECK(n);
    n->actor.x = home.x * TILE - TILE;
    n->actor.y = home.y * TILE - 12;
    n->behavior = NPC_WANDER;
    game.player.x += CACHE_WIDTH * TILE;
    game.player.y += CACHE_HEIGHT * TILE;
    update_visible_world(0, false);
    CHECK(!npc_find(NPC_ORRIN));
}

int main() {
    test_uncached_wanderer_retains_local_home();
    test_exact_clearance_and_clipped_retirement();
    test_four_direction_approaches();
    test_entry_casts_residents();
    test_casts_wait_for_spots_out_of_view();
    printf("npc index checks passed: %u\n", checks);
    return 0;
}
