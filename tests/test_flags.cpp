#include "flags.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>

template <uint16_t Count> void exercise() {
    FlagSet<Count> flags{};
    static_assert(sizeof(flags) == (Count / 8 + (Count % 8 != 0)), "exact bitset size");
    for(uint16_t i = 0; i < Count; ++i) {
        assert(!flags.get_flag(i));
        flags.set_flag(i);
        assert(flags.get_flag(i));
    }
    for(uint16_t i = 0; i < Count; ++i) {
        flags.toggle_flag(i);
        assert(!flags.get_flag(i));
        flags.toggle_flag(i);
        flags.clear_flag(i);
        assert(!flags.get_flag(i));
    }
    flags.set_flag(Count);
    flags.toggle_flag(65535);
    flags.clear_flag(Count);
    assert(!flags.get_flag(Count) && !flags.get_flag(65535));
    for(uint8_t byte : flags.bytes)
        assert(byte == 0);
}

int main() {
    exercise<4>();
    exercise<16>();
    exercise<37>();
    exercise<257>();
    puts("PASS flag bitsets: exact storage, every bit, byte boundaries and invalid indices");
}
