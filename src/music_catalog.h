#pragma once
#include "music_ids.h"

// Queue a different song with the interpreter's asynchronous music fade.
// Same-song requests leave playback alone; master/SFX volume is unchanged.
void music_play(MUSIC_TRACK track);
