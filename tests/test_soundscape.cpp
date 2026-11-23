#include "soundscape.h"
#include "chunk.h"
#include "map_positions.h"
#include "world_assets.h"

#include <initializer_list>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(value)                                                                               \
    do {                                                                                           \
        ++checks;                                                                                  \
        if(!(value)) {                                                                             \
            fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #value);                        \
            exit(1);                                                                               \
        }                                                                                          \
    } while(0)

bool dialogue_show(char const PROGMEM*) {
    return true;
}

bool dialogue_show(NPC_ID id, char const PROGMEM* words) {
    (void)id;
    return dialogue_show(words);
}

bool dialogue_ask(char const PROGMEM*) {
    return true;
}

bool dialogue_ask(NPC_ID id, char const PROGMEM* question) {
    (void)id;
    return dialogue_ask(question);
}

// The soundscape is a constant expression of the tick, so the guest can fold it and it keeps no
// state. The epoch divides the tick's wrap, so the pattern has no seam when the tick overflows.
static_assert(65536 % SOUND_EPOCH_TICKS == 0 && 65536 % (GUST_STEP_TICKS * GUST_SPACING) == 0);
static_assert(sound_pick(1234) == sound_pick(1234) && sound_pick(0) == sound_pick(255) &&
              sound_pick(0) != sound_pick(256));

// Every moment is a grain tick, so the platform's one cheap test of the tick catches all of them.
constexpr bool moments_are_grain_ticks() {
    for(unsigned epoch = 0; epoch < 256; ++epoch)
        if(!sound_listening(sound_offset((uint8_t)epoch)))
            return false;
    return true;
}

static_assert(moments_are_grain_ticks());

constexpr uint64_t bit(SFX effect) {
    constexpr SFX ambient[] = {
        SFX_BAT_SQUEAK, SFX_BEE_BUZZ, SFX_BIRD_CHIRP, SFX_BIRD_COO, SFX_BIRD_TRILL, SFX_BIRD_TWEET,
        SFX_CHAIN_RATTLE, SFX_CREAK, SFX_CRYSTAL_CHIME, SFX_DRIP_A, SFX_DRIP_B, SFX_FIRE_CRACKLE,
        SFX_RUMBLE, SFX_WATER_PLIP, SFX_WATER_SPLASH, SFX_WIND, SFX_WISP_MOAN,
    };
    for(unsigned i = 0; i < sizeof(ambient) / sizeof(ambient[0]); ++i)
        if(ambient[i] == effect)
            return UINT64_C(1) << i;
    return UINT64_C(1) << 63; // Effects outside the ambient catalog must fail the allowed-set check.
}

constexpr uint64_t BIRDS =
    bit(SFX_BIRD_CHIRP) | bit(SFX_BIRD_COO) | bit(SFX_BIRD_TRILL) | bit(SFX_BIRD_TWEET);
constexpr uint64_t WATER = bit(SFX_WATER_PLIP) | bit(SFX_WATER_SPLASH);
constexpr uint64_t DRIPS = bit(SFX_DRIP_A) | bit(SFX_DRIP_B);

// What each area may sound like. The tile classes decide when, and these sets decide what: birds
// and bees are heard outdoors, a creaking floor in a house, drips and bats underground.
constexpr uint64_t OUTDOOR_SOUNDS = BIRDS | WATER | bit(SFX_BEE_BUZZ) | bit(SFX_FIRE_CRACKLE);
constexpr uint64_t HOUSE_SOUNDS = bit(SFX_CREAK) | bit(SFX_FIRE_CRACKLE);
constexpr uint64_t CAVE_SOUNDS =
    DRIPS | bit(SFX_BAT_SQUEAK) | bit(SFX_RUMBLE) | bit(SFX_CRYSTAL_CHIME) | bit(SFX_FIRE_CRACKLE);
constexpr uint64_t DUNGEON_SOUNDS = DRIPS | bit(SFX_WISP_MOAN) | bit(SFX_CHAIN_RATTLE) |
                                    bit(SFX_RUMBLE) | bit(SFX_CRYSTAL_CHIME) |
                                    bit(SFX_FIRE_CRACKLE);

constexpr uint64_t area_sounds(uint8_t area) {
    switch(area) {
        case AREA_OUTDOOR:
            return OUTDOOR_SOUNDS;
        case AREA_INTERIORS:
            return HOUSE_SOUNDS;
        case AREA_CAVE:
            return CAVE_SOUNDS;
        default:
            return DUNGEON_SOUNDS;
    }
}

// Every sound that an area's tile classes can make with any roll.
static uint64_t reachable(uint8_t area) {
    uint64_t found = 0;
    for(unsigned ambient_class = 0; ambient_class <= AMBIENT_EMBER; ++ambient_class) {
        for(unsigned roll = 0; roll < 256; ++roll) {
            for(unsigned vary = 0; vary < 8; ++vary) {
                SFX sound = tile_sound(area, (uint8_t)ambient_class, (uint8_t)roll, (uint8_t)vary);
                if(sound != SFX_NONE)
                    found |= bit(sound);
            }
        }
    }
    return found;
}

static void voices() {
    for(uint8_t area = 1; area <= NUM_AREAS; ++area) {
        uint64_t found = reachable(area);
        // Only the area's own sounds are heard there, and all of them can be.
        CHECK((found & ~area_sounds(area)) == 0);
        CHECK(found == area_sounds(area));
        // A tile with no ambient class is silent, and so is a high roll: the sounds come on
        // occasion.
        for(unsigned roll = 0; roll < 256; ++roll)
            CHECK(tile_sound(area, AMBIENT_NONE, (uint8_t)roll, 0) == SFX_NONE);
        for(unsigned ambient_class = 0; ambient_class <= AMBIENT_EMBER; ++ambient_class)
            CHECK(tile_sound(area, (uint8_t)ambient_class, 255, 0) == SFX_NONE);
    }
    CHECK(tile_sound(0, AMBIENT_TREE, 0, 0) == SFX_NONE &&
          tile_sound(NUM_AREAS + 1, AMBIENT_TREE, 0, 0) == SFX_NONE);

    // Birds sing from the trees, and a cave has no trees to sing from.
    CHECK(((bit(tile_sound(AREA_OUTDOOR, AMBIENT_TREE, 0, 0)) & BIRDS) != 0));
    CHECK(tile_sound(AREA_CAVE, AMBIENT_TREE, 0, 0) == SFX_NONE);
    // Every variant of a group is chosen by some roll of the variety bits.
    uint64_t chosen = 0;
    for(unsigned vary = 0; vary < 256; ++vary)
        chosen |= bit(tile_sound(AREA_OUTDOOR, AMBIENT_TREE, 0, (uint8_t)vary));
    CHECK(chosen == BIRDS);
    // A fish splashes in the water, and a drop falls in a pool.
    CHECK((bit(tile_sound(AREA_OUTDOOR, AMBIENT_WATER, 0, 0)) & WATER) != 0);
    CHECK((bit(tile_sound(AREA_CAVE, AMBIENT_POOL, 0, 0)) & DRIPS) != 0);
}

// Every epoch has exactly one moment, and across epochs a sound is due about as often as the chance
// that the soundscape states.
static void moments() {
    unsigned due = 0, moments = 0;
    unsigned per_epoch[65536 / SOUND_EPOCH_TICKS] = {};
    unsigned early = 0, late = 0;
    for(uint32_t ticks = 0; ticks < 65536; ++ticks) {
        if(sound_moment((uint16_t)ticks)) {
            ++moments;
            ++per_epoch[ticks / SOUND_EPOCH_TICKS];
        }
        if(sound_due((uint16_t)ticks)) {
            ++due;
            (ticks % SOUND_EPOCH_TICKS < SOUND_EPOCH_TICKS / 2 ? early : late) += 1;
        }
    }
    for(unsigned count : per_epoch)
        CHECK(count == 1);
    CHECK(moments == 65536 / SOUND_EPOCH_TICKS);
    // About SOUND_CHANCE / 256 of the 256 epochs: a sound on occasion, not every epoch.
    unsigned expected = 65536 / SOUND_EPOCH_TICKS * SOUND_CHANCE / 256;
    CHECK(due + 40 > expected && due < expected + 40);
    // The moments are spread over the epoch, not stuck at its start.
    CHECK(early > due / 4 && late > due / 4);
    unsigned grains[SOUND_EPOCH_TICKS / SOUND_GRAIN_TICKS] = {};
    for(unsigned epoch = 0; epoch < 256; ++epoch)
        ++grains[sound_offset((uint8_t)epoch) / SOUND_GRAIN_TICKS];
    for(unsigned count : grains)
        CHECK(count > 2 && count < 20);

    // Consecutive epochs are not a fixed distance apart, or the sounds would keep a rhythm.
    unsigned gaps[512] = {};
    uint32_t previous = 0;
    bool have_previous = false;
    for(uint32_t ticks = 0; ticks < 65536; ++ticks) {
        if(!sound_moment((uint16_t)ticks))
            continue;
        if(have_previous && ticks - previous < 512)
            ++gaps[ticks - previous];
        previous = ticks;
        have_previous = true;
    }
    unsigned distinct = 0;
    for(unsigned count : gaps)
        distinct += count != 0;
    CHECK(distinct > 25);

    // A probe lies within the player's hearing, and probes differ from each other.
    unsigned places[2 * HEARING_REACH_X + 1][2 * HEARING_REACH_Y + 1] = {};
    for(uint16_t pick = 0; pick < 4096; ++pick) {
        for(uint8_t index = 0; index < SOUND_PROBES; ++index) {
            SoundProbe probe = sound_probe(pick, index);
            CHECK(probe.dx >= -(int)HEARING_REACH_X && probe.dx <= (int)HEARING_REACH_X);
            CHECK(probe.dy >= -(int)HEARING_REACH_Y && probe.dy <= (int)HEARING_REACH_Y);
            ++places[probe.dx + HEARING_REACH_X][probe.dy + HEARING_REACH_Y];
        }
    }
    for(auto const& column : places)
        for(unsigned count : column)
            CHECK(count > 0);
}

// A gust's first line reaches a tile once every 1024 ticks, on the tick that the grass there bends.
static void wind() {
    for(unsigned tile : {0u, 1u, 77u, 128u, 255u}) {
        unsigned heard = 0;
        for(uint32_t ticks = 0; ticks < 2048; ++ticks) {
            if(!wind_passes((uint16_t)ticks, (uint8_t)tile, (uint8_t)(tile / 3))) {
                continue;
            }
            ++heard;
            // The grass bends there as it passes, and the line has just arrived.
            CHECK(gust_bends(gust_front((uint16_t)ticks), (uint8_t)tile, (uint8_t)(tile / 3)));
            CHECK(ticks % GUST_STEP_TICKS == 0);
        }
        // One gust line per 1024 ticks at a tile, over two cycles.
        CHECK(heard == 2);
    }
}

// The menu picks its tick from the millisecond clock, which at a press is a multiple of the 20 ms
// frame plus jitter. Every tick is chosen, and none is chosen far more often than the others.
static void menu_ticks() {
    constexpr unsigned PRESSES = 2000, VARIANTS = SFX_MENU_TICKS_LAST - SFX_MENU_TICKS_FIRST + 1;
    for(unsigned jitter = 0; jitter < 3; ++jitter) {
        unsigned chosen[VARIANTS] = {};
        for(unsigned press = 0; press < PRESSES; ++press) {
            uint16_t millis = (uint16_t)(1000 + 20 * press + jitter);
            SFX tick = sfx_variant(SFX_MENU_TICKS_FIRST, SFX_MENU_TICKS_LAST, ambient_hash(millis));
            CHECK(tick >= SFX_MENU_TICKS_FIRST && tick <= SFX_MENU_TICKS_LAST);
            ++chosen[tick - SFX_MENU_TICKS_FIRST];
        }
        for(unsigned count : chosen)
            CHECK(count > PRESSES / VARIANTS / 2 && count < PRESSES / VARIANTS * 2);
    }
    // A group's index wraps around the group, so any index is a valid choice.
    for(unsigned index = 0; index < 1000; ++index) {
        SFX bird = sfx_variant(SFX_BIRDS_FIRST, SFX_BIRDS_LAST, (uint16_t)index);
        CHECK(bird >= SFX_BIRDS_FIRST && bird <= SFX_BIRDS_LAST);
    }
}

static void stand_at(TilePos cell, DIRECTION facing) {
    world_enter(cell.area, (uint16_t)(cell.x * TILE), (uint16_t)(cell.y * TILE), facing);
}

// Plays the soundscape for a long walk of ticks at one place, and reports the sounds it makes.
struct Heard {
    uint64_t set;
    unsigned count, winds;
};

static Heard listen(uint32_t from, uint32_t to) {
    Heard heard{};
    for(uint32_t ticks = from; ticks < to; ++ticks) {
        game.ticks = (uint16_t)ticks;
        SFX sound = ambient_sound_now();
        if(sound == SFX_NONE)
            continue;
        heard.set |= bit(sound);
        ++heard.count;
        heard.winds += sound == SFX_WIND;
    }
    return heard;
}

static void places() {
    game_new();
    // Outdoors: birds and the wind, and nothing from underground.
    stand_at(positions::PlayerStart, NORTH);
    Game before = game;
    Heard outdoors = listen(0, 3 * 65536 / 4);
    CHECK((outdoors.set & ~(OUTDOOR_SOUNDS | bit(SFX_WIND))) == 0);
    CHECK((outdoors.set & BIRDS) != 0 && (outdoors.set & bit(SFX_WIND)) != 0);
    // About one sound in six seconds at most, and not silent: on occasion. Every epoch holds at
    // most one sound, and a gust line adds one about every twenty seconds.
    unsigned epochs = 3 * 65536 / 4 / SOUND_EPOCH_TICKS;
    CHECK(outdoors.count - outdoors.winds <= epochs);
    CHECK(outdoors.count - outdoors.winds > epochs / 8);
    CHECK(outdoors.winds >= 3 * 65536 / 4 / 1024 - 1 && outdoors.winds <= 3 * 65536 / 4 / 1024 + 1);
    // Listening does not change the game: no state, no rng, no tick.
    game.ticks = before.ticks;
    CHECK(!memcmp(&game, &before, sizeof(Game)));

    // The same tick sounds the same, so a frozen tick repeats one answer and the platform asks
    // once per simulated frame.
    for(uint32_t ticks = 0; ticks < 4096; ++ticks) {
        game.ticks = (uint16_t)ticks;
        SFX first = ambient_sound_now();
        CHECK(first == ambient_sound_now());
    }

    // In a house: a creaking floor, and no wind and no birds. Houses have no sky.
    TilePos way_out = positions::ElderExit;
    stand_at({way_out.area, way_out.x, (uint8_t)(way_out.y - 1)}, NORTH);
    Heard indoors = listen(0, 65536);
    CHECK(indoors.winds == 0 && (indoors.set & ~HOUSE_SOUNDS) == 0);
    CHECK((indoors.set & bit(SFX_CREAK)) != 0);

    // In the cave: drips and the dark. No wind and no birds.
    TilePos lever = positions::CaveLever;
    stand_at({lever.area, lever.x, (uint8_t)(lever.y + 1)}, NORTH);
    Heard cave = listen(0, 65536);
    CHECK(cave.winds == 0 && (cave.set & ~CAVE_SOUNDS) == 0);
    CHECK((cave.set & DRIPS) != 0);
    CHECK(cave.count > 20);

    // In the dungeon: the crypt's own sounds.
    TilePos dungeon_lever = positions::DungeonLever;
    stand_at({dungeon_lever.area, dungeon_lever.x, (uint8_t)(dungeon_lever.y + 1)}, NORTH);
    Heard dungeon = listen(0, 65536);
    CHECK(dungeon.winds == 0 && (dungeon.set & ~DUNGEON_SOUNDS) == 0);
    CHECK(dungeon.count > 20);
}

int main() {
    voices();
    moments();
    wind();
    menu_ticks();
    places();
    printf("soundscape: %u checks passed\n", checks);
    return 0;
}
