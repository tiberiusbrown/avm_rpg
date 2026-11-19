#ifndef JOURNAL_H
#define JOURNAL_H

#include "rpg.h"
#include "text_wrap.h"

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

// The Journal's summaries wrap at JOURNAL_TEXT_WIDTH. See text_wrap.
template <typename RangeWidth>
uint8_t journal_wrap(char const PROGMEM* text, RangeWidth range_width, uint8_t* starts,
                     uint8_t capacity) {
    return text_wrap(text, JOURNAL_TEXT_WIDTH, range_width, starts, capacity);
}

#endif
