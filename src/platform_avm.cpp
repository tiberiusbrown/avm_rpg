#include "rpg.h"

#include <avm.h>
#include "music.h"
#include "sfx.h"

struct SfxTrack {
    void const AVM_PROGMEM* data;
};

#include "sfx_data.h"
static_assert(sizeof(SFX_TRACKS) / sizeof(SFX_TRACKS[0]) == SFX_COUNT);

constexpr uint8_t MUSIC_MASTER_VOLUME = 255;

void music_play(MUSIC_TRACK track) {
    auto current = avm_audio_get_music();
    if(current == MUSIC_TRACKS[track].data)
        return;

    avm_audio_play_fade(MUSIC_TRACKS[track].data);
}

char const PROGMEM* music_next() {
    auto current = avm_audio_get_music();
    MUSIC_TRACK next = MUSIC_DEFAULT;
    for(uint8_t i = 0; i < MUSIC_COUNT; ++i) {
        if(current == MUSIC_TRACKS[i].data) {
            next = (MUSIC_TRACK)((i + 1) % MUSIC_COUNT);
            break;
        }
    }
    music_play(next);
    return MUSIC_TRACKS[next].title;
}

RPG_NOINLINE void sfx_play(SFX effect) {
    avm_audio_sfx(SFX_TRACKS[effect].data);
}

static constexpr uint8_t LOWPASS_LEVELS[] PROGMEM = {
    0, 128, 112, 96, 80, 64, 56, 48, 40, 32, 24, 16, 12, 8, 4, 1};
static_assert(sizeof(LOWPASS_LEVELS) == OPTION_SLIDER_MAX + 1);

uint8_t option_level(OPTION_ITEM item) {
    if(item == OPTION_MUSIC)
        return game.options.music;
    if(item == OPTION_SFX)
        return game.options.sfx;
    return game.options.lowpass;
}

void options_apply() {
    avm_audio_music_volume(((uint16_t)game.options.music * 255 + OPTION_VOLUME_MAX / 2) /
                         OPTION_VOLUME_MAX);
    avm_audio_sfx_volume(((uint16_t)game.options.sfx * 255 + OPTION_VOLUME_MAX / 2) /
                       OPTION_VOLUME_MAX);
    avm_audio_lowpass(LOWPASS_LEVELS[game.options.lowpass]);
}

void option_adjust(OPTION_ITEM item, bool increase) {
    uint8_t level = option_level(item);
    if(increase ? level == option_max(item) : level == 0)
        return;
    level = (uint8_t)(level + (increase ? 1 : -1));
    if(item == OPTION_MUSIC) {
        // Finish a queued song change so its saved fade gain cannot undo this volume edit.
        auto pending = avm_audio_get_fade_song();
        if(pending)
            avm_audio_play(pending);
        game.options.music = level;
        avm_audio_music_volume(((uint16_t)level * 255 + OPTION_VOLUME_MAX / 2) / OPTION_VOLUME_MAX);
    } else if(item == OPTION_SFX) {
        game.options.sfx = level;
        avm_audio_sfx_volume(((uint16_t)level * 255 + OPTION_VOLUME_MAX / 2) / OPTION_VOLUME_MAX);
    } else {
        game.options.lowpass = level;
        avm_audio_lowpass(LOWPASS_LEVELS[level]);
    }
}

#ifdef RPG_DEVICE_QA
volatile uint16_t frame_serial;
volatile uint8_t frame_marker;
volatile uint8_t world_phase;

extern "C" __attribute__((noinline)) void rpg_frame_begin() {
    frame_marker = 1;
}

extern "C" __attribute__((noinline)) void rpg_frame_end() {
    frame_marker = 0;
    frame_serial = (uint16_t)(frame_serial + 1);
}

extern "C" __attribute__((noinline)) void rpg_render_begin() {
    frame_marker = 2;
}

extern "C" __attribute__((noinline)) void rpg_render_end() {
    frame_marker = 3;
}
#endif

bool game_load() {
    if(!save_exists)
        return false;
    AudioOptions options = game.options;
    bool loaded = avm_load();
    game.options = options;
    if(!loaded || !save_restore()) {
        save_exists = false;
        game_start();
        return false;
    }
    return true;
}

void game_save() {
    avm_save();
    save_exists = true;
}

extern "C" void setup() {
    avm_set_frame_rate(50);
    avm_set_text_font(AVM_FONT_BR5D);
    avm_set_text_mode(AVM_TEXT_WHITE_TRANSPARENT);
    avm_audio_volume(MUSIC_MASTER_VOLUME);
    save_exists = avm_load() && save_valid();
    if(!save_exists)
        game.options = AudioOptions{};
    options_apply();
    game_start();
}

extern "C" void loop() {
    title_screen();
    // The title screen only places the player. Refreshing the world here keeps the refresh
    // out of the title screen's frame.
    update_visible_world(0, false);

    avm_audio_rate(AVM_AUDIO_RATE_16KHZ);
    avm_audio_music_loop(1);
    music_play(MUSIC_DEFAULT);
    
    explore();
}
