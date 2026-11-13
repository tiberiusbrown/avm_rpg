#include "rpg.h"
#include "tile_ids.h"
#include "npc.h"
#include "actor_draw_order.h"

#include <avm.h>

static constexpr uint16_t DISPLAY_WIDTH = 128;

// Eight 20 ms steps per half of the transition. Seven intermediate erase masks live in flash;
// the endpoints need no sprite. A 16x64 strip repeats eight times across the framebuffer, keeping
// the asset under 1 KiB while letting the native sprite service do all the pixel work.
static constexpr auto FADE_SPRITE PROGMEM = [] {
    struct Sprite {
        uint8_t width, height;
        uint8_t pixels[FADE_STEPS - 1][16 * 8];
    } sprite{16, 64, {}};
    constexpr uint8_t BAYER[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
    for(uint8_t frame = 0; frame < FADE_STEPS - 1; ++frame) {
        for(uint8_t page = 0; page < 8; ++page) {
            for(uint8_t x = 0; x < 16; ++x) {
                uint8_t mask = 0;
                for(uint8_t bit = 0; bit < 8; ++bit) {
                    if(BAYER[bit % 4][x % 4] < (frame + 1) * 2)
                        mask |= (uint8_t)(1 << bit);
                }
                sprite.pixels[frame][page * 16 + x] = mask;
            }
        }
    }
    return sprite;
}();

// Apply black coverage without presenting, so an overlay can remain bright above the world.
static void draw_fade(uint8_t step) {
    if(step >= FADE_STEPS) {
        avm_draw_filled_rect_black(0, 0, DISPLAY_WIDTH, 64);
    } else if(step) {
        avm_set_sprite(&FADE_SPRITE);
        for(uint8_t x = 0; x < DISPLAY_WIDTH; x += 16)
            avm_draw_erase(x, 0, step - 1);
    }
}

// Preserve the framebuffer; fade-out accumulates masks without redrawing the outgoing scene.
void show_fade(uint8_t step) {
    draw_fade(step);
    input_wait_frame();
    avm_display(false);
}

void fade_to_black() {
    for(uint8_t step = 1; step <= FADE_STEPS; ++step)
        show_fade(step);
}

static void panel() {
    avm_draw_filled_rect_black(2, 2, 124, 60);

    avm_draw_filled_rect_white(2, 2, 124, 1);
    avm_draw_filled_rect_white(2, 61, 124, 1);
}

void rpg_render_world() {
    int16_t left = -(int16_t)(game.camera.x & (TILE - 1));
    int16_t top = -(int16_t)(game.camera.y & (TILE - 1));
    // The cache trails the view by its margin, so the view starts inside the window.
    uint8_t view_x = (uint8_t)((game.camera.x >> TILE_SHIFT) - game.cache.origin_tx);
    uint8_t view_y = (uint8_t)((game.camera.y >> TILE_SHIFT) - game.cache.origin_ty);
    uint8_t const* tiles = game.cache.tiles + view_y * CACHE_WIDTH + view_x;
    avm_set_sprite(tileset_sprites(area_tileset(game.area)));
    for(int16_t y = top; y < DISPLAY_TILE_HEIGHT * TILE; y += TILE) {
        // A fixed row span preserves unrolling. Native drawing clips the
        // extra column when the camera is tile-aligned.
        uint8_t const* row_end = tiles + VIEW_SPAN_WIDTH;
        for(int16_t x = left; tiles != row_end; x += TILE) {
            uint8_t tile = *tiles++;
            if(tile == TILE_OUTSIDE)
                continue;
            avm_draw_overwrite(x, y, tile);
        }
        tiles += CACHE_WIDTH - VIEW_SPAN_WIDTH;
    }

    Camera c = game.camera;
    avm_set_sprite(ACTOR_SPRITES);
    uint8_t order[NPC_CAPACITY + 1];
    uint8_t count = actor_draw_order(order);
    for(uint8_t i = 0; i < count; ++i) {
        uint8_t slot = order[i];
        Actor const& actor = actor_in_draw_slot(slot);
        uint8_t row =
            slot == PLAYER_DRAW_SLOT ? 0 : npc_definition(game.npcs.actors[slot].id)->sprite_row;
        avm_draw_plus_mask((int16_t)actor.x - (int16_t)c.x, (int16_t)actor.y - (int16_t)c.y,
                           actor_frame(actor, row, game.ticks));
    }

    if(game.can_interact)
        avm_draw_text(110, 7, F("A"));
}

// Rebuild the outgoing image: exploration and dialogue normally clear the framebuffer after
// presenting it. This also removes any overlay before the world is faded away.
void fade_world_out() {
    avm_draw_filled_rect_black(0, 0, DISPLAY_WIDTH, 64);
    rpg_render_world();
    fade_to_black();
}

// The caller finishes destination cache/tile/NPC updates while the OLED stays black. Re-rendering
// this frozen scene under the masks avoids a second framebuffer or any extra static RAM.
void fade_world_in() {
    for(uint8_t step = FADE_STEPS; step--;) {
        rpg_render_world();
        show_fade(step);
    }
    input_sync();
}

void render_title(bool can_continue, uint8_t selection) {
    avm_draw_text(14, 12, F("AVM / WAYFARER"));
    avm_draw_text(14, 26, F("Nocturne Grove"));

    avm_draw_text(22, 40, F("New game"));
    avm_draw_text(22, 49, can_continue ? F("Continue") : F("No valid save"));
    avm_draw_text(10, (int16_t)(40 + selection * 9), F(">"));

    avm_draw_text(14, 60, F("D-pad choose / A"));
}

static char const PROGMEM* menu_label(PAUSE_ITEM item) {
    static char const PROGMEM* const PROGMEM LABELS[] = {F("Resume"),  F("Save"), F("Party"),
                                                         F("Journal"), F("Map"),  F("Resign"),
                                                         F("Audio"),   F("Pass"), F("Load")};
    return LABELS[item];
}

uint8_t menu_width(bool in_battle) {
    uint16_t width = avm_text_width(F("Paused")) + 8;
    for(uint8_t item = 0; item < PAUSE_ITEM_COUNT; ++item) {
        if(!menu_item_visible((PAUSE_ITEM)item, save_exists, in_battle))
            continue;
        uint16_t item_width = avm_text_width(menu_label((PAUSE_ITEM)item)) + 14;
        if(item_width > width)
            width = item_width;
    }
    return (uint8_t)width;
}

void render_menu(uint8_t revealed, uint8_t width, PAUSE_ITEM selection, bool can_load,
                 char const PROGMEM* feedback, MENU_EXIT action, BattleScreen const* battle) {
    if(battle)
        render_battle(*battle);
    else
        rpg_render_world();
    uint8_t fade = (uint8_t)((uint16_t)revealed * (FADE_STEPS / 2) / width);
    draw_fade(action == MENU_AUDIO  ? FADE_STEPS / 2
              : action == MENU_LOAD ? FADE_STEPS - fade
                                    : fade);
    if(!revealed)
        return;

    int16_t x = (int16_t)revealed - width;
    avm_draw_filled_rect_black(x, 0, width, 64);
    avm_draw_filled_rect_white(x + 1, 1, width - 2, 62);
    avm_draw_filled_rect_black(x + 2, 2, width - 4, 60);
    avm_draw_text(x + 4, 8, F("Paused"));

    uint8_t y = 17;
    for(uint8_t item = 0; item < PAUSE_ITEM_COUNT; ++item) {
        if(!menu_item_visible((PAUSE_ITEM)item, can_load, battle != nullptr))
            continue;
        avm_draw_text(x + 10, y, menu_label((PAUSE_ITEM)item));
        if(item == selection)
            avm_draw_text(x + 4, y, F(">"));
        y += 7;
    }
    if(feedback && revealed == width)
        avm_draw_text(width + 4, 58, feedback);
}

void render_menu_confirmation(PAUSE_ITEM selection) {
    char const PROGMEM* prompt = F("A Accept  B Cancel");
    char const PROGMEM* title = selection == PAUSE_PASS ? F("Pass turn?") : F("Resign?");
    uint8_t width = (uint8_t)avm_text_width(prompt) + 12;
    int16_t x = (DISPLAY_WIDTH - width) / 2;
    constexpr uint8_t Y = 18, HEIGHT = 28;
    avm_draw_filled_rect_black(x, Y, width, HEIGHT);
    avm_draw_filled_rect_white(x + 1, Y + 1, width - 2, HEIGHT - 2);
    avm_draw_filled_rect_black(x + 2, Y + 2, width - 4, HEIGHT - 4);
    avm_draw_text((DISPLAY_WIDTH - avm_text_width(title)) / 2, Y + 10, title);
    avm_draw_text(x + 6, Y + 21, prompt);
}

void render_audio(uint8_t revealed, OPTION_ITEM selection, bool closing,
                  BattleScreen const* battle) {
    if(battle)
        render_battle(*battle);
    else
        rpg_render_world();
    draw_fade(closing ? (uint8_t)((uint16_t)revealed * (FADE_STEPS / 2) / AUDIO_WIDTH)
                      : FADE_STEPS / 2);
    if(!revealed)
        return;

    int16_t x = DISPLAY_WIDTH - revealed;
    avm_draw_filled_rect_black(x, 0, AUDIO_WIDTH, 64);
    avm_draw_filled_rect_white(x + 1, 1, AUDIO_WIDTH - 2, 62);
    avm_draw_filled_rect_black(x + 2, 2, AUDIO_WIDTH - 4, 60);
    avm_draw_text(x + 4, 8, F("Audio"));
    avm_draw_text(x + 51, 8, F("B resume"));

    static char const PROGMEM* const PROGMEM LABELS[] = {F("Music"), F("SFX"), F("Low-pass")};
    for(uint8_t item = 0; item < OPTION_ITEM_COUNT; ++item) {
        uint8_t y = 18 + item * 16;
        uint8_t level = option_level((OPTION_ITEM)item);
        avm_draw_text(x + 10, y, LABELS[item]);
        if(item == selection)
            avm_draw_text(x + 4, y, F(">"));
        if(!level)
            avm_draw_text(x + 63, y, F("Off"));
        else
            avm_draw_textf(x + 63, y, F("%u%%"),
                           (level * 100 + OPTION_SLIDER_MAX / 2) / OPTION_SLIDER_MAX);

        constexpr uint8_t TRACK_WIDTH = AUDIO_WIDTH - 20;
        avm_draw_filled_rect_white(x + 10, y + 5, TRACK_WIDTH, 1);
        avm_draw_filled_rect_white(x + 10, y + 3, 1, 5);
        avm_draw_filled_rect_white(x + 10 + TRACK_WIDTH - 1, y + 3, 1, 5);
        int16_t knob = x + 10 + (uint16_t)level * (TRACK_WIDTH - 3) / option_max((OPTION_ITEM)item);
        avm_draw_filled_rect_black(knob - 1, y + 1, 5, 9);
        avm_draw_filled_rect_white(knob, y + 2, 3, 7);
    }
}

void render_status() {
    rpg_render_world();
    panel();
    avm_draw_text(6, 11, F("TRAVELER"));

    avm_draw_textf(6, 25, F("HP %u/%u"), game.hp, game.max_hp);
    avm_draw_textf(6, 35, F("Lv %u Gold %u"), game.level, game.gold);
    avm_draw_text(6, 45, F("Nocturne Grove"));

    avm_draw_text(6, 58, F("A / B return"));
}
