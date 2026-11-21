#include "rpg.h"

uint16_t rng_next() {
    uint16_t x = game.rng;

    x ^= (uint16_t)(x << 7);
    x ^= (uint16_t)(x >> 9);
    x ^= (uint16_t)(x << 8);

    game.rng = x;
    return x;
}
