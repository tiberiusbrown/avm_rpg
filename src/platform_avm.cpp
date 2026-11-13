#include "rpg.h"

#include <avm.h>
#include "music.h"
#include "sfx.h"

struct SfxTrack {
    void const AVM_PROGMEM* data;
};

#include "sfx_data.h"
static_assert(sizeof(SFX_TRACKS) / sizeof(SFX_TRACKS[0]) == SFX_COUNT);

constexpr uint8_t MUSIC_MASTER_VOLUME = 128;

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

uint8_t saved_record[SAVE_BYTES] __attribute__((section(".saved"))) = {0};

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

bool game_has_save() {
    return avm_load() && save_valid(saved_record);
}

bool game_load() {
    if(!avm_load() || !save_decode(saved_record))
        return false;
    world_place(game.area, game.player.x, game.player.y, game.player.facing);
    return true;
}

bool game_save() {
    save_encode(saved_record);
    uint16_t expected_crc = save_checksum(saved_record);
    avm_save();

    // Read back and validate before reporting success; the write API has no result.
    return avm_load() && saved_record[SAVE_CRC_OFFSET] == (uint8_t)expected_crc &&
           saved_record[SAVE_CRC_OFFSET + 1] == (uint8_t)(expected_crc >> 8) &&
           save_checksum(saved_record) == expected_crc;
}

extern "C" void setup() {
    avm_set_frame_rate(50);
    avm_set_text_font(AVM_FONT_BR5D);
    avm_set_text_mode(AVM_TEXT_WHITE_TRANSPARENT);
    game_start();
}

extern "C" void loop() {
    title_screen();
    // The title screen only places the player. Refreshing the world here keeps the refresh
    // out of the title screen's frame.
    update_visible_world(0, false);

    //avm_audio_lowpass(AVM_AUDIO_LOWPASS_MILD);
    avm_audio_rate(AVM_AUDIO_RATE_16KHZ);
    avm_audio_volume(MUSIC_MASTER_VOLUME);
    avm_audio_music_loop(1);
    music_play(MUSIC_DEFAULT);
    
    explore();
}
