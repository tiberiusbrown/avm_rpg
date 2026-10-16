// Runs the same rules inside the real AVM interpreter, including a 59,520-byte
// chunk-major program asset. Results are read at probe_done by the SDK test.
#include "chunk.h"
#include "map_positions.h"
#include "tile_ids.h"

#include <avm.h>

volatile uint8_t world_phase;
volatile uint16_t probe_checks, probe_failure;
static uint8_t record[SAVE_BYTES];
static volatile uint16_t probe_dialogues, probe_choices;

bool dialogue_show(char const PROGMEM*) {
    ++probe_dialogues;
    return true;
}

bool dialogue_ask(char const PROGMEM*) {
    ++probe_choices;
    return true;
}

struct LargeTiles {
    uint8_t tiles[15u * 31u * 128u];
};

constexpr LargeTiles large_tiles() {
    LargeTiles result{};
    result.tiles[464u * 128u] = TILE_PORTAL;
    result.tiles[464u * 128u + 127u] = TILE_PATH;
    return result;
}

static constexpr LargeTiles PROGMEM LARGE_TILES = large_tiles();
static volatile uint8_t edge_x = 239, edge_y = 247;
static volatile uint16_t wide_flag = 36;

static void check(bool value, uint16_t line) {
    ++probe_checks;
    if(!value && !probe_failure)
        probe_failure = line;
}

#define CHECK(value) check((value), __LINE__)

extern "C" RPG_NOINLINE void probe_done() {
    avm_debug_break();
}

static RPG_NOINLINE void probe_large_map() {
    game_new();
    reload_tile_window(game.cache, LARGE_TILES.tiles, {240, 248}, {65535, 65535});
    CHECK(game.cache.origin_tx == 224 && game.cache.origin_ty == 240);
    CHECK(game.cache.tiles[7 * 17 + 15] == TILE_PATH);
    CHECK(game.cache.tiles[0] == TILE_PORTAL);
    CHECK(game.cache.tiles[(uint16_t)(edge_y - game.cache.origin_ty) * CACHE_WIDTH + edge_x -
                           game.cache.origin_tx] == TILE_PATH);
    CHECK(cached_tile({AREA_INTERIOR, 224, 240}) == TILE_OUTSIDE);
    CHECK(game.cache.tiles[16] == TILE_OUTSIDE && game.cache.tiles[8 * 17] == TILE_OUTSIDE);
}

static RPG_NOINLINE void probe_terrain() {
    game_new();
    CHECK(tile_solid(cached_tile(positions::NorthGate)));
    game.flags = {};
    game.set_flag(FLAG_GATE_OPEN);
    game.set_flag(FLAG_BRIDGE_REPAIRED);
    game_update(0);
    CHECK(cached_tile(positions::NorthGate) == TILE_PATH);
    CHECK(terrain_clear(120, 16));
    CHECK(cached_tile({AREA_OUTDOOR, 16, 8}) == TILE_PATH);
    game.flags = {};
    game_update(0);
    CHECK(cached_tile(positions::NorthGate) == TILE_WALL);
    CHECK(!terrain_clear(120, 16));
}

static RPG_NOINLINE void probe_saves() {
    game.flags = {};
    game.set_flag(FLAG_GATE_OPEN);
    world_enter(AREA_OUTDOOR, 120, 16, EAST);
    save_encode(record);
    world_enter(AREA_INTERIOR, 16, 56, NORTH);
    CHECK(save_decode(record));
    CHECK(game.dimensions.width_tiles == 32 && game.dimensions.height_tiles == 20);
    CHECK(game.get_flag(FLAG_GATE_OPEN) && terrain_clear(120, 16));
    record[SAVE_HEADER_BYTES] = 0;
    uint16_t crc = save_checksum(record);
    record[SAVE_CRC_OFFSET] = (uint8_t)crc;
    record[SAVE_CRC_OFFSET + 1] = (uint8_t)(crc >> 8);
    CHECK(save_decode(record));
    CHECK(!game.get_flag(FLAG_GATE_OPEN) && terrain_clear(game.player.x, game.player.y) &&
          (game.player.x != 120 || game.player.y != 16));
    game.flags = {};
    game.set_flag(FLAG_BRIDGE_REPAIRED);
    world_enter(AREA_OUTDOOR, 200, 72, EAST);
    CHECK(game.cache.origin_tx == 16);
    CHECK(cached_tile({AREA_OUTDOOR, 15, 7}) == TILE_OUTSIDE);
    world_enter(AREA_OUTDOOR, 120, 80, EAST);
    CHECK(game.player_is_in_region(positions::WaystoneRegion));
    CHECK(!game.player_is_in_region({{AREA_INTERIOR, 15, 10}, 2, 1}));
}

static RPG_NOINLINE void probe_flags_and_platform() {
    FlagSet<37> wide{};
    wide.set_flag(16);
    wide.set_flag(wide_flag);
    CHECK(wide.get_flag(16) && wide.get_flag(wide_flag) && !wide.get_flag(15));
    wide.toggle_flag(16);
    wide.clear_flag(wide_flag);
    CHECK(!wide.get_flag(16) && !wide.get_flag(wide_flag));
    game.ticks = 16;
    world_enter(AREA_OUTDOOR, 208, 144, NORTH);
    CHECK(cached_tile(positions::TidalPlatform) == TILE_WATER);
    game.ticks = 32;
    game_update(0);
    CHECK(cached_tile(positions::TidalPlatform) == TILE_PATH);
    uint8_t before = game.cache.tiles[0];
    set_tile({AREA_OUTDOOR, 240, 248}, TILE_PATH);
    CHECK(game.cache.tiles[0] == before);
}

static RPG_NOINLINE void probe_chunk_events() {
    game_new();
    world_enter(AREA_INTERIOR, 48, 40, NORTH);
    CHECK(!game.get_flag(FLAG_HELPED));
    game_update(BTN_A); // Held input alone cannot synthesize an A edge.
    CHECK(!game.get_flag(FLAG_HELPED));
    game_update(0, BTN_A);
    CHECK(game.get_flag(FLAG_HELPED) && game.gold == 5 && probe_choices == 1);
    game_update(BTN_A);
    CHECK(game.gold == 5 && probe_choices == 1);
    world_enter(AREA_OUTDOOR, 120, 48, SOUTH);
    game_update(0, BTN_A);
    CHECK(game.get_flag(FLAG_BRIDGE_REPAIRED));
    CHECK(cached_tile({AREA_OUTDOOR, 15, 7}) == TILE_WATER &&
          cached_tile({AREA_OUTDOOR, 17, 8}) == TILE_WATER);
    game_update(0);
    CHECK(cached_tile({AREA_OUTDOOR, 15, 7}) == TILE_PATH &&
          cached_tile({AREA_OUTDOOR, 17, 8}) == TILE_PATH);
    world_enter(AREA_INTERIOR, 16, 56, SOUTH);
    CHECK(game.dimensions.width_tiles == 16 && game.dimensions.height_tiles == 10);
    game_update(BTN_DOWN);
    CHECK(game.area == AREA_INTERIOR && game.player.y == 58);
    game_update(0);
    CHECK(game.area == AREA_OUTDOOR && game.player.x == 96 && game.player.y == 48);
    CHECK(game.dimensions.width_tiles == 32 && game.dimensions.height_tiles == 20);
    game_update(0);
    CHECK(game.area == AREA_OUTDOOR && !game.player.walking);
    CHECK(cached_tile(positions::HouseDoor) == TILE_PORTAL);
}

extern "C" void setup() {
    probe_large_map();
    probe_terrain();
    probe_saves();
    probe_flags_and_platform();
    probe_chunk_events();
    probe_done();
}

extern "C" void loop() {
    avm_idle();
}
