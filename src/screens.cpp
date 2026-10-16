#include "rpg.h"

#include <avm.h>

extern "C" void rpg_frame_begin();
extern "C" void rpg_frame_end();
extern "C" void rpg_render_begin();
extern "C" void rpg_render_end();

__attribute__((noinline)) void title_screen(Game& g) {
    bool can_continue = game_load(g);
    uint8_t selection = 0;

    for(;;) {
        render_title(can_continue, selection);
        avm_display(true);
        uint8_t pressed = input_wait_press();

        if(pressed & (BTN_UP | BTN_DOWN | BTN_LEFT | BTN_RIGHT))
            selection ^= 1;
        if(pressed & BTN_A) {
            if(!selection) {
                game_new(g);
                return;
            }
            if(can_continue && game_load(g))
                return;
            can_continue = false;
        }
    }
}

__attribute__((noinline)) void status_screen(Game const& g) {
    render_status(g);
    avm_display(true);
    input_wait_press(BTN_A | BTN_B);
}

__attribute__((noinline)) void menu_screen(Game& g) {
    uint8_t selection = 0;
    char const PROGMEM* feedback = nullptr;

    for(;;) {
        render_menu(g, selection, feedback);
        avm_display(true);
        uint8_t pressed = input_wait_press();

        if(pressed & BTN_B)
            return;
        if(pressed & (BTN_DOWN | BTN_RIGHT))
            selection = (uint8_t)((selection + 1) % 3);
        else if(pressed & (BTN_UP | BTN_LEFT))
            selection = (uint8_t)((selection + 2) % 3);
        else if(pressed & BTN_A) {
            if(selection == 0)
                status_screen(g);
            else if(selection == 1)
                feedback = game_save(g) ? F("Saved and checked") : F("Save failed");
            else
                return;
        }
    }
}

__attribute__((noinline)) void explore(Game& g) {
    input_sync();

    for(;;) {
        input_wait_frame();
        uint8_t pressed = input_pressed();

        if(pressed & BTN_B) {
            menu_screen(g);
            input_sync();
            continue;
        }
        if(pressed & BTN_A) {
            uint8_t id = interaction_target(g);
            if(id)
                dialogue_talk(g, id);
            else
                dialogue_show(g, F("Face someone and press A to talk."));
            input_sync();
            continue;
        }

        rpg_frame_begin();
        game_update(g, input_held());
        rpg_render_begin();
        rpg_render_world(g);
        avm_display(true);
        rpg_render_end();
        rpg_frame_end();
    }
}
