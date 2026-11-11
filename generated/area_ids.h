/* Generated from TMX/TSX; do not edit. */

#pragma once
#include <stdint.h>
inline constexpr uint8_t AREA_OUTDOOR = 1;
inline constexpr uint8_t AREA_INTERIORS = 2;
inline constexpr uint8_t TILESET_WORLD = 0;
inline constexpr uint8_t TILESET_HOUSE = 1;
inline constexpr uint8_t TILESET_COUNT = 2;
// Each area draws and collides with one tileset. Tile IDs are per tileset.
constexpr uint8_t area_tileset(uint8_t area) {
    switch(area) {
        case 2: return TILESET_HOUSE;
        default: return TILESET_WORLD;
    }
}
