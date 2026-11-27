// Installs the fixture maps before main (or before setup() on the guest), so engine tests never
// depend on the game's own maps.
#include "cast.h"
#include "field_tiles.h"
#include "tile_ids.h"
#include "world_assets.h"

static_assert(OUTDOOR_WIDTH == 64 && OUTDOOR_HEIGHT == 32,
              "the fixture field has the outdoor map's dimensions, which size the chunk tables");

namespace {

struct OpenFloor {
    uint8_t tiles[INTERIORS_WIDTH * INTERIORS_HEIGHT];
};

constexpr OpenFloor open_floor() {
    OpenFloor floor{};
    for(uint8_t& tile : floor.tiles)
        tile = HOUSE_TILE_FLOOR_WOOD_A;
    return floor;
}

constexpr OpenFloor PROGMEM FLOOR = open_floor();

constexpr Area PROGMEM OUTDOOR{
    OUTDOOR_WIDTH,         OUTDOOR_HEIGHT,
    OUTDOOR_CHUNK_COLUMNS, OUTDOOR_CHUNK_ROWS,
    FIXTURE_FIELD_TILES,   {(sites::PLAYER_START.x) * TILE, (sites::PLAYER_START.y) * TILE}};

constexpr Area PROGMEM INTERIORS{INTERIORS_WIDTH,      INTERIORS_HEIGHT, INTERIORS_CHUNK_COLUMNS,
                                 INTERIORS_CHUNK_ROWS, FLOOR.tiles,      {TILE, TILE}};

struct Install {
    Install() {
        world_area_override(AREA_OUTDOOR, &OUTDOOR);
        world_area_override(AREA_INTERIORS, &INTERIORS);
    }
};

Install install;

} // namespace
