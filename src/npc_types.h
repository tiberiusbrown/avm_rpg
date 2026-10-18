#ifndef NPC_TYPES_H
#define NPC_TYPES_H

// Explicit identities are save/quest contracts, never slot or authoring indices.
enum NPC_ID : uint8_t { NPC_NONE = 0, NPC_VILLAGER = 1, NPC_WANDERER = 2 };

enum NPC_BEHAVIOR : uint8_t { NPC_STAND, NPC_WANDER, NPC_TARGET, NPC_ROUTE };

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
inline constexpr uint8_t NPC_PLACEMENT_ABSENT = 255;
inline constexpr uint8_t NPC_PUSH_FRAMES = 8, NPC_STUCK_FRAMES = 24;

struct Npc {
    Actor actor;

    union {
        PixelPos target;
        uint8_t route;
    };

    NPC_ID id;
    uint8_t placement;
    NPC_BEHAVIOR behavior;
    uint8_t timer, waypoint;
    NPC_STATE state;
    uint8_t stuck, push;
};

struct NpcPool {
    Npc actors[NPC_CAPACITY];
    NPC_ID requests[NPC_CAPACITY];
    uint8_t request_count, capacity_faults;
    DIRECTION push_direction;
    uint8_t activation_deferrals;
};

static_assert(sizeof(Npc) == 18, "compact live NPC record");
static_assert(sizeof(NpcPool) == 156, "fixed NPC storage, independent of authored count");

#endif
