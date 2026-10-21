#pragma once
#include "music_catalog.h"
#include "test_music.h"

struct MusicTrack {
    void const AVM_PROGMEM* data;
    char const AVM_PROGMEM* title;
};

constexpr char AVM_PROGMEM TEST_TITLE[] = "Test";

constexpr MusicTrack AVM_PROGMEM MUSIC_TRACKS[] = {
    {TEST_MUSIC, TEST_TITLE},
};
static_assert(sizeof(MUSIC_TRACKS) / sizeof(MUSIC_TRACKS[0]) == MUSIC_COUNT);
