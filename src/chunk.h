#ifndef CHUNK_H
#define CHUNK_H

#include "rpg.h"
#include "npc.h"

enum HandlerStage : uint8_t { STAGE_GAMEPLAY, STAGE_TILES, STAGE_DIALOGUE };

// Frame events are shared by gameplay handlers and reset for each update.
extern uint8_t frame_pressed;
extern bool teleported;

void set_tile(TilePos pos, uint8_t tile);

template <TilePos POS> void set_tile(uint8_t tile) {
    if(POS.area != game.area || POS.x >= game.dimensions.width_tiles ||
       POS.y >= game.dimensions.height_tiles)
        return;
    uint16_t x = (uint16_t)(POS.x - game.cache.origin_tx);
    uint16_t y = (uint16_t)(POS.y - game.cache.origin_ty);
    if(x < CACHE_WIDTH && y < CACHE_HEIGHT)
        game.cache.tiles[y * CACHE_WIDTH + x] = tile;
}

template <TilePos POS> bool player_is_walking_onto() {
    return game.player.walking && game.player_is_on_tile<POS>() &&
           (game.previous_player_tile.area != POS.area || game.previous_player_tile.x != POS.x ||
            game.previous_player_tile.y != POS.y);
}

template <TilePos POS> bool player_just_interacted() {
    if(POS.area != game.area || !game.dimensions.pixel_bounds(game.player.x, game.player.y))
        return false;
    int16_t x = (int16_t)((game.player.x + FOOT_CENTER_X) >> TILE_SHIFT),
            y = (int16_t)((game.player.y + FOOT_CENTER_Y) >> TILE_SHIFT);
    switch(game.player.facing) {
        case NORTH:
            --y;
            break;
        case EAST:
            ++x;
            break;
        case SOUTH:
            ++y;
            break;
        case WEST:
            --x;
            break;
        default:
            return false;
    }
    if(x < 0 || y < 0 || x >= game.dimensions.width_tiles || y >= game.dimensions.height_tiles ||
       x != POS.x || y != POS.y)
        return false;
    game.can_interact = true;
    if(!(frame_pressed & BTN_A))
        return false;
    frame_pressed &= (uint8_t)~BTN_A;
    return true;
}

template <TilePos TARGET> bool teleport_player(DIRECTION facing = STILL) {
    if(teleported || facing > STILL)
        return false;
    MapDimensions dimensions = game.dimensions;
    if(TARGET.area != game.area) {
        Area const PROGMEM* a = world_area(TARGET.area);
        if(!a)
            return false;
        dimensions = {a->width, a->height};
    }
    if(TARGET.x >= dimensions.width_tiles || TARGET.y >= dimensions.height_tiles)
        return false;
    game.area = TARGET.area;
    npc_reset();
    game.dimensions = dimensions;
    game.player = {(uint16_t)((uint16_t)TARGET.x * TILE), (uint16_t)((uint16_t)TARGET.y * TILE),
                   facing == STILL ? game.player.facing : facing, 0};
    game.previous_player_tile.area = TARGET.area;
    game.previous_player_tile.x = TARGET.x;
    game.previous_player_tile.y = TARGET.y;
    game.can_interact = false;
    frame_pressed = 0;
    teleported = true;
    return true;
}

template <HandlerStage STAGE> void run_chunk(ChunkCoord coord);
template <HandlerStage STAGE> void process_all_chunks();
void refresh_tile_cache();

#ifndef RPG_AVM
// Test-only trace: no guest storage or per-feature registry.
extern uint16_t chunk_trace[3][4];
extern uint8_t chunk_trace_count[3];
extern unsigned feature_trace_count[3];
#endif

#endif
