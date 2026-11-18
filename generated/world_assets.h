/* Generated from TMX/TSX; do not edit. */

#pragma once
#include "rpg.h"
#include "tile_ids.h"
extern uint16_t const PROGMEM TILE_COLLISION[];
extern uint8_t const PROGMEM TILE_PORTALS[];
// One AMBIENT_CLASS per tile, in 256-entry blocks like TILE_COLLISION.
extern uint8_t const PROGMEM TILE_AMBIENT[];
inline constexpr uint8_t OUTDOOR_WIDTH = 240;
inline constexpr uint8_t OUTDOOR_HEIGHT = 248;
inline constexpr uint8_t OUTDOOR_CHUNK_COLUMNS = 30;
inline constexpr uint8_t OUTDOOR_CHUNK_ROWS = 62;
extern uint8_t const PROGMEM OUTDOOR_TILES[];
extern Area const PROGMEM OUTDOOR_AREA;
inline constexpr uint8_t INTERIORS_WIDTH = 72;
inline constexpr uint8_t INTERIORS_HEIGHT = 39;
inline constexpr uint8_t INTERIORS_CHUNK_COLUMNS = 9;
inline constexpr uint8_t INTERIORS_CHUNK_ROWS = 10;
extern uint8_t const PROGMEM INTERIORS_TILES[];
extern Area const PROGMEM INTERIORS_AREA;
inline constexpr uint8_t CAVE_WIDTH = 40;
inline constexpr uint8_t CAVE_HEIGHT = 26;
inline constexpr uint8_t CAVE_CHUNK_COLUMNS = 5;
inline constexpr uint8_t CAVE_CHUNK_ROWS = 7;
extern uint8_t const PROGMEM CAVE_TILES[];
extern Area const PROGMEM CAVE_AREA;
inline constexpr uint8_t DUNGEON_WIDTH = 44;
inline constexpr uint8_t DUNGEON_HEIGHT = 30;
inline constexpr uint8_t DUNGEON_CHUNK_COLUMNS = 6;
inline constexpr uint8_t DUNGEON_CHUNK_ROWS = 8;
extern uint8_t const PROGMEM DUNGEON_TILES[];
extern Area const PROGMEM DUNGEON_AREA;
struct StaticArea { uint8_t id, width, height, columns, rows; };
inline constexpr StaticArea STATIC_AREAS[] = {
    {1, 240, 248, 30, 62},
    {2, 72, 39, 9, 10},
    {3, 40, 26, 5, 7},
    {4, 44, 30, 6, 8},
};
