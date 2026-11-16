/* Generated from TMX/TSX; do not edit. */

#pragma once
#include <stdint.h>
#include "tile_ids.h"
inline constexpr uint8_t AREA_OUTDOOR = 1;
inline constexpr uint8_t AREA_INTERIORS = 2;
inline constexpr uint8_t AREA_CAVE = 3;
inline constexpr uint8_t AREA_DUNGEON = 4;
inline constexpr uint8_t TILESET_WORLD = 0;
inline constexpr uint8_t TILESET_CAVE = 1;
inline constexpr uint8_t TILESET_DUNGEON = 2;
inline constexpr uint8_t TILESET_HOUSE = 3;
inline constexpr uint8_t TILESET_COUNT = 4;
// Each area draws and collides with one tileset. Tile IDs are per tileset.
constexpr uint8_t area_tileset(uint8_t area) {
    switch(area) {
        case 2: return TILESET_HOUSE;
        case 3: return TILESET_CAVE;
        case 4: return TILESET_DUNGEON;
        default: return TILESET_WORLD;
    }
}
// Extra frames of a tileset's sprite sheet: `count` tiles from tile `first`, stored in
// tile order from sheet frame `base`. A count of zero means the tileset has none.
struct TileFrames {
    uint8_t first, count;
    uint16_t base;
};
// Every animated tile cycles through this many phases. Phase 0 is its atlas cell, and
// tileset_phase_sprites() returns a whole sprite sheet for each phase.
inline constexpr uint8_t TILE_PHASES = 4;
// The frame each tile shows while a gust passes over it.
constexpr TileFrames tile_wind_frames(uint8_t tileset) {
    switch(tileset) {
        case TILESET_WORLD: return {6, 84, TILE_SPRITE_COUNT};
        default: return {0, 0, 0};
    }
}
// What the ambient effects may do on a tile. Tiles of the first two classes have a
// wind frame.
enum AMBIENT_CLASS : uint8_t {
    AMBIENT_NONE,
    AMBIENT_GRASS,
    AMBIENT_FLOWERS,
    AMBIENT_WATER,
    AMBIENT_TREE,
    AMBIENT_FLOOR,
    AMBIENT_PERCH,
    AMBIENT_ROAD,
    AMBIENT_STEAM,
    AMBIENT_CAVE,
    AMBIENT_CRYPT,
    AMBIENT_DRIP,
    AMBIENT_POOL,
    AMBIENT_CRYSTAL,
    AMBIENT_EMBER,
};
// False for a tileset whose tiles all have the class AMBIENT_NONE.
constexpr bool tileset_has_ambient(uint8_t tileset) {
    return tileset == TILESET_WORLD || tileset == TILESET_CAVE || tileset == TILESET_DUNGEON || tileset == TILESET_HOUSE;
}
// True for an outdoor tileset, where birds fly overhead.
constexpr bool tileset_has_sky(uint8_t tileset) {
    return tileset == TILESET_WORLD;
}
