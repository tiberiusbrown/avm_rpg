#pragma once
#include "music_catalog.h"
#include "boss_music.h"
#include "venus_music.h"

struct MusicTrack {
    void const AVM_PROGMEM* data;
    char const AVM_PROGMEM* title;
};

constexpr char AVM_PROGMEM BOSS_TITLE[] = "Boss";
constexpr char AVM_PROGMEM VENUS_TITLE[] = "Venus";

constexpr MusicTrack AVM_PROGMEM MUSIC_TRACKS[] = {
    {BOSS_MUSIC, BOSS_TITLE},
    {VENUS_MUSIC, VENUS_TITLE},
};
static_assert(sizeof(MUSIC_TRACKS) / sizeof(MUSIC_TRACKS[0]) == MUSIC_COUNT);
