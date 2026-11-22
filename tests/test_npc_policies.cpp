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
using Policy = AtOneOf<positions::TessaWell, When<positions::TessaOrchard, [] { return first; }>,
                       When<positions::RowanHome, [] { return second; }>>;
using Absent = AtOneOf<NO_LOCATION, When<positions::RowanHome, [] { return first; }>>;
static_assert(sizeof(Policy::locations) / sizeof(TilePos) == 3);
static_assert(Policy::locations[1] == positions::TessaOrchard);
static_assert(Absent::locations[0] == NO_LOCATION);

int main() {
    CHECK(Policy::current() == positions::TessaWell);
    second = true;
    CHECK(Policy::current() == positions::RowanHome);
    first = true;
    CHECK(Policy::current() == positions::TessaOrchard);
    CHECK(Absent::current() == positions::RowanHome);
    first = second = false;
    CHECK(Absent::current() == NO_LOCATION);
    CHECK(AtOneOf<positions::OrrinLane>::current() == positions::OrrinLane);
    std::puts("policy precedence, absence and derived site checks passed");
}
