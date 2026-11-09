#include "rpg.h"
#include "chunk.h"
#include "world_assets.h"
#include "map_positions.h"
#include "npc.h"
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

bool dialogue_show(char const PROGMEM*) {
    return true;
}

bool dialogue_ask(char const PROGMEM*) {
    return true;
}

#include "world_scenarios.h"

static uint8_t authored_tile(Area const& area, unsigned x, unsigned y) {
    if(x >= area.width || y >= area.height)
        return TILE_OUTSIDE;
    return area.tiles[y * area.width + x];
}

static void window_edges() {
    static uint8_t tiles[(unsigned)MAX_MAP_WIDTH * MAX_MAP_HEIGHT];
    for(unsigned i = 0; i < sizeof(tiles); ++i)
        tiles[i] = (uint8_t)((i * 17 + i / 32) % TILE_OUTSIDE);
    unsigned widths[] = {0, 1, 7, 8, 9, 15, 16, 17, MAX_MAP_WIDTH};
    unsigned heights[] = {0, 1, 3, 4, 5, 7, 8, 9, MAX_MAP_HEIGHT};
    uint16_t cameras[] = {0, 1, 15, 16, 31, 47, 65535};
    for(unsigned width : widths) {
        for(unsigned height : heights) {
            Area area{(uint8_t)width,
                      (uint8_t)height,
                      (uint8_t)((width + 7) / 8),
                      (uint8_t)((height + 3) / 4),
                      tiles,
                      {0, 0}};
            unsigned max_x = width > DISPLAY_TILE_WIDTH ? (width - DISPLAY_TILE_WIDTH) * TILE : 0;
            unsigned max_y =
                height > DISPLAY_TILE_HEIGHT ? (height - DISPLAY_TILE_HEIGHT) * TILE : 0;
            for(uint16_t y : cameras) {
                for(uint16_t x : cameras) {
                    TileCache cache{};
                    reload_tile_window(cache, tiles, {area.width, area.height}, {x, y});
                    unsigned vx = (x < max_x ? x : max_x) / TILE;
                    unsigned vy = (y < max_y ? y : max_y) / TILE;
                    unsigned tx = vx >= CACHE_MARGIN ? vx - CACHE_MARGIN : 0;
                    unsigned ty = vy >= CACHE_MARGIN ? vy - CACHE_MARGIN : 0;
                    CHECK(cache.origin_tx == tx && cache.origin_ty == ty);
                    for(unsigned cy = 0; cy < CACHE_HEIGHT; ++cy)
                        for(unsigned cx = 0; cx < CACHE_WIDTH; ++cx)
                            CHECK(cache.tiles[cy * CACHE_WIDTH + cx] ==
                                  authored_tile(area, tx + cx, ty + cy));
                }
            }
        }
    }
}

static void terrain_footprints() {
    game_new();
    game.cache.origin_tx = 3;
    game.cache.origin_ty = 2;
    // Every arrangement of four solid/passable tiles, at every pixel alignment.
    // The oracle checks each foot pixel independently of the corner algorithm.
    for(unsigned mask = 0; mask < 16; ++mask) {
        memset(game.cache.tiles, TILE_GRASS_06, sizeof(game.cache.tiles));
        for(unsigned tile = 0; tile < 4; ++tile)
            game.cache.tiles[(2 + tile / 2) * CACHE_WIDTH + 3 + tile % 2] =
                mask & (1u << tile) ? TILE_TREE : TILE_GRASS_06;
        for(unsigned sy = 0; sy < TILE; ++sy) {
            for(unsigned sx = 0; sx < TILE; ++sx) {
                uint16_t x = (uint16_t)(6 * TILE + sx - FOOT_LEFT);
                uint16_t y = (uint16_t)(4 * TILE + sy - FOOT_TOP);
                bool expected = true;
                for(unsigned fy = FOOT_TOP; fy <= FOOT_BOTTOM; ++fy) {
                    for(unsigned fx = FOOT_LEFT; fx <= FOOT_RIGHT; ++fx) {
                        unsigned tile =
                            (((y + fy) >> TILE_SHIFT) - 4) * 2 + ((x + fx) >> TILE_SHIFT) - 6;
                        if(mask & (1u << tile))
                            expected = false;
                    }
                }
                CHECK(terrain_clear(x, y) == expected);
            }
        }
    }
}

// Pixel p is solid when its center falls in a solid triangle. Oracle: each foot pixel checked
// against its own tile mask, without the block loop.
static bool oracle_clear(uint16_t x, uint16_t y) {
    for(unsigned fy = FOOT_TOP; fy <= FOOT_BOTTOM; ++fy) {
        for(unsigned fx = FOOT_LEFT; fx <= FOOT_RIGHT; ++fx) {
            unsigned wx = x + fx, wy = y + fy;
            uint8_t tile = cached_tile({AREA_OUTDOOR, (uint8_t)(wx >> 4), (uint8_t)(wy >> 4)});
            uint16_t mask = tile_collision(tile);
            if(mask == 0xFFFF || (mask && collision_pixel_solid(mask, wx & 15, wy & 15)))
                return false;
        }
    }
    return true;
}

// NPCs treat any collision as a solid tile.
static bool oracle_any_solid(uint16_t x, uint16_t y) {
    for(unsigned fy = FOOT_TOP; fy <= FOOT_BOTTOM; ++fy) {
        for(unsigned fx = FOOT_LEFT; fx <= FOOT_RIGHT; ++fx) {
            unsigned wx = x + fx, wy = y + fy;
            if(tile_solid(cached_tile({AREA_OUTDOOR, (uint8_t)(wx >> 4), (uint8_t)(wy >> 4)})))
                return false;
        }
    }
    return true;
}

static void collision_triangles() {
    // Each pixel lies in one triangle, or on the shared edge of two on a quadrant diagonal.
    for(unsigned y = 0; y < TILE; ++y) {
        for(unsigned x = 0; x < TILE; ++x) {
            unsigned hits = 0;
            for(unsigned bit = 0; bit < 16; ++bit)
                hits += (unsigned)collision_pixel_solid(1u << bit, x, y);
            bool diagonal =
                (2 * (x & 7) - 7 == 2 * (y & 7) - 7) || (2 * (x & 7) - 7 == -(2 * (y & 7) - 7));
            CHECK(hits == (diagonal ? 2u : 1u));
            CHECK(collision_pixel_solid(0xFFFF, x, y));
            CHECK(!collision_pixel_solid(0, x, y));
            // Top half is the top two quadrants; bottom-right corner is one whole quadrant.
            CHECK(collision_pixel_solid(0x00FF, x, y) == (y < 8));
            CHECK(collision_pixel_solid(0xF000, x, y) == (x >= 8 && y >= 8));
        }
    }
    // Quadrant bits: top-left north triangle contains the top-left edge center.
    CHECK(collision_pixel_solid(0x0001, 4, 0) && !collision_pixel_solid(0x0001, 0, 4));
    CHECK(collision_pixel_solid(0x0010, 12, 0) && !collision_pixel_solid(0x0010, 15, 4));
    CHECK(collision_pixel_solid(0x0600, 0, 15) && collision_pixel_solid(0x0600, 7, 12));
    CHECK(!collision_pixel_solid(0x0600, 0, 12) && !collision_pixel_solid(0x0600, 3, 8));
}

static void player_shapes() {
    game_new();
    constexpr uint8_t SLOPE_TILE = TILE_ATLAS_25;
    // A 45 degree slope in the grass: solid bottom-right, half of the bottom-left quadrant.
    constexpr uint16_t SLOPE = 0xF600;
    set_tile({AREA_OUTDOOR, 9, 2}, SLOPE_TILE);
    CHECK(cached_tile({AREA_OUTDOOR, 9, 2}) == SLOPE_TILE);
    tile_collision_override(SLOPE_TILE, SLOPE);

    // Player feet test each pixel. NPCs still treat the whole tile as solid.
    for(unsigned y = 20; y <= 40; ++y) {
        for(unsigned x = 120; x <= 150; ++x) {
            CHECK(player_terrain_clear(x, y) == oracle_clear(x, y));
            CHECK(terrain_clear(x, y) == oracle_any_solid(x, y));
        }
    }

    // Walking east into the slope slides up one pixel per step, along the edge.
    game.player.x = 132;
    game.player.y = 32;
    static uint16_t const UP_SLIDE[][2] = {{133, 31}, {134, 30}, {135, 29}, {136, 28}};
    for(auto const& p : UP_SLIDE) {
        CHECK(actor_move(&game.player, EAST, 1));
        CHECK(game.player.x == p[0] && game.player.y == p[1]);
    }
    CHECK(player_terrain_clear(game.player.x, game.player.y));
    // The climb continues across the tile without stalling on any step.
    for(unsigned step = 0; step < 36; ++step) {
        uint16_t x = game.player.x;
        CHECK(actor_move(&game.player, EAST, 1) && game.player.x == x + 1);
    }
    CHECK(game.player.y < 28 && player_terrain_clear(game.player.x, game.player.y));

    // The mirrored slope slides down, and every step moves east with at most one pixel down.
    tile_collision_override(SLOPE_TILE, 0x30F3);
    game.player.x = 132;
    game.player.y = 32;
    int slides = 0;
    for(unsigned step = 0; step < 16; ++step) {
        uint16_t x = game.player.x, y = game.player.y;
        CHECK(actor_move(&game.player, EAST, 1));
        CHECK(game.player.x == x + 1 && (game.player.y == y || game.player.y == y + 1));
        slides += game.player.y == y + 1;
    }
    CHECK(slides > 0 && player_terrain_clear(game.player.x, game.player.y));

    // Vertical steps slide sideways along the same slope.
    tile_collision_override(SLOPE_TILE, SLOPE);
    game.player.x = 140;
    game.player.y = 23;
    slides = 0;
    for(unsigned step = 0; step < 16; ++step) {
        uint16_t x = game.player.x, y = game.player.y;
        CHECK(actor_move(&game.player, SOUTH, 1));
        CHECK(game.player.y == y + 1 && (game.player.x == x || game.player.x == x - 1));
        slides += game.player.x == x - 1;
    }
    CHECK(slides > 0 && player_terrain_clear(game.player.x, game.player.y));

    // A full overlap of the foot box never slides.
    tile_collision_override(SLOPE_TILE, 0xFFFF);
    game.player.x = 132;
    game.player.y = 32;
    CHECK(!actor_move(&game.player, EAST, 1));
    CHECK(game.player.x == 132 && game.player.y == 32);
    tile_collision_reset();
}

static void corner_assist() {
    game_new();
    // The wall sits in tile row 4, so the cache window (rows 2 to 8) covers both sides of it.
    constexpr uint8_t WALL_TILE = TILE_ATLAS_25;
    set_tile({AREA_OUTDOOR, 9, 4}, WALL_TILE);
    tile_collision_override(WALL_TILE, 0xFFFF);

    // Two pixels of the foot box below the wall's top edge: two sidesteps up, then the step.
    game.player.x = 132;
    game.player.y = 50;
    CHECK(actor_move(&game.player, EAST, 2));
    CHECK(game.player.x == 133 && game.player.y == 48);

    // One pixel of walking near the corner is one pixel of sidestep, with no jump and
    // the facing unchanged.
    game.player.x = 132;
    game.player.y = 50;
    CHECK(actor_move(&game.player, EAST, 1));
    CHECK(game.player.x == 132 && game.player.y == 49 && game.player.facing == EAST);

    // Two pixels above the wall's bottom edge: the sidesteps go down instead.
    game.player.x = 132;
    game.player.y = 70;
    CHECK(actor_move(&game.player, EAST, 2));
    CHECK(game.player.x == 133 && game.player.y == 72);

    // One pixel more overlap than the assist can clear, so the step stays blocked. The wall's
    // top edge is at y = 64, so the foot box overlaps (y - 48) rows.
    uint16_t deep = 48 + CORNER_ASSIST_PIXELS + 1;
    game.player.x = 132;
    game.player.y = deep;
    CHECK(!actor_move(&game.player, EAST, 1));
    CHECK(game.player.x == 132 && game.player.y == deep);
    tile_collision_reset();
}

// Blocked for the player at one world pixel: outside the map, or solid in its tile's mask.
static bool oracle_pixel_blocked(uint16_t wx, uint16_t wy) {
    if(!game.dimensions.pixel_bounds(wx, wy))
        return true;
    uint16_t mask =
        tile_collision(cached_tile({AREA_OUTDOOR, (uint8_t)(wx >> 4), (uint8_t)(wy >> 4)}));
    if(mask == 0xFFFF)
        return true;
    return mask != 0 && collision_pixel_solid(mask, wx & 15, wy & 15);
}

// Random partial masks: the foot-box test and the leading-edge probe agree with per-pixel
// oracles, so the table-driven tests cannot drift from the triangle rule.
static void random_masks() {
    game_new();
    constexpr uint8_t SHAPE = TILE_ATLAS_25;
    set_tile({AREA_OUTDOOR, 9, 2}, SHAPE);
    uint32_t seed = 2026;
    for(unsigned trial = 0; trial < 200; ++trial) {
        seed = seed * 1664525u + 1013904223u;
        tile_collision_override(SHAPE, (uint16_t)(seed >> 16));
        for(unsigned y = 20; y <= 40; ++y) {
            for(unsigned x = 128; x <= 152; ++x) {
                CHECK(player_terrain_clear(x, y) == oracle_clear(x, y));
                for(DIRECTION dir = NORTH; dir <= WEST; dir = (DIRECTION)(dir + 1)) {
                    bool horizontal = dir == EAST || dir == WEST;
                    uint8_t expected = 0;
                    for(unsigned i = 0; i < 8; ++i) {
                        unsigned wx = horizontal ? x + (dir == EAST ? FOOT_RIGHT : FOOT_LEFT)
                                                 : x + FOOT_LEFT + i;
                        unsigned wy = horizontal ? y + FOOT_TOP + i
                                                 : y + (dir == SOUTH ? FOOT_BOTTOM : FOOT_TOP);
                        if(oracle_pixel_blocked((uint16_t)wx, (uint16_t)wy))
                            expected = (uint8_t)(expected | (1u << i));
                    }
                    CHECK(player_leading_edge((uint16_t)x, (uint16_t)y, dir) == expected);
                }
            }
        }
    }
    tile_collision_reset();
}

// Tile edits last only until the next frame's reload. An edit that nothing reapplies resets
// by itself, even when the camera does not move.
static void tile_edits_reset_each_frame() {
    game_new();
    TilePos pos = {AREA_OUTDOOR, 4, 4}; // Grass inside the cache window.
    uint8_t authored = cached_tile(pos);
    uint8_t origin_x = game.cache.origin_tx, origin_y = game.cache.origin_ty;
    CHECK(authored != TILE_TREE);
    set_tile(pos, TILE_TREE);
    CHECK(cached_tile(pos) == TILE_TREE);
    update_visible_world(0, false);
    CHECK(cached_tile(pos) == authored);
    CHECK(game.cache.origin_tx == origin_x && game.cache.origin_ty == origin_y);
    // With no edit, the cache keeps matching the map frame after frame.
    update_visible_world(0, false);
    CHECK(cached_tile(pos) == authored);
}

int main() {
    run_world_scenarios();
    window_edges();
    terrain_footprints();
    collision_triangles();
    player_shapes();
    corner_assist();
    random_masks();
    tile_edits_reset_each_frame();
    game_new();
    Area const& area = *world_area(AREA_OUTDOOR);
    for(unsigned y = 0; y <= 128; y += 3) {
        for(unsigned x = 0; x <= 160; x += 5) {
            reload_tile_window(game.cache, area.tiles, {area.width, area.height},
                               {(uint16_t)x, (uint16_t)y});
            unsigned ox = x / TILE >= CACHE_MARGIN ? x / TILE - CACHE_MARGIN : 0;
            unsigned oy = y / TILE >= CACHE_MARGIN ? y / TILE - CACHE_MARGIN : 0;
            CHECK(game.cache.origin_tx == ox && game.cache.origin_ty == oy);
            for(unsigned cy = 0; cy < CACHE_HEIGHT; ++cy)
                for(unsigned cx = 0; cx < CACHE_WIDTH; ++cx)
                    CHECK(game.cache.tiles[cy * CACHE_WIDTH + cx] ==
                          authored_tile(area, ox + cx, oy + cy));
        }
    }
    // The 11x7 window intersects at most nine 8x4 chunks, in row-major order.
    world_enter(AREA_OUTDOOR, 7 * TILE, 5 * TILE, SOUTH);
    HandlerStage stages[] = {STAGE_GAMEPLAY, STAGE_TILES, STAGE_DIALOGUE};
    for(HandlerStage stage : stages) {
        CHECK(chunk_trace_count[stage] == 6);
        CHECK(chunk_trace[stage][0] == 0 && chunk_trace[stage][1] == 1);
        CHECK(chunk_trace[stage][2] == 3 && chunk_trace[stage][3] == 4);
        CHECK(chunk_trace[stage][4] == 6 && chunk_trace[stage][5] == 7);
    }
    CHECK(!terrain_clear(game.camera.x + CACHE_WIDTH * TILE, game.player.y));
    CHECK(cached_tile({AREA_OUTDOOR, 0, 0}) == TILE_OUTSIDE);
    printf("PASS chunk/cache world: %u checks\n", checks);
}
