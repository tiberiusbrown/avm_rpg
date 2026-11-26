#include "rpg.h"

#include <avm.h>
#include "ambient.h"
#include "music.h"
#include "sfx.h"

struct SfxTrack {
    void const AVM_PROGMEM* data;
};

#include "sfx_data.h"
static_assert(sizeof(SFX_TRACKS) / sizeof(SFX_TRACKS[0]) == SFX_COUNT);

constexpr uint8_t VOLUME_STEP = 255 / OPTION_VOLUME_MAX;

// Disabling the audio hardware stops playback. Remember the song to start when enabled again.
static void const PROGMEM* muted_song;

static void const PROGMEM* requested_song() {
    auto pending = avm_audio_get_fade_song();
    return pending ? pending : avm_audio_get_music();
}

void music_play(MUSIC_TRACK track) {
    auto song = MUSIC_TRACKS[track].data;
    if(game.options.disabled) {
        muted_song = song;
        return;
    }

    // While a fade runs, the queued song is the one that will play, not the one fading out.
    // Asking for the fading song then replaces the queue and restarts it.
    if(requested_song() != song)
        avm_audio_play_fade(song);
}

void music_play_area() {
    music_play(area_music(game.area));
}

char const PROGMEM* music_next() {
    auto current = game.options.disabled ? muted_song : avm_audio_get_music();
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

// The effect that waits for the playing one to end, plus one, so that zero means none.
static uint8_t sfx_waiting;

// The status bit that says an effect is playing.
constexpr uint8_t SFX_STATUS_PLAYING = 4;

RPG_NOINLINE void sfx_play(SFX effect, SFX then) {
    sfx_waiting = 0;
    if(effect == SFX_NONE) {
        effect = then;
        then = SFX_NONE;
    }
    // A muted effect still takes the music's voices, so silence means no effect at all.
    if(effect == SFX_NONE || game.options.disabled || !game.options.sfx)
        return;

    avm_audio_sfx(SFX_TRACKS[effect].data);
    if(then != SFX_NONE)
        sfx_waiting = (uint8_t)(then + 1);
}

RPG_NOINLINE void sfx_ambient(SFX effect) {
    if(effect == SFX_NONE || sfx_waiting || (avm_audio_status() & SFX_STATUS_PLAYING))
        return;
    sfx_play(effect);
}

RPG_NOINLINE void sfx_update() {
    if(!sfx_waiting || (avm_audio_status() & SFX_STATUS_PLAYING))
        return;
    SFX effect = (SFX)(sfx_waiting - 1);
    sfx_waiting = 0;
    // The player may have muted the effects while the first one played, in the Audio screen.
    if(!game.options.disabled && game.options.sfx)
        avm_audio_sfx(SFX_TRACKS[effect].data);
}

void sfx_menu_tick() {
    // Frames are 20 ms apart, so the clock at a press depends on when the player pressed.
    sfx_play(sfx_variant(SFX_MENU_TICKS_FIRST, SFX_MENU_TICKS_LAST, ambient_hash(avm_millis())));
}

uint8_t option_level(OPTION_ITEM item) {
    return item == OPTION_MUSIC ? game.options.music : game.options.sfx;
}

void option_toggle_audio() {
    game.options.disabled ^= 1;
    if(game.options.disabled) {
        muted_song = requested_song();
        sfx_waiting = 0;
        avm_audio_disable();
    } else if(muted_song) {
        avm_audio_play(muted_song);
        muted_song = nullptr;
    }
}

void option_adjust(OPTION_ITEM item, bool increase) {
    uint8_t level = option_level(item);
    if(increase ? level == OPTION_VOLUME_MAX : level == 0)
        return;
    level = (uint8_t)(level + (increase ? 1 : -1));
    if(item == OPTION_MUSIC) {
        // Finish a queued song change so its saved fade gain cannot undo this volume edit.
        auto pending = avm_audio_get_fade_song();
        if(pending)
            avm_audio_play(pending);
        game.options.music = level;
        avm_audio_music_volume(level * VOLUME_STEP);
    } else {
        game.options.sfx = level;
        avm_audio_sfx_volume(level * VOLUME_STEP);
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

// Journal redraws are not frames. Their own marks keep the frame serial and frame budget checks out
// of the Journal, and the device harness counts them separately.
extern "C" __attribute__((noinline)) void rpg_qa_journal_begin() {
    frame_marker = 5;
}

extern "C" __attribute__((noinline)) void rpg_qa_journal_end() {
    frame_marker = 6;
}
#endif

bool game_load() {
    shop_requested = false;
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
    save_exists = avm_load() && save_valid();
    if(!save_exists)
        game.options = AudioOptions{};
    if(game.options.music > OPTION_VOLUME_MAX)
        game.options.music = OPTION_VOLUME_MAX;
    if(game.options.sfx > OPTION_VOLUME_MAX)
        game.options.sfx = OPTION_VOLUME_MAX;
    avm_audio_music_volume(game.options.music * VOLUME_STEP);
    avm_audio_sfx_volume(game.options.sfx * VOLUME_STEP);
    game_start();
}

extern "C" void loop() {
    pre_title_screen();

    avm_audio_rate(AVM_AUDIO_RATE_16KHZ);
    avm_audio_music_loop(1);

    for(;;) {
        music_play(MUSIC_TITLE);
        title_screen();
        // Prepare the destination while the OLED stays black. A teleport reveals its own scene.
        bool teleported = update_visible_world(0, false);
        music_play_area();
        if(!teleported)
            fade_world_in();

        explore();
        sfx_waiting = 0;
        avm_audio_stop_sfx();
    }
}
