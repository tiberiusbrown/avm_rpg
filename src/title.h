#ifndef TITLE_H
#define TITLE_H

#include "ambient.h"

// The title screen's animation. Like the environment animation in ambient.h, everything here is
// a pure function of a tick counter, so the screen keeps no state beyond that counter.
//
// The picture itself is assets/images/title_scene.png. The positions below are pixels of that
// picture, so moving something in the picture means moving its constant here.

// The counter wraps here. Every period below divides it, so nothing jumps at the wrap.
inline constexpr uint16_t TITLE_CYCLE = 49152;

// Frames of title16.png. The generated sheet asserts the count.
enum TITLE_FRAME : uint8_t {
    TITLE_BIRD_FLY_LEFT = 0,    // wings up, down
    TITLE_BIRD_PERCH_LEFT = 2,  // standing, pecking
    TITLE_BIRD_FLY_RIGHT = 4,   // wings up, down
    TITLE_BIRD_PERCH_RIGHT = 6, // standing, pecking
    TITLE_RABBIT_LEFT = 8,      // sitting, hopping, alert
    TITLE_RABBIT_RIGHT = 11,    // sitting, hopping, alert
    TITLE_MOUSE = 14,           // two running poses, sitting; it faces left
    TITLE_SMOKE = 17,           // puff, cloud, wisp
    TITLE_BUTTERFLY = 20,       // wide, half, closed
    TITLE_LEAF = 23,            // two tumbling poses
    TITLE_BAT = 25,             // wings up, down
    TITLE_GLINT = 27,           // small, large
    TITLE_BOUGH = 29,           // the leaves at the end of the bough
    TITLE_SPRIG = 30,           // two sprigs that hang from the canopy
    TITLE_FRAME_COUNT = 32
};

// The size of title_logo.png. The generated sprite asserts it.
inline constexpr uint8_t TITLE_LOGO_WIDTH = 45, TITLE_LOGO_HEIGHT = 13;
inline constexpr int16_t TITLE_LOGO_Y = 0;

// A frame of the sheet and where its 16x16 cell goes.
struct TitleSprite {
    bool visible;
    uint8_t frame;
    int16_t x, y;
};

inline constexpr TitleSprite TITLE_HIDDEN{false, 0, 0, 0};

// A creature's art stands on the bottom of its cell, centered. `ground` is the first row below
// its feet.
constexpr TitleSprite title_standing(uint8_t frame, int16_t x, int16_t ground) {
    return {true, frame, (int16_t)(x - 8), (int16_t)(ground - 15)};
}

// Small effects are centered on pixel (7, 7) of their cell.
constexpr TitleSprite title_centered(uint8_t frame, int16_t x, int16_t y) {
    return {true, frame, (int16_t)(x - 7), (int16_t)(y - 7)};
}

// ---- Sky ----

// The stars of the picture's dark sky. Two of them at a time dim or flare.
struct TitlePoint {
    uint8_t x, y;
};

inline constexpr TitlePoint TITLE_STARS[8] = {{2, 2},   {21, 3}, {27, 10}, {34, 6},
                                              {17, 14}, {93, 4}, {95, 13}, {74, 19}};
inline constexpr uint8_t TITLE_TWINKLES = 2, TWINKLE_TICKS = 16;

enum TITLE_TWINKLE : uint8_t { TWINKLE_NONE, TWINKLE_DIM, TWINKLE_FLARE };

struct TitleTwinkle {
    TITLE_TWINKLE kind;
    TitlePoint star;
};

constexpr TitleTwinkle title_twinkle(uint8_t slot, uint16_t ticks) {
    // The slots change at different moments.
    uint16_t clock = (uint16_t)(ticks + slot * (TWINKLE_TICKS / TITLE_TWINKLES));
    uint16_t pick = ambient_hash((uint16_t)((clock / TWINKLE_TICKS) * 2 + slot + 1));
    TitlePoint star = TITLE_STARS[pick & 7];
    uint8_t kind = (pick >> 3) & 3;
    if(kind < 2)
        return {TWINKLE_DIM, star};
    if(kind == 2 && (clock & (TWINKLE_TICKS - 1)) < TWINKLE_TICKS / 2)
        return {TWINKLE_FLARE, star};
    return {TWINKLE_NONE, star};
}

// A shooting star crosses the sky left of the logo every 41 seconds.
inline constexpr uint16_t METEOR_PERIOD = 2048;
inline constexpr uint8_t METEOR_TICKS = 7;

constexpr TitleSprite title_meteor(uint16_t ticks) {
    uint16_t age = ticks & (METEOR_PERIOD - 1);
    if(age >= METEOR_TICKS)
        return TITLE_HIDDEN;
    return {true, 0, (int16_t)(36 - 2 * age), (int16_t)age};
}

// A bat crosses the sky once a minute.
inline constexpr uint16_t BAT_PERIOD = 3072, BAT_TICKS = 100;

constexpr TitleSprite title_bat(uint16_t ticks) {
    uint16_t age = (uint16_t)(ticks + 1000) % BAT_PERIOD;
    if(age >= BAT_TICKS)
        return TITLE_HIDDEN;
    return title_centered((uint8_t)(TITLE_BAT + ((ticks >> 2) & 1)),
                          (int16_t)(age + age / 2 - 8),
                          (int16_t)(7 + (ambient_sine((uint8_t)(age * 4)) >> 4)));
}

// ---- House ----

// Three puffs rise from the chimney and lean left on the wind. This never stops, so the screen
// always moves.
inline constexpr uint8_t TITLE_PUFFS = 3, PUFF_PERIOD = 128, PUFF_TICKS = 108;

constexpr TitleSprite title_smoke(uint8_t puff, uint16_t ticks) {
    uint8_t age = (uint8_t)(ticks + puff * 43) & (PUFF_PERIOD - 1);
    if(age >= PUFF_TICKS)
        return TITLE_HIDDEN;
    int8_t waver = (int8_t)(ambient_sine((uint8_t)(age * 3 + puff * 85)) >> 5);
    return title_centered((uint8_t)(TITLE_SMOKE + age / (PUFF_TICKS / 3)),
                          (int16_t)(29 - age / 6 + waver), (int16_t)(13 - age / 7));
}

// The candle behind the window: now and then a dark speck crosses the panes.
constexpr TitleSprite title_candle(uint16_t ticks) {
    uint16_t pick = ambient_hash((uint16_t)((ticks >> 2) + 1));
    if(pick & 3)
        return TITLE_HIDDEN;
    return {true, 0, (int16_t)(32 + ((pick >> 2) & 3)), (int16_t)(35 + ((pick >> 5) & 3))};
}

// ---- Tree ----

// A leaf clump leans one pixel either way with the wind, each at its own moment.
constexpr int8_t title_sway(uint8_t phase, uint16_t ticks) {
    return (int8_t)((ambient_sine((uint8_t)(ticks + phase)) + 32) >> 6);
}

struct TitleClump {
    uint8_t frame, x, y, phase;
};

inline constexpr TitleClump TITLE_CLUMPS[] = {
    {TITLE_BOUGH, 71, 13, 0}, {TITLE_SPRIG, 93, 3, 60}, {TITLE_SPRIG + 1, 104, 10, 150}};

// A falling leaf. The two slots have different periods, and each fall starts somewhere else
// under the canopy.
inline constexpr uint8_t TITLE_LEAVES = 2, LEAF_FALL_TICKS = 168;

constexpr TitleSprite title_leaf(uint8_t slot, uint16_t ticks) {
    uint16_t period = slot ? 384 : 256;
    uint16_t age = ticks % period;
    if(age >= LEAF_FALL_TICKS)
        return TITLE_HIDDEN;
    uint16_t start = 98 + (ambient_hash((uint16_t)((ticks / period) * 2 + slot + 1)) & 15);
    int8_t flutter = (int8_t)(ambient_sine((uint8_t)(age * 5)) >> 4);
    return title_centered((uint8_t)(TITLE_LEAF + ((age >> 3) & 1)),
                          (int16_t)(start - age / 5 + flutter), (int16_t)(17 + age / 4));
}

// ---- Birds ----

// A bird flies in from the upper left, stays on its perch, and flies off.
struct TitlePerch {
    int16_t x, ground;
    uint16_t period, offset, stay;
    bool leaves_right;
};

inline constexpr uint8_t BIRD_FLIGHT_TICKS = 64;
// The bough of the tree, and the pommel of the sword.
inline constexpr TitlePerch TITLE_PERCHES[] = {{93, 27, 768, 0, 320, true},
                                               {67, 20, 1536, 700, 256, false}};

constexpr TitleSprite title_bird(TitlePerch const& perch, uint16_t ticks) {
    uint16_t age = (uint16_t)(ticks + perch.offset) % perch.period;
    uint8_t flap = (ticks >> 2) & 1;
    if(age < BIRD_FLIGHT_TICKS) {
        int16_t away = BIRD_FLIGHT_TICKS - age;
        return title_standing((uint8_t)(TITLE_BIRD_FLY_RIGHT + flap), (int16_t)(perch.x - away),
                              (int16_t)(perch.ground - away / 2));
    }
    age = (uint16_t)(age - BIRD_FLIGHT_TICKS);
    if(age < perch.stay) {
        // It looks the other way for a while, and pecks now and then.
        uint8_t facing = (age >> 6) & 1 ? TITLE_BIRD_PERCH_LEFT : TITLE_BIRD_PERCH_RIGHT;
        return title_standing((uint8_t)(facing + (((age >> 3) & 7) == 5)), perch.x, perch.ground);
    }
    age = (uint16_t)(age - perch.stay);
    if(age < BIRD_FLIGHT_TICKS)
        return title_standing(
            (uint8_t)((perch.leaves_right ? TITLE_BIRD_FLY_RIGHT : TITLE_BIRD_FLY_LEFT) + flap),
            (int16_t)(perch.leaves_right ? perch.x + age : perch.x - age),
            (int16_t)(perch.ground - age * 3 / 4));
    return TITLE_HIDDEN;
}

// ---- Ground ----

// A rabbit hops in along the path from the left, sits and listens, and hops back out.
inline constexpr uint16_t RABBIT_PERIOD = 1536, RABBIT_OFFSET = 1200, RABBIT_STAY = 320;
inline constexpr uint8_t RABBIT_HOP_TICKS = 12, RABBIT_HOP_AIR = 8, RABBIT_HOP_PIXELS = 6;
inline constexpr uint8_t RABBIT_HOPS = 8;
inline constexpr int16_t RABBIT_X = 34, RABBIT_GROUND = 57;

constexpr TitleSprite title_rabbit(uint16_t ticks) {
    constexpr uint16_t TRAVEL = RABBIT_HOPS * RABBIT_HOP_TICKS;
    uint16_t age = (uint16_t)(ticks + RABBIT_OFFSET) % RABBIT_PERIOD;
    bool leaving = age >= TRAVEL + RABBIT_STAY;
    if(age >= TRAVEL && !leaving) {
        age = (uint16_t)(age - TRAVEL);
        uint8_t facing = age >= RABBIT_STAY / 2 ? TITLE_RABBIT_LEFT : TITLE_RABBIT_RIGHT;
        return title_standing((uint8_t)(facing + (((age >> 5) & 3) == 1 ? 2 : 0)), RABBIT_X,
                              RABBIT_GROUND);
    }
    // Hops are counted from the resting place outward, so both trips land on the same spots.
    uint16_t out = leaving ? (uint16_t)(age - TRAVEL - RABBIT_STAY) : (uint16_t)(TRAVEL - 1 - age);
    if(out >= TRAVEL)
        return TITLE_HIDDEN;
    uint8_t step = (uint8_t)(out % RABBIT_HOP_TICKS);
    // A hop moves through its air time, then rests. On the way in the steps run backward.
    uint8_t moved = leaving ? step : (uint8_t)(RABBIT_HOP_TICKS - 1 - step);
    bool air = moved < RABBIT_HOP_AIR;
    uint8_t along = air ? (uint8_t)(moved * RABBIT_HOP_PIXELS / RABBIT_HOP_AIR) : RABBIT_HOP_PIXELS;
    if(!leaving)
        along = (uint8_t)(RABBIT_HOP_PIXELS - along);
    int16_t distance = (int16_t)((out / RABBIT_HOP_TICKS) * RABBIT_HOP_PIXELS + along);
    uint8_t lift = air ? (uint8_t)((moved * (RABBIT_HOP_AIR - moved)) >> 2) : 0;
    return title_standing((uint8_t)((leaving ? TITLE_RABBIT_LEFT : TITLE_RABBIT_RIGHT) + air),
                          (int16_t)(RABBIT_X - distance), (int16_t)(RABBIT_GROUND - lift));
}

// A mouse runs along the front from the right, stops below the tree to sniff, and runs on.
inline constexpr uint16_t MOUSE_PERIOD = 1024, MOUSE_OFFSET = 300;
inline constexpr uint8_t MOUSE_RUN_IN = 44, MOUSE_STAY = 96;
inline constexpr int16_t MOUSE_START = 136, MOUSE_GROUND = 64;

constexpr TitleSprite title_mouse(uint16_t ticks) {
    uint16_t age = (uint16_t)(ticks + MOUSE_OFFSET) & (MOUSE_PERIOD - 1);
    uint8_t run = (uint8_t)(TITLE_MOUSE + ((age >> 2) & 1));
    if(age < MOUSE_RUN_IN)
        return title_standing(run, (int16_t)(MOUSE_START - age), MOUSE_GROUND);
    age = (uint16_t)(age - MOUSE_RUN_IN);
    if(age < MOUSE_STAY)
        return title_standing(TITLE_MOUSE + 2, MOUSE_START - MOUSE_RUN_IN, MOUSE_GROUND);
    age = (uint16_t)(age - MOUSE_STAY);
    if(age < MOUSE_START - MOUSE_RUN_IN + 8)
        return title_standing(run, (int16_t)(MOUSE_START - MOUSE_RUN_IN - age), MOUSE_GROUND);
    return TITLE_HIDDEN;
}

// A butterfly wanders across the clearing from the right.
inline constexpr uint16_t BUTTERFLY_PERIOD = 1024, BUTTERFLY_OFFSET = 650, BUTTERFLY_TICKS = 464;

constexpr TitleSprite title_butterfly(uint16_t ticks) {
    uint16_t age = (uint16_t)(ticks + BUTTERFLY_OFFSET) & (BUTTERFLY_PERIOD - 1);
    if(age >= BUTTERFLY_TICKS)
        return TITLE_HIDDEN;
    int16_t y = (int16_t)(55 + (ambient_sine((uint8_t)(age * 2)) >> 4) +
                          (ambient_sine((uint8_t)(age * 7)) >> 5));
    return title_centered((uint8_t)(TITLE_BUTTERFLY + ambient_flap((uint8_t)(ticks >> 2))),
                          (int16_t)(108 - age / 4), y);
}

// A glint runs up the sword's blade and flares at the guard.
inline constexpr uint16_t GLINT_PERIOD = 512, GLINT_OFFSET = 200;
inline constexpr uint8_t GLINT_RUN_TICKS = 28, GLINT_FLARE_TICKS = 6;

constexpr TitleSprite title_glint(uint16_t ticks) {
    uint16_t age = (uint16_t)(ticks + GLINT_OFFSET) & (GLINT_PERIOD - 1);
    if(age < GLINT_RUN_TICKS)
        return title_centered(TITLE_GLINT, (int16_t)(47 + age / 2), (int16_t)(60 - age));
    if(age < GLINT_RUN_TICKS + GLINT_FLARE_TICKS)
        return title_centered(TITLE_GLINT + 1, 61, 30);
    return TITLE_HIDDEN;
}

// ---- Air ----

// Dust motes drift left on the wind, bobbing, and wrap around the screen.
inline constexpr TitlePoint TITLE_MOTES[4] = {{20, 30}, {70, 37}, {100, 47}, {50, 54}};

constexpr TitleSprite title_mote(uint8_t mote, uint16_t ticks) {
    // Each one fades out for a moment now and then.
    if((((ticks >> 4) + mote * 3) & 7) == 0)
        return TITLE_HIDDEN;
    uint8_t drift = (uint8_t)(ticks >> (2 + (mote & 1)));
    int8_t bob = (int8_t)(ambient_sine((uint8_t)((ticks >> 1) + mote * 64)) >> 4);
    return {true, 0, (int16_t)((uint8_t)(TITLE_MOTES[mote].x - drift) & 127),
            (int16_t)(TITLE_MOTES[mote].y + bob)};
}

// Fireflies hover over the grass and glow for a second at a time.
inline constexpr TitlePoint TITLE_FIREFLIES[2] = {{97, 51}, {40, 53}};
inline constexpr uint8_t FIREFLY_PERIOD = 128, FIREFLY_GLOW_TICKS = 48;

// `frame` is 1 while the glow is at its brightest.
constexpr TitleSprite title_firefly(uint8_t fly, uint16_t ticks) {
    uint8_t age = (uint8_t)(ticks + fly * 90) & (FIREFLY_PERIOD - 1);
    if(age >= FIREFLY_GLOW_TICKS)
        return TITLE_HIDDEN;
    int8_t dx = (int8_t)(ambient_sine((uint8_t)((ticks >> 1) + fly * 100)) >> 4);
    int8_t dy = (int8_t)(ambient_sine((uint8_t)((ticks >> 2) + fly * 60)) >> 5);
    return {true, (uint8_t)(age >= 16 && age < 32), (int16_t)(TITLE_FIREFLIES[fly].x + dx),
            (int16_t)(TITLE_FIREFLIES[fly].y + dy)};
}

static_assert(TITLE_CYCLE % 1024 == 0 && TITLE_CYCLE % METEOR_PERIOD == 0 &&
                  TITLE_CYCLE % BAT_PERIOD == 0 && TITLE_CYCLE % RABBIT_PERIOD == 0 &&
                  TITLE_CYCLE % 384 == 0 && TITLE_CYCLE % 768 == 0 && TITLE_CYCLE % 1536 == 0,
              "every title animation period must divide TITLE_CYCLE");

#endif
