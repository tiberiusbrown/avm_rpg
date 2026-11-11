#include "world_assets.h"

// Two authored tiles in the far corner of the largest logical map.
constexpr unsigned SIGN_X = MAX_MAP_WIDTH - 8, SIGN_Y = MAX_MAP_HEIGHT - 4;
constexpr unsigned CORNER_X = MAX_MAP_WIDTH - 1, CORNER_Y = MAX_MAP_HEIGHT - 1;

struct LargeTiles {
    uint8_t tiles[240u * 248u]; // MAX_MAP_WIDTH x MAX_MAP_HEIGHT.
};
static_assert(MAX_MAP_WIDTH == 240 && MAX_MAP_HEIGHT == 248);

constexpr LargeTiles make_large_tiles() {
    LargeTiles result{};
    result.tiles[SIGN_Y * MAX_MAP_WIDTH + SIGN_X] = TILE_SIGN;
    result.tiles[CORNER_Y * MAX_MAP_WIDTH + CORNER_X] = TILE_GRASS_06;
    return result;
}

static constexpr LargeTiles PROGMEM LARGE_TILES = make_large_tiles();

static void run_world_scenarios() {
    game_new();
    CHECK(TILE == 16 && CHUNK_WIDTH == 8 && CHUNK_HEIGHT == 4);
    CHECK(CACHE_WIDTH == 11 && CACHE_HEIGHT == 7 && sizeof(TileCache) == 79);
    CHECK(TILE_COUNT == 255 && TILE_OUTSIDE == 255 && TILE_SIGN == 0x41);
    CHECK(tile_solid(TILE_OUTSIDE) && !tile_portal(TILE_OUTSIDE));
    CHECK(!tile_solid(TILE_GRASS_06) && !tile_solid(TILE_FLOWERS_26) && tile_solid(TILE_SIGN));
    CHECK(tile_solid(TILE_EMPTY) && tile_solid(TILE_SWITCH_LEFT) && tile_solid(TILE_POT));
    CHECK(tile_solid(TILE_POT_BROKEN) && tile_solid(TILE_CHEST_CLOSED) && tile_solid(TILE_CHEST_OPEN));
    game.dimensions = {MAX_MAP_WIDTH, MAX_MAP_HEIGHT};
    reload_tile_window(game.cache, LARGE_TILES.tiles, {MAX_MAP_WIDTH, MAX_MAP_HEIGHT},
                       {(uint16_t)(MAX_MAP_WIDTH * TILE), (uint16_t)(MAX_MAP_HEIGHT * TILE)});
    // The view clamps to the last full screen, and the cache trails it by one tile.
    CHECK(game.cache.origin_tx == MAX_MAP_WIDTH - 9 && game.cache.origin_ty == MAX_MAP_HEIGHT - 5);
    CHECK(game.cache.tiles[1 * CACHE_WIDTH + 1] == TILE_SIGN);     // Authored at SIGN_X, SIGN_Y.
    CHECK(game.cache.tiles[4 * CACHE_WIDTH + 8] == TILE_GRASS_06); // Authored at CORNER_X, CORNER_Y.
    CHECK(game.cache.tiles[9] == TILE_OUTSIDE && game.cache.tiles[5 * CACHE_WIDTH] == TILE_OUTSIDE);
    TilePos other_area{(uint8_t)(game.area + 1), SIGN_X, SIGN_Y};
    CHECK(cached_tile(other_area) == TILE_OUTSIDE);
    CHECK(cached_tile({game.area, SIGN_X - 2, SIGN_Y}) == TILE_OUTSIDE);
    set_tile(other_area, TILE_TREE);
    CHECK(game.cache.tiles[1 * CACHE_WIDTH + 1] == TILE_SIGN);
    set_tile({game.area, SIGN_X, SIGN_Y}, TILE_TREE);
    CHECK(game.cache.tiles[1 * CACHE_WIDTH + 1] == TILE_TREE);
    set_tile({game.area, MAX_MAP_WIDTH, MAX_MAP_HEIGHT}, TILE_TREE);
    CHECK(game.cache.tiles[9] == TILE_OUTSIDE);
    reload_tile_window(game.cache, nullptr, {1, 1},
                       {(uint16_t)(positions::PlayerStart.x * TILE), (uint16_t)(positions::PlayerStart.y * TILE)});
    CHECK(game.cache.origin_tx == 0 && game.cache.origin_ty == 0);
    for(auto tile : game.cache.tiles)
        CHECK(tile == TILE_OUTSIDE);
    game_new();
    CHECK(game.player_is_on_tile(positions::PlayerStart));
    CHECK((game.player_is_in_region({positions::PlayerStart, 3, 3})));
    CHECK((!game.player_is_in_region({{(uint8_t)(AREA_OUTDOOR + 1), positions::PlayerStart.x, positions::PlayerStart.y}, 3, 3})));
    CHECK((!game.player_is_in_region({positions::VillagerEast, 2, 2})));
    CHECK((!teleport_player<TilePos{AREA_OUTDOOR, MAX_MAP_WIDTH, MAX_MAP_HEIGHT}>()));
    CHECK((!teleport_player<TilePos{255, positions::PlayerStart.x, positions::PlayerStart.y}>())); // No such area.
    CHECK(teleport_player<positions::PlayerStart>(EAST));
    CHECK(game.player.x == positions::PlayerStart.x * TILE &&
          game.player.y == positions::PlayerStart.y * TILE && game.player.facing == EAST);
    CHECK(!game.player.walking && teleported);
}
