/* Generated from TMX/TSX; do not edit. */

#pragma once
#include "rpg.h"
#include "tile_ids.h"
extern uint16_t const PROGMEM TILE_COLLISION[];
extern uint8_t const PROGMEM TILE_PORTALS[];
// One AMBIENT_CLASS per tile, in 256-entry blocks like TILE_COLLISION.
extern uint8_t const PROGMEM TILE_AMBIENT[];
inline constexpr uint8_t OUTDOOR_WIDTH = 64;
inline constexpr uint8_t OUTDOOR_HEIGHT = 32;
inline constexpr uint8_t OUTDOOR_CHUNK_COLUMNS = 8;
inline constexpr uint8_t OUTDOOR_CHUNK_ROWS = 8;
extern uint8_t const PROGMEM OUTDOOR_TILES[];
extern Area const PROGMEM OUTDOOR_AREA;
inline constexpr uint8_t INTERIORS_WIDTH = 76;
inline constexpr uint8_t INTERIORS_HEIGHT = 11;
inline constexpr uint8_t INTERIORS_CHUNK_COLUMNS = 10;
inline constexpr uint8_t INTERIORS_CHUNK_ROWS = 3;
extern uint8_t const PROGMEM INTERIORS_TILES[];
extern Area const PROGMEM INTERIORS_AREA;
// Pixels per tile of WORLD_MAP_SPRITE.
inline constexpr uint8_t WORLD_MAP_SCALE = 2;
struct StaticArea { uint8_t id, width, height, columns, rows; };
inline constexpr StaticArea STATIC_AREAS[] = {
    {1, 64, 32, 8, 8},
    {2, 76, 11, 10, 3},
};
