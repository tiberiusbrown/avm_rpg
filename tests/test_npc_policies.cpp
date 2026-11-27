#include "cast.h"
#include "npc_policy.h"
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
using Policy = AtOneOf<sites::WELL, When<sites::ORCHARD, [] { return first; }>,
                       When<sites::HOME, [] { return second; }>>;
using Absent = AtOneOf<NO_LOCATION, When<sites::HOME, [] { return first; }>>;
static_assert(sizeof(Policy::locations) / sizeof(TilePos) == 3);
static_assert(Policy::locations[1] == sites::ORCHARD);
static_assert(Absent::locations[0] == NO_LOCATION);

int main() {
    CHECK(Policy::current() == sites::WELL);
    second = true;
    CHECK(Policy::current() == sites::HOME);
    first = true;
    CHECK(Policy::current() == sites::ORCHARD);
    CHECK(Absent::current() == sites::HOME);
    first = second = false;
    CHECK(Absent::current() == NO_LOCATION);
    CHECK(AtOneOf<sites::LANE>::current() == sites::LANE);
    std::puts("policy precedence, absence and derived site checks passed");
}
