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

// Present a frozen scene at a given black coverage, then keep it visible for one frame. This
// preserves the framebuffer; fade-out accumulates its masks without redrawing the outgoing scene.
void show_fade(uint8_t step) {
    if(step == FADE_STEPS) {
        avm_draw_filled_rect_black(0, 0, DISPLAY_WIDTH, 64);
    } else if(step) {
        avm_set_sprite(&FADE_SPRITE);
        for(uint8_t x = 0; x < DISPLAY_WIDTH; x += 16)
            avm_draw_erase(x, 0, step - 1);
    }
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

void render_menu(uint8_t selection, char const PROGMEM* feedback, uint8_t audio_rate) {
    rpg_render_world();
    panel();
    avm_draw_text(6, 10, F("TRAVEL MENU"));

    avm_draw_text(22, 18, F("Status"));
    avm_draw_text(22, 26, F("Save game"));
    avm_draw_text(22, 34, F("Return"));
    avm_draw_text(22, 42, F("Switch music"));
    avm_draw_text(22, 50,
                  audio_rate == AVM_AUDIO_RATE_16KHZ ? F("Audio: 16 kHz") : F("Audio: 8 kHz"));
    avm_draw_text(8, (int16_t)(18 + selection * 8), F(">"));

    avm_draw_text(6, 58, feedback ? feedback : F("A select / B back"));
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
