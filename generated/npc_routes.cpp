/* Generated from TMX/TSX; do not edit. */

#include "npc_routes.h"
static constexpr RoutePoint PROGMEM VaultPatrol_POINTS[] = {
    {22, 16},
    {25, 16},
};
static constexpr NpcRoute PROGMEM VaultPatrol_ROUTE{VaultPatrol_POINTS, 2, ROUTE_REVERSE, 16, 4};
NpcRoute const PROGMEM* npc_route(uint8_t id) {
    switch(id) {
        case 1: return &VaultPatrol_ROUTE;
        default: return nullptr;
    }
}
