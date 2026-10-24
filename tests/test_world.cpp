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

static void terrain_footprints() {
    game_new();
    game.cache.origin_tx = 3;
    game.cache.origin_ty = 2;
    // Every arrangement of four solid/passable tiles, at every pixel alignment.
    // The oracle checks each foot pixel independently of the corner algorithm.
    for(unsigned mask = 0; mask < 16; ++mask) {
        memset(game.cache.tiles, TILE_GROUND, sizeof(game.cache.tiles));
        for(unsigned tile = 0; tile < 4; ++tile)
            game.cache.tiles[(2 + tile / 2) * CACHE_WIDTH + 3 + tile % 2] =
                mask & (1u << tile) ? TILE_TREE : TILE_GROUND;
        for(unsigned sy = 0; sy < TILE; ++sy) {
            for(unsigned sx = 0; sx < TILE; ++sx) {
                uint16_t x = (uint16_t)(6 * TILE + sx - FOOT_LEFT);
                uint16_t y = (uint16_t)(4 * TILE + sy - FOOT_TOP);
                bool expected = true;
                for(unsigned fy = FOOT_TOP; fy <= FOOT_BOTTOM; ++fy) {
                    for(unsigned fx = FOOT_LEFT; fx <= FOOT_RIGHT; ++fx) {
                        unsigned tile = (((y + fy) >> TILE_SHIFT) - 4) * 2 +
                                        ((x + fx) >> TILE_SHIFT) - 6;
                        if(mask & (1u << tile))
                            expected = false;
                    }
                }
                CHECK(terrain_clear(x, y) == expected);
            }
        }
    }
}

int main() {
    run_world_scenarios();
    terrain_footprints();
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
