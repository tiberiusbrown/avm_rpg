#ifndef BATTLE_CURSOR_H
#define BATTLE_CURSOR_H

#include <stdint.h>

// The cursor's rendered position is kept in sub-pixel units, so easing is never stuck by
// whole-pixel rounding. Rendering rounds each position to the nearest pixel.
inline constexpr int16_t CURSOR_SUBPIXELS = 16;

// Moves a rendered coordinate halfway toward its target. Within one sub-pixel unit of the target it
// snaps there, so easing always lands exactly on the target. Without that snap, halving a distance
// of one unit would move nothing, and the cursor would stop a pixel short.
constexpr int16_t cursor_ease(int16_t rendered, int16_t target) {
    int16_t distance = (int16_t)(target - rendered);
    if(distance >= -1 && distance <= 1)
        return target;
    return (int16_t)(rendered + distance / 2);
}

// The whole pixel nearest a rendered coordinate. Coordinates are never negative.
constexpr int16_t cursor_pixel(int16_t rendered) {
    return (int16_t)((rendered + CURSOR_SUBPIXELS / 2) / CURSOR_SUBPIXELS);
}

// The rendered coordinate of a whole pixel.
constexpr int16_t cursor_subpixel(int16_t pixel) {
    return (int16_t)(pixel * CURSOR_SUBPIXELS);
}

#endif
