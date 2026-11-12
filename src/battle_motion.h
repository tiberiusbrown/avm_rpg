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
// Frames each hidden or visible phase of a flash lasts.
inline constexpr uint8_t BATTLE_FLASH_STEP = 4;

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

// Whether a flashing unit is hidden at a frame counted from the start of its flash. Each flash
// starts hidden and alternates every BATTLE_FLASH_STEP frames.
constexpr bool battle_flash_hidden(uint8_t frame) {
    return ((frame / BATTLE_FLASH_STEP) & 1) == 0;
}

#endif
