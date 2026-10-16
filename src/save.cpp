#include "rpg.h"

#include <string.h>

static void put16(uint8_t* p, uint16_t n) {
    p[0] = (uint8_t)n;
    p[1] = (uint8_t)(n >> 8);
}

static uint16_t get16(uint8_t const* p) {
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

uint16_t save_checksum(uint8_t const* in, uint16_t length) {
    uint16_t crc = 0xffff;
    uint16_t i;
    uint8_t b;

    for(i = 0; i < length; ++i) {
        crc ^= (uint16_t)in[i] << 8;
        for(b = 0; b < 8; ++b)
            crc = (uint16_t)((crc << 1) ^ ((crc & 0x8000) ? 0x1021 : 0));
    }

    return crc;
}

void save_encode(uint8_t out[SAVE_BYTES]) {
    memset(out, 0, SAVE_BYTES);

    out[0] = 'R';
    out[1] = 'P';
    out[2] = SAVE_VERSION;
    out[3] = SAVE_BYTES;

    out[4] = game.area;
    out[5] = game.player.facing;
    put16(out + 6, game.player.x);
    put16(out + 8, game.player.y);

    out[10] = game.hp;
    out[11] = game.max_hp;
    out[12] = game.level;

    put16(out + 14, game.gold);
    put16(out + 16, game.rng);
    put16(out + 18, FLAG_COUNT);
    memcpy(out + SAVE_HEADER_BYTES, game.flags.bytes, WorldFlags::BYTE_COUNT);

    put16(out + SAVE_CRC_OFFSET, save_checksum(out));
}

bool save_decode(uint8_t const* in, uint16_t length) {
    if(length < 4 || in[0] != 'R' || in[1] != 'P')
        return false;
    bool legacy = in[2] == 1 || in[2] == 2;
    uint16_t size = in[3];
    if((!legacy && in[2] != SAVE_VERSION) || size < 24 || size > length || (legacy && size != 24) ||
       get16(in + size - 2) != save_checksum(in, size - 2))
        return false;

    WorldFlags flags{};
    uint16_t rng;
    if(legacy) {
        uint16_t old = get16(in + 16);
        if((old & (uint16_t)~(in[2] == 1 ? 1 : 31)) || in[20] || in[21])
            return false;
        if(old & 1)
            flags.set_flag(FLAG_HELPED);
        if(old & 2)
            flags.set_flag(FLAG_GATE_OPEN);
        if(old & 4)
            flags.set_flag(FLAG_BRIDGE_REPAIRED);
        // Retired NPC bit 3 is intentionally discarded; waystone was bit 4.
        if(old & 16)
            flags.set_flag(FLAG_WAYSTONE);
        rng = get16(in + 18);
    } else {
        uint16_t count = get16(in + 18);
        uint16_t bytes = count / 8 + (count % 8 != 0);
        if(!count || count > FLAG_COUNT || SAVE_HEADER_BYTES + bytes > size - 2)
            return false;
        if(count % 8 && (in[SAVE_HEADER_BYTES + bytes - 1] >> (count % 8)))
            return false;
        for(uint16_t i = SAVE_HEADER_BYTES + bytes; i < size - 2; ++i)
            if(in[i])
                return false;
        memcpy(flags.bytes, in + SAVE_HEADER_BYTES, bytes);
        rng = get16(in + 16);
    }
    uint16_t x = get16(in + 6), y = get16(in + 8);
    Area const PROGMEM* area = world_area(in[4]);
    if(!area || in[5] > WEST || !in[11] || in[10] > in[11] || !in[12] || in[13] || !rng)
        return false;
    MapDimensions dimensions{area->width, area->height};
    if(!dimensions.pixel_bounds(x, y))
        return false;

    // Nothing mutates until every field is valid. No struct bytes are saved.
    memset(&game, 0, sizeof(game));
    game.hp = in[10];
    game.max_hp = in[11];
    game.level = in[12];
    game.gold = get16(in + 14);
    game.flags = flags;
    game.rng = rng;
    world_enter(in[4], x, y, in[5]);
    return true;
}
