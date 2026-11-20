#include "rpg.h"
#include "battle_cursor.h"
#include "journal.h"
#include "music_catalog.h"
#include "party_ui.h"
#include "quest.h"
#include "sfx.h"
#include "soundscape.h"
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

// The Party panel is one loop over its pages, so a press changes the page without replaying the
// slide. Only a change to the picture is drawn and presented. The panel closes from the overview
// with B, and every other page returns to the page above it.
__attribute__((noinline)) void party_screen() {
    PartyUi ui;
    party_ui_open(ui);
    int16_t revealed = 0;
    bool closing = false;
    bool redraw = true;
    input_sync();

    for(;;) {
        input_wait_frame();
        int16_t pixel = cursor_pixel(revealed);
        revealed = cursor_ease(revealed, closing ? 0 : cursor_subpixel(PARTY_WIDTH));
        if(redraw || cursor_pixel(revealed) != pixel) {
            render_party((uint8_t)cursor_pixel(revealed), closing, ui);
            // A waiting page keeps its frame in RAM, as the title and battle do.
            avm_display(false);
            redraw = false;
        }
        if(closing) {
            if(!revealed)
                return;
            continue;
        }
        uint8_t pressed = input_pressed();
        if(!pressed)
            continue;

        PARTY_PAGE before = ui.page;
        switch(party_ui_press(ui, game.party, pressed)) {
            case PARTY_EVENT_NONE:
                continue;
            case PARTY_EVENT_EDGE:
                sfx_play(SFX_CURSOR_EDGE);
                continue;
            case PARTY_EVENT_TICK:
                sfx_menu_tick();
                break;
            case PARTY_EVENT_SELECT:
                sfx_play(SFX_SELECT);
                break;
            case PARTY_EVENT_CANCEL:
                sfx_play(SFX_CANCEL);
                break;
            case PARTY_EVENT_LEARN:
                if(!party_learn_perk(game.party.members[ui.member], party_ui_choice_track(ui))) {
                    sfx_play(SFX_CURSOR_EDGE);
                    continue;
                }
                party_ui_perk_learned(ui);
                sfx_play(SFX_SELECT);
                break;
            case PARTY_EVENT_CLOSE:
                sfx_play(SFX_CANCEL);
                closing = true;
                break;
        }
        redraw = true;
        // A detail page needs its description measured before the scroll limits apply.
        if(ui.page != before && party_ui_describes(ui.page))
            party_layout(ui);
    }
}

// Tracking A toggles the open quest. Only an active quest can be tracked, so any other quest
// answers with the cursor-edge sound instead.
static void journal_track(JournalView& view) {
    if(quest_status(view.open) != QUEST_ACTIVE) {
        sfx_play(SFX_CURSOR_EDGE);
        return;
    }
    sfx_play(SFX_SELECT);
    if(quest_selected() == view.open)
        quest_deselect();
    else
        quest_select(view.open);
}

// The list highlights and opens quests. The detail view tracks, scrolls and goes back. The world
// does not simulate while the Journal is open, and the view lives only in this frame.
__attribute__((noinline)) void journal_screen() {
    JournalView view{};
    int16_t revealed = 0;
    bool closing = false;
    // Only a changed panel is drawn and presented. While it slides, and after input changes the
    // view, a frame redraws it. Otherwise the frame waits for input and keeps the framebuffer, as the
    // dialogue's waiting frames do.
    bool redraw = true;
    input_sync();

    for(;;) {
        input_wait_frame();
        int16_t pixel = cursor_pixel(revealed);
        revealed = cursor_ease(revealed, closing ? 0 : cursor_subpixel(JOURNAL_WIDTH));
        if(redraw || cursor_pixel(revealed) != pixel) {
            RPG_QA_MARK(rpg_qa_journal_begin());
            render_journal((uint8_t)cursor_pixel(revealed), closing, view);
            avm_display(false);
            RPG_QA_MARK(rpg_qa_journal_end());
            redraw = false;
        }
        if(closing) {
            if(!revealed)
                return;
            continue;
        }
        uint8_t pressed = input_pressed();
        if(view.open == QUEST_NONE) {
            uint8_t count = quest_journal_count();
            if(pressed & BTN_B) {
                sfx_play(SFX_CANCEL);
                closing = true;
            } else if(count && (pressed & (BTN_UP | BTN_DOWN))) {
                sfx_menu_tick();
                view.selection =
                    (uint8_t)((view.selection + (pressed & BTN_DOWN ? 1 : count - 1)) % count);
                redraw = true;
            } else if(count && (pressed & BTN_A)) {
                sfx_play(SFX_SELECT);
                journal_open(view, quest_journal_entry(view.selection));
                redraw = true;
            }
        } else if(pressed & BTN_B) {
            sfx_play(SFX_CANCEL);
            view.open = QUEST_NONE;
            redraw = true;
        } else if(pressed & BTN_A) {
            journal_track(view);
            redraw = true;
        } else if((pressed & BTN_UP) && view.scroll) {
            --view.scroll;
            sfx_menu_tick();
            redraw = true;
        } else if((pressed & BTN_DOWN) && view.scroll + JOURNAL_DETAIL_LINES < view.line_count) {
            ++view.scroll;
            sfx_menu_tick();
            redraw = true;
        }
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
            sfx_play(SFX_CANCEL);
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
    sfx_menu_tick();

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
            if(pressed & BTN_B) {
                sfx_play(SFX_CANCEL);
                confirmation = 0;
            } else if(confirmation == 1 && (pressed & BTN_A)) {
                sfx_play(SFX_SELECT);
                confirmation = 0;
                action = selection == PAUSE_PASS ? MENU_PASS : MENU_RESIGN;
                closing = true;
            } else if(confirmation > 1)
                --confirmation;
            continue;
        }
        if(pressed & BTN_B) {
            sfx_play(SFX_CANCEL);
            closing = true;
            feedback = nullptr;
        } else if(pressed & (BTN_DOWN | BTN_UP)) {
            sfx_menu_tick();
            do {
                selection =
                    (PAUSE_ITEM)((selection + (pressed & BTN_DOWN ? 1 : PAUSE_ITEM_COUNT - 1)) %
                                 PAUSE_ITEM_COUNT);
            } while(!menu_item_visible(selection, save_exists, battle != nullptr));
            feedback = nullptr;
        } else if(pressed & BTN_A) {
            sfx_play(SFX_SELECT);
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
                case PAUSE_JOURNAL:
                    action = MENU_JOURNAL;
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
            sfx_play(SFX_CANCEL);
            closing = true;
            continue;
        }
        if(pressed & (BTN_UP | BTN_DOWN)) {
            sfx_menu_tick();
            selection =
                (OPTION_ITEM)((selection + (pressed & BTN_DOWN ? 1 : OPTION_ITEM_COUNT - 1)) %
                              OPTION_ITEM_COUNT);
            horizontal = 0;
        }

        uint8_t held = input_held() & (BTN_LEFT | BTN_RIGHT);
        if(held == (BTN_LEFT | BTN_RIGHT))
            held = 0;
        bool adjust = false, fresh = false;
        if(held != horizontal || !held) {
            horizontal = held;
            repeat = 12;
            adjust = fresh = held != 0;
        } else if(repeat && !--repeat) {
            adjust = true;
            repeat = 3;
        }
        if(adjust) {
            uint8_t before = option_level(selection);
            option_adjust(selection, held == BTN_RIGHT);
            // The tick plays at the new SFX level. A slider at its end bumps once per press, so a
            // held direction does not buzz.
            if(option_level(selection) != before)
                sfx_menu_tick();
            else if(fresh)
                sfx_play(SFX_CURSOR_EDGE);
        }
    }
}

RPG_NOINLINE static void show_quest_level_ups(LevelUpNotice notice) {
    rpg_render_world();
    render_level_up_notice(F("LEVEL UP"), notice);
    avm_display(false);
    input_wait_press(BTN_A);
    input_sync();
}

__attribute__((noinline)) void explore() {
    input_sync();

    for(;;) {
        dialogue_close();
        if(LevelUpNotice notice = quest_take_level_ups(); notice.members)
            show_quest_level_ups(notice);
        input_wait_frame();
        uint8_t pressed = input_pressed();

        if(pressed & BTN_B) {
            // Refresh after the menu's locals have left the stack: chunk dialogue can enter battle.
            MENU_EXIT action = menu_screen();
            if(action == MENU_PARTY)
                party_screen();
            else if(action == MENU_JOURNAL)
                journal_screen();
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
        // The soundscape judges every eighth tick, is silent on almost all of those, and never cuts
        // off another effect.
        if(sound_listening(game.ticks))
            sfx_ambient(ambient_sound_now());
        dialogue_close();
        RPG_QA_MARK(rpg_render_begin());
        rpg_render_world();
        avm_display(true);
        RPG_QA_MARK(rpg_render_end());
        RPG_QA_MARK(rpg_frame_end());
    }
}
