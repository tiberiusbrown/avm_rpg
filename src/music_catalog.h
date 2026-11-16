#pragma once
#include "area_ids.h"
#include "music_ids.h"

// The song an area plays while the player explores it. Every area names its song here, so a
// new score for an area replaces its line and nothing else.
constexpr MUSIC_TRACK area_music(uint8_t area) {
    switch(area) {
        case AREA_CAVE: return MUSIC_CAVE;
        case AREA_OUTDOOR: return MUSIC_PEACEFUL;
        case AREA_INTERIORS: return MUSIC_PEACEFUL;
        case AREA_DUNGEON: return MUSIC_PEACEFUL;
        default: return MUSIC_DEFAULT;
    }
}

// Queue a different song with the interpreter's asynchronous music fade.
// Same-song requests leave playback alone; master/SFX volume is unchanged.
void music_play(MUSIC_TRACK track);
// music_play for the song of the area the player is in.
void music_play_area();
