#include "soundscape.h"

// Out of line: the exploration loop calls it, and inlined it would grow that loop's frame, which
// every path through the game's deepest calls stands on.
RPG_NOINLINE SFX ambient_sound_now() {
    uint16_t ticks = game.ticks;
    // Only the grain ticks are judged, and the platform does not even call on the others.
    if(!sound_listening(ticks))
        return SFX_NONE;
    bool moment = sound_moment(ticks);

    uint8_t player_x = (uint8_t)((game.player.x + FOOT_CENTER_X) >> TILE_SHIFT);
    uint8_t player_y = (uint8_t)((game.player.y + FOOT_CENTER_Y) >> TILE_SHIFT);
    // Only a tileset with wind frames has gusts.
    if(tile_wind_frames(area_tileset(game.area)).count && wind_passes(ticks, player_x, player_y))
        return SFX_WIND;
    if(!moment)
        return SFX_NONE;

    uint16_t pick = sound_pick(ticks);
    if(!sound_rolls(pick))
        return SFX_NONE;

    for(uint8_t index = 0; index < SOUND_PROBES; ++index) {
        SoundProbe probe = sound_probe(pick, index);
        int16_t x = (int16_t)(player_x + probe.dx), y = (int16_t)(player_y + probe.dy);
        if(x < 0 || y < 0 || x >= game.dimensions.width_tiles || y >= game.dimensions.height_tiles)
            continue;
        uint8_t tile = cached_tile({game.area, (uint8_t)x, (uint8_t)y});
        SFX sound = tile_sound(game.area, tile_ambient(tile), probe.roll, probe.vary);
        if(sound != SFX_NONE)
            return sound;
    }
    return SFX_NONE;
}
