#ifndef NPC_TYPES_H
#define NPC_TYPES_H

#include "npc_ids.h"


// NPC_TRAVEL walks toward the residence and never gives up while visible.
enum NPC_BEHAVIOR : uint8_t { NPC_STAND, NPC_WANDER, NPC_TARGET, NPC_ROUTE, NPC_TRAVEL };

enum NPC_ROUTE_MODE : uint8_t { ROUTE_ONCE, ROUTE_LOOP, ROUTE_REVERSE };

enum NPC_STATE : uint8_t {
    NPC_STATE_NONE = 0,
    NPC_BACKWARD = 1,
    NPC_PASS_PLAYER = 2,
    NPC_PUSHED = 4,
    NPC_WALK_PHASE = 8,
    NPC_TARGET_FAILED = 16,
    NPC_YIELDING = 32,
    NPC_CAN_DETOUR = 64,
    NPC_ROUTE_PENDING = 128
};

constexpr NPC_STATE operator|(NPC_STATE left, NPC_STATE right) {
    return (NPC_STATE)((uint8_t)left | (uint8_t)right);
}

constexpr NPC_STATE operator&(NPC_STATE left, NPC_STATE right) {
    return (NPC_STATE)((uint8_t)left & (uint8_t)right);
}

constexpr NPC_STATE operator^(NPC_STATE left, NPC_STATE right) {
    return (NPC_STATE)((uint8_t)left ^ (uint8_t)right);
}

constexpr NPC_STATE operator~(NPC_STATE value) {
    return (NPC_STATE)(uint8_t)~(uint8_t)value;
}

constexpr NPC_STATE& operator|=(NPC_STATE& left, NPC_STATE right) {
    return left = left | right;
}

constexpr NPC_STATE& operator&=(NPC_STATE& left, NPC_STATE right) {
    return left = left & right;
}

constexpr NPC_STATE& operator^=(NPC_STATE& left, NPC_STATE right) {
    return left = left ^ right;
}

inline constexpr uint8_t NPC_CAPACITY = 8;
// Frame counts at 50 Hz. Stuck counts frames: each blocked step attempt adds the NPC's period.
inline constexpr uint8_t NPC_PUSH_FRAMES = 16, NPC_STUCK_FRAMES = 48;
// A blocked NPC waits NPC_DETOUR_FRAMES, then walks around the obstacle. Each detour sidesteps up
// to NPC_DETOUR_MAX pixels, and a leg (route segment or target) allows NPC_DETOURS_PER_LEG.
inline constexpr uint8_t NPC_DETOUR_FRAMES = 12, NPC_DETOUR_MAX = 15, NPC_DETOURS_PER_LEG = 3;
static_assert(NPC_DETOUR_MAX <= 15 && NPC_DETOURS_PER_LEG <= 3,
              "detour state packs into one byte");
static_assert((NPC_PUSH_FRAMES & (NPC_PUSH_FRAMES - 1)) == 0,
              "push detour mask needs a power of two");

struct Npc {
    Actor actor;

    union {
        PixelPos target;
        uint8_t route;
    };

    NPC_ID id;
    NPC_BEHAVIOR behavior;
    uint8_t timer, waypoint;
    NPC_STATE state;
    uint8_t stuck, push;
    uint8_t detour;   // Sidestep state of a detour around an obstacle (npc.cpp). Keeps the record even-sized.
};


struct NpcPool {
    Npc actors[NPC_CAPACITY];
    // Each NPC_ID's residence: a dynamic NPC's destination, or a static NPC's fixed home. Saved.
    TilePos residence[NPC_ID_LIMIT];
    // Set by scene entry until the next reconcile casts the scene's NPCs.
    uint8_t entering;
    uint8_t activation_deferrals;
    DIRECTION push_direction;
};

static_assert(NPC_ID_LIMIT <= NPC_CAPACITY, "every NPC ID can own a slot, so none is evicted");
static_assert(sizeof(Npc) == 18, "compact live NPC record");
static_assert(sizeof(NpcPool) == 156, "fixed NPC storage, independent of authored count");

#endif
