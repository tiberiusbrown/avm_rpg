#ifndef BATTLE_MOTION_H
#define BATTLE_MOTION_H

#include <stdint.h>

// Battle motion timing at the 50 Hz frame rate. An attacker lunges toward its victim, holds the
// lunge for about a quarter second, and returns. A unit that loses HP flashes for about a second.
inline constexpr uint8_t BATTLE_LUNGE_PIXELS = 4;
// Frames to lunge out, and the same number to return.
inline constexpr uint8_t BATTLE_LUNGE_FRAMES = 2;
inline constexpr uint8_t BATTLE_LUNGE_PAUSE_FRAMES = 12;
inline constexpr uint8_t BATTLE_FLASH_FRAMES = 48;
// A move whose only damage is its mover's blood price flashes for this shorter time: two hidden and
// two visible phases, about a third of a second.
inline constexpr uint8_t BATTLE_PRICE_FLASH_FRAMES = 16;
// Frames each hidden or visible phase of a flash lasts.
inline constexpr uint8_t BATTLE_FLASH_STEP = 4;

// An archer's arrow flies straight at its target, this many pixels a frame, before the target
// reacts. Its sprite is one 16-pixel cell; it starts half a cell out from the archer's cell and
// lands half a cell short of the target's, so a shot at range 2 takes 5 frames and one at range 4
// takes 13.
inline constexpr uint8_t BATTLE_ARROW_PIXELS = 4;
inline constexpr uint8_t BATTLE_CELL_PIXELS = 16;

// Frames an arrow flies to a target `cells` away along a row or column.
constexpr uint8_t battle_arrow_frames(uint8_t cells) {
    return (uint8_t)((cells * BATTLE_CELL_PIXELS - BATTLE_CELL_PIXELS) / BATTLE_ARROW_PIXELS + 1);
}

// The arrow's offset from its archer's cell, in pixels along its flight, at a frame of that flight.
constexpr uint8_t battle_arrow_offset(uint8_t frame) {
    return (uint8_t)(BATTLE_CELL_PIXELS / 2 + frame * BATTLE_ARROW_PIXELS);
}

// The attacker's offset from its cell, in pixels, at a frame of its lunge. It moves out in equal
// steps, holds at full reach, and moves back in the same steps.
constexpr uint8_t battle_lunge_pixels(uint8_t frame) {
    constexpr uint8_t step = BATTLE_LUNGE_PIXELS / BATTLE_LUNGE_FRAMES;
    constexpr uint8_t hold_end = BATTLE_LUNGE_FRAMES + BATTLE_LUNGE_PAUSE_FRAMES;
    if(frame < BATTLE_LUNGE_FRAMES)
        return (uint8_t)(step * (frame + 1));
    if(frame < hold_end)
        return BATTLE_LUNGE_PIXELS;
    uint8_t back = (uint8_t)(frame - hold_end);
    if(back < BATTLE_LUNGE_FRAMES)
        return (uint8_t)(BATTLE_LUNGE_PIXELS - step * (back + 1));
    return 0;
}

// A rogue's knight leap is a fast arcing jump: it covers its two cells along the long axis at this
// many pixels a frame, and one cell along the short axis at half that, so it lands after
// BATTLE_LEAP_FRAMES frames. On the way it rises and falls on a parabola that peaks this many
// pixels above the straight line at mid-flight.
inline constexpr uint8_t BATTLE_LEAP_PIXELS = 4;
inline constexpr uint8_t BATTLE_LEAP_FRAMES = 2 * BATTLE_CELL_PIXELS / BATTLE_LEAP_PIXELS;
inline constexpr uint8_t BATTLE_LEAP_HEIGHT = 10;

// The leaper's height above its straight path, from the pixels it still has to cover along the long
// axis. The parabola is symmetric, so the distance left gives the same height as the distance
// flown.
constexpr uint8_t battle_leap_lift(uint8_t remaining) {
    uint8_t left = (uint8_t)(remaining / BATTLE_LEAP_PIXELS);
    if(left >= BATTLE_LEAP_FRAMES)
        return 0;
    return (uint8_t)(4 * BATTLE_LEAP_HEIGHT * left * (BATTLE_LEAP_FRAMES - left) /
                     (BATTLE_LEAP_FRAMES * BATTLE_LEAP_FRAMES));
}

// The enemy's thinking indicator: a white-bordered black box at the center of the 128x64 screen
// with three 3x3 dots, drawn over the board while the search runs. Each progress step lights one
// more dot, then none, so the dots fill in a cycle of four steps.
inline constexpr uint8_t BATTLE_THINKING_WIDTH = 21;
inline constexpr uint8_t BATTLE_THINKING_HEIGHT = 9;
inline constexpr uint8_t BATTLE_THINKING_X = (128 - BATTLE_THINKING_WIDTH) / 2;
inline constexpr uint8_t BATTLE_THINKING_Y = (64 - BATTLE_THINKING_HEIGHT) / 2;
inline constexpr uint8_t BATTLE_THINKING_DOTS = 3;
inline constexpr uint8_t BATTLE_THINKING_DOT = 3;
inline constexpr uint8_t BATTLE_THINKING_SPACING = 5;

// Dots lit at a progress step, counting steps from 1.
constexpr uint8_t battle_thinking_lit(uint8_t step) {
    return (uint8_t)(step % (BATTLE_THINKING_DOTS + 1));
}

// The space between the box's edge and the outer dots, the same on both sides.
inline constexpr uint8_t BATTLE_THINKING_MARGIN =
    (BATTLE_THINKING_WIDTH - (BATTLE_THINKING_DOTS - 1) * BATTLE_THINKING_SPACING -
     BATTLE_THINKING_DOT) /
    2;

// The left edge of a dot, centered in the box.
constexpr uint8_t battle_thinking_dot_x(uint8_t dot) {
    return (uint8_t)(BATTLE_THINKING_X + BATTLE_THINKING_MARGIN + dot * BATTLE_THINKING_SPACING);
}

// Whether a flashing unit is hidden at a frame counted from the start of its flash. Each flash
// starts hidden and alternates every BATTLE_FLASH_STEP frames.
constexpr bool battle_flash_hidden(uint8_t frame) {
    return ((frame / BATTLE_FLASH_STEP) & 1) == 0;
}

#endif
