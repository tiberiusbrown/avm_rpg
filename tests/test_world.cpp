#include "chunk.h"
#include "world_assets.h"
#include "map_positions.h"
#include "logic/handlers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(test)                                                                                \
    do {                                                                                           \
        ++checks;                                                                                  \
        if(!(test)) {                                                                              \
            fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #test);                         \
            exit(1);                                                                               \
        }                                                                                          \
    } while(0)

bool dialogue_show(char const PROGMEM*) {
    return true;
}

static bool verify_dialogue_cache;
static TileCache expected_dialogue_cache;

bool dialogue_ask(char const PROGMEM*) {
    if(verify_dialogue_cache) {
        CHECK(chunk_trace_count[STAGE_TILES] == 4);
        CHECK(feature_trace_count[STAGE_TILES] == 10);
        CHECK(!memcmp(&expected_dialogue_cache, &game.cache, sizeof(game.cache)));
    }
    return true;
}

// Test oracle for the authored chunk-major asset, independent of cache loading.
static uint8_t authored_tile(Area const& area, unsigned x, unsigned y) {
    if(x >= area.width || y >= area.height)
        return TILE_OUTSIDE;
    unsigned chunk = (y / 8) * area.chunk_columns + x / 16;
    return area.tiles[chunk * 128 + (y % 8) * 16 + x % 16];
}

static void reloads() {
    game_new();
    for(unsigned py = 0; py <= 96; ++py) {
        for(unsigned px = 0; px <= 128; ++px) {
            reload_tile_window(game.cache, world_area(AREA_OUTDOOR)->tiles, game.dimensions,
                               {(uint16_t)px, (uint16_t)py});
            CHECK(game.cache.origin_tx == px / 8 && game.cache.origin_ty == py / 8);
            for(unsigned y = 0; y < 9; ++y)
                for(unsigned x = 0; x < 17; ++x)
                    CHECK(game.cache.tiles[y * 17 + x] ==
                          authored_tile(*world_area(AREA_OUTDOOR), px / 8 + x, py / 8 + y));
        }
    }
    // Actual largest supported layout; chunk 464 and offsets near 59519.
    static uint8_t tiles[15 * 31 * 128];
    for(unsigned i = 0; i < sizeof(tiles); ++i)
        tiles[i] = (uint8_t)((i / 128 + i % 128) % 251);
    Area a{240, 248, 15, 31, tiles, {0, 0}};
    reload_tile_window(game.cache, tiles, {240, 248}, {65535, 65535});
    CHECK(game.cache.origin_tx == 224 && game.cache.origin_ty == 240);
    for(unsigned y = 0; y < 9; ++y)
        for(unsigned x = 0; x < 17; ++x)
            CHECK(game.cache.tiles[y * 17 + x] ==
                  (x == 16 || y == 8 ? TILE_OUTSIDE : authored_tile(a, 224 + x, 240 + y)));
    reload_tile_window(game.cache, tiles, {19, 11}, {65535, 65535});
    CHECK(game.cache.origin_tx == 3 && game.cache.origin_ty == 3);
    CHECK(game.cache.tiles[16] == TILE_OUTSIDE && game.cache.tiles[8 * 17] == TILE_OUTSIDE);
}

static void mutations_and_frames() {
    game_new();
    CHECK(sizeof(game.cache.tiles) == 153);
    CHECK(chunk_trace_count[STAGE_GAMEPLAY] == 4 && chunk_trace_count[STAGE_TILES] == 4);
    for(unsigned i = 0; i < 4; ++i)
        CHECK(chunk_trace[STAGE_GAMEPLAY][i] == i && chunk_trace[STAGE_TILES][i] == i);
    unsigned features = feature_trace_count[STAGE_GAMEPLAY];
    CHECK(features == 10); // Gate, six bridge cells, door, two waystone cells.
    game_update(0);
    CHECK(chunk_trace_count[STAGE_GAMEPLAY] == 4 && chunk_trace_count[STAGE_TILES] == 4 &&
          feature_trace_count[STAGE_GAMEPLAY] == features &&
          feature_trace_count[STAGE_TILES] == features);
    for(unsigned i = 0; i < 4; ++i)
        CHECK(chunk_trace[STAGE_GAMEPLAY][i] == i && chunk_trace[STAGE_TILES][i] == i);
    CHECK(tile_solid(cached_tile(positions::NorthGate)));
    CHECK(!terrain_clear(120, 16));
    game.set_flag(FLAG_GATE_OPEN);
    game.set_flag(FLAG_BRIDGE_REPAIRED);
    game_update(0);
    CHECK(!tile_solid(cached_tile(positions::NorthGate)));
    CHECK(terrain_clear(120, 16));
    CHECK(cached_tile({AREA_OUTDOOR, 16, 8}) == TILE_PATH);
    game.flags = {};
    game_update(0);
    CHECK(cached_tile(positions::NorthGate) == TILE_WALL);
    CHECK(cached_tile({AREA_OUTDOOR, 16, 8}) == TILE_WATER);
    // A direct flag change takes effect when the next cache is constructed.
    game.flags = {};
    game.set_flag(FLAG_GATE_OPEN);
    CHECK(tile_solid(cached_tile(positions::NorthGate)));
    game_update(0);
    CHECK(!tile_solid(cached_tile(positions::NorthGate)));
    game.set_flag(FLAG_BRIDGE_REPAIRED);
    world_enter(AREA_OUTDOOR, 200, 72, EAST);
    CHECK(game.cache.origin_tx == 16);
    CHECK(cached_tile({AREA_OUTDOOR, 15, 7}) == TILE_OUTSIDE);
    CHECK(game.cache.tiles[(7 - game.cache.origin_ty) * 17] == TILE_PATH);
    CHECK(feature_trace_count[STAGE_GAMEPLAY] == 5 &&
          feature_trace_count[STAGE_TILES] ==
              5); // Four bridge cells and one waystone cell in the live chunks.
    game.ticks = 16;
    world_enter(AREA_OUTDOOR, 208, 144, NORTH);
    CHECK(tile_solid(cached_tile(positions::TidalPlatform)));
    game.ticks = 32;
    update_visible_world();
    CHECK(!tile_solid(cached_tile(positions::TidalPlatform)));
    game.flags = {};
    game.set_flag(FLAG_GATE_OPEN);
    world_enter(AREA_OUTDOOR, 120, 16, EAST);
    game.flags = {};
    game_update(0);
    CHECK(terrain_clear(game.player.x, game.player.y));
    CHECK(game.player.x != 120 || game.player.y != 16);
    world_enter(AREA_OUTDOOR, 120, 80, EAST);
    game.clear_flag(FLAG_WAYSTONE);
    game.gold = 0;
    game_update(0);
    CHECK(game.gold == 1 && (game.get_flag(FLAG_WAYSTONE)));
    game_update(0);
    CHECK(game.gold == 1); // A persistent guard protects the per-tile reward calls.
    uint8_t saved[SAVE_BYTES];
    save_encode(saved);
    CHECK(save_decode(saved) && game.gold == 1);

    memset(game.cache.tiles, 0xa5, sizeof(game.cache.tiles));
    game.cache.origin_tx = 16;
    game.cache.origin_ty = 8;
    Game before = game;
    set_tile({AREA_OUTDOOR, 15, 8}, TILE_PATH);
    set_tile({AREA_OUTDOOR, 16, 7}, TILE_PATH);
    set_tile({AREA_INTERIOR, 16, 8}, TILE_PATH);
    set_tile({AREA_OUTDOOR, 240, 248}, TILE_PATH);
    CHECK(!memcmp(&before, &game, sizeof(game)));
    set_tile({AREA_OUTDOOR, 16, 8}, TILE_PATH);
    before.cache.tiles[0] = TILE_PATH;
    CHECK(!memcmp(&before, &game, sizeof(game)));
}

static void cache_bounds() {
    game_new();
    Game before = game;
    CHECK(cached_tile({AREA_INTERIOR, 4, 5}) == TILE_OUTSIDE);
    CHECK(cached_tile({0, 4, 5}) == TILE_OUTSIDE);
    CHECK(cached_tile({AREA_OUTDOOR, 17, 5}) == TILE_OUTSIDE);
    CHECK(cached_tile({AREA_OUTDOOR, 4, 10}) == TILE_OUTSIDE);
    CHECK(cached_tile({AREA_OUTDOOR, 255, 255}) == TILE_OUTSIDE);
    CHECK(!terrain_clear(200, 40)); // Valid map pixels outside the resident window.
    CHECK(!memcmp(&before, &game, sizeof(game)));

    // Cache misses must not dispatch even when a dynamic tile would be passable.
    game.set_flag(FLAG_GATE_OPEN);
    game.set_flag(FLAG_BRIDGE_REPAIRED);
    world_enter(AREA_OUTDOOR, 200, 72, EAST);
    before = game;
    CHECK(cached_tile({AREA_OUTDOOR, 15, 7}) == TILE_OUTSIDE);
    CHECK(cached_tile(positions::NorthGate) == TILE_OUTSIDE);
    CHECK(cached_tile({AREA_OUTDOOR, 16, 8}) == TILE_PATH);
    CHECK(cached_tile({AREA_OUTDOOR, 33, 8}) == TILE_OUTSIDE);
    CHECK(cached_tile({AREA_OUTDOOR, 16, 4}) == TILE_OUTSIDE);
    CHECK(!memcmp(&before, &game, sizeof(game)));
}

static void regions() {
    game_new();
    world_enter(AREA_OUTDOOR, 120, 80, EAST);
    CHECK(game.player_is_in_region(positions::WaystoneRegion));
    CHECK(!game.player_is_in_region({{AREA_INTERIOR, 15, 10}, 2, 1}));
    CHECK(!game.player_is_in_region({{AREA_OUTDOOR, 15, 10}, 0, 1}));
    CHECK(!game.player_is_in_region({{AREA_OUTDOOR, 15, 10}, 255, 255}));
    game.player.x = 132;
    CHECK(!game.player_is_in_region(positions::WaystoneRegion)); // Exclusive right edge.
    game.player.x = 65535;
    CHECK(!game.player_is_in_region(positions::WaystoneRegion));
    CHECK(!world_area(0) && !world_area(255));
}

inline constexpr TilePos TEST_LEFT{AREA_OUTDOOR, 4, 3};
inline constexpr TilePos TEST_RIGHT{AREA_OUTDOOR, 8, 3};
inline constexpr TilePos TEST_INTERIOR_RIGHT{AREA_INTERIOR, 16, 7};
inline constexpr TilePos TEST_INTERIOR_BOTTOM{AREA_INTERIOR, 15, 10};
static_assert(positions::EasternBridgeRegion.contains({AREA_OUTDOOR, 17, 8}),
              "inclusive last tile");
static_assert(!positions::EasternBridgeRegion.contains({AREA_OUTDOOR, 18, 8}),
              "exclusive right edge");
static_assert(!positions::EasternBridgeRegion.contains({AREA_INTERIOR, 15, 7}),
              "area-local region");

static void area_dimensions() {
    game_new();
    CHECK(game.dimensions.width_tiles == 32 && game.dimensions.height_tiles == 20);
    world_enter(AREA_INTERIOR, 16, 56, NORTH);
    CHECK(game.dimensions.width_tiles == 16 && game.dimensions.height_tiles == 10);
    CHECK(game_camera().x == 0 && game_camera().y == 16);

    // The resident edge column/row can lie beyond the smaller area's logical bounds.
    Game before = game;
    set_tile<TEST_INTERIOR_RIGHT>(TILE_PATH);
    set_tile<TEST_INTERIOR_BOTTOM>(TILE_PATH);
    set_tile(TEST_INTERIOR_RIGHT, TILE_PATH);
    set_tile(TEST_INTERIOR_BOTTOM, TILE_PATH);
    CHECK(!memcmp(&before, &game, sizeof(game)));
    CHECK(game.dimensions.pixel_bounds(120, 72));
    CHECK(!game.dimensions.pixel_bounds(121, 72));
    CHECK(!game.dimensions.pixel_bounds(120, 73));

    uint8_t saved[SAVE_BYTES];
    save_encode(saved);
    world_enter(AREA_OUTDOOR, 200, 72, EAST);
    CHECK(game.dimensions.width_tiles == 32 && game.dimensions.height_tiles == 20);
    before = game;
    // This X fits the active outdoor map, but not the saved interior destination.
    saved[6] = 128;
    uint16_t crc = save_checksum(saved);
    saved[SAVE_CRC_OFFSET] = (uint8_t)crc;
    saved[SAVE_CRC_OFFSET + 1] = (uint8_t)(crc >> 8);
    CHECK(!save_decode(saved));
    CHECK(!memcmp(&before, &game, sizeof(game)));
    saved[6] = 16;
    crc = save_checksum(saved);
    saved[SAVE_CRC_OFFSET] = (uint8_t)crc;
    saved[SAVE_CRC_OFFSET + 1] = (uint8_t)(crc >> 8);
    CHECK(save_decode(saved));
    CHECK(game.area == AREA_INTERIOR && game.dimensions.width_tiles == 16 &&
          game.dimensions.height_tiles == 10);
    CHECK(teleport_player<positions::InteriorArrival>());
    CHECK(game.dimensions.width_tiles == 16 && game.dimensions.height_tiles == 10);
    update_visible_world();
    CHECK(teleport_player<positions::OutdoorArrival>());
    CHECK(game.dimensions.width_tiles == 32 && game.dimensions.height_tiles == 20);
    update_visible_world();
    CHECK(terrain_clear(game.player.x, game.player.y));
}

// Declared after the generated dispatch templates: ADL must still find these.
namespace handler_impl {
inline void update_north_gate(Tiles<TEST_LEFT>) {
    set_tile<TEST_LEFT>(TILE_PORTAL);
}

template <HandlerStage STAGE> void update_platform(Tag<TEST_RIGHT, STAGE>) {
    game.gold = (uint8_t)STAGE + 1;
}

// Resolves the intersection of a position-wide and stage-wide overload.
inline void update_platform(Tiles<TEST_RIGHT>) {
    game.gold = 9;
}
} // namespace handler_impl

static void tag_overload_dispatch() {
    game_new();
    game.flags.set_flag(FLAG_GATE_OPEN);
    Game before = game;
    Handler<TEST_LEFT, STAGE_GAMEPLAY>{}.update_north_gate();
    Handler<TEST_LEFT, STAGE_TILES>{}.update_lantern();
    CHECK(!memcmp(&before, &game, sizeof(game)));
    // Specific position/stage beats the generic stage overload.
    Handler<TEST_LEFT, STAGE_TILES>{}.update_north_gate();
    CHECK(cached_tile(TEST_LEFT) == TILE_PORTAL);
    // A single handwritten Tiles<POS> overload applies at any other position.
    Handler<TEST_RIGHT, STAGE_TILES>{}.update_north_gate();
    CHECK(cached_tile(TEST_RIGHT) == TILE_PATH);
    Handler<positions::NorthGate, STAGE_TILES>{}.update_north_gate();
    CHECK(cached_tile(positions::NorthGate) == TILE_PATH);
    // A position-specific Tag<POS, STAGE> overload works across stages.
    Handler<TEST_RIGHT, STAGE_GAMEPLAY>{}.update_platform();
    CHECK(game.gold == 1);
    Handler<TEST_RIGHT, STAGE_DIALOGUE>{}.update_platform();
    CHECK(game.gold == 3);
    Handler<TEST_RIGHT, STAGE_TILES>{}.update_platform();
    CHECK(game.gold == 9);
    game.gold = 0;
    game.flags.clear_flag(FLAG_WAYSTONE);
    game.player.x = 128;
    game.player.y = 80;
    CHECK(game.player_is_on_tile(positions::Waystone_1));
    Handler<positions::Waystone_1, STAGE_TILES>{}.update_waystone();
    CHECK(!game.get_flag(FLAG_WAYSTONE) && game.gold == 0);
    Handler<positions::Waystone_1, STAGE_GAMEPLAY>{}.update_waystone();
    CHECK(game.get_flag(FLAG_WAYSTONE) && game.gold == 1);
    game_update(0);
    CHECK(game.gold == 1);
}

inline constexpr TilePos TEST_PLAYER_TILE{AREA_OUTDOOR, 8, 10};
inline constexpr TilePos TEST_OTHER_AREA{AREA_INTERIOR, 8, 9};
inline constexpr TilePos TEST_NORTH{AREA_OUTDOOR, 8, 9};
inline constexpr TilePos TEST_EAST{AREA_OUTDOOR, 9, 10};
inline constexpr TilePos TEST_SOUTH{AREA_OUTDOOR, 8, 11};
inline constexpr TilePos TEST_WEST{AREA_OUTDOOR, 7, 10};
inline constexpr TilePos TEST_BAD_AREA{0, 2, 7};
inline constexpr TilePos TEST_BAD_X{AREA_INTERIOR, 16, 0};

template <TilePos const& TARGET> void check_facing(uint8_t facing) {
    game.player.facing = facing;
    frame_pressed = BTN_A;
    CHECK(!player_just_interacted<TEST_OTHER_AREA>());
    CHECK(!player_just_interacted<TEST_PLAYER_TILE>());
    CHECK(frame_pressed == BTN_A);
    CHECK(player_just_interacted<TARGET>());
    CHECK(!player_just_interacted<TARGET>());
    CHECK(game.can_interact);
    frame_pressed = 0;
    CHECK(!player_just_interacted<TARGET>());
}

static void chunk_events() {
    game_new();
    world_enter(AREA_OUTDOOR, 64, 80, NORTH);
    frame_pressed = BTN_A;
    check_facing<TEST_NORTH>(NORTH);
    check_facing<TEST_EAST>(EAST);
    check_facing<TEST_SOUTH>(SOUTH);
    check_facing<TEST_WEST>(WEST);
    game.player = {64, 80, NORTH, 1};
    game.previous_player_tile = {game.area, 8, 9};
    CHECK(player_is_walking_onto<TEST_PLAYER_TILE>());
    CHECK(!player_is_walking_onto<TEST_OTHER_AREA>());
    game.player.walking = 0;
    CHECK(!player_is_walking_onto<TEST_PLAYER_TILE>());
    game.player.walking = 1;
    game.previous_player_tile = {game.area, 8, 10};
    CHECK(!player_is_walking_onto<TEST_PLAYER_TILE>());
    CHECK(!teleport_player<TEST_BAD_AREA>(NORTH));
    CHECK(!teleport_player<TEST_BAD_X>(NORTH));
    frame_pressed = BTN_A;
    CHECK(teleport_player<positions::InteriorArrival>(SOUTH));
    CHECK(teleported && !frame_pressed && !game.player.walking);
    CHECK(game.area == AREA_INTERIOR && game.player.x == 16 && game.player.y == 56);
    CHECK(!teleport_player<positions::OutdoorArrival>());
    update_visible_world(); // Prepare the destination after direct helper testing.
    for(uint8_t y = 0; y < CACHE_HEIGHT; ++y)
        for(uint8_t x = 0; x < CACHE_WIDTH; ++x)
            CHECK(game.cache.tiles[y * CACHE_WIDTH + x] == authored_tile(*world_area(game.area),
                                                                         game.cache.origin_tx + x,
                                                                         game.cache.origin_ty + y));
    // Landing on a portal while loading/teleporting is not walking onto it.
    world_enter(AREA_INTERIOR, 16, 64, SOUTH);
    game_update(0);
    CHECK(game.area == AREA_INTERIOR);
    // Generated handlers consume actual input, rather than a separate registry.
    world_enter(AREA_OUTDOOR, 112, 16, WEST);
    game_update(0, BTN_A);
    CHECK(!game.get_flag(FLAG_GATE_OPEN));
    game.player.facing = EAST;
    game_update(0, BTN_A);
    CHECK(game.get_flag(FLAG_GATE_OPEN));
    CHECK(cached_tile(positions::NorthGate) == TILE_WALL); // Dialogue follows the tile stage.
    game_update(BTN_A);
    CHECK(game.get_flag(FLAG_GATE_OPEN));
    CHECK(cached_tile(positions::NorthGate) == TILE_PATH);
    world_enter(AREA_OUTDOOR, 120, 48, SOUTH);
    game_update(0, BTN_A);
    CHECK(game.get_flag(FLAG_BRIDGE_REPAIRED));
    CHECK(game.get_flag(FLAG_GATE_OPEN));
}

static void three_stage_dispatch() {
    game_new();
    game.set_flag(FLAG_BRIDGE_REPAIRED);
    world_enter(AREA_OUTDOOR, 120, 48, SOUTH);
    expected_dialogue_cache = game.cache;
    memset(game.cache.tiles, 0xa5, sizeof(game.cache.tiles));
    verify_dialogue_cache = true;
    game_update(0, BTN_A);
    verify_dialogue_cache = false;
    CHECK(!game.get_flag(FLAG_BRIDGE_REPAIRED) && !frame_pressed);
    CHECK(chunk_trace_count[STAGE_GAMEPLAY] == 4 && chunk_trace_count[STAGE_TILES] == 4 &&
          chunk_trace_count[STAGE_DIALOGUE] == 4);
    for(uint8_t y = 7; y <= 8; ++y)
        for(uint8_t x = 15; x <= 17; ++x)
            CHECK(cached_tile({AREA_OUTDOOR, x, y}) == TILE_PATH);
    // Dialogue flag changes remain deferred until the following tile pass.
    game_update(0);
    for(uint8_t y = 7; y <= 8; ++y)
        for(uint8_t x = 15; x <= 17; ++x)
            CHECK(cached_tile({AREA_OUTDOOR, x, y}) == TILE_WATER);

    // A latched teleport must not abort any calls in the tile stage.
    game.set_flag(FLAG_BRIDGE_REPAIRED);
    teleported = true;
    frame_pressed = BTN_A;
    process_all_chunks<STAGE_TILES>();
    for(uint8_t y = 7; y <= 8; ++y)
        for(uint8_t x = 15; x <= 17; ++x)
            CHECK(cached_tile({AREA_OUTDOOR, x, y}) == TILE_PATH);
    CHECK(frame_pressed == BTN_A && teleported && game.gold == 0);
    CHECK(game.get_flag(FLAG_BRIDGE_REPAIRED));
    game_update(0);
    CHECK(!teleported && !frame_pressed);

    game_new();
    game.player = {96, 40, NORTH, 1};
    game.previous_player_tile = {AREA_OUTDOOR, 12, 6};
    game_update(0);
    CHECK(teleported && game.area == AREA_INTERIOR && !game.player.walking);
    CHECK(chunk_trace_count[STAGE_GAMEPLAY] == 1 && feature_trace_count[STAGE_GAMEPLAY] == 2);
    CHECK(chunk_trace_count[STAGE_TILES] == 2 && feature_trace_count[STAGE_TILES] == 2);
    CHECK(chunk_trace_count[STAGE_DIALOGUE] == 0);
    CHECK(!game.get_flag(FLAG_HELPED) && !frame_pressed);
    game_update(0);
    CHECK(!teleported && game.area == AREA_INTERIOR && chunk_trace_count[STAGE_DIALOGUE] == 2);
}

int main() {
    reloads();
    mutations_and_frames();
    cache_bounds();
    regions();
    area_dimensions();
    tag_overload_dispatch();
    chunk_events();
    three_stage_dispatch();
    printf("PASS M1.5 world: %u checks\n", checks);
}
