#include "equipment.h"
#include "rpg.h"
#include "chunk.h"
#include "world_assets.h"
#include "map_positions.h"
#include "npc.h"
#include "logic/handlers.h"
#include "music_catalog.h"
#include "story_stub.h"
#include <initializer_list>
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

bool dialogue_show(NPC_ID id, char const PROGMEM* words) {
    (void)id;
    return dialogue_show(words);
}

static bool accept_prompt = true;

bool dialogue_ask(char const PROGMEM*) {
    return accept_prompt;
}

bool dialogue_ask(NPC_ID id, char const PROGMEM* question) {
    (void)id;
    return dialogue_ask(question);
}

#include "world_scenarios.h"

// The sounds that the rules asked for since the last sfx_stub_reset, in order. An ambient request
// is one that the platform drops while another effect plays, so mechanisms are judged on the
// others: a trigger's sound and the sound of what it moves are one request, with its follow-up.
static unsigned played(bool ambient = false) {
    unsigned count = 0;
    for(unsigned i = 0; i < sfx_stub_count(); ++i)
        count += sfx_stub_record(i).ambient == ambient;
    return count;
}

static SfxRecord const& nth_played(unsigned n, bool ambient = false) {
    for(unsigned i = 0, seen = 0; i < sfx_stub_count(); ++i)
        if(sfx_stub_record(i).ambient == ambient && seen++ == n)
            return sfx_stub_record(i);
    fprintf(stderr, "FAIL no sound number %u\n", n);
    exit(1);
}

#define EXPECT_SOUND(effect_, then_)                                                               \
    do {                                                                                           \
        CHECK(played() == 1);                                                                      \
        CHECK(nth_played(0).effect == (effect_) && nth_played(0).then == (then_));                 \
    } while(0)

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
    cache.origin_tx = positions::TestField.x;
    cache.origin_ty = positions::TestField.y;
    // Every arrangement of four solid/passable tiles, at every pixel alignment.
    // The oracle checks each foot pixel independently of the corner algorithm.
    for(unsigned mask = 0; mask < 16; ++mask) {
        memset(cache.tiles, TILE_GRASS_06, sizeof(cache.tiles));
        for(unsigned tile = 0; tile < 4; ++tile)
            cache.tiles[(2 + tile / 2) * CACHE_WIDTH + 3 + tile % 2] =
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
    cache.origin_tx = positions::TestField.x;
    cache.origin_ty = positions::TestField.y;
    memset(cache.tiles, TILE_GRASS_06, sizeof(cache.tiles));
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
    cache.origin_tx = positions::TestField.x;
    cache.origin_ty = positions::TestField.y;
    memset(cache.tiles, TILE_GRASS_06, sizeof(cache.tiles));
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
    cache.origin_tx = positions::TestField.x;
    cache.origin_ty = positions::TestField.y;
    memset(cache.tiles, TILE_GRASS_06, sizeof(cache.tiles));
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

// Every prefix, suffix and full foot span against each individual triangle.
// Checking one bit at a time covers every table bit; combined masks use their union.
static void foot_triangle_windows() {
    game_new();
    cache.origin_tx = positions::TestField.x;
    cache.origin_ty = positions::TestField.y;
    memset(cache.tiles, TILE_GRASS_06, sizeof(cache.tiles));
    constexpr uint8_t SHAPE = TILE_ATLAS_25;
    set_tile(field_tile(2, 1), SHAPE);
    for(uint8_t bit = 0; bit < 16; ++bit) {
        tile_collision_override(SHAPE, (uint16_t)(1u << bit));
        for(int16_t y = 1 - FOOT_HEIGHT; y < TILE; ++y) {
            for(int16_t x = 1 - FOOT_WIDTH; x < TILE; ++x) {
                uint16_t px = field_x(2 * TILE + x - FOOT_LEFT);
                uint16_t py = field_y(TILE + y - FOOT_TOP);
                CHECK(player_terrain_clear(px, py) == oracle_clear(px, py));
            }
        }
    }
    tile_collision_reset();
}

static void leading_edge_boundaries() {
    game_new();
    cache.origin_tx = cache.origin_ty = 0;
    constexpr uint8_t SHAPE = TILE_ATLAS_25;
    memset(cache.tiles, SHAPE, sizeof(cache.tiles));
    constexpr MapDimensions DIMENSIONS[] = {{0, 0}, {1, 1}, {3, 2}, {2, 3}};
    constexpr uint16_t POINTS[] = {0,  1,  3,  4,  7,  8,     11,    12,    15,    16,    17,   23,
                                   24, 31, 32, 33, 63, 65520, 65527, 65528, 65531, 65532, 65535};
    uint32_t seed = 451;
    for(unsigned trial = 0; trial < 32; ++trial) {
        seed = seed * 1664525u + 1013904223u;
        tile_collision_override(SHAPE, (uint16_t)(seed >> 16));
        for(MapDimensions dimensions : DIMENSIONS) {
            game.dimensions = dimensions;
            for(uint16_t y : POINTS) {
                for(uint16_t x : POINTS) {
                    for(DIRECTION dir = NORTH; dir <= WEST; dir = (DIRECTION)(dir + 1)) {
                        bool horizontal = dir == EAST || dir == WEST;
                        uint8_t expected = 0;
                        for(unsigned i = 0; i < 8; ++i) {
                            uint16_t wx =
                                (uint16_t)(horizontal ? x + (dir == EAST ? FOOT_RIGHT : FOOT_LEFT)
                                                      : x + FOOT_LEFT + i);
                            uint16_t wy = (uint16_t)(horizontal ? y + FOOT_TOP + i
                                                                : y + (dir == SOUTH ? FOOT_BOTTOM
                                                                                    : FOOT_TOP));
                            if(oracle_pixel_blocked(wx, wy))
                                expected |= (uint8_t)(1u << i);
                        }
                        CHECK(player_leading_edge(x, y, dir) == expected);
                    }
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
    uint8_t origin_x = cache.origin_tx, origin_y = cache.origin_ty;
    CHECK(authored != TILE_TREE);
    set_tile(pos, TILE_TREE);
    CHECK(cached_tile(pos) == TILE_TREE);
    update_visible_world(0, false);
    CHECK(cached_tile(pos) == authored);
    CHECK(cache.origin_tx == origin_x && cache.origin_ty == origin_y);
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
    CHECK(tile_solid(cached_tile(door)) && tile_solid(TILE_BOULDER_TALL));
    game_update(0, 0);
    CHECK(game.area == AREA_OUTDOOR && game.can_interact);

    sfx_stub_reset();
    game_update(0, BTN_A);
    CHECK(game.area == AREA_INTERIORS);
    EXPECT_SOUND(SFX_DOOR_OPEN, SFX_NONE);
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
    sfx_stub_reset();
    for(unsigned i = 0; i < 2 * TILE && game.area == AREA_INTERIORS; ++i)
        game_update(BTN_DOWN, 0);
    CHECK(game.area == AREA_OUTDOOR);
    EXPECT_SOUND(SFX_DOOR_CLOSE, SFX_NONE);
    CHECK(game.player.x == door.x * TILE && game.player.y == (door.y + 1) * TILE &&
          game.player.facing == SOUTH);
    CHECK(tile_solid(TILE_BOULDER_TALL));
    // The player arrives off the door, so nothing fires again.
    game_update(0, 0);
    CHECK(game.area == AREA_OUTDOOR);
}

static void stand(TilePos cell, DIRECTION facing) {
    world_enter(cell.area, (uint16_t)(cell.x * TILE), (uint16_t)(cell.y * TILE), facing);
}

static TilePos below(TilePos cell, uint8_t rows = 1) {
    return {cell.area, cell.x, (uint8_t)(cell.y + rows)};
}

static bool player_at(TilePos cell, DIRECTION facing) {
    return game.area == cell.area && game.player.x == cell.x * TILE &&
           game.player.y == cell.y * TILE && game.player.facing == facing;
}

// Walks until the area changes, for two tiles at most. Only the update that changes the area
// reports a teleport.
static void walk_through(uint8_t button) {
    uint8_t area = game.area;
    for(unsigned i = 0; i < 2 * TILE && game.area == area; ++i) {
        bool moved = game_update(button, 0);
        CHECK(moved == (game.area != area));
    }
    CHECK(game.area != area);
}

// Both ends of one passage are walked through, and the doorway leads back to the ground below the
// entrance.
static void walk_passage(handler_impl::Passage const& passage) {
    bool wall_row = passage.inside_step > 0;
    TilePos floor{passage.inside.area, passage.inside.x,
                  (uint8_t)(passage.inside.y + passage.inside_step)};
    // Both ends are walkable, and so is the floor on each side.
    stand(below(passage.outside), NORTH);
    CHECK(!tile_solid(cached_tile(passage.outside)) && !tile_portal(cached_tile(passage.outside)));
    CHECK(player_terrain_clear(game.player.x, game.player.y));
    game_update(0, 0);
    CHECK(game.area == passage.outside.area && !game.can_interact);
    walk_through(BTN_UP);
    CHECK(player_at(floor, wall_row ? SOUTH : NORTH));
    CHECK(!tile_solid(cached_tile(passage.inside)) && !tile_solid(cached_tile(floor)));
    CHECK(player_terrain_clear(game.player.x, game.player.y));
    // Standing still does nothing. Walking back onto the doorway leaves.
    game_update(0, 0);
    CHECK(game.area == passage.inside.area);
    walk_through(wall_row ? BTN_UP : BTN_DOWN);
    CHECK(player_at(below(passage.outside), SOUTH));
    game_update(0, 0);
    CHECK(game.area == passage.outside.area);
}

// The cave mouth, the mine door and the ladder hole are walked into, and each interior doorway
// leads back to the ground below its entrance.
static void passages() {
    using namespace handler_impl;
    game_new();
    CHECK(area_tileset(AREA_CAVE) == TILESET_CAVE && area_tileset(AREA_DUNGEON) == TILESET_DUNGEON);
    for(Passage const& passage : AreaPassages<AREA_CAVE>::TABLE)
        walk_passage(passage);
    for(Passage const& passage : AreaPassages<AREA_DUNGEON>::TABLE)
        walk_passage(passage);
    static_assert(
        passage_arrival<positions::CaveExit>() ==
        TilePos{AREA_OUTDOOR, positions::CaveMouth.x, (uint8_t)(positions::CaveMouth.y + 1)});
    static_assert(passage_facing<positions::CaveMouth>() == NORTH &&
                  passage_facing<positions::CaveHole>() == SOUTH &&
                  passage_facing<positions::CaveLadder>() == SOUTH);
}

// The cave has a song of its own. The other areas play Morning in the Meadow until they get theirs.
static_assert(area_music(AREA_CAVE) == MUSIC_CAVE && area_music(AREA_OUTDOOR) == MUSIC_PEACEFUL &&
              area_music(AREA_INTERIORS) == MUSIC_PEACEFUL &&
              area_music(AREA_DUNGEON) == MUSIC_PEACEFUL);

// The super creepy track is kept for a later scene, so no area plays it yet.
static_assert(area_music(AREA_CAVE) != MUSIC_CREEPY && area_music(AREA_OUTDOOR) != MUSIC_CREEPY &&
              area_music(AREA_INTERIORS) != MUSIC_CREEPY &&
              area_music(AREA_DUNGEON) != MUSIC_CREEPY && MUSIC_DEFAULT != MUSIC_CREEPY);

// Every cell of one mechanism shows the map's tile while its flag is clear and its own tile while
// the flag is set, from the tile pass alone: the cache holds no state between frames.
static void mechanism_state(handler_impl::Mechanism const& entry) {
    Area const& area = *world_area(entry.cells.origin.area);
    for(uint8_t dy = 0; dy < entry.cells.height; ++dy) {
        for(uint8_t dx = 0; dx < entry.cells.width; ++dx) {
            TilePos cell{entry.cells.origin.area, (uint8_t)(entry.cells.origin.x + dx),
                         (uint8_t)(entry.cells.origin.y + dy)};
            game_new();
            sfx_stub_reset();
            // Watch from the nearest walkable cell that is not the cell itself.
            game.area = cell.area;
            TilePos vantage = cell;
            for(int oy = -2; oy <= 2 && vantage == cell; ++oy)
                for(int ox = -3; ox <= 3 && vantage == cell; ++ox)
                    if((ox || oy) &&
                       tile_collision(authored_tile(area, cell.x + ox, cell.y + oy)) == 0)
                        vantage = {cell.area, (uint8_t)(cell.x + ox), (uint8_t)(cell.y + oy)};
            CHECK(!(vantage == cell));
            stand(vantage, SOUTH);
            CHECK(!game.get_flag(entry.flag));
            CHECK(cached_tile(cell) == authored_tile(area, cell.x, cell.y));
            CHECK(cached_tile(cell) != entry.tile);
            game.set_flag(entry.flag);
            update_visible_world(0, false);
            CHECK(cached_tile(cell) == entry.tile);
            update_visible_world(0, false);
            CHECK(cached_tile(cell) == entry.tile);
            game.clear_flag(entry.flag);
            update_visible_world(0, false);
            CHECK(cached_tile(cell) == authored_tile(area, cell.x, cell.y));
            // A flag that a load or a test sets changes the cells without a sound: only the
            // player's own operation of a mechanism is heard.
            CHECK(played() == 0);
        }
    }
}

// Every mechanism of the cave and of the dungeon.
static void mechanism_states() {
    using namespace handler_impl;
    for(Mechanism const& entry : AreaMechanisms<AREA_CAVE>::TABLE)
        mechanism_state(entry);
    for(Mechanism const& entry : AreaMechanisms<AREA_DUNGEON>::TABLE)
        mechanism_state(entry);
}

// The kinds say what a cell does and sounds like, and every table row has one.
static void mechanism_kinds() {
    using namespace handler_impl;
    for(uint8_t index = 0; index <= MECHANISM_RUNE; ++index) {
        MechanismTraits traits = mechanism_traits((MECHANISM_KIND)index);
        // Whatever the player works is heard, and what is thrown back is heard going back.
        CHECK(traits.input == INPUT_NONE || traits.on != SFX_NONE);
        CHECK(!traits.toggles || (traits.input != INPUT_NONE && traits.off != SFX_NONE));
        // Every sound that a kind names is a real effect.
        CHECK(traits.on == SFX_NONE || traits.on < SFX_COUNT);
        CHECK(traits.off == SFX_NONE || traits.off < SFX_COUNT);
    }
    // A lever is the one kind that toggles, and a plate is the one that the player steps on.
    CHECK(mechanism_traits(MECHANISM_LEVER).toggles && !mechanism_traits(MECHANISM_PLATE).toggles);
    CHECK(mechanism_traits(MECHANISM_PLATE).input == INPUT_STEP &&
          mechanism_traits(MECHANISM_LEVER).input == INPUT_FACE);
    CHECK(mechanism_traits(MECHANISM_GATE).input == INPUT_NONE &&
          mechanism_traits(MECHANISM_BRIDGE).input == INPUT_NONE);
    // The gate that a lever throws open and shut is heard to open and to close.
    CHECK(mechanism_traits(MECHANISM_GATE).on != mechanism_traits(MECHANISM_GATE).off);
    CHECK(mechanism_traits(MECHANISM_LEVER).on != mechanism_traits(MECHANISM_LEVER).off);

    // Every flag of every table can be changed by the player, and every kind is a real one.
    auto check_table = [](auto const& table) {
        for(Mechanism const& entry : table) {
            CHECK(entry.kind <= MECHANISM_RUNE);
            bool worked = false;
            for(Mechanism const& other : table)
                worked |= other.flag == entry.flag && mechanism_traits(other.kind).input != INPUT_NONE;
            CHECK(worked);
        }
    };
    check_table(AreaMechanisms<AREA_CAVE>::TABLE);
    check_table(AreaMechanisms<AREA_DUNGEON>::TABLE);
}

static void press_a() {
    game_update(0, 0);
    CHECK(game.can_interact);
    game_update(0, BTN_A);
}

// The cave: a lever and its gate, the latching plate and its bridge, the chest and the vent.
static void cave_mechanisms() {
    using namespace handler_impl;
    game_new();
    // The lever opens the vault gate, and throwing it back closes the gate.
    stand(below(positions::CaveLever), NORTH);
    CHECK(cached_tile(positions::CaveLever) == CAVE_TILE_SWITCH_LEFT);
    sfx_stub_reset();
    press_a();
    // The lever clicks on, and then the gate that it opens is heard to grind.
    EXPECT_SOUND(SFX_LEVER_ON, SFX_GRIND_OPEN);
    CHECK(game.get_flag(FLAG_CAVE_GATE) &&
          cached_tile(positions::CaveLever) == CAVE_TILE_SWITCH_RIGHT);
    stand(below(positions::CaveGate), NORTH);
    CHECK(cached_tile(positions::CaveGate) == CAVE_TILE_GATE_FRONT_OPEN);
    for(unsigned i = 0; i < 2 * TILE; ++i)
        game_update(BTN_UP, 0);
    CHECK(game.player.y == (positions::CaveGate.y - 1) * TILE);
    stand(below(positions::CaveLever), NORTH);
    sfx_stub_reset();
    press_a();
    EXPECT_SOUND(SFX_LEVER_OFF, SFX_GRIND_CLOSE);
    CHECK(!game.get_flag(FLAG_CAVE_GATE) &&
          cached_tile(positions::CaveLever) == CAVE_TILE_SWITCH_LEFT);
    stand(below(positions::CaveGate), NORTH);
    CHECK(cached_tile(positions::CaveGate) == CAVE_TILE_GATE_FRONT &&
          tile_solid(CAVE_TILE_GATE_FRONT));
    for(unsigned i = 0; i < 2 * TILE; ++i)
        game_update(BTN_UP, 0);
    CHECK(game.player.y > positions::CaveGate.y * TILE);

    // The plate goes down under the player and stays down, and the bridge spans the chasm.
    TilePos plate = positions::CavePlate;
    stand({plate.area, (uint8_t)(plate.x + 1), plate.y}, WEST);
    CHECK(!game.get_flag(FLAG_CAVE_BRIDGE) && cached_tile(plate) == CAVE_TILE_PRESSURE_PLATE);
    sfx_stub_reset();
    for(unsigned i = 0; i < TILE; ++i)
        game_update(BTN_LEFT, 0);
    CHECK(game.get_flag(FLAG_CAVE_BRIDGE) && cached_tile(plate) == CAVE_TILE_PRESSURE_PLATE_DOWN);
    // Standing on the plate for many frames presses it once, and the bridge runs out.
    EXPECT_SOUND(SFX_PLATE_DOWN, SFX_BRIDGE_EXTEND);
    for(unsigned i = 0; i < TILE; ++i)
        game_update(BTN_RIGHT, 0);
    // Stepping off and back on takes no second sound: the plate stays down.
    for(unsigned i = 0; i < TILE; ++i)
        game_update(BTN_LEFT, 0);
    CHECK(played() == 1);
    for(unsigned i = 0; i < TILE; ++i)
        game_update(BTN_RIGHT, 0);
    CHECK(game.get_flag(FLAG_CAVE_BRIDGE) && cached_tile(plate) == CAVE_TILE_PRESSURE_PLATE_DOWN);
    TilePos near_side = below(positions::CaveBridge_1);
    stand(near_side, NORTH);
    CHECK(cached_tile(positions::CaveBridge_0) == CAVE_TILE_BRIDGE_VERTICAL &&
          cached_tile(positions::CaveBridge_1) == CAVE_TILE_BRIDGE_VERTICAL);
    for(unsigned i = 0; i < 3 * TILE; ++i)
        game_update(BTN_UP, 0);
    CHECK(game.player.y == (positions::CaveBridge_0.y - 1) * TILE);
    game.clear_flag(FLAG_CAVE_BRIDGE);
    stand(near_side, NORTH);
    CHECK(cached_tile(positions::CaveBridge_0) == CAVE_TILE_CHASM_EDGE &&
          cached_tile(positions::CaveBridge_1) == CAVE_TILE_CHASM_RIM);
    for(unsigned i = 0; i < 3 * TILE; ++i)
        game_update(BTN_UP, 0);
    CHECK(game.player.y == near_side.y * TILE - FOOT_TOP);

    // The chest pays once.
    uint16_t gold = game.gold;
    stand(below(positions::CaveChest), NORTH);
    CHECK(cached_tile(positions::CaveChest) == CAVE_TILE_CHEST_CLOSED);
    game.supplies.quantity[SUPPLY_ETHER] = SUPPLY_LIMIT;
    Supplies blocked = game.supplies;
    sfx_stub_reset();
    press_a();
    CHECK(!game.get_flag(FLAG_CAVE_CHEST) && game.gold == gold);
    CHECK(!equipment_owned(ITEM_TRAVELERS_BOOTS));
    CHECK(!memcmp(&blocked, &game.supplies, sizeof(Supplies)));
    CHECK(cached_tile(positions::CaveChest) == CAVE_TILE_CHEST_CLOSED && played() == 0);
    --game.supplies.quantity[SUPPLY_ETHER];
    press_a();
    CHECK(game.get_flag(FLAG_CAVE_CHEST) && game.gold == gold + 25);
    CHECK(equipment_owned(ITEM_TRAVELERS_BOOTS));
    CHECK(cached_tile(positions::CaveChest) == CAVE_TILE_CHEST_OPEN);
    EXPECT_SOUND(SFX_CHEST_OPEN, SFX_NONE);
    CHECK(supply_count(game.supplies, SUPPLY_POTION) == 3);
    CHECK(supply_count(game.supplies, SUPPLY_ETHER) == SUPPLY_LIMIT);
    // The saved flag restores the open lid and prevents another reward.
    CHECK(save_restore());
    CHECK(equipment_owned(ITEM_TRAVELERS_BOOTS));
    update_visible_world(0, false);
    CHECK(cached_tile(positions::CaveChest) == CAVE_TILE_CHEST_OPEN);
    press_a();
    CHECK(supply_count(game.supplies, SUPPLY_POTION) == 3);
    CHECK(supply_count(game.supplies, SUPPLY_ETHER) == SUPPLY_LIMIT);
    CHECK(game.gold == gold + 25);
    CHECK(played() == 1);

    // The vent blows by the clock alone and blocks its passage while it blows.
    static_assert(vent_blows(0) && vent_blows(VENT_BLOW_TICKS - 1) &&
                  !vent_blows(VENT_BLOW_TICKS) && !vent_blows(255) && vent_blows(256) &&
                  65536 % 256 == 0);
    stand(below(positions::CaveVent), NORTH);
    game.ticks = VENT_BLOW_TICKS;
    update_visible_world(0, false);
    CHECK(cached_tile(positions::CaveVent) == CAVE_TILE_VENT && !tile_solid(CAVE_TILE_VENT));
    game.ticks = 256;
    update_visible_world(0, false);
    CHECK(cached_tile(positions::CaveVent) == CAVE_TILE_VENT_ACTIVE &&
          tile_solid(CAVE_TILE_VENT_ACTIVE));
    // The vent hisses on the tick that it starts to blow, and not while it rests or keeps
    // blowing. The hiss is ambient, so it never cuts off a sound that answers the player.
    for(uint16_t ticks : {(uint16_t)255, (uint16_t)1, (uint16_t)VENT_BLOW_TICKS,
                          (uint16_t)(VENT_BLOW_TICKS + 1)}) {
        sfx_stub_reset();
        game.ticks = ticks;
        update_visible_world(0, false);
        CHECK(played(true) == 0 && played() == 0);
    }
    sfx_stub_reset();
    game.ticks = 512;
    update_visible_world(0, false);
    CHECK(played() == 0 && played(true) >= 1);
    for(unsigned i = 0; i < played(true); ++i)
        CHECK(nth_played(i, true).effect == SFX_STEAM_HISS);
    // A player caught on the vent when it blows is moved off it, not trapped.
    game.ticks = VENT_BLOW_TICKS;
    stand(positions::CaveVent, NORTH);
    CHECK(player_terrain_clear(game.player.x, game.player.y));
    game.ticks = 256;
    update_visible_world(0, false);
    CHECK(player_terrain_clear(game.player.x, game.player.y));
    CHECK(game.player.y != positions::CaveVent.y * TILE);
}

// The dungeon: the four seals in order, then the vault chest.
static void dungeon_mechanisms() {
    using namespace handler_impl;
    game_new();
    // The hall lever lowers the spikes.
    stand(below(positions::DungeonSpikes_1), NORTH);
    CHECK(cached_tile(positions::DungeonSpikes_0) == DUNGEON_TILE_SPIKES &&
          cached_tile(positions::DungeonSpikes_1) == DUNGEON_TILE_SPIKES);
    for(unsigned i = 0; i < TILE; ++i)
        game_update(BTN_UP, 0);
    CHECK(game.player.y == (positions::DungeonSpikes_1.y + 1) * TILE - FOOT_TOP);
    stand(below(positions::DungeonLever), NORTH);
    sfx_stub_reset();
    press_a();
    EXPECT_SOUND(SFX_LEVER_ON, SFX_SCRAPE_OPEN);
    CHECK(game.get_flag(FLAG_DUNGEON_SPIKES));
    stand(below(positions::DungeonSpikes_1), NORTH);
    CHECK(cached_tile(positions::DungeonSpikes_0) == DUNGEON_TILE_SPIKES_DOWN &&
          cached_tile(positions::DungeonSpikes_1) == DUNGEON_TILE_SPIKES_DOWN);
    for(unsigned i = 0; i < 3 * TILE; ++i)
        game_update(BTN_UP, 0);
    CHECK(game.player.y == (positions::DungeonSpikes_0.y - 1) * TILE);

    // The cell door opens once, and the plate behind it opens the east gate.
    TilePos door = positions::DungeonCellDoor;
    stand(below(door), NORTH);
    CHECK(cached_tile(door) == DUNGEON_TILE_BARS_DOOR && tile_solid(DUNGEON_TILE_BARS_DOOR));
    sfx_stub_reset();
    press_a();
    CHECK(game.get_flag(FLAG_DUNGEON_CELL) && cached_tile(door) == DUNGEON_TILE_BARS_DOOR_OPEN);
    EXPECT_SOUND(SFX_DOOR_UNLOCK, SFX_NONE);
    // The open door takes no more presses, and makes no more sound.
    game_update(0, 0);
    CHECK(!game.can_interact);
    CHECK(played() == 1);
    CHECK(positions::DungeonPlate.x == door.x && positions::DungeonPlate.y == door.y - 1);
    CHECK(!game.get_flag(FLAG_DUNGEON_GATE));
    sfx_stub_reset();
    for(unsigned i = 0; i < 2 * TILE; ++i)
        game_update(BTN_UP, 0);
    CHECK(game.player.y == positions::DungeonPlate.y * TILE && game.get_flag(FLAG_DUNGEON_GATE));
    EXPECT_SOUND(SFX_PLATE_DOWN, SFX_GRIND_OPEN);
    CHECK(cached_tile(positions::DungeonPlate) == DUNGEON_TILE_PRESSURE_PLATE_DOWN);
    TilePos gate = positions::DungeonGate;
    stand({gate.area, (uint8_t)(gate.x - 1), gate.y}, EAST);
    CHECK(cached_tile(gate) == DUNGEON_TILE_GATE_SIDE_OPEN);
    for(unsigned i = 0; i < 2 * TILE; ++i)
        game_update(BTN_RIGHT, 0);
    CHECK(game.player.x == (gate.x + 1) * TILE);

    // The east wing's lever sinks the blocks.
    stand(below(positions::DungeonBlocks_0), NORTH);
    CHECK(cached_tile(positions::DungeonBlocks_0) == DUNGEON_TILE_BLOCK_RAISED &&
          cached_tile(positions::DungeonBlocks_1) == DUNGEON_TILE_BLOCK_RAISED);
    TilePos lever = positions::DungeonBlockLever;
    stand({lever.area, (uint8_t)(lever.x + 1), lever.y}, WEST);
    sfx_stub_reset();
    press_a();
    EXPECT_SOUND(SFX_LEVER_ON, SFX_GRIND_OPEN);
    CHECK(game.get_flag(FLAG_DUNGEON_BLOCKS));
    stand(below(positions::DungeonBlocks_0), NORTH);
    CHECK(cached_tile(positions::DungeonBlocks_0) == DUNGEON_TILE_BLOCK_LOWERED &&
          cached_tile(positions::DungeonBlocks_1) == DUNGEON_TILE_BLOCK_LOWERED);
    for(unsigned i = 0; i < 2 * TILE; ++i)
        game_update(BTN_UP, 0);
    CHECK(game.player.y == (positions::DungeonBlocks_0.y - 1) * TILE);

    // The orb lights the runes and opens the vault.
    TilePos orb = positions::DungeonOrb, vault = positions::DungeonVaultDoor;
    stand(below(orb), NORTH);
    CHECK(cached_tile(orb) == DUNGEON_TILE_ORB_DARK);
    sfx_stub_reset();
    press_a();
    // The runes only light, so the vault door is what is heard to move after the chime.
    EXPECT_SOUND(SFX_ORB_LIGHT, SFX_GRIND_OPEN);
    CHECK(game.get_flag(FLAG_DUNGEON_ORB) && cached_tile(orb) == DUNGEON_TILE_ORB_LIT);
    game_update(0, 0);
    CHECK(cached_tile(positions::DungeonRuneWest) == DUNGEON_TILE_RUNE_LIT &&
          cached_tile(positions::DungeonRuneEast) == DUNGEON_TILE_RUNE_LIT);
    stand(below(vault), NORTH);
    CHECK(cached_tile(vault) == DUNGEON_TILE_ARCHWAY);
    for(unsigned i = 0; i < 2 * TILE; ++i)
        game_update(BTN_UP, 0);
    CHECK(game.player.y == (vault.y - 1) * TILE);
    game.clear_flag(FLAG_DUNGEON_ORB);
    stand(below(vault), NORTH);
    CHECK(cached_tile(vault) == DUNGEON_TILE_PORTCULLIS && tile_solid(DUNGEON_TILE_PORTCULLIS));

    uint16_t gold = game.gold;
    stand(below(positions::DungeonChest), NORTH);
    uint8_t revives = game.supplies.quantity[SUPPLY_REVIVE];
    game.supplies.quantity[SUPPLY_REVIVE] = SUPPLY_LIMIT;
    Supplies blocked = game.supplies;
    sfx_stub_reset();
    press_a();
    CHECK(!game.get_flag(FLAG_DUNGEON_CHEST) && game.gold == gold && played() == 0);
    CHECK(!memcmp(&blocked, &game.supplies, sizeof(Supplies)));
    CHECK(!equipment_owned(ITEM_DAWN_PENDANT) && !equipment_owned(ITEM_VITAL_PENDANT));
    game.supplies.quantity[SUPPLY_REVIVE] = revives;

    // A pre-owned item or overflowing gold must also block the complete claim.
    CHECK(equipment_acquire(ITEM_VITAL_PENDANT));
    blocked = game.supplies;
    press_a();
    CHECK(!game.get_flag(FLAG_DUNGEON_CHEST) && game.gold == gold && played() == 0);
    CHECK(!equipment_owned(ITEM_DAWN_PENDANT) && equipment_owned(ITEM_VITAL_PENDANT));
    CHECK(!memcmp(&blocked, &game.supplies, sizeof(Supplies)));
    game.clear_flag(equipment_flag(ITEM_VITAL_PENDANT));
    game.gold = UINT16_MAX;
    press_a();
    CHECK(!game.get_flag(FLAG_DUNGEON_CHEST) && game.gold == UINT16_MAX && played() == 0);
    CHECK(!equipment_owned(ITEM_DAWN_PENDANT) && !equipment_owned(ITEM_VITAL_PENDANT));
    CHECK(!memcmp(&blocked, &game.supplies, sizeof(Supplies)));
    game.gold = gold;

    sfx_stub_reset();
    press_a();
    EXPECT_SOUND(SFX_CHEST_OPEN, SFX_NONE);
    CHECK(game.get_flag(FLAG_DUNGEON_CHEST) && game.gold == gold + 25);
    CHECK(equipment_owned(ITEM_DAWN_PENDANT) && equipment_owned(ITEM_VITAL_PENDANT));
    CHECK(cached_tile(positions::DungeonChest) == DUNGEON_TILE_CHEST_OPEN);
    CHECK(supply_count(game.supplies, SUPPLY_ELIXIR) == 2);
    CHECK(supply_count(game.supplies, SUPPLY_REVIVE) == 2);
    CHECK(save_restore());
    update_visible_world(0, false);
    CHECK(cached_tile(positions::DungeonChest) == DUNGEON_TILE_CHEST_OPEN);
    press_a();
    CHECK(game.gold == gold + 25);
    CHECK(supply_count(game.supplies, SUPPLY_ELIXIR) == 2);
    CHECK(supply_count(game.supplies, SUPPLY_REVIVE) == 2);
}

// Each message answers a press from the cell in front of it.
static void messages() {
    TilePos const all[] = {positions::CaveSign, positions::CaveNotice, positions::DungeonPlaque};
    for(TilePos cell : all) {
        game_new();
        stand(below(cell), NORTH);
        CHECK(tile_solid(cached_tile(cell)));
        game_update(0, 0);
        CHECK(game.can_interact);
    }
}

// A battle overwrites the runtime workspace, which holds the view, the camera and the cached foot
// answers. Reclaiming it rebuilds the view from the world and the flags, so a persistent tile change
// (the gate open by its flag) comes back, and no cached collision answer survives.
static void battle_reclaim() {
    game_new();
    stand(below(positions::CaveLever), NORTH);
    game.set_flag(FLAG_CAVE_GATE);
    update_visible_world(0, false);
    CHECK(cached_tile(positions::CaveGate) == CAVE_TILE_GATE_FRONT_OPEN);
    TileCache const view = cache;
    Camera const shown = camera;

    memset(&runtime_workspace, 0xA5, sizeof(runtime_workspace));
    world_reclaim_workspace();
    CHECK(!memcmp(&cache, &view, sizeof(view)));
    CHECK(camera.x == shown.x && camera.y == shown.y);
    // The tile pass reapplies the gate through set_tile, which counts as an edit, just as it does
    // each frame. Only the cached foot answer must be gone.
    CHECK(!foot_cache.known);
    CHECK(game.npcs.terrain_dirty);
    CHECK(cached_tile(positions::CaveGate) == CAVE_TILE_GATE_FRONT_OPEN);

    // The next frame rebuilds the same view, so the reclaimed workspace matches an unbroken one.
    update_visible_world(0, false);
    CHECK(!memcmp(&cache, &view, sizeof(view)) && camera.x == shown.x && camera.y == shown.y);
}

static void inn_recovery() {
    game_new();
    stand(below(positions::InnDoor), NORTH);
    press_a();
    CHECK(player_at(handler_impl::house_inside(positions::InnDoor), NORTH));

    // Follow the floor from the arrival mat to the counter, then face it.
    for(unsigned i = 0; i < 2 * TILE; ++i)
        game_update(BTN_LEFT, 0);
    game_update(BTN_UP, 0);
    game_update(0, 0);
    CHECK(game.can_interact);
    PartyMember& traveler = game.party.members[0];
    traveler.hp = traveler.ap = 0;
    accept_prompt = false;
    press_a();
    CHECK(traveler.hp == 0 && traveler.ap == 0);
    accept_prompt = true;
    press_a();
    CHECK(traveler.hp == party_max_hp(traveler) && traveler.ap == party_max_ap(traveler));
    CHECK(game.area == AREA_INTERIORS);

    for(unsigned i = 0; i < 2 * TILE; ++i)
        game_update(BTN_RIGHT, 0);
    for(unsigned i = 0; i < TILE && game.area == AREA_INTERIORS; ++i)
        game_update(BTN_DOWN, 0);
    CHECK(player_at(handler_impl::house_front(positions::InnExit), SOUTH));
}

int main() {
    run_world_scenarios();
    battle_reclaim();
    house_doors();
    inn_recovery();
    passages();
    mechanism_states();
    mechanism_kinds();
    cave_mechanisms();
    dungeon_mechanisms();
    messages();
    window_edges();
    terrain_footprints();
    foot_triangle_windows();
    collision_triangles();
    player_shapes();
    corner_assist();
    random_masks();
    leading_edge_boundaries();
    tile_edits_reset_each_frame();
    game_new();
    Area const& area = *world_area(AREA_OUTDOOR);
    for(unsigned y = 0; y <= 128; y += 3) {
        for(unsigned x = 0; x <= 160; x += 5) {
            reload_tile_window(cache, area.tiles, {area.width, area.height},
                               {(uint16_t)x, (uint16_t)y});
            unsigned ox = x / TILE >= CACHE_MARGIN ? x / TILE - CACHE_MARGIN : 0;
            unsigned oy = y / TILE >= CACHE_MARGIN ? y / TILE - CACHE_MARGIN : 0;
            CHECK(cache.origin_tx == ox && cache.origin_ty == oy);
            for(unsigned cy = 0; cy < CACHE_HEIGHT; ++cy)
                for(unsigned cx = 0; cx < CACHE_WIDTH; ++cx)
                    CHECK(cache.tiles[cy * CACHE_WIDTH + cx] ==
                          authored_tile(area, ox + cx, oy + cy));
        }
    }
    // The 11x7 window intersects at most nine 8x4 chunks. The trace records each chunk's index in
    // the map's chunk grid, in row-major order.
    world_enter(AREA_OUTDOOR, positions::TestField.x * TILE, positions::TestField.y * TILE, SOUTH);
    Area const& map = *world_area(AREA_OUTDOOR);
    Camera view = game_camera();
    unsigned left = view.x >> TILE_SHIFT, top = view.y >> TILE_SHIFT;
    left = left >= CACHE_MARGIN ? left - CACHE_MARGIN : 0;
    top = top >= CACHE_MARGIN ? top - CACHE_MARGIN : 0;
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
    HandlerStage stages[] = {STAGE_GAMEPLAY, STAGE_TILES, STAGE_DIALOGUE, STAGE_NPCS};
    for(HandlerStage stage : stages) {
        CHECK(chunk_trace_count[stage] == chunks);
        for(unsigned i = 0; i < chunks; ++i)
            CHECK(chunk_trace[stage][i] == expected[i]);
    }
    CHECK(!terrain_clear(camera.x + CACHE_WIDTH * TILE, game.player.y));
    TilePos beyond{AREA_OUTDOOR, (uint8_t)((view.x >> TILE_SHIFT) + CACHE_WIDTH),
                   (uint8_t)(view.y >> TILE_SHIFT)};
    CHECK(cached_tile(beyond) == TILE_OUTSIDE); // Just past the window
    printf("PASS chunk/cache world: %u checks\n", checks);
}
