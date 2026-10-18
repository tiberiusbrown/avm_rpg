#include "world_assets.h"

struct LargeTiles {
    uint8_t tiles[30u * 62u * 32u];
};

constexpr LargeTiles make_large_tiles() {
    LargeTiles result{};
    result.tiles[1859u * 32u] = TILE_SIGN;
    result.tiles[59519u] = TILE_GRASS;
    return result;
}

static constexpr LargeTiles PROGMEM LARGE_TILES = make_large_tiles();

static void run_world_scenarios() {
    game_new();
    CHECK(TILE == 16 && CHUNK_WIDTH == 8 && CHUNK_HEIGHT == 4);
    CHECK(CACHE_WIDTH == 9 && CACHE_HEIGHT == 5 && sizeof(TileCache) == 47);
    CHECK(TILE_COUNT == 255 && TILE_OUTSIDE == 255 && TILE_SIGN == 0x41);
    CHECK(tile_solid(TILE_OUTSIDE) && !tile_portal(TILE_OUTSIDE));
    CHECK(!tile_solid(TILE_GROUND) && tile_solid(TILE_SIGN));
    game.dimensions = {240, 248};
    reload_tile_window(game.cache, LARGE_TILES.tiles, {240, 248}, {65535, 65535});
    CHECK(game.cache.origin_tx == 232 && game.cache.origin_ty == 244);
    CHECK(game.cache.tiles[0] == TILE_SIGN);
    CHECK(game.cache.tiles[3 * CACHE_WIDTH + 7] == TILE_GRASS);
    CHECK(game.cache.tiles[8] == TILE_OUTSIDE && game.cache.tiles[4 * CACHE_WIDTH] == TILE_OUTSIDE);
    CHECK(cached_tile({2, 232, 244}) == TILE_OUTSIDE);
    CHECK(cached_tile({game.area, 231, 244}) == TILE_OUTSIDE);
    set_tile({2, 232, 244}, TILE_TREE);
    CHECK(game.cache.tiles[0] == TILE_SIGN);
    set_tile({game.area, 232, 244}, TILE_TREE);
    CHECK(game.cache.tiles[0] == TILE_TREE);
    set_tile({game.area, 255, 255}, TILE_TREE);
    CHECK(game.cache.tiles[8] == TILE_OUTSIDE);
    reload_tile_window(game.cache, nullptr, {1, 1}, {0, 0});
    CHECK(game.cache.origin_tx == 0 && game.cache.origin_ty == 0);
    for(auto tile : game.cache.tiles)
        CHECK(tile == TILE_OUTSIDE);
    game_new();
    CHECK(game.player_is_on_tile(positions::PlayerStart));
    CHECK((game.player_is_in_region({{AREA_OUTDOOR, 6, 4}, 3, 3})));
    CHECK((!game.player_is_in_region({{2, 6, 4}, 3, 3})));
    CHECK((!game.player_is_in_region({{AREA_OUTDOOR, 17, 11}, 2, 2})));
    CHECK((!teleport_player<TilePos{AREA_OUTDOOR, 255, 255}>()));
    CHECK((!teleport_player<TilePos{2, 1, 1}>()));
    CHECK(teleport_player<positions::PlayerStart>(EAST));
    CHECK(game.player.x == 112 && game.player.y == 80 && game.player.facing == EAST);
    CHECK(!game.player.walking && teleported);
}
