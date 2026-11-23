#ifndef QUEST_ARROW_H
#define QUEST_ARROW_H

#include "rpg.h"

struct QuestArrowPlacement {
    uint8_t x, y, frame;
};

namespace quest_arrow_detail {

inline uint32_t product(uint16_t bearing, uint8_t pixels) {
    // Each byte product fits a word. AVM can issue a widening byte multiply for each.
    uint16_t low = (uint16_t)(uint8_t)bearing * pixels;
    uint16_t high = (uint16_t)(uint8_t)(bearing >> 8) * pixels;
    return ((uint32_t)high << 8) + low;
}

inline uint8_t offset(uint32_t numerator, uint16_t divisor) {
    numerator += divisor / 2;
    if(numerator < divisor)
        return 0;
    if(numerator <= UINT16_MAX)
        return (uint16_t)numerator / divisor;

    // A tile coordinate is one byte and the projected distance is at most 127 pixels.
    // The rounded numerator fits 19 bits. Shift both operands to use the native word divider.
    // Truncating the divisor can only overestimate the result. Correct with the original ratio.
    uint8_t pixels = (uint16_t)(numerator >> 3) / (divisor >> 3);
    uint32_t covered = product(divisor, pixels);
    while(covered > numerator) {
        --pixels;
        covered -= divisor;
    }
    return pixels;
}

} // namespace quest_arrow_detail

// Intersect the signed player-to-target bearing with the inset screen. The origin is the player's
// screen position before centering the sprite. The bearing must be nonzero.
inline QuestArrowPlacement quest_arrow_at_edge(int16_t player_x, int16_t player_y, int16_t dx,
                                               int16_t dy, uint8_t inset) {
    constexpr uint8_t WIDTH = DISPLAY_TILE_WIDTH * TILE, HEIGHT = DISPLAY_TILE_HEIGHT * TILE;
    constexpr uint8_t CENTER = TILE / 2 - QUEST_ARROW_SIZE / 2;
    constexpr uint16_t MAX_DELTA = UINT8_MAX * TILE;
    static_assert((uint32_t)MAX_DELTA * (WIDTH - 1) + MAX_DELTA / 2 < (1ul << 19));

    uint8_t right = WIDTH - QUEST_ARROW_SIZE - inset;
    uint8_t bottom = HEIGHT - QUEST_ARROW_SIZE - inset;
    // Work in sprite top-left coordinates. Keep the ray origin strictly inside the rectangle
    // so turning through a bearing parallel to an edge still has a continuous intersection.
    player_x += CENTER;
    player_y += CENTER;
    uint8_t x = player_x <= inset ? inset + 1 : (player_x >= right ? right - 1 : (uint8_t)player_x);
    uint8_t y =
        player_y <= inset ? inset + 1 : (player_y >= bottom ? bottom - 1 : (uint8_t)player_y);
    uint8_t edge_x = dx < 0 ? inset : right;
    uint8_t edge_y = dy < 0 ? inset : bottom;
    uint8_t run_x = dx < 0 ? x - edge_x : edge_x - x;
    uint8_t run_y = dy < 0 ? y - edge_y : edge_y - y;
    uint16_t ax = dx < 0 ? -dx : dx;
    uint16_t ay = dy < 0 ? -dy : dy;

    uint32_t y_at_edge = quest_arrow_detail::product(ay, run_x);
    uint32_t x_at_edge = quest_arrow_detail::product(ax, run_y);
    // Cross-multiply to pick the first boundary. Only the winning coordinate needs division.
    if(ax && y_at_edge <= x_at_edge) {
        uint8_t pixels = quest_arrow_detail::offset(y_at_edge, ax);
        x = edge_x;
        y = dy < 0 ? y - pixels : y + pixels;
    } else {
        uint8_t pixels = quest_arrow_detail::offset(x_at_edge, ay);
        x = dx < 0 ? x - pixels : x + pixels;
        y = edge_y;
    }

    // Orientation still uses the sheet's eight bearings. It has no effect on placement.
    uint8_t column = ax * 2 < ay ? 1 : (dx < 0 ? 0 : 2);
    uint8_t row = ay * 2 < ax ? 1 : (dy < 0 ? 0 : 2);
    uint8_t cell = row * 3 + column;
    return {x, y, (uint8_t)(cell - (cell > 4))};
}

#endif
