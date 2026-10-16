#include "rpg.h"

#include <avm.h>

static void text(int16_t x, int16_t y, char const PROGMEM* s) {
    avm_draw_text_P(x, y, s);
}

static void panel() {
    avm_draw_filled_rect_black(2, 2, 124, 60);

    avm_draw_filled_rect_white(2, 2, 124, 1);
    avm_draw_filled_rect_white(2, 61, 124, 1);
}

void rpg_render_world(Game const& g) {
    Camera c = game_camera(g);
    Area const PROGMEM* a = world_area(g.area);
    uint8_t tx, ty, i;
    uint8_t x0 = (uint8_t)(c.x >> 3), y0 = (uint8_t)(c.y >> 3);

    avm_set_sprite(TILE_SPRITES);
    for(ty = y0; ty < a->height && ty < y0 + 9; ++ty) {
        for(tx = x0; tx < a->width && tx < x0 + 17; ++tx) {
            uint8_t tile = a->tiles[(uint16_t)ty * a->width + tx];
            if(g.area == INTERIOR && tile == 0)
                continue;
            if(tile == 4)
                continue; /* Blank paths require no drawing on a cleared buffer. */
            avm_draw_overwrite((int16_t)tx * 8 - (int16_t)c.x, (int16_t)ty * 8 - (int16_t)c.y,
                               tile);
        }
    }

    avm_set_sprite(ACTOR_SPRITES);
    for(i = 0; i < g.npc_count; ++i) {
        Actor const* n = &g.npcs[i].actor;
        avm_draw_plus_mask((int16_t)n->x - (int16_t)c.x, (int16_t)n->y - (int16_t)c.y,
                           (uint16_t)(n->facing * 2 + (n->walking ? ((g.ticks >> 2) & 1) : 0)));
        avm_draw_filled_rect_white((int16_t)n->x - (int16_t)c.x + 2,
                                   (int16_t)n->y - (int16_t)c.y - 2, 4, 1);
    }
    avm_draw_plus_mask(
        (int16_t)g.player.x - (int16_t)c.x, (int16_t)g.player.y - (int16_t)c.y,
        (uint16_t)(g.player.facing * 2 + (g.player.walking ? ((g.ticks >> 2) & 1) : 0)));

    if(interaction_target(g))
        text(110, 7, F("A"));
}

void render_title(bool can_continue, uint8_t selection) {
    text(14, 12, F("AVM / WAYFARER"));
    text(14, 26, F("M1 exploration lab"));

    text(22, 40, F("New game"));
    text(22, 49, can_continue ? F("Continue") : F("No valid save"));
    text(10, (int16_t)(40 + selection * 9), F(">"));

    text(14, 60, F("D-pad choose / A"));
}

void render_menu(Game const& g, uint8_t selection, char const PROGMEM* feedback) {
    rpg_render_world(g);
    panel();
    text(6, 11, F("TRAVEL MENU"));

    text(22, 25, F("Status"));
    text(22, 35, F("Save game"));
    text(22, 45, F("Return"));
    text(8, (int16_t)(25 + selection * 10), F(">"));

    text(6, 58, feedback ? feedback : F("A select / B back"));
}

void render_status(Game const& g) {
    rpg_render_world(g);
    panel();
    text(6, 11, F("TRAVELER"));

    avm_draw_textf_P(6, 25, F("HP %u/%u"), g.hp, g.max_hp);
    avm_draw_textf_P(6, 35, F("Lv %u Gold %u"), g.level, g.gold);
    text(6, 45, g.flags & FLAG_HELPED ? F("Lantern: lit") : F("Lantern: unlit"));

    text(6, 58, F("A / B return"));
}
