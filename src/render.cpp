#include "rpg.h"
#include "tile_ids.h"
#include "npc.h"
#include "actor_draw_order.h"

#include <avm.h>

static void panel() {
    avm_draw_filled_rect_black(2, 2, 124, 60);

    avm_draw_filled_rect_white(2, 2, 124, 1);
    avm_draw_filled_rect_white(2, 61, 124, 1);
}

void rpg_render_world() {
    int16_t left = -(int16_t)(game.camera.x & (TILE - 1));
    int16_t top = -(int16_t)(game.camera.y & (TILE - 1));
    uint8_t const* tiles = game.cache.tiles;
    avm_set_sprite(TILE_SPRITES);
    for(int16_t y = top; y < (CACHE_HEIGHT - 1) * TILE; y += TILE) {
        // A fixed row span preserves unrolling. Native drawing clips the
        // extra column when the camera is tile-aligned.
        uint8_t const* row_end = tiles + CACHE_WIDTH;
        for(int16_t x = left; tiles != row_end; x += TILE) {
            uint8_t tile = *tiles++;
            if(tile == TILE_OUTSIDE)
                continue;
            avm_draw_overwrite(x, y, tile);
        }
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
