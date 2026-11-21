#include "encounter.h"
#include "chunk.h"
#include "logic/npc_logic.h"
#include "quest.h"
#include "world_assets.h"

#include <string.h>

uint8_t frame_pressed;
bool teleported;

// The foot position that last passed player_terrain_clear (foot_cache) answers for the area, the
// position, and the terrain. Terrain changes only through tile edits and reloads that discard them,
// so until one of those, a repeat of this position needs no tile reads.
static bool player_foot_clear_known() {
    return foot_cache.known && foot_cache.area == game.area && foot_cache.x == game.player.x &&
           foot_cache.y == game.player.y;
}

// Terrain changed under the NPCs: none keeps known-clear feet until a full check confirms them.
static void npc_feet_unknown() {
    for(auto& n : game.npcs.actors) {
        if(!n.id)
            break;
        n.policy_pending &= (uint8_t)~4;
    }
    game.npcs.terrain_dirty = 1;
}

void note_tile_edit() {
    foot_cache.known = false;
    foot_cache.tiles_edited = true;
    npc_feet_unknown();
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
    // One 256-entry block per tileset. The active area selects the block.
    return TILE_COLLISION[(uint16_t)((uint16_t)area_tileset(game.area) << 8 | tile)];
}

uint8_t tile_ambient(uint8_t tile) {
    return TILE_AMBIENT[(uint16_t)((uint16_t)area_tileset(game.area) << 8 | tile)];
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

// The same quadrant shapes read down a column, for vertical leading edges.
static constexpr auto QUADRANT_COLUMNS PROGMEM = [] {
    struct Columns {
        uint8_t col[16][8];
    } columns{};
    for(uint8_t nibble = 0; nibble < 16; ++nibble)
        for(uint8_t x = 0; x < 8; ++x)
            for(uint8_t y = 0; y < 8; ++y)
                if(quadrant_solid(nibble, x, y))
                    columns.col[nibble][x] |= (uint8_t)(1u << y);
    return columns;
}();

bool collision_pixel_solid(uint16_t mask, uint8_t x, uint8_t y) {
    // Triangles are closed shapes. A pixel center on a quadrant diagonal is solid when either
    // triangle that shares the diagonal is solid, so a diagonal edge rasterizes as one line.
    uint8_t quadrant = (uint8_t)(((y >> 3) << 1) | (x >> 3));
    return quadrant_solid((uint8_t)((mask >> (quadrant * 4)) & 0xF), (uint8_t)(x & 7),
                          (uint8_t)(y & 7));
}

#ifndef RPG_AVM
uint16_t chunk_trace[4][CHUNK_TRACE_CAPACITY];
uint8_t chunk_trace_count[4];
unsigned feature_trace_count[4];
#endif

template <uint8_t ROW = 0>
static void copy_full_tile_rows(uint8_t* dst, uint8_t const PROGMEM* src, uint8_t width) {
    memcpy_P(dst, src, CACHE_WIDTH);
    if constexpr(ROW + 1 < CACHE_HEIGHT)
        copy_full_tile_rows<ROW + 1>(dst + CACHE_WIDTH, src + width, width);
}

static void reload_tile_window_clamped(TileCache& cache, uint8_t const PROGMEM* tiles,
                                      MapDimensions dimensions, Camera camera) {
    if(!tiles) {
        memset(&cache, TILE_OUTSIDE, sizeof(cache));
        cache.origin_tx = cache.origin_ty = 0;
        return;
    }
    uint8_t width = dimensions.width_tiles, height = dimensions.height_tiles;
    uint8_t view_tx = (uint8_t)(camera.x >> TILE_SHIFT),
            view_ty = (uint8_t)(camera.y >> TILE_SHIFT);
    cache.origin_tx = view_tx >= CACHE_MARGIN ? (uint8_t)(view_tx - CACHE_MARGIN) : 0;
    cache.origin_ty = view_ty >= CACHE_MARGIN ? (uint8_t)(view_ty - CACHE_MARGIN) : 0;
    uint8_t rows = height - cache.origin_ty;
    if(rows > CACHE_HEIGHT)
        rows = CACHE_HEIGHT;
    uint8_t row_bytes = width - cache.origin_tx;
    if(row_bytes > CACHE_WIDTH)
        row_bytes = CACHE_WIDTH;
    // Interior windows overwrite every cell. Only map edges need sentinel fill.
    if(rows < CACHE_HEIGHT || row_bytes < CACHE_WIDTH)
        memset(cache.tiles, TILE_OUTSIDE, sizeof(cache.tiles));
    if(!rows || !row_bytes)
        return;
    uint8_t* dst = cache.tiles;
    uint8_t const PROGMEM* src = tiles + (uint16_t)cache.origin_ty * width + cache.origin_tx;

    if(rows == CACHE_HEIGHT && row_bytes == CACHE_WIDTH) {
        copy_full_tile_rows(dst, src, width);
        return;
    }

    // Map rows are contiguous. Uncopied right/bottom cells retain the sentinel.
    for(uint8_t row = 0; row < rows; ++row) {
        memcpy_P(dst, src, row_bytes);
        dst += CACHE_WIDTH;
        src += width;
    }
}

void reload_tile_window(TileCache& cache, uint8_t const PROGMEM* tiles, MapDimensions dimensions,
                        Camera camera) {
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
    reload_tile_window_clamped(cache, tiles, dimensions, camera);
}

void reload_tile_cache() {
    camera = game_camera();
    if(foot_cache.tiles_edited) {
        npc_feet_unknown();
        foot_cache.known = false;
        foot_cache.tiles_edited = false;
    }
    reload_tile_window_clamped(cache, world_area(game.area)->tiles, game.dimensions, camera);
}

void world_reclaim_workspace() {
    // A battle overwrote the window, the camera and the foot cache, so none of their answers hold.
    foot_cache.known = false;
    foot_cache.tiles_edited = false;
    npc_feet_unknown();
    // The reload rebuilds the window from the world assets, and the tile pass reapplies every
    // persistent tile change from its flags, as each frame does. Nothing else is needed.
    refresh_tile_cache();
}

void set_tile(TilePos pos, uint8_t tile) {
    if(pos.area != game.area || pos.x >= game.dimensions.width_tiles ||
       pos.y >= game.dimensions.height_tiles)
        return;
    uint16_t x = (uint16_t)(pos.x - cache.origin_tx);
    uint16_t y = (uint16_t)(pos.y - cache.origin_ty);
    if(x < CACHE_WIDTH && y < CACHE_HEIGHT) {
        note_tile_edit();
        cache.tiles[y * CACHE_WIDTH + x] = tile;
    }
}

uint8_t cached_tile(TilePos pos) {
    if(pos.area != game.area)
        return TILE_OUTSIDE;
    int16_t x = (int16_t)pos.x - cache.origin_tx;
    int16_t y = (int16_t)pos.y - cache.origin_ty;
    if(x >= 0 && x < CACHE_WIDTH && y >= 0 && y < CACHE_HEIGHT)
        return cache.tiles[(uint16_t)y * CACHE_WIDTH + x];
    return TILE_OUTSIDE;
}

// Each part of the foot box in one tile is either the full span, a prefix or
// a suffix. Encode those TILE + width - 1 spans, separately for each axis.
static constexpr uint8_t FOOT_X_SPANS = TILE + FOOT_WIDTH - 1;
static constexpr uint8_t FOOT_Y_SPANS = TILE + FOOT_HEIGHT - 1;

static constexpr auto FOOT_TRIANGLES PROGMEM = [] {
    struct Rectangles {
        uint16_t mask[FOOT_X_SPANS * FOOT_Y_SPANS];
    } rectangles{};
    for(uint8_t yi = 0; yi < FOOT_Y_SPANS; ++yi) {
        uint8_t y0 = yi < FOOT_HEIGHT ? 0 : yi - FOOT_HEIGHT + 1;
        uint8_t y1 = yi < FOOT_HEIGHT ? yi : y0 + FOOT_HEIGHT - 1;
        if(y1 >= TILE)
            y1 = TILE - 1;
        for(uint8_t xi = 0; xi < FOOT_X_SPANS; ++xi) {
            uint8_t x0 = xi < FOOT_WIDTH ? 0 : xi - FOOT_WIDTH + 1;
            uint8_t x1 = xi < FOOT_WIDTH ? xi : x0 + FOOT_WIDTH - 1;
            if(x1 >= TILE)
                x1 = TILE - 1;
            uint16_t mask = 0;
            for(uint8_t qy = 0; qy < 2; ++qy) {
                for(uint8_t qx = 0; qx < 2; ++qx) {
                    uint8_t ox = qx * 8, oy = qy * 8;
                    if(x1 < ox || x0 >= ox + 8 || y1 < oy || y0 >= oy + 8)
                        continue;
                    uint8_t left = x0 > ox ? x0 - ox : 0;
                    uint8_t right = x1 < ox + 7 ? x1 - ox : 7;
                    uint8_t top = y0 > oy ? y0 - oy : 0;
                    uint8_t bottom = y1 < oy + 7 ? y1 - oy : 7;
                    uint8_t flags = 0;
                    if(top < 4 && right >= top && left + top <= 7)
                        flags |= 1;
                    if(right >= 4 && bottom >= 7 - right && top <= right)
                        flags |= 2;
                    if(bottom >= 4 && right >= 7 - bottom && left <= bottom)
                        flags |= 4;
                    if(left < 4 && bottom >= left && top + left <= 7)
                        flags |= 8;
                    mask |= (uint16_t)flags << (qy * 8 + qx * 4);
                }
            }
            rectangles.mask[(uint16_t)yi * FOOT_X_SPANS + xi] = mask;
        }
    }
    return rectangles;
}();

// Each intersected tile sees a prefix, a suffix, or the full foot span.
// The same offset encodes all three, avoiding clipping and per-row scans.
RPG_NOINLINE static bool foot_hits_mask(uint16_t mask, uint16_t left, uint16_t top, uint16_t tile) {
    uint8_t xi = (uint8_t)(left + FOOT_WIDTH - 1 - (uint16_t)(uint8_t)tile * TILE);
    uint8_t yi = (uint8_t)(top + FOOT_HEIGHT - 1 - (uint16_t)(tile >> 8) * TILE);
    return (mask & FOOT_TRIANGLES.mask[(uint16_t)yi * FOOT_X_SPANS + xi]) != 0;
}

__attribute__((noinline)) uint8_t player_leading_edge(uint16_t x, uint16_t y, DIRECTION dir) {
    static_assert(FOOT_WIDTH == 8 && FOOT_HEIGHT == 8, "sidestep bits cover the 8-pixel edge");
    bool horizontal = dir == EAST || dir == WEST;
    // The column of an east or west step, or the row of a north or south one.
    uint16_t fixed = horizontal ? (uint16_t)(x + (dir == EAST ? FOOT_RIGHT : FOOT_LEFT))
                                : (uint16_t)(y + (dir == SOUTH ? FOOT_BOTTOM : FOOT_TOP));
    if(!game.dimensions.pixel_bounds(horizontal ? fixed : 0, horizontal ? 0 : fixed))
        return 0xFF;
    uint16_t along = horizontal ? (uint16_t)(y + FOOT_TOP) : (uint16_t)(x + FOOT_LEFT);
    uint16_t limit =
        (uint16_t)(horizontal ? game.dimensions.height_tiles : game.dimensions.width_tiles) * TILE -
        TILE;
    uint8_t const PROGMEM* lines = horizontal
                                       ? reinterpret_cast<uint8_t const PROGMEM*>(&QUADRANT_COLUMNS)
                                       : reinterpret_cast<uint8_t const PROGMEM*>(&QUADRANT_ROWS);
    uint8_t bits = 0;
    uint16_t tile_key = 0, mask = 0;
    bool loaded = false;
    for(uint8_t i = 0; i < 8;) {
        // A run ends at the next quadrant, map boundary or end of the edge. One
        // table byte tests each run; advancing the coordinate preserves 16-bit wrap.
        uint8_t offset = (uint8_t)(along & 7);
        uint8_t count = (uint8_t)(8 - offset);
        if(count > 8 - i)
            count = (uint8_t)(8 - i);
        uint8_t line = 0xFF;
        if(along <= limit) {
            if(limit - along < count)
                count = (uint8_t)(limit - along + 1);
            uint16_t wx = horizontal ? fixed : along;
            uint16_t wy = horizontal ? along : fixed;
            uint16_t key = (uint16_t)(((wy >> TILE_SHIFT) << 8) | (wx >> TILE_SHIFT));
            if(!loaded || key != tile_key) {
                tile_key = key;
                loaded = true;
                mask = tile_collision(cached_tile(
                    {game.area, (uint8_t)(wx >> TILE_SHIFT), (uint8_t)(wy >> TILE_SHIFT)}));
            }
            uint8_t shift = horizontal ? (uint8_t)((along & 8) | ((fixed & 8) >> 1))
                                       : (uint8_t)((fixed & 8) | ((along & 8) >> 1));
            uint8_t nibble = (uint8_t)((mask >> shift) & 0xF);
            line = nibble == 0xF ? 0xFF : nibble ? lines[nibble * 8 + (fixed & 7)] : 0;
        }
        bits |= (uint8_t)(((line >> offset) & ((1u << count) - 1)) << i);
        along = (uint16_t)(along + count);
        i = (uint8_t)(i + count);
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
            if(foot_hits_mask(mask, left, top, (uint16_t)(ty << 8 | tx)))
                return false;
        }
    }
    foot_cache.known = true;
    foot_cache.area = game.area;
    foot_cache.x = x;
    foot_cache.y = y;
    return true;
}

bool terrain_clear(uint16_t x, uint16_t y) {
    if(!game.dimensions.pixel_bounds(x, y))
        return false;
        // Every tile in this footprint uses the same collision atlas. Resolve its
        // program-memory base once, rather than repeating the area switch and base
        // address arithmetic for each of up to four cached tile reads.
#ifdef RPG_AVM
    auto const* masks = &TILE_COLLISION[(uint16_t)area_tileset(game.area) << 8];
    auto solid = [masks](TilePos pos) { return masks[cached_tile(pos)] != 0; };
#else
    // Native collision overrides remain observable by the rules tests.
    auto solid = [](TilePos pos) { return tile_solid(cached_tile(pos)); };
#endif
    // Pixel bounds checked before narrowing to byte tile coordinates.
    uint8_t x1 = (uint8_t)((x + FOOT_LEFT) >> TILE_SHIFT),
            x2 = (uint8_t)((x + FOOT_RIGHT) >> TILE_SHIFT);
    uint8_t y1 = (uint8_t)((y + FOOT_TOP) >> TILE_SHIFT),
            y2 = (uint8_t)((y + FOOT_BOTTOM) >> TILE_SHIFT);
    // A foot box intersects at most two columns and two rows. Preserve the
    // row-major short circuit without loop counters or duplicate tile reads.
    static_assert(FOOT_WIDTH <= TILE && FOOT_HEIGHT <= TILE);
    if(solid({game.area, x1, y1}))
        return false;
    if(x2 != x1 && solid({game.area, x2, y1}))
        return false;
    if(y2 != y1) {
        if(solid({game.area, x1, y2}))
            return false;
        if(x2 != x1 && solid({game.area, x2, y2}))
            return false;
    }
    return true;
}

// One pixel of a step from a footprint already known clear. Every tile the old footprint covered
// was clear, so a tile can block only if the step enters it. (x, y) is the new position.
bool terrain_step_clear(uint16_t x, uint16_t y, DIRECTION dir) {
    if(!game.dimensions.pixel_bounds(x, y))
        return false;
    bool horizontal = dir == EAST || dir == WEST;
    // The leading edge of the step, and the edge it leaves one pixel behind.
    uint16_t lead = horizontal ? (uint16_t)(x + (dir == EAST ? FOOT_RIGHT : FOOT_LEFT))
                               : (uint16_t)(y + (dir == SOUTH ? FOOT_BOTTOM : FOOT_TOP));
    uint16_t trail = dir == EAST || dir == SOUTH ? (uint16_t)(lead - 1) : (uint16_t)(lead + 1);
    if((lead >> TILE_SHIFT) == (trail >> TILE_SHIFT))
        return true;
    // The entered column or row, and the tiles it spans along the edge.
    uint8_t entered = (uint8_t)(lead >> TILE_SHIFT);
    uint16_t first = horizontal ? (uint16_t)(y + FOOT_TOP) : (uint16_t)(x + FOOT_LEFT);
    uint16_t last = horizontal ? (uint16_t)(y + FOOT_BOTTOM) : (uint16_t)(x + FOOT_RIGHT);
#ifdef RPG_AVM
    auto const* masks = &TILE_COLLISION[(uint16_t)area_tileset(game.area) << 8];
    auto solid = [masks](TilePos pos) { return masks[cached_tile(pos)] != 0; };
#else
    auto solid = [](TilePos pos) { return tile_solid(cached_tile(pos)); };
#endif
    for(uint16_t along = first >> TILE_SHIFT; along <= (last >> TILE_SHIFT); ++along) {
        TilePos pos = horizontal ? TilePos{game.area, entered, (uint8_t)along}
                                 : TilePos{game.area, (uint8_t)along, entered};
        if(solid(pos))
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
    npc_stage_begin();
    process_all_chunks<STAGE_NPCS>();
    npc_stage_end();
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
            npc_stage_begin();
            process_all_chunks<STAGE_NPCS>();
            npc_stage_end();
        }
    }
    // Quest evaluation runs after every world stage and dialogue, and before the frame ends. It
    // never runs inside a stage, so it cannot reenter the world update.
    quest_settle();
    RPG_PHASE(6);
    game.previous_player_tile = {game.area,
                                 (uint8_t)((game.player.x + FOOT_CENTER_X) >> TILE_SHIFT),
                                 (uint8_t)((game.player.y + FOOT_CENTER_Y) >> TILE_SHIFT)};
    if(teleported)
        fade_world_in();
    return teleported;
}

bool world_place(uint8_t area, uint16_t x, uint16_t y, DIRECTION facing) {
    Area const PROGMEM* a = world_area(area);
    if(!a)
        return false;
    foot_cache.known = false;
    game.area = area;
    encounter_restore();
    npc_reset();
    game.dimensions = {a->width, a->height};
    game.player = {x, y, facing, 0};
    game.previous_player_tile = {area, (uint8_t)((x + FOOT_CENTER_X) >> TILE_SHIFT),
                                 (uint8_t)((y + FOOT_CENTER_Y) >> TILE_SHIFT)};
    return true;
}

void world_enter(uint8_t area, uint16_t x, uint16_t y, DIRECTION facing) {
    if(world_place(area, x, y, facing))
        update_visible_world(0, false);
}
