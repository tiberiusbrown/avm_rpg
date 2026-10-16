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

static bool restore() {
    return avm_load() && save_decode(game, saved_record);
}

extern "C" void setup() {
    bool valid = avm_save_exists() && restore();

    game_boot(game, valid);

    avm_set_frame_rate(20);
    avm_set_text_font(AVM_FONT_BR5D);
    avm_set_text_mode(AVM_TEXT_WHITE_TRANSPARENT);
}

extern "C" void loop() {
    COMMAND command;
    uint16_t expected_crc;

    if(!avm_next_frame())
        return;

    rpg_frame_begin();
    avm_poll_buttons();
    command = game_tick(game, avm_buttons());

    if(command == SAVE_COMMAND) {
        save_encode(game, saved_record);
        expected_crc = save_checksum(saved_record);
        avm_save();

        /* The API has no write result; read back and validate before reporting success. */
        game.save_valid = (uint8_t)(avm_load() && saved_record[22] == (uint8_t)expected_crc &&
                                    saved_record[23] == (uint8_t)(expected_crc >> 8) &&
                                    save_checksum(saved_record) == expected_crc);
        game.feedback = 40;
    } else if(command == LOAD_COMMAND) {
        if(!restore())
            game_boot(game, false);
        game.previous_buttons = avm_buttons();
    }

    rpg_render_begin();
    rpg_render(game);
    avm_display(true);
    rpg_render_end();
    rpg_frame_end();
}
