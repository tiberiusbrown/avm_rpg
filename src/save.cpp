#include "rpg.h"

#include <string.h>

static void put16(uint8_t* p, uint16_t n) {
    p[0] = (uint8_t)n;
    p[1] = (uint8_t)(n >> 8);
}

static uint16_t get16(uint8_t const* p) {
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

uint16_t save_checksum(uint8_t const* in) {
    uint16_t crc = 0xffff;
    uint8_t i, b;

    for(i = 0; i < 22; ++i) {
        crc ^= (uint16_t)in[i] << 8;
        for(b = 0; b < 8; ++b)
            crc = (uint16_t)((crc << 1) ^ ((crc & 0x8000) ? 0x1021 : 0));
    }

    return crc;
}

void save_encode(Game const& g, uint8_t out[SAVE_BYTES]) {
    memset(out, 0, SAVE_BYTES);

    out[0] = 'R';
    out[1] = 'P';
    out[2] = SAVE_VERSION;
    out[3] = SAVE_BYTES;

    out[4] = g.area;
    out[5] = g.player.facing;
    put16(out + 6, g.player.x);
    put16(out + 8, g.player.y);

    out[10] = g.hp;
    out[11] = g.max_hp;
    out[12] = g.level;

    put16(out + 14, g.gold);
    put16(out + 16, g.flags);
    put16(out + 18, g.rng);

    put16(out + 22, save_checksum(out));
}

bool save_decode(Game& g, uint8_t const in[SAVE_BYTES]) {
    uint16_t x = get16(in + 6), y = get16(in + 8);

    if(in[0] != 'R' || in[1] != 'P' || in[2] != SAVE_VERSION || in[3] != SAVE_BYTES ||
       get16(in + 22) != save_checksum(in) || in[4] < OUTDOOR || in[4] > INTERIOR || in[5] > WEST ||
       !in[11] || in[10] > in[11] || !in[12] || in[13] || in[20] || in[21] ||
       (get16(in + 16) & (uint16_t)~FLAG_HELPED) || !get16(in + 18) ||
       !world_save_position_valid(in[4], x, y))
        return false;

    /* Nothing mutates until every field is valid. No struct bytes are saved. */
    game_new(g);
    g.hp = in[10];
    g.max_hp = in[11];
    g.level = in[12];
    g.gold = get16(in + 14);
    g.flags = get16(in + 16);
    g.rng = get16(in + 18);

    world_enter(g, in[4], x, y, in[5]);
    return true;
}
