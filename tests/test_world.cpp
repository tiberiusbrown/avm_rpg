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

// Synthetic scenarios paint their window at the map point TestField. Positions in them are pixel
// offsets from that point's top-left corner, so no absolute map coordinates appear here.
static uint16_t field_x(unsigned dx) {
    return (uint16_t)(positions::TestField.x * TILE + dx);
}

static uint16_t field_y(unsigned dy) {
    return (uint16_t)(positions::TestField.y * TILE + dy);
}

static TilePos field_tile(unsigned dx, unsigned dy) {
    return TilePos{AREA_OUTDOOR, (uint8_t)(positions::TestField.x + dx),
                   (uint8_t)(positions::TestField.y + dy)};
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
    game.cache.origin_tx = positions::TestField.x;
    game.cache.origin_ty = positions::TestField.y;
    // Every arrangement of four solid/passable tiles, at every pixel alignment.
    // The oracle checks each foot pixel independently of the corner algorithm.
    for(unsigned mask = 0; mask < 16; ++mask) {
        memset(game.cache.tiles, TILE_GRASS_06, sizeof(game.cache.tiles));
        for(unsigned tile = 0; tile < 4; ++tile)
            game.cache.tiles[(2 + tile / 2) * CACHE_WIDTH + 3 + tile % 2] =
                mask & (1u << tile) ? TILE_TREE : TILE_GRASS_06;
        for(unsigned sy = 0; sy < TILE; ++sy) {
            for(unsigned sx = 0; sx < TILE; ++sx) {
                uint16_t x = field_x(3 * TILE + sx - FOOT_LEFT);
                uint16_t y = field_y(2 * TILE + sy - FOOT_TOP);
                bool expected = true;
                for(unsigned fy = FOOT_TOP; fy <= FOOT_BOTTOM; ++fy) {
                    for(unsigned fx = FOOT_LEFT; fx <= FOOT_RIGHT; ++fx) {
                        unsigned tile = (((y + fy) >> TILE_SHIFT) - positions::TestField.y - 2) * 2 +
                                        ((x + fx) >> TILE_SHIFT) - positions::TestField.x - 3;
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
    // Paint an open window at TestField, so the scenario does not depend on the map around it.
    game.cache.origin_tx = positions::TestField.x;
    game.cache.origin_ty = positions::TestField.y;
    memset(game.cache.tiles, TILE_GRASS_06, sizeof(game.cache.tiles));
    constexpr uint8_t SLOPE_TILE = TILE_ATLAS_25;
    // A 45 degree slope in the grass: solid bottom-right, half of the bottom-left quadrant.
    constexpr uint16_t SLOPE = 0xF600;
    set_tile(field_tile(7, 2), SLOPE_TILE);
    CHECK(cached_tile(field_tile(7, 2)) == SLOPE_TILE);
    tile_collision_override(SLOPE_TILE, SLOPE);

    // Player feet test each pixel. NPCs still treat the whole tile as solid.
    for(unsigned y = 20; y <= 40; ++y) {
        for(unsigned x = 88; x <= 118; ++x) {
            CHECK(player_terrain_clear(field_x(x), field_y(y)) == oracle_clear(field_x(x), field_y(y)));
            CHECK(terrain_clear(field_x(x), field_y(y)) == oracle_any_solid(field_x(x), field_y(y)));
        }
    }

    // Walking east into the slope slides up one pixel per step, along the edge.
    game.player.x = field_x(100);
    game.player.y = field_y(32);
    static uint16_t const UP_SLIDE[][2] = {{101, 31}, {102, 30}, {103, 29}, {104, 28}};
    for(auto const& p : UP_SLIDE) {
        CHECK(actor_move(&game.player, EAST, 1));
        CHECK(game.player.x == field_x(p[0]) && game.player.y == field_y(p[1]));
    }
    CHECK(player_terrain_clear(game.player.x, game.player.y));
    // The climb continues across the tile without stalling on any step.
    for(unsigned step = 0; step < 36; ++step) {
        uint16_t x = game.player.x;
        CHECK(actor_move(&game.player, EAST, 1) && game.player.x == x + 1);
    }
    CHECK(game.player.y < field_y(28) && player_terrain_clear(game.player.x, game.player.y));

    // The mirrored slope slides down, and every step moves east with at most one pixel down.
    tile_collision_override(SLOPE_TILE, 0x30F3);
    game.player.x = field_x(100);
    game.player.y = field_y(32);
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
    game.player.x = field_x(108);
    game.player.y = field_y(23);
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
    game.player.x = field_x(100);
    game.player.y = field_y(32);
    CHECK(!actor_move(&game.player, EAST, 1));
    CHECK(game.player.x == field_x(100) && game.player.y == field_y(32));
    tile_collision_reset();
}

static void corner_assist() {
    game_new();
    // Paint an open window at TestField. The wall sits two tiles down, so the window covers both
    // sides of it.
    game.cache.origin_tx = positions::TestField.x;
    game.cache.origin_ty = positions::TestField.y;
    memset(game.cache.tiles, TILE_GRASS_06, sizeof(game.cache.tiles));
    constexpr uint8_t WALL_TILE = TILE_ATLAS_25;
    set_tile(field_tile(7, 2), WALL_TILE);
    tile_collision_override(WALL_TILE, 0xFFFF);

    // Two pixels of the foot box below the wall's top edge: two sidesteps up, then the step.
    game.player.x = field_x(100);
    game.player.y = field_y(18);
    CHECK(actor_move(&game.player, EAST, 2));
    CHECK(game.player.x == field_x(101) && game.player.y == field_y(16));

    // One pixel of walking near the corner is one pixel of sidestep, with no jump and
    // the facing unchanged.
    game.player.x = field_x(100);
    game.player.y = field_y(18);
    CHECK(actor_move(&game.player, EAST, 1));
    CHECK(game.player.x == field_x(100) && game.player.y == field_y(17) && game.player.facing == EAST);

    // Two pixels above the wall's bottom edge: the sidesteps go down instead.
    game.player.x = field_x(100);
    game.player.y = field_y(38);
    CHECK(actor_move(&game.player, EAST, 2));
    CHECK(game.player.x == field_x(101) && game.player.y == field_y(40));

    // One pixel more overlap than the assist can clear, so the step stays blocked. The wall's
    // top edge is 32 pixels into the window, so the foot box overlaps (y - field_y(16)) rows.
    uint16_t deep = field_y(16 + CORNER_ASSIST_PIXELS + 1);
    game.player.x = field_x(100);
    game.player.y = deep;
    CHECK(!actor_move(&game.player, EAST, 1));
    CHECK(game.player.x == field_x(100) && game.player.y == deep);
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
    // Paint an open window at TestField, with the shaped tile inside the sampled pixels below.
    game.cache.origin_tx = positions::TestField.x;
    game.cache.origin_ty = positions::TestField.y;
    memset(game.cache.tiles, TILE_GRASS_06, sizeof(game.cache.tiles));
    constexpr uint8_t SHAPE = TILE_ATLAS_25;
    set_tile(field_tile(2, 1), SHAPE);
    uint32_t seed = 2026;
    for(unsigned trial = 0; trial < 200; ++trial) {
        seed = seed * 1664525u + 1013904223u;
        tile_collision_override(SHAPE, (uint16_t)(seed >> 16));
        for(unsigned ry = 4; ry <= 24; ++ry) {
            for(unsigned rx = 16; rx <= 40; ++rx) {
                unsigned x = field_x(rx), y = field_y(ry);
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
    // Open road just east of the spawn, inside the cache window.
    TilePos pos = {AREA_OUTDOOR, (uint8_t)(positions::PlayerStart.x + 1), positions::PlayerStart.y};
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

// Activating a door enters its house, and walking onto the exit returns to the door. Each
// area collides with its own tileset, so one tile ID means different tiles in each.
static void house_doors() {
    game_new();
    TilePos door = positions::ElderDoor, way_out = positions::ElderExit;
    CHECK(area_tileset(AREA_OUTDOOR) == TILESET_WORLD && area_tileset(AREA_INTERIORS) == TILESET_HOUSE);
    static_assert(TILE_BOULDER_TALL == HOUSE_TILE_EDGE_TOP_EXIT, "one ID, two tiles");

    // Stand in front of the door, facing it.
    world_enter(AREA_OUTDOOR, (uint16_t)(door.x * TILE), (uint16_t)((door.y + 1) * TILE), NORTH);
    CHECK(tile_solid(cached_tile(door)) && tile_collision(TILE_BOULDER_TALL) == 0xFFFF);
    game_update(0, 0);
    CHECK(game.area == AREA_OUTDOOR && game.can_interact);

    game_update(0, BTN_A);
    CHECK(game.area == AREA_INTERIORS);
    CHECK(game.dimensions.width_tiles == INTERIORS_WIDTH &&
          game.dimensions.height_tiles == INTERIORS_HEIGHT);
    CHECK(game.player.x == way_out.x * TILE && game.player.y == (way_out.y - 1) * TILE &&
          game.player.facing == NORTH);
    // The same ID is now the walkable exit, and house walls are solid.
    CHECK(cached_tile(way_out) == HOUSE_TILE_EDGE_TOP_EXIT);
    CHECK(tile_collision(HOUSE_TILE_EDGE_TOP_EXIT) == 0 && tile_solid(HOUSE_TILE_WALL) &&
          tile_solid(HOUSE_TILE_EMPTY) && !tile_solid(HOUSE_TILE_FLOOR_WOOD_A));
    CHECK(player_terrain_clear(game.player.x, game.player.y));

    // Standing still inside does nothing. Walking down onto the exit leaves, with no button.
    game_update(0, 0);
    CHECK(game.area == AREA_INTERIORS);
    for(unsigned i = 0; i < 2 * TILE && game.area == AREA_INTERIORS; ++i)
        game_update(BTN_DOWN, 0);
    CHECK(game.area == AREA_OUTDOOR);
    CHECK(game.player.x == door.x * TILE && game.player.y == (door.y + 1) * TILE &&
          game.player.facing == SOUTH);
    CHECK(tile_collision(TILE_BOULDER_TALL) == 0xFFFF);
    // The player arrives off the door, so nothing fires again.
    game_update(0, 0);
    CHECK(game.area == AREA_OUTDOOR);
}

int main() {
    run_world_scenarios();
    house_doors();
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
    // The 11x7 window intersects at most nine 8x4 chunks. The trace records each chunk's index in
    // the map's chunk grid, in row-major order.
    world_enter(AREA_OUTDOOR, positions::TestField.x * TILE, positions::TestField.y * TILE, SOUTH);
    Area const& map = *world_area(AREA_OUTDOOR);
    Camera camera = game_camera();
    unsigned left = camera.x >> TILE_SHIFT, top = camera.y >> TILE_SHIFT;
    unsigned right = left + CACHE_WIDTH - 1, bottom = top + CACHE_HEIGHT - 1;
    if(right >= game.dimensions.width_tiles)
        right = game.dimensions.width_tiles - 1;
    if(bottom >= game.dimensions.height_tiles)
        bottom = game.dimensions.height_tiles - 1;
    uint16_t expected[CHUNK_TRACE_CAPACITY];
    unsigned chunks = 0;
    for(unsigned cy = top / CHUNK_HEIGHT; cy <= bottom / CHUNK_HEIGHT; ++cy)
        for(unsigned cx = left / CHUNK_WIDTH; cx <= right / CHUNK_WIDTH; ++cx)
            expected[chunks++] = (uint16_t)(cy * map.chunk_columns + cx);
    HandlerStage stages[] = {STAGE_GAMEPLAY, STAGE_TILES, STAGE_DIALOGUE};
    for(HandlerStage stage : stages) {
        CHECK(chunk_trace_count[stage] == chunks);
        for(unsigned i = 0; i < chunks; ++i)
            CHECK(chunk_trace[stage][i] == expected[i]);
    }
    CHECK(!terrain_clear(game.camera.x + CACHE_WIDTH * TILE, game.player.y));
    TilePos beyond{AREA_OUTDOOR, (uint8_t)((camera.x >> TILE_SHIFT) + CACHE_WIDTH),
                   (uint8_t)(camera.y >> TILE_SHIFT)};
    CHECK(cached_tile(beyond) == TILE_OUTSIDE); // Just past the window
    printf("PASS chunk/cache world: %u checks\n", checks);
}
