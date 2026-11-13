#ifndef SFX_H
#define SFX_H

#include "sfx_ids.h"

// Starts a one-shot sound effect from assets/sfx, named SFX_<FILE>. It replaces any effect still
// playing. An effect takes the music's harmony voice, its noise voice, or both, for as long as it
// lasts; the melody and bass keep playing.
void sfx_play(SFX effect);

#endif
