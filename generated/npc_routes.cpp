/* Generated from TMX/TSX; do not edit. */

#include "npc_routes.h"
static constexpr RoutePoint PROGMEM CourierLoop_POINTS[] = {
    {15, 4},
    {17, 4},
};
static constexpr NpcRoute PROGMEM CourierLoop_ROUTE{CourierLoop_POINTS, 2, ROUTE_LOOP, 2, 1};
static constexpr RoutePoint PROGMEM CourierReverse_POINTS[] = {
    {15, 4},
    {17, 4},
};
static constexpr NpcRoute PROGMEM CourierReverse_ROUTE{CourierReverse_POINTS, 2, ROUTE_REVERSE, 2, 1};
static constexpr RoutePoint PROGMEM CourierWalk_POINTS[] = {
    {15, 4},
    {19, 4},
    {19, 6},
    {23, 6},
};
static constexpr NpcRoute PROGMEM CourierWalk_ROUTE{CourierWalk_POINTS, 4, ROUTE_ONCE, 6, 1};
NpcRoute const PROGMEM* npc_route(uint8_t id) {
    switch(id) {
        case 1: return &CourierLoop_ROUTE;
        case 2: return &CourierReverse_ROUTE;
        case 3: return &CourierWalk_ROUTE;
        default: return nullptr;
    }
}
