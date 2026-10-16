#include "chunk.h"
#include "world_assets.h"

#include <string.h>

uint8_t frame_pressed;
bool teleported;

#ifndef RPG_AVM
uint16_t chunk_trace[3][4];
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
    uint8_t columns = (uint8_t)(((uint16_t)width + CHUNK_WIDTH - 1) / CHUNK_WIDTH);
    uint16_t max_x = width > 16 ? (uint16_t)(width - 16) * 8 : 0;
    uint16_t max_y = height > 8 ? (uint16_t)(height - 8) * 8 : 0;
    if(camera.x > max_x)
        camera.x = max_x;
    if(camera.y > max_y)
        camera.y = max_y;
    cache.origin_tx = (uint8_t)(camera.x >> 3);
    cache.origin_ty = (uint8_t)(camera.y >> 3);
    uint8_t* dst = cache.tiles;

    // Contiguous row spans use the native program-memory copy service. No full
    // chunk is ever resident. The sentinel also fills the extra edge row/column.
    for(uint8_t row = 0; row < CACHE_HEIGHT; ++row) {
        memset(dst, TILE_OUTSIDE, CACHE_WIDTH);
        uint16_t y = (uint16_t)cache.origin_ty + row;
        uint16_t x = cache.origin_tx;
        uint8_t done = 0;
        if(y < height) {
            while(done < CACHE_WIDTH && x < width) {
                uint8_t count = (uint8_t)(16 - (x & 15));
                if(count > CACHE_WIDTH - done)
                    count = (uint8_t)(CACHE_WIDTH - done);
                if(count > width - x)
                    count = (uint8_t)(width - x);
                uint16_t chunk = (y >> 3) * columns + (x >> 4);
                uint16_t offset = chunk * 128 + (y & 7) * 16 + (x & 15);
                memcpy(dst + done, tiles + offset, count);
                done = (uint8_t)(done + count);
                x += count;
            }
        }
        dst += CACHE_WIDTH;
    }
}

void reload_tile_cache() {
    game.camera = game_camera();
    reload_tile_window(game.cache, world_area(game.area)->tiles, game.dimensions, game.camera);
}

void set_tile(TilePos pos, uint8_t tile) {
    if(pos.area != game.area || pos.x >= game.dimensions.width_tiles ||
       pos.y >= game.dimensions.height_tiles)
        return;
    uint16_t x = (uint16_t)(pos.x - game.cache.origin_tx);
    uint16_t y = (uint16_t)(pos.y - game.cache.origin_ty);
    if(x < CACHE_WIDTH && y < CACHE_HEIGHT)
        game.cache.tiles[y * CACHE_WIDTH + x] = tile;
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

bool terrain_clear(uint16_t x, uint16_t y) {
    if(!game.dimensions.pixel_bounds(x, y))
        return false;
    // Pixel bounds checked before narrowing to byte tile coordinates.
    uint8_t x1 = (uint8_t)((x + 1) >> 3), x2 = (uint8_t)((x + 6) >> 3);
    uint8_t y1 = (uint8_t)((y + 3) >> 3), y2 = (uint8_t)((y + 7) >> 3);
    for(uint8_t ty = y1; ty <= y2; ++ty)
        for(uint8_t tx = x1; tx <= x2; ++tx)
            if(tile_solid(cached_tile({game.area, tx, ty})))
                return false;
    return true;
}

bool Game::player_is_on_tile(TilePos tile) const {
    return tile.area == area && dimensions.pixel_bounds(player.x, player.y) &&
           (player.x + 4) >> 3 == tile.x && (player.y + 6) >> 3 == tile.y;
}

bool Game::player_is_in_region(TileRegion region) const {
    if(region.origin.area != area || !dimensions.pixel_bounds(player.x, player.y))
        return false;
    uint16_t right = (uint16_t)region.origin.x + region.width;
    uint16_t bottom = (uint16_t)region.origin.y + region.height;
    if(!region.width || !region.height || right > dimensions.width_tiles ||
       bottom > dimensions.height_tiles)
        return false;
    uint16_t x = (player.x + 4) >> 3, y = (player.y + 6) >> 3;
    return region.contains({area, (uint8_t)x, (uint8_t)y});
}

// A closing feature ejects N/E/S/W at increasing distances of 1..8 pixels.
static bool eject_actor(Actor& actor) {
    uint16_t x = actor.x, y = actor.y;
    for(uint8_t radius = 1; radius <= 8; ++radius) {
        for(uint8_t dir = NORTH; dir <= WEST; ++dir) {
            int16_t nx = (int16_t)x, ny = (int16_t)y;
            if(dir == NORTH)
                ny -= radius;
            if(dir == EAST)
                nx += radius;
            if(dir == SOUTH)
                ny += radius;
            if(dir == WEST)
                nx -= radius;
            if(nx >= 0 && ny >= 0 && terrain_clear((uint16_t)nx, (uint16_t)ny)) {
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
    uint8_t left = (uint8_t)(camera.x >> 3), top = (uint8_t)(camera.y >> 3);
    uint16_t right = (uint16_t)left + CACHE_WIDTH - 1;
    uint16_t bottom = (uint16_t)top + CACHE_HEIGHT - 1;
    if(right >= game.dimensions.width_tiles)
        right = game.dimensions.width_tiles - 1;
    if(bottom >= game.dimensions.height_tiles)
        bottom = game.dimensions.height_tiles - 1;
    for(uint8_t cy = (uint8_t)(top >> 3); cy <= bottom >> 3; ++cy) {
        for(uint8_t cx = (uint8_t)(left >> 4); cx <= right >> 4; ++cx) {
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
    if(!terrain_clear(game.player.x, game.player.y) && !eject_actor(game.player)) {
        Area const PROGMEM* a = world_area(game.area);
        game.player.x = a->spawn.x;
        game.player.y = a->spawn.y;
        game.player.walking = 0;
        game.can_interact = false;
        refresh_tile_cache();
        RPG_PHASE(6);
    }
}

bool update_visible_world(uint8_t pressed) {
    frame_pressed = pressed;
    teleported = false;
    game.can_interact = false;
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
    if(!teleported) {
        // Blocking work sees the complete cache. Flag changes wait for the next tiles pass.
        RPG_PHASE(5);
        process_all_chunks<STAGE_DIALOGUE>();
        if(teleported) {
            refresh_tile_cache();
            RPG_PHASE(6);
            repair_player_overlap();
        }
    }
    RPG_PHASE(6);
    game.previous_player_tile = {game.area, (uint8_t)((game.player.x + 4) >> 3),
                                 (uint8_t)((game.player.y + 6) >> 3)};
    return teleported;
}

void world_enter(uint8_t area, uint16_t x, uint16_t y, uint8_t facing) {
    Area const PROGMEM* a = world_area(area);
    if(!a)
        return;
    game.area = area;
    game.dimensions = {a->width, a->height};
    game.player = {x, y, facing, 0};
    game.previous_player_tile = {area, (uint8_t)((x + 4) >> 3), (uint8_t)((y + 6) >> 3)};
    update_visible_world();
}

template void process_all_chunks<STAGE_GAMEPLAY>();
template void process_all_chunks<STAGE_TILES>();
template void process_all_chunks<STAGE_DIALOGUE>();
