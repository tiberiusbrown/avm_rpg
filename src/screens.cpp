#include "rpg.h"

#include <avm.h>

extern "C" void rpg_frame_begin();
extern "C" void rpg_frame_end();
extern "C" void rpg_render_begin();
extern "C" void rpg_render_end();

__attribute__((noinline)) void title_screen() {
    bool can_continue = game_load();
    uint8_t selection = 0;

    for(;;) {
        render_title(can_continue, selection);
        avm_display(true);
        uint8_t pressed = input_wait_press();

        if(pressed & (BTN_UP | BTN_DOWN | BTN_LEFT | BTN_RIGHT))
            selection ^= 1;
        if(pressed & BTN_A) {
            if(!selection) {
                game_new();
                return;
            }
            if(can_continue && game_load())
                return;
            can_continue = false;
        }
    }
}

__attribute__((noinline)) void status_screen() {
    render_status();
    avm_display(true);
    input_wait_press(BTN_A | BTN_B);
}

__attribute__((noinline)) void menu_screen() {
    static uint8_t audio_rate = AVM_AUDIO_RATE_16KHZ;
    uint8_t selection = 0;
    char const PROGMEM* feedback = nullptr;

    for(;;) {
        render_menu(selection, feedback, audio_rate);
        avm_display(true);
        uint8_t pressed = input_wait_press();

        if(pressed & BTN_B)
            return;
        if(pressed & (BTN_DOWN | BTN_RIGHT))
            selection = (uint8_t)((selection + 1) % 5);
        else if(pressed & (BTN_UP | BTN_LEFT))
            selection = (uint8_t)((selection + 4) % 5);
        else if(pressed & BTN_A) {
            if(selection == 0)
                status_screen();
            else if(selection == 1)
                feedback = game_save() ? F("Saved and checked") : F("Save failed");
            else if(selection == 2)
                return;
            else if(selection == 3)
                feedback = music_next();
            else {
                audio_rate ^= 1;
                avm_audio_rate(audio_rate);
            }
        }
    }
}

__attribute__((noinline)) void explore() {
    input_sync();

    for(;;) {
        input_wait_frame();
        uint8_t pressed = input_pressed();

        if(pressed & BTN_B) {
            menu_screen();
            input_sync();
            continue;
        }
        if(pressed & BTN_A) {
            game_update(0, pressed);
            input_sync();
            continue;
        }

        rpg_frame_begin();
        game_update(input_held());
        rpg_render_begin();
        rpg_render_world();
        avm_display(true);
        rpg_render_end();
        rpg_frame_end();
    }
}
