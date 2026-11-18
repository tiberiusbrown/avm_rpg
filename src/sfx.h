#ifndef SFX_H
#define SFX_H

#include "sfx_ids.h"

// The value after the last effect: no sound.
inline constexpr SFX SFX_NONE = SFX_COUNT;

// Starts a one-shot sound effect from assets/sfx, named SFX_<FILE>. It replaces any effect still
// playing. An effect takes the music's harmony voice, its noise voice, or both, for as long as it
// lasts; the melody and bass keep playing.
//
// `then` waits for `effect` to end and then plays, unless another effect starts first. That chains
// a trigger's own sound to the sound of what it moves. A SFX_NONE `effect` starts `then` at once,
// and two SFX_NONE make no sound. Nothing plays while the SFX option is zero, because a muted
// effect would still take its voices from the music.
void sfx_play(SFX effect, SFX then = SFX_NONE);

// A sound of the surroundings. It plays only when no other effect is playing or waiting, so it
// never cuts off a sound that answers the player. SFX_NONE does nothing.
void sfx_ambient(SFX effect);

// The first and last of each group of variants. A group's effects are contiguous, so a random
// index picks one.
inline constexpr SFX SFX_MENU_TICKS_FIRST = SFX_MENU_TICK_A, SFX_MENU_TICKS_LAST = SFX_MENU_TICK_E;
inline constexpr SFX SFX_BIRDS_FIRST = SFX_BIRD_CHIRP, SFX_BIRDS_LAST = SFX_BIRD_TWEET;
inline constexpr SFX SFX_DRIPS_FIRST = SFX_DRIP_A, SFX_DRIPS_LAST = SFX_DRIP_B;
inline constexpr SFX SFX_WATERS_FIRST = SFX_WATER_PLIP, SFX_WATERS_LAST = SFX_WATER_SPLASH;

// The effect at `index` of a group, which wraps around the group's size.
constexpr SFX sfx_variant(SFX first, SFX last, uint16_t index) {
    return (SFX)(first + index % (last - first + 1));
}

// One of the small menu ticks, chosen at random, for a cursor that moves through a list.
void sfx_menu_tick();

// Frame housekeeping, run once per displayed frame: starts the effect that waits for the playing
// one as soon as that one ends.
void sfx_update();

#endif
