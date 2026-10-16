#include "rpg.h"

#include <avm.h>

Game game;
uint8_t saved_record[SAVE_BYTES] __attribute__((section(".saved"))) = {0};

volatile uint16_t frame_serial;
volatile uint8_t frame_marker;

extern "C" __attribute__((noinline)) void rpg_frame_begin() {
    frame_marker = 1;
}

extern "C" __attribute__((noinline)) void rpg_frame_end() {
    frame_marker = 0;
    ++frame_serial;
}

extern "C" __attribute__((noinline)) void rpg_render_begin() {
    frame_marker = 2;
}

extern "C" __attribute__((noinline)) void rpg_render_end() {
    frame_marker = 3;
}

bool game_load(Game& g) {
    return avm_load() && save_decode(g, saved_record);
}

bool game_save(Game const& g) {
    save_encode(g, saved_record);
    uint16_t expected_crc = save_checksum(saved_record);
    avm_save();

    // Read back and validate before reporting success; the write API has no result.
    return avm_load() && saved_record[22] == (uint8_t)expected_crc &&
           saved_record[23] == (uint8_t)(expected_crc >> 8) &&
           save_checksum(saved_record) == expected_crc;
}

extern "C" void setup() {
    avm_set_frame_rate(20);
    avm_set_text_font(AVM_FONT_BR5D);
    avm_set_text_mode(AVM_TEXT_WHITE_TRANSPARENT);
    game_new(game);
}

extern "C" void loop() {
    title_screen(game);
    explore(game);
}
