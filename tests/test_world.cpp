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
    unsigned chunk = (y / 4) * area.chunk_columns + x / 8;
    return area.tiles[chunk * 32 + (y % 4) * 8 + x % 8];
}

int main() {
    run_world_scenarios();
    game_new();
    Area const& area = *world_area(AREA_OUTDOOR);
    for(unsigned y = 0; y <= 128; y += 3) {
        for(unsigned x = 0; x <= 160; x += 5) {
            reload_tile_window(game.cache, area.tiles, {area.width, area.height},
                               {(uint16_t)x, (uint16_t)y});
            CHECK(game.cache.origin_tx == x / 16 && game.cache.origin_ty == y / 16);
            for(unsigned cy = 0; cy < CACHE_HEIGHT; ++cy)
                for(unsigned cx = 0; cx < CACHE_WIDTH; ++cx)
                    CHECK(game.cache.tiles[cy * CACHE_WIDTH + cx] ==
                          authored_tile(area, x / 16 + cx, y / 16 + cy));
        }
    }
    // A scrolling window intersects at most four 8x4 chunks, in row-major order.
    world_enter(AREA_OUTDOOR, 7 * TILE, 5 * TILE, SOUTH);
    HandlerStage stages[] = {STAGE_GAMEPLAY, STAGE_TILES, STAGE_DIALOGUE};
    for(HandlerStage stage : stages) {
        CHECK(chunk_trace_count[stage] == 4);
        CHECK(chunk_trace[stage][0] == 0 && chunk_trace[stage][1] == 1);
        CHECK(chunk_trace[stage][2] == 3 && chunk_trace[stage][3] == 4);
    }
    CHECK(!terrain_clear(game.camera.x + CACHE_WIDTH * TILE, game.player.y));
    CHECK(cached_tile({AREA_OUTDOOR, 0, 0}) == TILE_OUTSIDE);
    printf("PASS chunk/cache world: %u checks\n", checks);
}
