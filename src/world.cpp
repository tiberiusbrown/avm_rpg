#include "chunk.h"
#include "logic/npc_logic.h"
#include "world_assets.h"

#include <string.h>

uint8_t frame_pressed;
bool teleported;

// The foot position that last passed player_terrain_clear. Its answer depends only on the area,
// the position, and the terrain, and terrain changes only through tile edits and reloads that
// discard them. Until one of those events, a repeat of this position needs no tile reads.
static bool foot_clear_known;
static uint8_t foot_clear_area;
static uint16_t foot_clear_x, foot_clear_y;
// A reload discards tile edits, so it changes the terrain when an edit preceded it.
static bool cache_edited;

static bool player_foot_clear_known() {
    return foot_clear_known && foot_clear_area == game.area && foot_clear_x == game.player.x &&
           foot_clear_y == game.player.y;
}

void note_tile_edit() {
    foot_clear_known = false;
    cache_edited = true;
}

#ifndef RPG_AVM
static uint16_t collision_override[256];
static bool collision_overridden[256];

void tile_collision_override(uint8_t tile, uint16_t mask) {
    note_tile_edit();
    collision_override[tile] = mask;
    collision_overridden[tile] = true;
}

void tile_collision_reset() {
    note_tile_edit();
    memset(collision_overridden, 0, sizeof(collision_overridden));
}
#endif

uint16_t tile_collision(uint8_t tile) {
#ifndef RPG_AVM
    if(collision_overridden[tile])
        return collision_override[tile];
#endif
    return TILE_COLLISION[tile];
}

bool tile_solid(uint8_t tile) {
    return tile_collision(tile) != 0;
}

// One quadrant's triangle rule. (x, y) are quadrant-local pixels from 0 to 7, and the nibble
// orders its bits north, east, south, west. A pixel center is solid when it lies in a solid
// triangle, or on the shared edge of a solid one.
constexpr bool quadrant_solid(uint8_t nibble, uint8_t x, uint8_t y) {
    int16_t u = (int16_t)(2 * x - 7), v = (int16_t)(2 * y - 7);
    int16_t au = u < 0 ? (int16_t)-u : u, av = v < 0 ? (int16_t)-v : v;
    bool across = ((nibble >> (u > 0 ? 1 : 3)) & 1) != 0;
    bool along = ((nibble >> (v > 0 ? 2 : 0)) & 1) != 0;
    if(au > av)
        return across;
    if(av > au)
        return along;
    return across || along;
}

// Solid pixels of every quadrant pattern. Bit x of row[nibble][y] is pixel (x, y), so one
// foot-box row inside a quadrant is a single byte test.
struct QuadrantRows {
    uint8_t row[16][8];
};

constexpr QuadrantRows make_quadrant_rows() {
    QuadrantRows rows{};
    for(uint8_t nibble = 0; nibble < 16; ++nibble)
        for(uint8_t y = 0; y < 8; ++y)
            for(uint8_t x = 0; x < 8; ++x)
                if(quadrant_solid(nibble, x, y))
                    rows.row[nibble][y] = (uint8_t)(rows.row[nibble][y] | (1u << x));
    return rows;
}

// 128 bytes of flash, computed from quadrant_solid at compile time.
static constexpr QuadrantRows PROGMEM QUADRANT_ROWS = make_quadrant_rows();

bool collision_pixel_solid(uint16_t mask, uint8_t x, uint8_t y) {
    // Triangles are closed shapes. A pixel center on a quadrant diagonal is solid when either
    // triangle that shares the diagonal is solid, so a diagonal edge rasterizes as one line.
    uint8_t quadrant = (uint8_t)(((y >> 3) << 1) | (x >> 3));
    return quadrant_solid((uint8_t)((mask >> (quadrant * 4)) & 0xF), (uint8_t)(x & 7),
                          (uint8_t)(y & 7));
}

#ifndef RPG_AVM
uint16_t chunk_trace[3][CHUNK_TRACE_CAPACITY];
uint8_t chunk_trace_count[3];
unsigned feature_trace_count[3];
#endif

void reload_tile_window(TileCache& cache, uint8_t const PROGMEM* tiles, MapDimensions dimensions,
                        Camera camera) {
    if(!tiles) {
        memset(&cache, TILE_OUTSIDE, sizeof(cache));
        cache.origin_tx = cache.origin_ty = 0;
        return;
    }
    uint8_t width = dimensions.width_tiles, height = dimensions.height_tiles;
    // Clamp the view to the map, as game_camera does. The cache then trails the view by the
    // margin; its far edge may extend past the map, where the sentinel tile remains.
    uint16_t max_x = width > DISPLAY_TILE_WIDTH ? (uint16_t)(width - DISPLAY_TILE_WIDTH) * TILE : 0;
    uint16_t max_y =
        height > DISPLAY_TILE_HEIGHT ? (uint16_t)(height - DISPLAY_TILE_HEIGHT) * TILE : 0;
    if(camera.x > max_x)
        camera.x = max_x;
    if(camera.y > max_y)
        camera.y = max_y;
    uint8_t view_tx = (uint8_t)(camera.x >> TILE_SHIFT),
            view_ty = (uint8_t)(camera.y >> TILE_SHIFT);
    cache.origin_tx = view_tx >= CACHE_MARGIN ? (uint8_t)(view_tx - CACHE_MARGIN) : 0;
    cache.origin_ty = view_ty >= CACHE_MARGIN ? (uint8_t)(view_ty - CACHE_MARGIN) : 0;
    memset(cache.tiles, TILE_OUTSIDE, sizeof(cache.tiles));

    uint8_t rows = height - cache.origin_ty;
    if(rows > CACHE_HEIGHT)
        rows = CACHE_HEIGHT;
    uint8_t row_bytes = width - cache.origin_tx;
    if(row_bytes > CACHE_WIDTH)
        row_bytes = CACHE_WIDTH;
    if(!rows || !row_bytes)
        return;
    uint8_t* dst = cache.tiles;
    uint8_t const PROGMEM* src = tiles + (uint16_t)cache.origin_ty * width + cache.origin_tx;

    // Map rows are contiguous. Uncopied right/bottom cells retain the sentinel.
    for(uint8_t row = 0; row < rows; ++row) {
        memcpy_P(dst, src, row_bytes);
        dst += CACHE_WIDTH;
        src += width;
    }
}

void reload_tile_cache() {
    game.camera = game_camera();
    if(cache_edited) {
        foot_clear_known = false;
        cache_edited = false;
    }
    reload_tile_window(game.cache, world_area(game.area)->tiles, game.dimensions, game.camera);
}

void set_tile(TilePos pos, uint8_t tile) {
    if(pos.area != game.area || pos.x >= game.dimensions.width_tiles ||
       pos.y >= game.dimensions.height_tiles)
        return;
    uint16_t x = (uint16_t)(pos.x - game.cache.origin_tx);
    uint16_t y = (uint16_t)(pos.y - game.cache.origin_ty);
    if(x < CACHE_WIDTH && y < CACHE_HEIGHT) {
        note_tile_edit();
        game.cache.tiles[y * CACHE_WIDTH + x] = tile;
    }
}

uint8_t cached_tile(TilePos pos) {
    if(pos.area != game.area)
        return TILE_OUTSIDE;
    int16_t x = (int16_t)pos.x - game.cache.origin_tx;
    int16_t y = (int16_t)pos.y - game.cache.origin_ty;
    if(x >= 0 && x < CACHE_WIDTH && y >= 0 && y < CACHE_HEIGHT)
        return game.cache.tiles[(uint16_t)y * CACHE_WIDTH + x];
    return TILE_OUTSIDE;
}

// True when any pixel of [x0,x1] x [y0,y1] is solid in one tile's mask. A row crosses at most
// two quadrants, and each crossing is one window over that quadrant's row byte.
static bool mask_hits(uint16_t mask, uint8_t x0, uint8_t x1, uint8_t y0, uint8_t y1) {
    for(uint8_t y = y0; y <= y1; ++y) {
        uint8_t left = (uint8_t)((y >> 3) << 1), qy = (uint8_t)(y & 7);
        if(x0 < 8) {
            uint8_t nibble = (uint8_t)((mask >> (4 * left)) & 0xF);
            uint8_t hi = x1 < 7 ? x1 : 7;
            uint8_t window = (uint8_t)(((2u << hi) - 1) & ~((1u << x0) - 1));
            if(QUADRANT_ROWS.row[nibble][qy] & window)
                return true;
        }
        if(x1 >= 8) {
            uint8_t nibble = (uint8_t)((mask >> (4 * (left + 1))) & 0xF);
            uint8_t lo = x0 > 8 ? (uint8_t)(x0 - 8) : 0;
            uint8_t window = (uint8_t)(((2u << (x1 - 8)) - 1) & ~((1u << lo) - 1));
            if(QUADRANT_ROWS.row[nibble][qy] & window)
                return true;
        }
    }
    return false;
}

// Solid at one pixel of a tile's mask. Whole-tile masks skip the table.
static bool mask_pixel(uint16_t mask, uint8_t x, uint8_t y) {
    if(!mask)
        return false;
    if(mask == 0xFFFF)
        return true;
    uint8_t quadrant = (uint8_t)(((y >> 3) << 1) | (x >> 3));
    uint8_t nibble = (uint8_t)((mask >> (4 * quadrant)) & 0xF);
    return ((QUADRANT_ROWS.row[nibble][y & 7] >> (x & 7)) & 1) != 0;
}

__attribute__((noinline)) uint8_t player_leading_edge(uint16_t x, uint16_t y, DIRECTION dir) {
    static_assert(FOOT_WIDTH == 8 && FOOT_HEIGHT == 8, "sidestep bits cover the 8-pixel edge");
    bool horizontal = dir == EAST || dir == WEST;
    // The column of an east or west step, or the row of a north or south one.
    uint16_t fixed = horizontal ? (uint16_t)(x + (dir == EAST ? FOOT_RIGHT : FOOT_LEFT))
                                : (uint16_t)(y + (dir == SOUTH ? FOOT_BOTTOM : FOOT_TOP));
    uint8_t bits = 0;
    uint16_t tile_key = 0, mask = 0;
    bool loaded = false;
    for(uint8_t i = 0; i < 8; ++i) {
        uint16_t wx = horizontal ? fixed : (uint16_t)(x + FOOT_LEFT + i);
        uint16_t wy = horizontal ? (uint16_t)(y + FOOT_TOP + i) : fixed;
        bool blocked = true;
        if(game.dimensions.pixel_bounds(wx, wy)) {
            uint16_t key = (uint16_t)(((wy >> TILE_SHIFT) << 8) | (wx >> TILE_SHIFT));
            if(!loaded || key != tile_key) {
                tile_key = key;
                loaded = true;
                mask = tile_collision(cached_tile(
                    {game.area, (uint8_t)(wx >> TILE_SHIFT), (uint8_t)(wy >> TILE_SHIFT)}));
            }
            blocked = mask_pixel(mask, (uint8_t)(wx & (TILE - 1)), (uint8_t)(wy & (TILE - 1)));
        }
        if(blocked)
            bits = (uint8_t)(bits | (1u << i));
    }
    return bits;
}

// The player's foot box checked pixel by pixel against each tile's collision mask.
bool player_terrain_clear(uint16_t x, uint16_t y) {
    if(!game.dimensions.pixel_bounds(x, y))
        return false;
    uint16_t left = x + FOOT_LEFT, right = x + FOOT_RIGHT;
    uint16_t top = y + FOOT_TOP, bottom = y + FOOT_BOTTOM;
    for(uint16_t ty = top >> TILE_SHIFT; ty <= (bottom >> TILE_SHIFT); ++ty) {
        for(uint16_t tx = left >> TILE_SHIFT; tx <= (right >> TILE_SHIFT); ++tx) {
            uint16_t mask = tile_collision(cached_tile({game.area, (uint8_t)tx, (uint8_t)ty}));
            if(!mask)
                continue;
            if(mask == 0xFFFF)
                return false;
            uint16_t ox = tx << TILE_SHIFT, oy = ty << TILE_SHIFT;
            uint16_t lo_x = left > ox ? left : ox,
                     hi_x = right < ox + TILE - 1 ? right : ox + TILE - 1;
            uint16_t lo_y = top > oy ? top : oy,
                     hi_y = bottom < oy + TILE - 1 ? bottom : oy + TILE - 1;
            if(mask_hits(mask, (uint8_t)(lo_x - ox), (uint8_t)(hi_x - ox), (uint8_t)(lo_y - oy),
                         (uint8_t)(hi_y - oy)))
                return false;
        }
    }
    foot_clear_known = true;
    foot_clear_area = game.area;
    foot_clear_x = x;
    foot_clear_y = y;
    return true;
}

bool terrain_clear(uint16_t x, uint16_t y) {
    if(!game.dimensions.pixel_bounds(x, y))
        return false;
    // Pixel bounds checked before narrowing to byte tile coordinates.
    uint8_t x1 = (uint8_t)((x + FOOT_LEFT) >> TILE_SHIFT),
            x2 = (uint8_t)((x + FOOT_RIGHT) >> TILE_SHIFT);
    uint8_t y1 = (uint8_t)((y + FOOT_TOP) >> TILE_SHIFT),
            y2 = (uint8_t)((y + FOOT_BOTTOM) >> TILE_SHIFT);
    // A foot box intersects at most two columns and two rows. Preserve the
    // row-major short circuit without loop counters or duplicate tile reads.
    static_assert(FOOT_WIDTH <= TILE && FOOT_HEIGHT <= TILE);
    if(tile_solid(cached_tile({game.area, x1, y1})))
        return false;
    if(x2 != x1 && tile_solid(cached_tile({game.area, x2, y1})))
        return false;
    if(y2 != y1) {
        if(tile_solid(cached_tile({game.area, x1, y2})))
            return false;
        if(x2 != x1 && tile_solid(cached_tile({game.area, x2, y2})))
            return false;
    }
    return true;
}

bool Game::player_is_on_tile(TilePos tile) const {
    return tile.area == area && dimensions.pixel_bounds(player.x, player.y) &&
           (player.x + FOOT_CENTER_X) >> TILE_SHIFT == tile.x &&
           (player.y + FOOT_CENTER_Y) >> TILE_SHIFT == tile.y;
}

bool Game::player_is_in_region(TileRegion region) const {
    if(region.origin.area != area || !dimensions.pixel_bounds(player.x, player.y))
        return false;
    uint16_t right = (uint16_t)region.origin.x + region.width;
    uint16_t bottom = (uint16_t)region.origin.y + region.height;
    if(!region.width || !region.height || right > dimensions.width_tiles ||
       bottom > dimensions.height_tiles)
        return false;
    uint16_t x = (player.x + FOOT_CENTER_X) >> TILE_SHIFT,
             y = (player.y + FOOT_CENTER_Y) >> TILE_SHIFT;
    return region.contains({area, (uint8_t)x, (uint8_t)y});
}

// A closing feature ejects N/E/S/W at increasing distances of 1..16 pixels. Only the player
// ejects, so this uses the player's collision.
static bool eject_actor(Actor& actor) {
    uint16_t x = actor.x, y = actor.y;
    for(uint8_t radius = 1; radius <= TILE; ++radius) {
        for(DIRECTION dir = NORTH; dir <= WEST; dir = (DIRECTION)(dir + 1)) {
            int16_t nx = (int16_t)x, ny = (int16_t)y;
            if(dir == NORTH)
                ny -= radius;
            if(dir == EAST)
                nx += radius;
            if(dir == SOUTH)
                ny += radius;
            if(dir == WEST)
                nx -= radius;
            if(nx >= 0 && ny >= 0 && player_terrain_clear((uint16_t)nx, (uint16_t)ny)) {
                actor.x = (uint16_t)nx;
                actor.y = (uint16_t)ny;
                actor.walking = 0;
                return true;
            }
        }
    }
    return false;
}

template <HandlerStage STAGE> void process_all_chunks() {
    Camera camera = STAGE == STAGE_GAMEPLAY ? game_camera() : game.camera;
    uint8_t left = (uint8_t)(camera.x >> TILE_SHIFT), top = (uint8_t)(camera.y >> TILE_SHIFT);
    uint16_t right = (uint16_t)left + CACHE_WIDTH - 1;
    uint16_t bottom = (uint16_t)top + CACHE_HEIGHT - 1;
    if(right >= game.dimensions.width_tiles)
        right = game.dimensions.width_tiles - 1;
    if(bottom >= game.dimensions.height_tiles)
        bottom = game.dimensions.height_tiles - 1;
    for(uint8_t cy = (uint8_t)(top / CHUNK_HEIGHT); cy <= bottom / CHUNK_HEIGHT; ++cy) {
        for(uint8_t cx = (uint8_t)(left / CHUNK_WIDTH); cx <= right / CHUNK_WIDTH; ++cx) {
            run_chunk<STAGE>({game.area, cx, cy});
            if constexpr(STAGE != STAGE_TILES) {
                if(teleported)
                    return;
            }
        }
    }
}

void refresh_tile_cache() {
    RPG_PHASE(4);
    reload_tile_cache();
    RPG_PHASE(5);
    process_all_chunks<STAGE_TILES>();
}

static void repair_player_overlap() {
    if(!player_foot_clear_known() && !player_terrain_clear(game.player.x, game.player.y) &&
       !eject_actor(game.player)) {
        Area const PROGMEM* a = world_area(game.area);
        game.player.x = a->spawn.x;
        game.player.y = a->spawn.y;
        game.player.walking = 0;
        game.can_interact = false;
        refresh_tile_cache();
        RPG_PHASE(6);
    }
}

bool update_visible_world(uint8_t pressed, bool simulate) {
    frame_pressed = pressed;
    teleported = false;
    game.can_interact = false;
    npc_begin_frame();
#ifndef RPG_AVM
    memset(chunk_trace_count, 0, sizeof(chunk_trace_count));
    memset(feature_trace_count, 0, sizeof(feature_trace_count));
#endif
    // Complete gameplay, then every tile update, before allowing blocking dialogue.
    RPG_PHASE(5);
    process_all_chunks<STAGE_GAMEPLAY>();
    // Teleports abandon outgoing gameplay. Only tiles run at the destination.
    refresh_tile_cache();
    RPG_PHASE(6);
    repair_player_overlap();
    npc_reconcile();
    if(!teleported) {
        if(simulate && !(pressed & BTN_A))
            npc_update();
        npc_interact();
        // Blocking work sees the complete cache. Flag changes wait for the next tiles pass.
        RPG_PHASE(5);
        process_all_chunks<STAGE_DIALOGUE>();
        npc_dialogue();
        if(teleported) {
            refresh_tile_cache();
            RPG_PHASE(6);
            repair_player_overlap();
        }
    }
    RPG_PHASE(6);
    game.previous_player_tile = {game.area,
                                 (uint8_t)((game.player.x + FOOT_CENTER_X) >> TILE_SHIFT),
                                 (uint8_t)((game.player.y + FOOT_CENTER_Y) >> TILE_SHIFT)};
    return teleported;
}

void world_enter(uint8_t area, uint16_t x, uint16_t y, DIRECTION facing) {
    Area const PROGMEM* a = world_area(area);
    if(!a)
        return;
    foot_clear_known = false;
    game.area = area;
    npc_reset();
    game.dimensions = {a->width, a->height};
    game.player = {x, y, facing, 0};
    game.previous_player_tile = {area, (uint8_t)((x + FOOT_CENTER_X) >> TILE_SHIFT),
                                 (uint8_t)((y + FOOT_CENTER_Y) >> TILE_SHIFT)};
    update_visible_world(0, false);
}

template void process_all_chunks<STAGE_GAMEPLAY>();
template void process_all_chunks<STAGE_TILES>();
template void process_all_chunks<STAGE_DIALOGUE>();
