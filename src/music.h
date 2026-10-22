#pragma once
#include "music_catalog.h"

struct MusicTrack {
    void const AVM_PROGMEM* data;
    char const AVM_PROGMEM* title;
};

#include "music_tracks.h"
static_assert(sizeof(MUSIC_TRACKS) / sizeof(MUSIC_TRACKS[0]) == MUSIC_COUNT);
