#include "logic/area_village.h"
#include "map_handler_catalog.h"
#include <cstdio>
#include <cstdlib>

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if(!(x)) {                                                                                 \
            std::fprintf(stderr, "%d: %s\n", __LINE__, #x);                                        \
            std::exit(1);                                                                          \
        }                                                                                          \
    } while(0)
static bool first, second;
using Policy = AtOneOf<positions::VillagerHome, When<positions::VillagerEast, [] { return first; }>,
                       When<positions::VillagerWest, [] { return second; }>>;
using Absent = AtOneOf<NO_LOCATION, When<positions::RowanHome, [] { return first; }>>;
static_assert(sizeof(Policy::locations) / sizeof(TilePos) == 3);
static_assert(Policy::locations[1] == positions::VillagerEast);
static_assert(Absent::locations[0] == NO_LOCATION);

int main() {
    CHECK(Policy::current() == positions::VillagerHome);
    second = true;
    CHECK(Policy::current() == positions::VillagerWest);
    first = true;
    CHECK(Policy::current() == positions::VillagerEast);
    CHECK(Absent::current() == positions::RowanHome);
    first = second = false;
    CHECK(Absent::current() == NO_LOCATION);
    CHECK(AtOneOf<positions::WandererHome>::current() == positions::WandererHome);
    std::puts("policy precedence, absence and derived site checks passed");
}
