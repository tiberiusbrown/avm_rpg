#include "rpg.h"
#include "battle_cursor.h"

#include <avm.h>

__attribute__((noinline)) void title_screen() {
    uint8_t selection = 0;

    for(;;) {
        render_title(save_exists, selection);
        avm_display(true);
        uint8_t pressed = input_wait_press();

        if(pressed & (BTN_UP | BTN_DOWN | BTN_LEFT | BTN_RIGHT))
            selection ^= 1;
        if(pressed & BTN_A) {
            if(!selection) {
                game_start();
                return;
            }
            if(save_exists && game_load())
                return;
        }
    }
}

__attribute__((noinline)) void status_screen() {
    render_status();
    avm_display(true);
    input_wait_press(BTN_A | BTN_B);
}

bool menu_item_visible(PAUSE_ITEM item, bool can_load) {
    return (item != PAUSE_MAP || game.area == AREA_OUTDOOR) && (item != PAUSE_LOAD || can_load);
}

__attribute__((noinline)) bool menu_screen() {
    PAUSE_ITEM selection = PAUSE_RESUME;
    uint8_t width = menu_width();
    int16_t revealed = 0;
    bool closing = false;
    bool load_requested = false;
    char const PROGMEM* feedback = nullptr;
    input_sync();

    for(;;) {
        input_wait_frame();
        revealed = cursor_ease(revealed, closing ? 0 : cursor_subpixel(width));
        render_menu((uint8_t)cursor_pixel(revealed), width, selection, save_exists, feedback,
                    load_requested);
        avm_display(true);
        uint8_t pressed = input_pressed();

        if(closing) {
            if(!revealed)
                return load_requested;
            continue;
        }
        if(pressed & BTN_B) {
            closing = true;
            feedback = nullptr;
        } else if(pressed & (BTN_DOWN | BTN_UP)) {
            do {
                selection =
                    (PAUSE_ITEM)((selection + (pressed & BTN_DOWN ? 1 : PAUSE_ITEM_COUNT - 1)) %
                                 PAUSE_ITEM_COUNT);
            } while(!menu_item_visible(selection, save_exists));
            feedback = nullptr;
        } else if(pressed & BTN_A) {
            switch(selection) {
                case PAUSE_RESUME:
                    closing = true;
                    feedback = nullptr;
                    break;
                case PAUSE_SAVE:
                    game_save();
                    feedback = F("Saved");
                    break;
                case PAUSE_PARTY:
                    status_screen();
                    input_sync();
                    break;
                case PAUSE_LOAD:
                    load_requested = true;
                    closing = true;
                    feedback = nullptr;
                    break;
                default:
                    break;
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
            // Refresh after the menu's locals have left the stack: chunk dialogue can enter battle.
            if(menu_screen()) {
                game_load();
                update_visible_world(0, false);
                fade_world_in();
            }
            input_sync();
            continue;
        }
        if(pressed & BTN_A) {
            game_update(0, pressed);
            input_sync();
            continue;
        }

        RPG_QA_MARK(rpg_frame_begin());
        game_update(input_held());
        RPG_QA_MARK(rpg_render_begin());
        rpg_render_world();
        avm_display(true);
        RPG_QA_MARK(rpg_render_end());
        RPG_QA_MARK(rpg_frame_end());
    }
}
