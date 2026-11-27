#ifndef SOUNDSCAPE_H
#define SOUNDSCAPE_H

#include "ambient.h"
#include "sfx.h"

// The sounds of the environment: birds outdoors, drips in a cave, a creaking floor in a house. Like
// the animation in ambient.h, every rule is a pure function of the game tick, the area and a tile,
// so the soundscape keeps no state: no static RAM, nothing in the save, and a frozen tick is a
// silent one. The tick advances once per simulated frame, so each moment is judged exactly once.
//
// Sounds come from the places the player sees. Once per epoch the soundscape may pick a few tiles
// around the player, and the ambient class of the tile that it finds decides what is heard, with
// the same classes that decide which creature or effect appears there. A bird sings from the trees
// and a fish splashes in the water.

// An epoch lasts this many ticks, which is 5.12 seconds at 50 Hz. It holds at most one sound.
inline constexpr uint16_t SOUND_EPOCH_TICKS = 256;
static_assert(SOUND_EPOCH_TICKS == 256, "sound_moment compares the low byte of the tick");

// The soundscape only judges every eighth tick, which is the gusts' step and 160 ms: ample for a
// sound that comes on occasion. The guest pays dozens of cycles for each instruction, so the other
// seven frames in eight pay for one test, and the platform makes that test itself.
inline constexpr uint8_t SOUND_GRAIN_TICKS = 8;
static_assert(SOUND_GRAIN_TICKS == GUST_STEP_TICKS, "a gust is heard on a tick that is judged");

constexpr bool sound_listening(uint16_t ticks) {
    return !(ticks & (SOUND_GRAIN_TICKS - 1));
}

// The chance, out of 256, that an epoch holds a sound at all. The tile it finds then decides
// whether that sound is heard, so the sounds come on occasion, about every fifteen seconds.
inline constexpr uint8_t SOUND_CHANCE = 176;

// How far the player hears from, in tiles: the screen and a little more. A sound looks at up to
// SOUND_PROBES tiles inside this reach and takes the first that has a voice.
inline constexpr uint8_t HEARING_REACH_X = 5, HEARING_REACH_Y = 3, SOUND_PROBES = 3;

// A gust's first line is heard as it passes the player, once every 1024 ticks.
inline constexpr uint16_t WIND_SOUND_BAND = 0;

// A group's members, named once each, are exactly the span from its first effect to its last in the
// generated catalog. A new score that sorts into a group without being listed here fails the build,
// and so does a group that the catalog no longer keeps together.
template <unsigned N> constexpr bool group_is(SFX const (&members)[N], SFX first, SFX last) {
    if(first + N - 1 != last)
        return false;
    for(unsigned i = 0; i < N; ++i)
        if(members[i] != first + i)
            return false;
    return true;
}

static_assert(group_is({SFX_BIRD_CHIRP, SFX_BIRD_COO, SFX_BIRD_TRILL, SFX_BIRD_TWEET},
                       SFX_BIRDS_FIRST, SFX_BIRDS_LAST) &&
                  group_is({SFX_DRIP_A, SFX_DRIP_B}, SFX_DRIPS_FIRST, SFX_DRIPS_LAST) &&
                  group_is({SFX_WATER_PLIP, SFX_WATER_SPLASH}, SFX_WATERS_FIRST, SFX_WATERS_LAST) &&
                  group_is({SFX_MENU_TICK_A, SFX_MENU_TICK_B, SFX_MENU_TICK_C, SFX_MENU_TICK_D,
                            SFX_MENU_TICK_E},
                           SFX_MENU_TICKS_FIRST, SFX_MENU_TICKS_LAST),
              "every sound group is contiguous: update sfx.h and this list with assets/sfx");

// The tick of each epoch at which its sound is due, a multiple of the grain. Eight-bit steps only,
// each of them invertible, so every grain comes round exactly once in 32 epochs' worth of offsets
// and consecutive epochs are not a fixed distance apart.
constexpr uint8_t sound_offset(uint8_t epoch) {
    uint8_t x = (uint8_t)(epoch * 0xB5u + 0x3Bu);
    x ^= (uint8_t)(x >> 3);
    x = (uint8_t)(x * 0x6Du);
    x ^= (uint8_t)(x >> 5);
    return (uint8_t)(x & ~(SOUND_GRAIN_TICKS - 1));
}

constexpr bool sound_moment(uint16_t ticks) {
    return (uint8_t)ticks == sound_offset((uint8_t)(ticks >> 8));
}

// The epoch's pick: the roll that decides whether the epoch holds a sound in bits 0 to 7, and bits
// to place it with in bits 8 to 15. It runs once per epoch, on the tick of the sound's moment.
constexpr uint16_t sound_pick(uint16_t ticks) {
    uint16_t epoch = (uint16_t)(ticks / SOUND_EPOCH_TICKS);
    return ambient_hash(ambient_hash((uint16_t)(epoch * 0x9E37u + 0x2F1Bu)));
}

constexpr bool sound_rolls(uint16_t pick) {
    return (uint8_t)pick < SOUND_CHANCE;
}

// Whether a sound is due on this tick: the epoch's moment, and a roll that comes up.
constexpr bool sound_due(uint16_t ticks) {
    return sound_moment(ticks) && sound_rolls(sound_pick(ticks));
}

// Where a probe looks: a tile offset from the player's tile, within the hearing reach.
struct SoundProbe {
    int8_t dx, dy;
    uint8_t roll; // Uniform chance for this probe.
    uint8_t vary; // More bits, for choosing between a voice's variants.
};

constexpr SoundProbe sound_probe(uint16_t pick, uint8_t probe) {
    uint16_t place = ambient_hash((uint16_t)(pick + probe * 0x4F6Du + 1));
    uint16_t taste = ambient_hash((uint16_t)(place ^ 0x5A5Au));
    return {(int8_t)((uint8_t)place % (2 * HEARING_REACH_X + 1) - HEARING_REACH_X),
            (int8_t)((uint8_t)(place >> 8) % (2 * HEARING_REACH_Y + 1) - HEARING_REACH_Y),
            (uint8_t)taste, (uint8_t)(taste >> 8)};
}

// Chances out of 256 that a tile of each class sounds when a probe finds it. Cumulative within a
// class, as in ambient.h: a roll below the first value makes the first sound, and below the second
// value the second.
inline constexpr uint8_t TREE_BIRD_SONG = 200, PERCH_BIRD_SONG = 150, GRASS_BIRD_SONG = 90;
inline constexpr uint8_t FLOWERS_BEE_BUZZ = 150, WATER_STIRRING = 150, FIRE_CRACKLING = 150;
inline constexpr uint8_t FLOOR_BOARD_CREAK = 110;
inline constexpr uint8_t CAVE_BAT_CALL = 50, CAVE_DRIP_SOUND = 140, CAVE_ROCK_RUMBLE = 156;
inline constexpr uint8_t DRIP_DRIP_SOUND = 190, POOL_DRIP_SOUND = 130, CRYSTAL_CHIME_SOUND = 140;
inline constexpr uint8_t CRYPT_WISP_MOAN = 80, CRYPT_CHAIN_RATTLE = 140, CRYPT_ROCK_RUMBLE = 156;

// The effect when the roll falls below the chance, and silence otherwise.
constexpr SFX heard(uint8_t roll, uint8_t chance, SFX effect) {
    return roll < chance ? effect : SFX_NONE;
}

constexpr SFX bird_song(uint8_t vary) {
    return sfx_variant(SFX_BIRDS_FIRST, SFX_BIRDS_LAST, vary);
}

constexpr SFX water_sound(uint8_t vary) {
    return sfx_variant(SFX_WATERS_FIRST, SFX_WATERS_LAST, vary);
}

constexpr SFX drip_sound(uint8_t vary) {
    return sfx_variant(SFX_DRIPS_FIRST, SFX_DRIPS_LAST, vary);
}

// The classes that the cave and the dungeon share: water dripping into pools, crystals and flames.
constexpr SFX underground_sound(uint8_t ambient_class, uint8_t roll, uint8_t vary) {
    switch(ambient_class) {
        case AMBIENT_DRIP:
            return heard(roll, DRIP_DRIP_SOUND, drip_sound(vary));
        case AMBIENT_POOL:
            return heard(roll, POOL_DRIP_SOUND, drip_sound(vary));
        case AMBIENT_CRYSTAL:
            return heard(roll, CRYSTAL_CHIME_SOUND, SFX_CRYSTAL_CHIME);
        case AMBIENT_EMBER:
            return heard(roll, FIRE_CRACKLING, SFX_FIRE_CRACKLE);
        default:
            return SFX_NONE;
    }
}

// What a tile of the given ambient class sounds like in a tileset, or SFX_NONE for silence. Only
// the classes that belong to the tileset are listed: a class has one meaning per tileset, and
// `steam` is a chimney or a campfire outdoors, a stove or a cauldron in a house.
constexpr SFX tile_sound(uint8_t tileset, uint8_t ambient_class, uint8_t roll, uint8_t vary) {
    switch(tileset) {
        case TILESET_WORLD:
            switch(ambient_class) {
                case AMBIENT_TREE:
                    return heard(roll, TREE_BIRD_SONG, bird_song(vary));
                case AMBIENT_PERCH:
                    return heard(roll, PERCH_BIRD_SONG, bird_song(vary));
                case AMBIENT_GRASS:
                    return heard(roll, GRASS_BIRD_SONG, bird_song(vary));
                case AMBIENT_FLOWERS:
                    return heard(roll, FLOWERS_BEE_BUZZ, SFX_BEE_BUZZ);
                case AMBIENT_WATER:
                    return heard(roll, WATER_STIRRING, water_sound(vary));
                case AMBIENT_STEAM:
                    return heard(roll, FIRE_CRACKLING, SFX_FIRE_CRACKLE);
                default:
                    return SFX_NONE;
            }
        case TILESET_HOUSE:
            switch(ambient_class) {
                case AMBIENT_FLOOR:
                    return heard(roll, FLOOR_BOARD_CREAK, SFX_CREAK);
                case AMBIENT_STEAM:
                    return heard(roll, FIRE_CRACKLING, SFX_FIRE_CRACKLE);
                default:
                    return SFX_NONE;
            }
        case TILESET_CAVE:
            if(ambient_class == AMBIENT_CAVE) {
                // A bat, a drop or the rock itself.
                return roll < CAVE_BAT_CALL     ? SFX_BAT_SQUEAK
                       : roll < CAVE_DRIP_SOUND ? drip_sound(vary)
                                                : heard(roll, CAVE_ROCK_RUMBLE, SFX_RUMBLE);
            }
            return underground_sound(ambient_class, roll, vary);
        case TILESET_DUNGEON:
            if(ambient_class == AMBIENT_CRYPT) {
                // A wisp, a chain or the stone itself.
                return roll < CRYPT_WISP_MOAN      ? SFX_WISP_MOAN
                       : roll < CRYPT_CHAIN_RATTLE ? SFX_CHAIN_RATTLE
                                                   : heard(roll, CRYPT_ROCK_RUMBLE, SFX_RUMBLE);
            }
            return underground_sound(ambient_class, roll, vary);
        default:
            return SFX_NONE;
    }
}

// True on the tick when a gust's line reaches the tile that the player stands on, so that the wind
// is heard as the grass bends around the player. Only a tileset with wind frames has gusts.
constexpr bool wind_passes(uint16_t ticks, uint8_t tile_x, uint8_t tile_y) {
    if(ticks & (GUST_STEP_TICKS - 1))
        return false;
    uint8_t distance = gust_distance(gust_front(ticks), tile_x, tile_y);
    return (uint8_t)(distance - GUST_BANDS[WIND_SOUND_BAND].behind) <
           GUST_BANDS[WIND_SOUND_BAND].width;
}

// The sound to start now, from the current game: SFX_NONE on almost every tick. The platform plays
// it with sfx_ambient after each simulated frame.
SFX ambient_sound_now();

#endif
