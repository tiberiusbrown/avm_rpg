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
inline constexpr uint8_t NPC_PUSH_FRAMES = 8, NPC_STUCK_FRAMES = 24;

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
    uint8_t reserved; // Keeps the record even-sized, so host and AVM layouts match.
};

// Inclusive visible tile rectangle. Area 0 means no view has been admitted yet.
struct NpcView {
    uint8_t area, left, right, top, bottom;
    bool operator==(NpcView const&) const = default;
};

struct NpcPool {
    Npc actors[NPC_CAPACITY];
    // Where each dynamic NPC_ID lives while not in a slot. Saved; survives area changes.
    TilePos residence[NPC_ID_LIMIT];
    // Static NPCs requested by chunk handlers this frame, highest priority first.
    NPC_ID requests[NPC_CAPACITY];
    uint8_t request_count;
    // Admissible NPCs without a slot, sorted by residence area, chunk row, and chunk column.
    NPC_ID dormant[NPC_ID_LIMIT];
    uint8_t dormant_count;
    // One bit per NPC_ID: visible but blocked, retried each frame while still in view.
    uint8_t pending;
    NpcView view;
    uint8_t capacity_faults, activation_deferrals;
    DIRECTION push_direction;
    uint8_t reserved; // Keeps the pool even-sized, so host and AVM layouts match.
};

static_assert(NPC_ID_LIMIT <= 8, "pending admissions use one bit per NPC ID");
static_assert(sizeof(Npc) == 18, "compact live NPC record");
static_assert(sizeof(NpcPool) == 176, "fixed NPC storage, independent of authored count");

#endif
