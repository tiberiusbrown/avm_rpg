#include "rpg.h"
#include "battle_cursor.h"
#include "music_catalog.h"
#include "title.h"
#include "world_map.h"
#include "world_assets.h"

#include <avm.h>

// A or B continues the saved game when there is one, and starts a new game otherwise.
__attribute__((noinline)) void title_screen() {
    uint16_t ticks = 0;
    input_sync();

    for(;;) {
        input_wait_frame();
        render_title(ticks);
        // The picture stays in the buffer; the next frame draws over all of it.
        avm_display(false);
        ticks = (uint16_t)(ticks + 1 == TITLE_CYCLE ? 0 : ticks + 1);

        if(input_pressed() & (BTN_A | BTN_B))
            break;
    }

    // Exploration expects the cleared buffer that its own frames leave behind.
    avm_draw_filled_rect_black(0, 0, 128, 64);
    // A load that fails prepares a new game itself.
    if(save_exists)
        game_load();
    else
        game_start();
}

__attribute__((noinline)) void party_screen() {
    int16_t revealed = 0;
    bool closing = false;
    input_sync();

    for(;;) {
        input_wait_frame();
        revealed = cursor_ease(revealed, closing ? 0 : cursor_subpixel(PARTY_WIDTH));
        render_party((uint8_t)cursor_pixel(revealed), closing);
        avm_display(true);
        if(closing) {
            if(!revealed)
                return;
        } else if(input_pressed() & (BTN_A | BTN_B))
            closing = true;
    }
}

__attribute__((noinline)) void map_screen() {
    int16_t x = (game.player.x + FOOT_CENTER_X) / TILE;
    int16_t y = (game.player.y + FOOT_CENTER_Y) / TILE;
    WorldMapView view{{(int16_t)(map_clamp(x, OUTDOOR_WIDTH - 1) * MAP_SUBPIXELS)},
                      {(int16_t)(map_clamp(y, OUTDOOR_HEIGHT - 1) * MAP_SUBPIXELS)},
                      map_clamp(x - 64, OUTDOOR_WIDTH > 128 ? OUTDOOR_WIDTH - 128 : 0),
                      map_clamp(y - 32, OUTDOOR_HEIGHT > 64 ? OUTDOOR_HEIGHT - 64 : 0)};
    uint8_t ticks = 0;
    transition_world_map(view, ticks, false);
    input_sync();
    for(;;) {
        input_wait_frame();
        if(input_pressed() & BTN_B) {
            transition_world_map(view, (uint8_t)(ticks - 1), true);
            return;
        }
        update_world_map(view, input_held(), OUTDOOR_WIDTH, OUTDOOR_HEIGHT);
        render_world_map(view, ticks++);
        avm_display(true);
    }
}

bool menu_item_visible(PAUSE_ITEM item, bool can_load, bool in_battle) {
    switch(item) {
        case PAUSE_SAVE:
        case PAUSE_PARTY:
        case PAUSE_JOURNAL:
            return !in_battle;
        case PAUSE_MAP:
            return !in_battle && game.area == AREA_OUTDOOR;
        case PAUSE_PASS:
        case PAUSE_RESIGN:
            return in_battle;
        case PAUSE_LOAD:
            return !in_battle && can_load;
        default:
            return true;
    }
}

__attribute__((noinline)) MENU_EXIT menu_screen(BattleScreen const* battle, PAUSE_ITEM initial) {
    PAUSE_ITEM selection = initial;
    uint8_t width = menu_width(battle != nullptr);
    int16_t revealed = 0;
    bool closing = false;
    MENU_EXIT action = MENU_RESUME;
    char const PROGMEM* feedback = nullptr;
    // Zero is closed; one waits for a fresh A; larger values guard the first 240 ms.
    uint8_t confirmation = 0;
    constexpr uint8_t CONFIRM_GUARD_FRAMES = 12;
    input_sync();

    for(;;) {
        input_wait_frame();
        revealed = cursor_ease(revealed, closing ? 0 : cursor_subpixel(width));
        render_menu((uint8_t)cursor_pixel(revealed), width, selection, save_exists, feedback,
                    action, battle);
        if(confirmation)
            render_menu_confirmation(selection);
        avm_display(true);
        uint8_t pressed = input_pressed();

        if(closing) {
            if(!revealed || (menu_opens_screen(action) && !cursor_pixel(revealed)))
                return action;
            continue;
        }
        if(confirmation) {
            if(pressed & BTN_B)
                confirmation = 0;
            else if(confirmation == 1 && (pressed & BTN_A)) {
                confirmation = 0;
                action = selection == PAUSE_PASS ? MENU_PASS : MENU_RESIGN;
                closing = true;
            } else if(confirmation > 1)
                --confirmation;
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
            } while(!menu_item_visible(selection, save_exists, battle != nullptr));
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
                    action = MENU_PARTY;
                    closing = true;
                    feedback = nullptr;
                    break;
                case PAUSE_MAP:
                    action = MENU_MAP;
                    closing = true;
                    feedback = nullptr;
                    break;
                case PAUSE_PASS:
                case PAUSE_RESIGN:
                    confirmation = CONFIRM_GUARD_FRAMES + 1;
                    feedback = nullptr;
                    break;
                case PAUSE_LOAD:
                    action = MENU_LOAD;
                    closing = true;
                    feedback = nullptr;
                    break;
                case PAUSE_AUDIO:
                    action = MENU_AUDIO;
                    closing = true;
                    feedback = nullptr;
                    break;
                default:
                    break;
            }
        }
    }
}

__attribute__((noinline)) void audio_screen(BattleScreen const* battle) {
    OPTION_ITEM selection = OPTION_MUSIC;
    int16_t revealed = 0;
    bool closing = false;
    uint8_t horizontal = 0, repeat = 0;
    input_sync();

    for(;;) {
        input_wait_frame();
        revealed = cursor_ease(revealed, closing ? 0 : cursor_subpixel(AUDIO_WIDTH));
        render_audio((uint8_t)cursor_pixel(revealed), selection, closing, battle);
        avm_display(true);
        uint8_t pressed = input_pressed();
        if(closing) {
            if(!revealed)
                return;
            continue;
        }
        if(pressed & BTN_B) {
            closing = true;
            continue;
        }
        if(pressed & (BTN_UP | BTN_DOWN)) {
            selection =
                (OPTION_ITEM)((selection + (pressed & BTN_DOWN ? 1 : OPTION_ITEM_COUNT - 1)) %
                              OPTION_ITEM_COUNT);
            horizontal = 0;
        }

        uint8_t held = input_held() & (BTN_LEFT | BTN_RIGHT);
        if(held == (BTN_LEFT | BTN_RIGHT))
            held = 0;
        bool adjust = false;
        if(held != horizontal || !held) {
            horizontal = held;
            repeat = 12;
            adjust = held != 0;
        } else if(repeat && !--repeat) {
            adjust = true;
            repeat = 3;
        }
        if(adjust)
            option_adjust(selection, held == BTN_RIGHT);
    }
}

__attribute__((noinline)) void explore() {
    input_sync();

    for(;;) {
        dialogue_close();
        input_wait_frame();
        uint8_t pressed = input_pressed();

        if(pressed & BTN_B) {
            // Refresh after the menu's locals have left the stack: chunk dialogue can enter battle.
            MENU_EXIT action = menu_screen();
            if(action == MENU_PARTY)
                party_screen();
            else if(action == MENU_MAP)
                map_screen();
            if(action == MENU_LOAD) {
                game_load();
                update_visible_world(0, false);
                music_play_area();
                // Keep the black load frame while rebuilding the saved world. Its first render
                // starts a fresh frame, so validation/casting and drawing do not share a budget.
                input_wait_frame();
                avm_display(true);
                fade_world_in();
            } else if(action == MENU_AUDIO)
                audio_screen();
            input_sync();
            continue;
        }
        if(pressed & BTN_A) {
            // A teleport takes the player to an area with its own song.
            if(game_update(0, pressed))
                music_play_area();
            input_sync();
            continue;
        }

        RPG_QA_MARK(rpg_frame_begin());
        if(game_update(input_held()))
            music_play_area();
        dialogue_close();
        RPG_QA_MARK(rpg_render_begin());
        rpg_render_world();
        avm_display(true);
        RPG_QA_MARK(rpg_render_end());
        RPG_QA_MARK(rpg_frame_end());
    }
}
