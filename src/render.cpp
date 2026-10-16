#include "rpg.h"
#include "tile_ids.h"
#include "npc.h"

#include <avm.h>

static void panel() {
    avm_draw_filled_rect_black(2, 2, 124, 60);

    avm_draw_filled_rect_white(2, 2, 124, 1);
    avm_draw_filled_rect_white(2, 61, 124, 1);
}

void rpg_render_world() {
    Camera c = game.camera;
    uint8_t tx, ty;
    avm_set_sprite(TILE_SPRITES);
    for(ty = 0; ty < CACHE_HEIGHT; ++ty) {
        for(tx = 0; tx < CACHE_WIDTH; ++tx) {
            uint8_t tile = game.cache.tiles[(uint16_t)ty * CACHE_WIDTH + tx];
            if(tile == TILE_OUTSIDE || (game.area == AREA_INTERIOR && tile == TILE_GRASS) ||
               tile == TILE_PATH)
                continue;
            avm_draw_overwrite((int16_t)tx * 8 - (int16_t)(c.x & 7),
                               (int16_t)ty * 8 - (int16_t)(c.y & 7), tile);
        }
    }

    avm_set_sprite(ACTOR_SPRITES);
    for(auto const& n : game.npcs.actors) {
        if(!n.id || !npc_visible(n.actor.x, n.actor.y))
            continue;
        int16_t x = (int16_t)n.actor.x - (int16_t)c.x;
        int16_t y = (int16_t)n.actor.y - (int16_t)c.y;
        avm_draw_plus_mask(x, y,
                           n.actor.facing * 2 + (n.actor.walking ? ((game.ticks >> 2) & 1) : 0));
        avm_draw_filled_rect_white(x + 2, y, 4, 1);
    }
    avm_draw_plus_mask(
        (int16_t)game.player.x - (int16_t)c.x, (int16_t)game.player.y - (int16_t)c.y,
        (uint16_t)(game.player.facing * 2 + (game.player.walking ? ((game.ticks >> 2) & 1) : 0)));

    if(game.can_interact)
        avm_draw_text(110, 7, F("A"));
}

void render_title(bool can_continue, uint8_t selection) {
    avm_draw_text(14, 12, F("AVM / WAYFARER"));
    avm_draw_text(14, 26, F("M1.6 essential NPCs"));

    avm_draw_text(22, 40, F("New game"));
    avm_draw_text(22, 49, can_continue ? F("Continue") : F("No valid save"));
    avm_draw_text(10, (int16_t)(40 + selection * 9), F(">"));

    avm_draw_text(14, 60, F("D-pad choose / A"));
}

void render_menu(uint8_t selection, char const PROGMEM* feedback) {
    rpg_render_world();
    panel();
    avm_draw_text(6, 11, F("TRAVEL MENU"));

    avm_draw_text(22, 25, F("Status"));
    avm_draw_text(22, 35, F("Save game"));
    avm_draw_text(22, 45, F("Return"));
    avm_draw_text(8, (int16_t)(25 + selection * 10), F(">"));

    avm_draw_text(6, 58, feedback ? feedback : F("A select / B back"));
}

void render_status() {
    rpg_render_world();
    panel();
    avm_draw_text(6, 11, F("TRAVELER"));

    avm_draw_textf(6, 25, F("HP %u/%u"), game.hp, game.max_hp);
    avm_draw_textf(6, 35, F("Lv %u Gold %u"), game.level, game.gold);
    avm_draw_text(6, 45, game.get_flag(FLAG_HELPED) ? F("Lantern: lit") : F("Lantern: unlit"));

    avm_draw_text(6, 58, F("A / B return"));
}
