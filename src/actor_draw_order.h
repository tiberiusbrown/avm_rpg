#ifndef ACTOR_DRAW_ORDER_H
#define ACTOR_DRAW_ORDER_H

#include "npc.h"

inline constexpr uint8_t PLAYER_DRAW_SLOT = NPC_CAPACITY;

inline Actor const& actor_in_draw_slot(uint8_t slot) {
    return slot == PLAYER_DRAW_SLOT ? game.player : game.npcs.actors[slot].actor;
}

inline uint8_t actor_draw_order(uint8_t (&order)[NPC_CAPACITY + 1]) {
    uint8_t count = 0;
    for(uint8_t slot = 0; slot < NPC_CAPACITY; ++slot) {
        Npc const& npc = game.npcs.actors[slot];
        Actor const& actor = npc.actor;
        if(!npc.id)
            break;
        if(!npc_visible(actor.x, actor.y))
            continue;

        // All sprites have the same height, so origin Y also orders their feet.
        // Stable insertion sort: at most 36 comparisons/shifts, nine stack bytes,
        // and no changes to NPC simulation order. Only NPC slots are present
        // here, so comparisons do not need to branch on the player slot.
        uint8_t index = count;
        while(index && game.npcs.actors[order[index - 1]].actor.y > actor.y) {
            order[index] = order[index - 1];
            --index;
        }
        order[index] = slot;
        ++count;
    }

    // Insert the player last, preserving its foreground priority at equal Y.
    uint8_t index = count;
    while(index && game.npcs.actors[order[index - 1]].actor.y > game.player.y) {
        order[index] = order[index - 1];
        --index;
    }
    order[index] = PLAYER_DRAW_SLOT;
    ++count;
    return count;
}

#endif
