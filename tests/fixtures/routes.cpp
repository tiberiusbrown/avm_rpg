// The fixture patrol. It lives apart from the other stand-ins so that a test can define its own
// routes instead.
#include "cast.h"
#include "enemies.h"

NpcRoute const PROGMEM* npc_route(uint8_t id) {
    static constexpr RoutePoint PROGMEM POINTS[] = {{sites::GUARD.x, sites::GUARD.y},
                                                    {sites::GUARD.x + 3, sites::GUARD.y}};
    static constexpr NpcRoute PROGMEM PATROL{POINTS, 2, ROUTE_REVERSE, 16, AREA_INTERIORS};
    return id == FIXTURE_PATROL ? &PATROL : nullptr;
}
