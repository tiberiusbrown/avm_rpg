/* Generated from TMX/TSX; do not edit. */

#pragma once
#include "rpg.h"
#include "tile_ids.h"
extern uint8_t const PROGMEM TILE_SOLID[];
extern uint8_t const PROGMEM TILE_PORTALS[];
inline constexpr uint8_t OUTDOOR_WIDTH = 32;
inline constexpr uint8_t OUTDOOR_HEIGHT = 20;
inline constexpr uint8_t OUTDOOR_CHUNK_COLUMNS = 2;
inline constexpr uint8_t OUTDOOR_CHUNK_ROWS = 3;
extern uint8_t const PROGMEM OUTDOOR_TILES[];
extern Area const PROGMEM OUTDOOR_AREA;
inline constexpr uint8_t INTERIOR_WIDTH = 16;
inline constexpr uint8_t INTERIOR_HEIGHT = 10;
inline constexpr uint8_t INTERIOR_CHUNK_COLUMNS = 1;
inline constexpr uint8_t INTERIOR_CHUNK_ROWS = 2;
extern uint8_t const PROGMEM INTERIOR_TILES[];
extern Area const PROGMEM INTERIOR_AREA;
inline constexpr uint8_t NPC_HALL_WIDTH = 32;
inline constexpr uint8_t NPC_HALL_HEIGHT = 8;
inline constexpr uint8_t NPC_HALL_CHUNK_COLUMNS = 2;
inline constexpr uint8_t NPC_HALL_CHUNK_ROWS = 1;
extern uint8_t const PROGMEM NPC_HALL_TILES[];
extern Area const PROGMEM NPC_HALL_AREA;
