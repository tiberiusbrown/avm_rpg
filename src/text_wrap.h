#ifndef TEXT_WRAP_H
#define TEXT_WRAP_H

#include "rpg.h"

// Greedy word wrap of a string in program memory. Nothing is copied: line k is
// text[starts[k], starts[k + 1]), with its trailing spaces excluded, and starts[count] marks the
// end of the text. A line is at most `width` pixels as drawn. `range_width(text, from, to)` returns
// the width of text[from, to) as it will be drawn. It must hold no state, and the caller measures
// in short chunks, as the dialogue does. Returns the number of lines, which is at most `capacity`;
// longer text is truncated.
template <typename RangeWidth>
uint8_t text_wrap(char const PROGMEM* text, uint16_t width, RangeWidth range_width, uint8_t* starts,
                  uint8_t capacity) {
    uint8_t count = 0, start = 0;
    while(text[start] == ' ')
        ++start;
    while(text[start] && count < capacity) {
        starts[count++] = start;
        uint8_t end = start, probe = start;
        for(;;) {
            uint8_t word = probe;
            while(text[word] == ' ')
                ++word;
            if(!text[word])
                break;
            while(text[word] && text[word] != ' ')
                ++word;
            if(range_width(text, start, word) > width)
                break;
            end = probe = word;
        }
        if(end == start) {
            // One word wider than the line is broken between characters, so the text is kept.
            end = start + 1;
            while(text[end] && text[end] != ' ' && range_width(text, start, end + 1) <= width)
                ++end;
        }
        start = end;
        while(text[start] == ' ')
            ++start;
    }
    starts[count] = start;
    return count;
}

#endif
