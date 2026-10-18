#ifndef WORLD_TYPES_H
#define WORLD_TYPES_H

#include <stdint.h>
#include "flags.h"

inline constexpr uint8_t TILE = 16, TILE_SHIFT = 4;
inline constexpr uint8_t FOOT_WIDTH = 8, FOOT_HEIGHT = 8;
inline constexpr uint8_t FOOT_LEFT = (TILE - FOOT_WIDTH) / 2;
inline constexpr uint8_t FOOT_RIGHT = FOOT_LEFT + FOOT_WIDTH - 1;
inline constexpr uint8_t FOOT_TOP = TILE - FOOT_HEIGHT, FOOT_BOTTOM = TILE - 1;
inline constexpr uint8_t FOOT_CENTER_X = FOOT_LEFT + FOOT_WIDTH / 2;
inline constexpr uint8_t FOOT_CENTER_Y = FOOT_TOP + FOOT_HEIGHT / 2;
inline constexpr uint8_t CHUNK_WIDTH = 8, CHUNK_HEIGHT = 4;
inline constexpr uint8_t CACHE_WIDTH = 9, CACHE_HEIGHT = 5;
inline constexpr uint8_t MAX_MAP_WIDTH = 240, MAX_MAP_HEIGHT = 248;
inline constexpr uint8_t TILE_OUTSIDE = 255;

struct TilePos {
    uint8_t area, x, y;

    constexpr bool operator==(TilePos const&) const = default;
};

struct ChunkCoord {
    uint8_t area, x, y;
};

struct MapDimensions {
    uint8_t width_tiles, height_tiles;

    bool pixel_bounds(uint16_t x, uint16_t y) const {
        return x <= (uint16_t)width_tiles * TILE - TILE &&
               y <= (uint16_t)height_tiles * TILE - TILE;
    }
};

struct PixelPos {
    uint16_t x, y;
};

struct TileRegion {
    TilePos origin;
    uint8_t width, height;

    constexpr bool contains(TilePos tile) const {
        return tile.area == origin.area && tile.x >= origin.x && tile.y >= origin.y &&
               (uint16_t)tile.x < (uint16_t)origin.x + width &&
               (uint16_t)tile.y < (uint16_t)origin.y + height;
    }
};

struct TileCache {
    uint8_t origin_tx, origin_ty;
    uint8_t tiles[CACHE_WIDTH * CACHE_HEIGHT];
};

static_assert(sizeof(TilePos) == 3 && sizeof(ChunkCoord) == 3, "byte tile/chunk coordinates");
static_assert(sizeof(MapDimensions) == 2, "byte map dimensions");
static_assert(sizeof(PixelPos) == 4, "16-bit pixel coordinates");
static_assert(sizeof(TileCache) == 47 && sizeof(TileCache::tiles) == 45, "only 45 tile entries");
static_assert(sizeof(TileRegion) == 5, "byte feature footprint/origin");

#endif
