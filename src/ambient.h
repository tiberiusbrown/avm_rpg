#ifndef AMBIENT_H
#define AMBIENT_H

#include "rpg.h"

// Environment animation. Everything here is a pure function of game.ticks and a world position,
// so it keeps no state: no static RAM, nothing in the save, and a frozen tick shows a frozen
// scene. Positions are world tiles or world pixels, so an effect stays where it is while the
// camera scrolls.
//
// Cost rule: the work that runs every frame is a handful of instructions per visible chunk and
// per bird lane. Everything else runs only for an effect that is on screen.

// ---- Animated tiles ----

// Each phase of the tile cycle lasts this many ticks, which is 160 ms at 50 Hz. The renderer
// selects the tileset's sheet for the phase, so an animated tile costs nothing per tile.
inline constexpr uint8_t TILE_PHASE_TICKS = 8;

constexpr uint8_t tile_phase(uint16_t ticks) {
    return (uint8_t)((ticks / TILE_PHASE_TICKS) & (TILE_PHASES - 1));
}

// ---- Wind gusts ----

// A gust is a diagonal line of bent grass that rolls toward the south-east. Its front is the
// diagonal tx + ty == front (mod GUST_SPACING) and advances one tile every GUST_STEP_TICKS, so
// the pattern repeats every 1024 ticks. A second line follows the first. A line one tile wide
// redraws at most VIEW_SPAN_HEIGHT tiles in a frame.
inline constexpr uint8_t GUST_STEP_TICKS = 8;
inline constexpr uint8_t GUST_SPACING = 128;

struct GustBand {
    uint8_t behind, width;
};

inline constexpr GustBand GUST_BANDS[] = {{0, 1}, {48, 1}};

constexpr uint8_t gust_front(uint16_t ticks) {
    return (uint8_t)((ticks / GUST_STEP_TICKS) & (GUST_SPACING - 1));
}

// How far the diagonal of a tile lies behind the front, from 0 to GUST_SPACING - 1.
constexpr uint8_t gust_distance(uint8_t front, uint8_t tx, uint8_t ty) {
    return (uint8_t)((front - tx - ty) & (GUST_SPACING - 1));
}

constexpr bool gust_bends(uint8_t front, uint8_t tx, uint8_t ty) {
    uint8_t distance = gust_distance(front, tx, ty);
    for(GustBand band : GUST_BANDS)
        if((uint8_t)(distance - band.behind) < band.width)
            return true;
    return false;
}

// True when a gust line crosses a view whose top-left tile lies `origin` behind the front.
// The renderer asks this before it reads anything else.
constexpr bool gust_in_view(uint8_t origin) {
    for(GustBand band : GUST_BANDS)
        if((uint8_t)(origin - band.behind) < band.width + VIEW_SPAN_WIDTH + VIEW_SPAN_HEIGHT - 2)
            return true;
    return false;
}

// ---- Shared helpers ----

// One xorshift round, as rng_next uses. Zero maps to zero.
constexpr uint16_t ambient_hash(uint16_t x) {
    x ^= (uint16_t)(x << 7);
    x ^= (uint16_t)(x >> 9);
    x ^= (uint16_t)(x << 8);
    return x;
}

// A sine-shaped wave over one turn of 256 steps, from -64 to 64: two parabolic arcs, so it
// eases through its turning points as a flying thing does. It is zero at 0 and at 128.
constexpr int8_t ambient_sine(uint8_t turn) {
    uint8_t x = turn & 127;
    int8_t arc = (int8_t)((uint16_t)(x * (128 - x)) >> 6);
    return turn & 128 ? (int8_t)-arc : arc;
}

// Wings go wide, half, closed, half.
constexpr uint8_t ambient_flap(uint8_t step) {
    step &= 3;
    return step == 3 ? 1 : step;
}

// ---- Creatures and effects on tiles ----

// Frames of ambient16.png. The generated sheet asserts the count.
enum AMBIENT_FRAME : uint8_t {
    FRAME_BUTTERFLY = 0, // wide, half, closed
    FRAME_BIRD = 3,      // wings up, level, down
    FRAME_LEAF = 6,      // two tumbling poses
    FRAME_FISH = 8,      // six frames of one jump
    FRAME_RIPPLE = 14,   // three growing rings
    FRAME_MOTH = 17,     // wings up, down
    FRAME_GLINT = 19,    // small, large
    FRAME_RABBIT = 21,   // ears, head, head turned
    FRAME_BEE = 24,      // wings up, down
    FRAME_PERCHED = 26,  // standing, pecking
    FRAME_STEAM = 28,    // puff, cloud, wisp
    FRAME_DUST = 31,     // speck, twinkle
    FRAME_FAR_BIRD = 33, // far away, nearer
    AMBIENT_FRAME_COUNT = 35
};

// Every 8x4 chunk has one slot. Once per epoch the slot picks one tile of its chunk, and the
// class of that tile decides what appears there, if anything.
inline constexpr uint8_t AMBIENT_EPOCH_TICKS = 128;

// A chunk's seed. The renderer steps it from chunk to chunk by adding these constants.
inline constexpr uint16_t AMBIENT_SEED = 0x5BD1, AMBIENT_SEED_X = 0x9E37, AMBIENT_SEED_Y = 0x6B4D;

constexpr uint16_t ambient_seed(uint8_t chunk_x, uint8_t chunk_y) {
    return (uint16_t)(chunk_x * AMBIENT_SEED_X + chunk_y * AMBIENT_SEED_Y + AMBIENT_SEED);
}

// The slot's pick for the epoch that holds `ticks`: the tile in bits 0 to 4, and all sixteen
// bits for the roll and the variety. One add, one shift, one multiply and a fold: this much runs
// for every chunk in view on every frame. Each chunk starts its epochs at its own time.
constexpr uint16_t ambient_pick(uint16_t seed, uint16_t ticks) {
    uint16_t pick =
        (uint16_t)((uint16_t)(seed ^ ((uint16_t)(ticks + seed) / AMBIENT_EPOCH_TICKS)) * 0x6D2Bu);
    return (uint16_t)(pick ^ (pick >> 8));
}

struct AmbientSlot {
    uint8_t x, y; // The tile, within its chunk.
    uint8_t roll; // Uniform chance for this epoch.
    uint8_t vary; // More bits of the pick, for variety between instances.
    uint8_t age;  // Ticks into the epoch, below AMBIENT_EPOCH_TICKS.
};

constexpr AmbientSlot ambient_slot_of(uint16_t seed, uint16_t pick, uint16_t ticks) {
    return {(uint8_t)(pick & (CHUNK_WIDTH - 1)), (uint8_t)((pick >> 3) & (CHUNK_HEIGHT - 1)),
            (uint8_t)(pick >> 8), (uint8_t)pick,
            (uint8_t)((uint16_t)(ticks + seed) & (AMBIENT_EPOCH_TICKS - 1))};
}

constexpr AmbientSlot ambient_slot(uint8_t chunk_x, uint8_t chunk_y, uint16_t ticks) {
    uint16_t seed = ambient_seed(chunk_x, chunk_y);
    return ambient_slot_of(seed, ambient_pick(seed, ticks), ticks);
}

struct AmbientSprite {
    bool visible;
    uint8_t frame;
    int8_t dx, dy; // Pixels from the tile's top-left corner to the sprite's.
};

inline constexpr AmbientSprite AMBIENT_HIDDEN{false, 0, 0, 0};

// A wandering flight that leaves the tile and returns to it. It is the sum of two motions. The
// wander is a pair of slow waves with their own speeds and starting points, both taken from the
// slot, so every instance has its own path. The orbit is a small quick circle around the
// wandering point: the flutter of a butterfly, the spiral of a moth, the buzz of a bee. An
// envelope scales both from nothing at the ends of the epoch, up to `reach` pixels of wander
// across and half that up and down, so the flight starts and ends on the tile whatever the
// waves are doing.
struct AmbientFlight {
    uint8_t frame, reach, speed_x, speed_y;
    uint8_t orbit, orbit_speed; // Radius in pixels, and turn steps per tick.
};

constexpr AmbientSprite ambient_flight(AmbientSlot slot, AmbientFlight flight) {
    uint8_t edge = slot.age < AMBIENT_EPOCH_TICKS / 2
                       ? slot.age
                       : (uint8_t)(AMBIENT_EPOCH_TICKS - 1 - slot.age);
    uint8_t reach = edge < flight.reach ? edge : flight.reach;
    // The orbit opens up over the first eight ticks and closes over the last eight.
    uint8_t radius = (uint8_t)(flight.orbit * (edge < 8 ? edge : 8));
    uint8_t turn = (uint8_t)(slot.age * flight.orbit_speed + slot.vary);
    int8_t across = ambient_sine((uint8_t)(slot.age * flight.speed_x + slot.vary * 3));
    int8_t along = ambient_sine((uint8_t)(slot.age * flight.speed_y + slot.roll * 5));
    return {true, flight.frame,
            (int8_t)((int16_t)(across * reach) / 64 + (int16_t)(ambient_sine(turn) * radius) / 512),
            (int8_t)((int16_t)(along * reach) / 128 +
                     (int16_t)(ambient_sine((uint8_t)(turn + 64)) * radius) / 512)};
}

// How long a flier rests on its tile at each end of its flight.
inline constexpr uint8_t FLIGHT_REST_TICKS = 5;

constexpr bool ambient_resting(uint8_t age) {
    return age < FLIGHT_REST_TICKS || age >= AMBIENT_EPOCH_TICKS - FLIGHT_REST_TICKS;
}

// Chances out of 256, cumulative within a class: a slot's roll below the first value shows the
// first effect, below the second value the second effect, and so on.
inline constexpr uint8_t GRASS_BUTTERFLY = 96, GRASS_RABBIT = 136;
inline constexpr uint8_t FLOWERS_BUTTERFLY = 144, FLOWERS_BEE = 232;
inline constexpr uint8_t WATER_FISH = 44, WATER_RIPPLE = 110, WATER_GLINT = 190;
inline constexpr uint8_t TREE_LEAF = 96, TREE_BIRD = 150;
inline constexpr uint8_t FLOOR_MOTH = 80, FLOOR_DUST = 150;
inline constexpr uint8_t PERCH_BIRD = 120, ROAD_LEAF = 56, STEAM_PUFF = 210;

inline constexpr uint8_t FISH_FRAMES = 6, FISH_FRAME_TICKS = 8;
inline constexpr uint8_t RIPPLE_FRAMES = 3, RIPPLE_FRAME_TICKS = 12;
inline constexpr uint8_t GLINT_TICKS = 40, RABBIT_TICKS = 96, LEAF_TICKS = 64;
inline constexpr uint8_t SKITTER_TICKS = 96, DUST_TICKS = 96, STEAM_TICKS = 60;
// A visiting bird flies in for this long, perches, and takes as long to leave.
inline constexpr uint8_t VISIT_FLIGHT_TICKS = 36;
// Where a perched bird stands: its feet are on the top of a rock or a sign.
inline constexpr int8_t PERCH_DY = -12;

// A butterfly holds its wings open and glides for one beat in four, at its own time.
constexpr uint8_t butterfly_frame(AmbientSlot slot, uint16_t ticks) {
    if(ambient_resting(slot.age))
        return FRAME_BUTTERFLY + 2;
    if(((slot.age >> 3) & 3) == (slot.vary & 3))
        return FRAME_BUTTERFLY;
    return (uint8_t)(FRAME_BUTTERFLY + ambient_flap((uint8_t)(ticks >> 2)));
}

// A bird comes down out of the sky to a perch, stands and pecks, and leaves the other way. Far
// away it is a speck, so it arrives and leaves without popping.
constexpr AmbientSprite ambient_visit(AmbientSlot slot, uint16_t ticks) {
    uint8_t age = slot.age;
    bool leaving = age >= AMBIENT_EPOCH_TICKS - VISIT_FLIGHT_TICKS;
    if(age >= VISIT_FLIGHT_TICKS && !leaving)
        return {true, (uint8_t)(FRAME_PERCHED + (((age >> 3) & 3) == 3)), 0, PERCH_DY};
    // Distance left to the perch, from VISIT_FLIGHT_TICKS down to 1, or back up when leaving.
    uint8_t away = leaving ? (uint8_t)(age - (AMBIENT_EPOCH_TICKS - VISIT_FLIGHT_TICKS - 1))
                           : (uint8_t)(VISIT_FLIGHT_TICKS - age);
    uint8_t frame = away > 24   ? FRAME_FAR_BIRD
                    : away > 12 ? FRAME_FAR_BIRD + 1
                                : (uint8_t)(FRAME_BIRD + ambient_flap((uint8_t)(ticks >> 2)));
    // It arrives from one side and leaves by the other. The path is a swoop: steep far away, and
    // level as it comes in to land.
    bool west = ((slot.vary & 1) != 0) != leaving;
    int8_t across = (int8_t)((away * 3) / 4);
    return {true, frame, (int8_t)(west ? -across : across),
            (int8_t)(PERCH_DY - (uint8_t)((uint16_t)(away * away) / 48))};
}

// What a slot shows on a tile of the given class. A flight starts and ends on its tile. The
// sprite may pass over neighboring tiles of any kind, as a flying thing does.
#ifdef RPG_AVM
// Kept inline in the one function that draws an effect, so it adds no frame of its own.
__attribute__((always_inline))
#endif
constexpr AmbientSprite
ambient_sprite(uint8_t ambient_class, AmbientSlot slot, uint16_t ticks) {
    uint8_t age = slot.age, roll = slot.roll, vary = slot.vary;
    // The classes with a wandering flier describe it here, and one copy of the flight runs below.
    AmbientFlight flight{};
    switch(ambient_class) {
        case AMBIENT_GRASS:
        case AMBIENT_FLOWERS:
            if(roll < (ambient_class == AMBIENT_GRASS ? GRASS_BUTTERFLY : FLOWERS_BUTTERFLY)) {
                // A butterfly drifts in wide, slow, uneven loops.
                flight = {butterfly_frame(slot, ticks),     20, (uint8_t)(2 + (vary >> 7)),
                          (uint8_t)(5 + ((vary >> 4) & 3)), 2,  13};
            } else if(ambient_class == AMBIENT_FLOWERS) {
                // A bee works the blossoms in small quick loops.
                if(roll >= FLOWERS_BEE)
                    return AMBIENT_HIDDEN;
                // It hangs by one blossom, shivering, and drifts to the next.
                flight = {(uint8_t)(FRAME_BEE + ((ticks >> 1) & 1)), 9, (uint8_t)(2 + (vary >> 7)),
                          (uint8_t)(3 + ((vary >> 4) & 1)),          2, 29};
            } else {
                // A rabbit sits up out of the grass, looks around and ducks down again.
                if(roll >= GRASS_RABBIT || age >= RABBIT_TICKS)
                    return AMBIENT_HIDDEN;
                uint8_t stage = age >> 3;
                uint8_t frame = stage == 0 || stage == RABBIT_TICKS / 8 - 1 ? 0
                                : ((stage - 1) >> 1) & 1                    ? 2
                                                                            : 1;
                return {true, (uint8_t)(FRAME_RABBIT + frame), 0, 0};
            }
            break;
        case AMBIENT_WATER: {
            // Rings and glints sit a little off the middle of the tile. A fish stays on it.
            int8_t dx = (int8_t)(((vary >> 3) & 3) - 2), dy = (int8_t)(((vary >> 5) & 3) - 2);
            if(roll < WATER_FISH) {
                if(age < FISH_FRAMES * FISH_FRAME_TICKS)
                    return {true, (uint8_t)(FRAME_FISH + age / FISH_FRAME_TICKS), 0, 0};
            } else if(roll < WATER_RIPPLE) {
                if(age < RIPPLE_FRAMES * RIPPLE_FRAME_TICKS)
                    return {true, (uint8_t)(FRAME_RIPPLE + age / RIPPLE_FRAME_TICKS), dx, dy};
            } else if(roll < WATER_GLINT) {
                if(age < GLINT_TICKS)
                    return {true, (uint8_t)(FRAME_GLINT + ((age >> 3) & 1)), (int8_t)(dx * 2),
                            (int8_t)(dy * 2)};
            }
            return AMBIENT_HIDDEN;
        }
        case AMBIENT_TREE:
            // A leaf drops from the crown to the foot of the tile, swaying.
            if(roll < TREE_LEAF) {
                if(age >= LEAF_TICKS)
                    return AMBIENT_HIDDEN;
                return {true, (uint8_t)(FRAME_LEAF + ((age >> 3) & 1)),
                        (int8_t)(ambient_sine((uint8_t)(age * 6 + vary)) / 12),
                        (int8_t)(age / 4 - 6)};
            }
            if(roll >= TREE_BIRD)
                return AMBIENT_HIDDEN;
            // A bird leaves the crown, swings wide around the tree and comes back to it.
            flight = {ambient_resting(age)
                          ? (uint8_t)FRAME_PERCHED
                          : (uint8_t)(FRAME_BIRD + ambient_flap((uint8_t)(ticks >> 2))),
                      28,
                      (uint8_t)(2 + (vary >> 7)),
                      (uint8_t)(5 + ((vary >> 5) & 1)),
                      0,
                      0};
            break;
        case AMBIENT_FLOOR:
            if(roll < FLOOR_MOTH) {
                // It spirals: quick circles around a point that strays slowly about the room.
                flight = {(uint8_t)(FRAME_MOTH + (ambient_resting(age) ? 1 : (ticks >> 1) & 1)),
                          12,
                          (uint8_t)(2 + (vary >> 7)),
                          (uint8_t)(3 + ((vary >> 4) & 3)),
                          5,
                          11};
                break;
            }
            // A speck of dust drifts up through the light and twinkles.
            if(roll < FLOOR_DUST && age < DUST_TICKS)
                return {true, (uint8_t)(FRAME_DUST + (((age >> 3) & 3) == 2)),
                        (int8_t)((vary >> 5) + age / 16), (int8_t)(6 - age / 8)};
            return AMBIENT_HIDDEN;
        case AMBIENT_PERCH:
            if(roll < PERCH_BIRD)
                return ambient_visit(slot, ticks);
            return AMBIENT_HIDDEN;
        case AMBIENT_ROAD:
            // A dry leaf skips along the paving on the wind.
            if(roll < ROAD_LEAF && age < SKITTER_TICKS) {
                int8_t hop = ambient_sine((uint8_t)((age * 8) & 127));
                return {true, (uint8_t)(FRAME_LEAF + ((age >> 2) & 1)), (int8_t)(age / 2 - 16),
                        (int8_t)(4 - hop / 16)};
            }
            return AMBIENT_HIDDEN;
        case AMBIENT_STEAM:
            // A puff of steam rises and thins out.
            if(roll < STEAM_PUFF && age < STEAM_TICKS)
                return {true, (uint8_t)(FRAME_STEAM + age / (STEAM_TICKS / 3)),
                        (int8_t)(ambient_sine((uint8_t)(age * 4)) / 32), (int8_t)(-6 - age / 6)};
            return AMBIENT_HIDDEN;
        default:
            return AMBIENT_HIDDEN;
    }
    return ambient_flight(slot, flight);
}

// ---- Birds ----

// Birds cross the sky in lanes. A lane is a band BIRD_BAND pixels high at the top of every
// BIRD_LANE pixels of world height, so a view 64 pixels high never shows two lanes and the
// renderer tests one lane per frame. A lane holds a train of places BIRD_SPACING pixels apart that
// moves east or west, one or two pixels per tick by lane. A quarter of the places are empty.
// A place holds a leader and up to two followers.
inline constexpr uint16_t BIRD_SPACING = 512;
inline constexpr uint8_t BIRD_LANE_SHIFT = 7;
inline constexpr uint8_t BIRD_BAND_TOP = 13, BIRD_BAND = 16;
inline constexpr int8_t BIRD_TRAIL_X = 9, BIRD_TRAIL_Y = 5;
// A follower repeats its leader's rise and fall this many clock steps later.
inline constexpr uint8_t BIRD_TRAIL_CLOCK = 24;
// A bird flaps and climbs for this part of each turn of its clock, then glides down.
inline constexpr uint8_t BIRD_FLAP_CLOCK = 160;
// How far a bird rises and falls around its height.
inline constexpr int8_t BIRD_SWING = 8;

// The one lane whose birds can show in a view that starts at this world y. A bird's sprite, with
// its swing and a follower's offset, stays within the 64 pixels below the lane's top.
constexpr uint8_t bird_lane(uint16_t camera_y) {
    return (uint8_t)((uint16_t)(camera_y + 64) >> BIRD_LANE_SHIFT);
}

static_assert(BIRD_BAND_TOP - BIRD_SWING - BIRD_TRAIL_Y >= 0 &&
                  BIRD_BAND_TOP + BIRD_BAND - 1 + BIRD_SWING + BIRD_TRAIL_Y + TILE <= 64,
              "a lane's birds fit in the 64 pixels that bird_lane() assumes");

struct AmbientBird {
    bool visible;
    int16_t x;         // Screen x of the leader's sprite.
    uint16_t y;        // World y of the height that the birds rise and fall around.
    uint8_t clock;     // Where the leader is in its flap and glide turn, 128 ticks long.
    int8_t trail;      // Screen x step from each bird to the one behind it.
    uint8_t followers; // 0 to 2.
};

struct BirdPose {
    int8_t dy;
    uint8_t frame;
};

// A bird beats its wings and climbs, then holds them level and sinks: one smooth rise and fall
// per turn of the clock.
inline constexpr auto BIRD_POSES PROGMEM = [] {
    struct Poses {
        uint16_t value[256];
    } poses{};
    for(uint16_t clock = 0; clock < 256; ++clock) {
        uint8_t dy = (uint8_t)(ambient_sine((uint8_t)(clock + 64)) / (64 / BIRD_SWING));
        uint8_t frame = (uint8_t)(FRAME_BIRD +
            (clock < BIRD_FLAP_CLOCK ? ambient_flap((uint8_t)(clock >> 3)) : 1));
        poses.value[clock] = (uint16_t)(dy | (uint16_t)frame << 8);
    }
    return poses;
}();

constexpr BirdPose ambient_bird_pose(uint8_t clock) {
    uint16_t pose = BIRD_POSES.value[clock];
    return {(int8_t)pose, (uint8_t)(pose >> 8)};
}

constexpr AmbientBird ambient_bird(uint8_t lane, uint16_t ticks, uint16_t camera_x) {
    // The test for a bird in view is one multiply and a few adds. The hash runs only then.
    uint16_t seed = (uint16_t)(lane * 0x3D5Bu + 0xB1D5u);
    bool east = (seed & 1) != 0;
    uint16_t travel = seed & 0x400 ? (uint16_t)(ticks * 2) : ticks;
    // The train's world x. 65536 is a multiple of the spacing, so the tick wrap is seamless.
    uint16_t train = (uint16_t)(east ? seed + travel : seed - travel);
    uint16_t offset = (uint16_t)(train - camera_x + TILE);
    int16_t x = (int16_t)(offset % BIRD_SPACING) - TILE;
    if(x >= 128 + TILE)
        return {false, 0, 0, 0, 0, 0};
    uint16_t bird = ambient_hash((uint16_t)(seed + (offset / BIRD_SPACING) * 0x45u + 1));
    if((bird & 3) == 0)
        return {false, 0, 0, 0, 0, 0};
    uint8_t flock = (uint8_t)((bird >> 2) & 3);
    return {true,
            x,
            (uint16_t)(((uint16_t)lane << BIRD_LANE_SHIFT) + BIRD_BAND_TOP +
                       ((bird >> 8) & (BIRD_BAND - 1))),
            (uint8_t)(ticks * 2 + (bird >> 3)),
            (int8_t)(east ? -BIRD_TRAIL_X : BIRD_TRAIL_X),
            (uint8_t)(flock ? flock - 1 : 0)};
}

#endif
