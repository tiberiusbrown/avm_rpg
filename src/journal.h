#ifndef JOURNAL_H
#define JOURNAL_H

#include "rpg.h"

// The Journal is one full-width panel. Its list has JOURNAL_ROWS entries, and its detail view shows
// JOURNAL_DETAIL_LINES lines of a summary that was wrapped once when the quest opened.
inline constexpr uint8_t JOURNAL_WIDTH = 128;
inline constexpr uint8_t JOURNAL_ROWS = 4;
inline constexpr uint8_t JOURNAL_DETAIL_LINES = 4;
inline constexpr uint8_t JOURNAL_TEXT_WIDTH = 110;
// Summaries longer than this many lines are truncated. Native tests check the shipped summaries.
inline constexpr uint8_t JOURNAL_SUMMARY_LINES = 10;

struct JournalView {
    uint8_t selection; // highlighted row among listed quests
    QUEST_ID open;     // QUEST_NONE while the list is shown
    uint8_t scroll;    // first visible summary line
    uint8_t line_count;
    // Panel-local positions of text whose width is measured when the view changes, not per frame.
    uint8_t status_x; // the status value
    uint8_t track_x;  // the tracking hint, right-aligned
    uint8_t goal_x;   // the goal's text
    uint8_t starts[JOURNAL_SUMMARY_LINES + 1];
};

// Greedy word wrap of a string in program memory. Nothing is copied: line k is
// text[starts[k], starts[k + 1]), with its trailing spaces excluded, and starts[count] marks the end
// of the text. `range_width(text, from, to)` returns the width of text[from, to) as it will be drawn.
// It must hold no state, and the caller measures in short chunks, as the dialogue does. Returns the
// number of lines.
template <typename RangeWidth>
uint8_t journal_wrap(char const PROGMEM* text, RangeWidth range_width, uint8_t* starts,
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
            if(range_width(text, start, word) > JOURNAL_TEXT_WIDTH)
                break;
            end = probe = word;
        }
        if(end == start) {
            // One word wider than the line is broken between characters, so the text is kept.
            end = start + 1;
            while(text[end] && text[end] != ' ' &&
                  range_width(text, start, end + 1) <= JOURNAL_TEXT_WIDTH)
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
