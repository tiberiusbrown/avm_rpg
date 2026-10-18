#ifndef ACTOR_DRAW_ORDER_H
#define ACTOR_DRAW_ORDER_H

#include "npc.h"

inline constexpr uint8_t PLAYER_DRAW_SLOT = NPC_CAPACITY;

inline Actor const& actor_in_draw_slot(uint8_t slot) {
    return slot == PLAYER_DRAW_SLOT ? game.player : game.npcs.actors[slot].actor;
}

inline uint8_t actor_draw_order(uint8_t (&order)[NPC_CAPACITY + 1]) {
    uint8_t count = 0;
    for(uint8_t slot = 0; slot <= PLAYER_DRAW_SLOT; ++slot) {
        Actor const& actor = actor_in_draw_slot(slot);
        if(slot != PLAYER_DRAW_SLOT &&
           (!game.npcs.actors[slot].id || !npc_visible(actor.x, actor.y)))
            continue;

        // All sprites have the same height, so origin Y also orders their feet.
        // Stable insertion sort: at most 36 comparisons/shifts, nine stack bytes,
        // and no changes to NPC simulation order. The player wins equal-Y ties.
        uint8_t index = count;
        while(index && actor_in_draw_slot(order[index - 1]).y > actor.y) {
            order[index] = order[index - 1];
            --index;
        }
        order[index] = slot;
        ++count;
    }
    return count;
}

#endif
