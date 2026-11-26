#ifndef TITLE_H
#define TITLE_H

#include "ambient.h"

// The title screen's animation. Like the environment animation in ambient.h, everything here is
// a pure function of a tick counter, so the screen keeps no state beyond that counter.
//
// The picture is drawn in layers. assets/images/title_scene.png is the background: sky, castle,
// bridge, water and the near bank. In front of it stand three pines and a rock, each a picture of
// its own (title_pine_*.png, title_rock.png), so a creature drawn between two layers passes
// behind the nearer one. The positions below are pixels of the 128x64 screen, so moving
// something in a picture means moving its constant here.

// The counter wraps here. Every period below divides it, so nothing jumps at the wrap.
inline constexpr uint16_t TITLE_CYCLE = 49152;

// The flag, the trees, the stars and the water never stop. The visitors take turns instead: each
// one comes at its own moment of this program of about two minutes, with quiet between them.
inline constexpr uint16_t TITLE_PROGRAM = 6144;
inline constexpr uint16_t ROCK_BIRD_START = 256, RABBIT_START = 1280, FLOCK_START = 2304;
inline constexpr uint16_t SECOND_BIRD_START = 3840;
// The mouse comes twice in a program.
inline constexpr uint16_t MOUSE_PERIOD = TITLE_PROGRAM / 2, MOUSE_START_TICK = 2816;

// How long ago a visitor's turn began, from 0 to `period` - 1. `start` is less than `period`.
constexpr uint16_t title_turn(uint16_t start, uint16_t ticks, uint16_t period = TITLE_PROGRAM) {
    return (uint16_t)(ticks + period - start) % period;
}

// The counter moved on by `offset`, still wrapping at TITLE_CYCLE. Things that share one rule
// use it to keep out of step with each other.
constexpr uint16_t title_clock(uint16_t ticks, uint8_t offset) {
    uint16_t clock = (uint16_t)(ticks + offset);
    return clock >= TITLE_CYCLE ? (uint16_t)(clock - TITLE_CYCLE) : clock;
}

// Frames of title16.png. The generated sheet asserts the count.
enum TITLE_FRAME : uint8_t {
    TITLE_BIRD_FLY_LEFT = 0,    // wings up, down
    TITLE_BIRD_PERCH_LEFT = 2,  // standing, pecking
    TITLE_BIRD_FLY_RIGHT = 4,   // wings up, down
    TITLE_BIRD_PERCH_RIGHT = 6, // standing, pecking
    TITLE_RABBIT_LEFT = 8,      // sitting, hopping, alert
    TITLE_RABBIT_RIGHT = 11,    // sitting, hopping, alert
    TITLE_MOUSE_LEFT = 14,      // two running poses
    TITLE_MOUSE_SIT = 16,       // it faces left
    TITLE_MOUSE_RIGHT = 17,     // two running poses
    TITLE_FLAG = 19,            // four phases of one wave
    TITLE_FISH = 23,            // rising, level, falling; it heads right
    TITLE_SPLASH = 26,          // the splash, then its ring
    TITLE_RIPPLE = 28,          // short, then spread
    TITLE_FAR_BIRD = 30,        // wings up, level, down; it heads left
    TITLE_FRAME_COUNT = 33
};

// The size of title_logo.png, which the generated sprite asserts, and where it goes: the open
// sky between the flag and the right pine.
inline constexpr uint8_t TITLE_LOGO_WIDTH = 45, TITLE_LOGO_HEIGHT = 13;
inline constexpr int16_t TITLE_LOGO_X = 64, TITLE_LOGO_Y = 6;
inline constexpr uint8_t PRE_TITLE_MOVE_STEPS = 16;

// A frame of the sheet and where its 16x16 cell goes. A single pixel uses only the position.
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

struct TitlePoint {
    uint8_t x, y;
};

// ---- Wind ----

// A gust comes in at the left edge every twenty seconds and crosses the picture at two pixels a
// tick. Wherever it is blowing, the flag streams, the pines lean over and dust flies past.
inline constexpr uint16_t GUST_PERIOD = 1024, GUST_START = 600;
inline constexpr uint8_t GUST_TICKS = 128, GUST_RISE_TICKS = 32, GUST_HOLD_TICKS = 64;
inline constexpr uint8_t GUST_FULL = 16;

// Ticks since the gust's front entered the picture, from 0 to GUST_PERIOD - 1.
constexpr uint16_t title_gust_age(uint16_t ticks) {
    return (uint16_t)(ticks + GUST_PERIOD - GUST_START) & (GUST_PERIOD - 1);
}

// How hard the gust blows at column `x`, from 0 (calm) to GUST_FULL: it rises in small steps,
// holds, and dies away half as fast.
constexpr uint8_t title_gust(uint8_t x, uint16_t ticks) {
    uint16_t age = title_gust_age(ticks);
    uint8_t reached = x >> 1;
    if(age < reached || age >= (uint16_t)(reached + GUST_TICKS))
        return 0;
    uint8_t blowing = (uint8_t)(age - reached);
    if(blowing < GUST_RISE_TICKS)
        return (uint8_t)(blowing / (GUST_RISE_TICKS / GUST_FULL));
    if(blowing < GUST_HOLD_TICKS)
        return GUST_FULL;
    return (uint8_t)((GUST_TICKS - blowing) / ((GUST_TICKS - GUST_HOLD_TICKS) / GUST_FULL));
}

// Dust flies along with the gust, each mote some way behind the front and bobbing on its row.
struct TitleMote {
    uint8_t behind, y;
};

inline constexpr TitleMote TITLE_MOTES[] = {{0, 23},  {12, 41}, {26, 30}, {38, 47},
                                            {54, 26}, {66, 37}, {84, 44}, {98, 28}};
inline constexpr uint8_t TITLE_MOTE_COUNT = sizeof(TITLE_MOTES) / sizeof(TITLE_MOTES[0]);
// A mote is a streak this many pixels long.
inline constexpr uint8_t MOTE_LENGTH = 2;

constexpr TitleSprite title_mote(uint8_t mote, uint16_t ticks) {
    uint16_t front = (uint16_t)(title_gust_age(ticks) * 2);
    uint8_t behind = TITLE_MOTES[mote].behind;
    if(front < behind || front >= (uint16_t)(behind + 128 - MOTE_LENGTH))
        return TITLE_HIDDEN;
    int8_t bob = (int8_t)(ambient_sine((uint8_t)(front + mote * 40)) >> 4);
    return {true, 0, (int16_t)(front - behind), (int16_t)(TITLE_MOTES[mote].y + bob)};
}

// ---- Sky ----

// Stars of the background's open sky with room around them. Now and then one of them flares
// from a single pixel into a small cross and fades back.
inline constexpr TitlePoint TITLE_STARS[16] = {
    {2, 9},   {30, 23}, {30, 26}, {35, 4},  {39, 4},  {50, 4},  {60, 8}, {72, 28},
    {75, 28}, {77, 22}, {86, 24}, {89, 26}, {90, 23}, {92, 31}, {97, 2}, {99, 30}};
inline constexpr uint8_t TITLE_TWINKLES = 2;
inline constexpr uint16_t TWINKLE_PERIOD = 256;
inline constexpr uint8_t TWINKLE_TICKS = 40, TWINKLE_OFFSET = 150;

// The star that slot `slot` flares now, if any. A slot has a turn every five seconds, at its own
// moment, and lets one turn in four pass.
constexpr TitleSprite title_twinkle(uint8_t slot, uint16_t ticks) {
    uint16_t clock = title_clock(ticks, (uint8_t)(slot * TWINKLE_OFFSET));
    if((clock & (TWINKLE_PERIOD - 1)) >= TWINKLE_TICKS)
        return TITLE_HIDDEN;
    uint16_t pick = ambient_hash((uint16_t)((clock / TWINKLE_PERIOD) * 4 + slot + 1));
    if((pick & 0x300) == 0)
        return TITLE_HIDDEN;
    TitlePoint star = TITLE_STARS[pick & 15];
    return {true, 0, star.x, star.y};
}

// Three far birds cross the sky from right to left, under the name and over the bridge. Their
// path rises to the middle of the picture and sinks again; each bird wanders a little around its
// own place in the loose line and beats its wings in its own time, gliding now and then.
inline constexpr uint8_t TITLE_FLOCK = 3;
inline constexpr uint16_t FLOCK_TICKS = 256;
inline constexpr int16_t FLOCK_START_X = 136, FLOCK_Y = 30;

struct TitleFlockPlace {
    uint8_t behind;
    int8_t below;
};

inline constexpr TitleFlockPlace TITLE_FLOCK_PLACES[TITLE_FLOCK] = {{0, 0}, {10, 3}, {19, -2}};

constexpr TitleSprite title_flock(uint8_t bird, uint16_t ticks) {
    uint16_t age = title_turn(FLOCK_START, ticks);
    if(age >= FLOCK_TICKS)
        return TITLE_HIDDEN;
    uint8_t along = (uint8_t)age;
    // Wings go up, level, down, level; one stretch in four is a glide on level wings.
    uint8_t wings =
        (((along >> 5) + bird) & 3) == 3 ? 1 : ambient_flap((uint8_t)((along >> 2) + bird * 3));
    int16_t x = (int16_t)(FLOCK_START_X - ((along * 5) >> 3) + TITLE_FLOCK_PLACES[bird].behind +
                          (ambient_sine((uint8_t)(along * 3 + bird * 90)) >> 5));
    // The body lifts on the downstroke.
    int16_t y = (int16_t)(FLOCK_Y - ((ambient_sine((uint8_t)(along >> 1)) * 4) >> 6) +
                          TITLE_FLOCK_PLACES[bird].below +
                          (ambient_sine((uint8_t)(along * 5 + bird * 70)) >> 5) - (wings == 2));
    return title_centered((uint8_t)(TITLE_FAR_BIRD + wings), x, y);
}

// ---- Castle ----

// The flag flies from the pole on the turret. The cell's left column is the pixel right of the
// pole. A gust makes it stream.
inline constexpr TitlePoint TITLE_FLAG_AT{56, 0};

constexpr uint8_t title_flag_frame(uint16_t ticks) {
    uint8_t phase = (uint8_t)(title_gust(TITLE_FLAG_AT.x, ticks) ? ticks >> 1 : ticks >> 3);
    return (uint8_t)(TITLE_FLAG + (phase & 3));
}

// ---- Trees ----

// A pine is drawn in bands of eight rows, top band first. The top band has weight `top_weight`
// and each band below it one less, down to the bands that never move; a band is shifted by its
// weight times the tree's lean.
struct TitleTree {
    int16_t x;
    uint8_t y, bands, top_weight;
    // Where the tree stands, for the gust, and where it is in its own slow sway.
    uint8_t center, phase;
};

inline constexpr TitleTree PINE_SMALL{21, 24, 5, 4, 31, 152};
inline constexpr TitleTree PINE_LEFT{-7, 0, 8, 5, 12, 0};
inline constexpr TitleTree PINE_RIGHT{95, 0, 8, 5, 117, 208};

// The lean of a tree in 256ths of a pixel per unit of weight, positive to the right. A calm
// tree sways one pixel either way at its top, once in ten seconds. A gust bends it over to the
// right, two pixels at the top of a tall pine, and lets it come back as the wind dies.
//
// A band moves when its weight times the lean crosses a half pixel, so a lean that only brushes
// such a point would flick the band over and straight back. The sway's reach, the gust's push
// and each tree's phase are chosen so that this never happens: tests/test_ambient.cpp checks
// that every band rests for at least half a second between moves, and more than a second
// before it turns back.
inline constexpr uint8_t TREE_CALM_SWAY = 63, TREE_GUST_LEAN = 4;

constexpr int16_t title_tree_lean(TitleTree const& tree, uint16_t ticks) {
    int16_t calm = (int16_t)(ambient_sine((uint8_t)((ticks >> 1) + tree.phase)) * TREE_CALM_SWAY);
    return (int16_t)((calm >> 6) + title_gust(tree.center, ticks) * TREE_GUST_LEAN);
}

constexpr int8_t title_band_shift(TitleTree const& tree, uint8_t band, int16_t lean) {
    if(band >= tree.top_weight)
        return 0;
    return (int8_t)(((tree.top_weight - band) * lean + 128) >> 8);
}

// ---- Water ----

// The open water between the far shore and the near bank, where ripples and fish may show.
inline constexpr uint8_t WATER_LEFT = 38, WATER_TOP = 44;

// Ripples come and go on the water, each slot at its own moment and each time somewhere new.
inline constexpr uint8_t TITLE_RIPPLES = 3, RIPPLE_PERIOD = 64, RIPPLE_TICKS = 28;
inline constexpr uint8_t RIPPLE_SPREAD_TICK = 10;

constexpr TitleSprite title_ripple(uint8_t slot, uint16_t ticks) {
    uint16_t clock = title_clock(ticks, (uint8_t)(slot * 23));
    uint8_t age = clock & (RIPPLE_PERIOD - 1);
    if(age >= RIPPLE_TICKS)
        return TITLE_HIDDEN;
    uint16_t pick = ambient_hash((uint16_t)((clock / RIPPLE_PERIOD) * 4 + slot + 1));
    // Six rows of water, 64 columns.
    return title_centered((uint8_t)(TITLE_RIPPLE + (age >= RIPPLE_SPREAD_TICK)),
                          (int16_t)(WATER_LEFT + (pick & 63)),
                          (int16_t)(WATER_TOP + ((((pick >> 6) & 7) * 3) >> 2)));
}

// A fish jumps out of the water every ten seconds or so: it leaves a ring where it rose, arcs
// to the right, and splashes back in.
inline constexpr uint16_t FISH_PERIOD = 512, FISH_START = 200;
inline constexpr uint8_t FISH_JUMP_TICKS = 20, FISH_SPLASH_TICKS = 18, FISH_REACH = 10;

struct TitleFish {
    TitleSprite fish, ring;
};

constexpr TitleFish title_fish(uint16_t ticks) {
    uint16_t clock = (uint16_t)(ticks + FISH_PERIOD - FISH_START);
    uint16_t age = clock & (FISH_PERIOD - 1);
    uint16_t pick = ambient_hash((uint16_t)(clock / FISH_PERIOD + 1));
    // One turn in four it stays down.
    if(age >= FISH_JUMP_TICKS + FISH_SPLASH_TICKS || (pick & 0x3000) == 0)
        return {TITLE_HIDDEN, TITLE_HIDDEN};
    int16_t x = (int16_t)(WATER_LEFT + 2 + (pick & 31) + ((pick >> 5) & 15));
    int16_t water = (int16_t)(WATER_TOP + 2 + ((pick >> 9) & 3));
    if(age < FISH_JUMP_TICKS) {
        uint8_t jump = (uint8_t)age;
        uint8_t pose = jump < 7 ? 0 : jump < 13 ? 1 : 2;
        return {title_centered((uint8_t)(TITLE_FISH + pose), (int16_t)(x + jump / 2),
                               (int16_t)(water - ((jump * (FISH_JUMP_TICKS - jump)) >> 4))),
                jump < 12 ? title_centered((uint8_t)(TITLE_RIPPLE + (jump >= 5)), x, water)
                          : TITLE_HIDDEN};
    }
    uint8_t since = (uint8_t)(age - FISH_JUMP_TICKS);
    return {TITLE_HIDDEN, title_centered((uint8_t)(TITLE_SPLASH + (since >= 7)),
                                         (int16_t)(x + FISH_REACH), water)};
}

// ---- Bank ----

// The rock on the bank, in front of everything that walks there.
inline constexpr TitlePoint TITLE_ROCK_AT{61, 56};

// A bird flies down to the top of the rock, stays a while, and flies on the same way.
struct TitlePerch {
    int16_t x, ground;
    uint16_t start, stay;
    bool from_right;
};

inline constexpr uint8_t BIRD_FLIGHT_TICKS = 64;
inline constexpr TitlePerch TITLE_PERCHES[] = {{70, 56, ROCK_BIRD_START, 288, true},
                                               {70, 56, SECOND_BIRD_START, 224, false}};

constexpr TitleSprite title_bird(TitlePerch const& perch, uint16_t ticks) {
    uint16_t age = title_turn(perch.start, ticks);
    uint8_t flying = (uint8_t)((perch.from_right ? TITLE_BIRD_FLY_LEFT : TITLE_BIRD_FLY_RIGHT) +
                               ((ticks >> 2) & 1));
    if(age < BIRD_FLIGHT_TICKS) {
        int16_t away = BIRD_FLIGHT_TICKS - age;
        return title_standing(flying, (int16_t)(perch.from_right ? perch.x + away : perch.x - away),
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
        return title_standing(flying, (int16_t)(perch.from_right ? perch.x - age : perch.x + age),
                              (int16_t)(perch.ground - age * 3 / 4));
    return TITLE_HIDDEN;
}

// A rabbit hops out from behind the left pine, sits and listens in the open, and hops back.
inline constexpr uint16_t RABBIT_STAY = 320;
inline constexpr uint8_t RABBIT_HOP_TICKS = 12, RABBIT_HOP_AIR = 8, RABBIT_HOP_PIXELS = 6;
inline constexpr uint8_t RABBIT_HOPS = 8;
inline constexpr int16_t RABBIT_X = 52, RABBIT_GROUND = 58;

constexpr TitleSprite title_rabbit(uint16_t ticks) {
    constexpr uint16_t TRAVEL = RABBIT_HOPS * RABBIT_HOP_TICKS;
    uint16_t age = title_turn(RABBIT_START, ticks);
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

// A mouse darts out from behind the right pine, sits up to sniff, and darts back.
inline constexpr uint8_t MOUSE_DART_TICKS = 14, MOUSE_DART_PIXELS = 2, MOUSE_STAY = 90;
inline constexpr int16_t MOUSE_HIDE_X = 114, MOUSE_GROUND = 60;

constexpr TitleSprite title_mouse(uint16_t ticks) {
    constexpr int16_t STOP_X = MOUSE_HIDE_X - MOUSE_DART_TICKS * MOUSE_DART_PIXELS;
    uint16_t age = title_turn(MOUSE_START_TICK, ticks, MOUSE_PERIOD);
    uint8_t pose = (age >> 1) & 1;
    if(age < MOUSE_DART_TICKS)
        return title_standing((uint8_t)(TITLE_MOUSE_LEFT + pose),
                              (int16_t)(MOUSE_HIDE_X - age * MOUSE_DART_PIXELS), MOUSE_GROUND);
    age = (uint16_t)(age - MOUSE_DART_TICKS);
    if(age < MOUSE_STAY)
        return title_standing(TITLE_MOUSE_SIT, STOP_X, MOUSE_GROUND);
    age = (uint16_t)(age - MOUSE_STAY);
    if(age < MOUSE_DART_TICKS)
        return title_standing((uint8_t)(TITLE_MOUSE_RIGHT + pose),
                              (int16_t)(STOP_X + age * MOUSE_DART_PIXELS), MOUSE_GROUND);
    return TITLE_HIDDEN;
}

static_assert(TITLE_CYCLE % TITLE_PROGRAM == 0 && TITLE_CYCLE % GUST_PERIOD == 0 &&
                  TITLE_CYCLE % FISH_PERIOD == 0 && TITLE_CYCLE % 512 == 0 &&
                  TITLE_PROGRAM % MOUSE_PERIOD == 0,
              "every title animation period must divide TITLE_CYCLE");
// No fish is in the air when the counter wraps and its pick changes.
static_assert(FISH_PERIOD - FISH_START >= FISH_JUMP_TICKS + FISH_SPLASH_TICKS);

#endif
