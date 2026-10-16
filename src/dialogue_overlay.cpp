#include "rpg.h"

#include <avm.h>

static constexpr uint8_t LEFT = 6, RIGHT = 122, FIRST_LINE = 39, LAST_LINE = 53;

static uint8_t dialogue_x, dialogue_y;

// Exposed for debugger/device QA; no dialogue state lives in Game or its save.
volatile uint8_t dialogue_prompt;

static bool separator(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static void clear_overlay() {
    avm_draw_filled_rect_black(2, 30, 124, 34);
    avm_draw_filled_rect_white(2, 30, 124, 1);
    dialogue_x = LEFT;
    dialogue_y = FIRST_LINE;
}

static void open_overlay(Game const& g) {
    avm_set_text_font(AVM_FONT_BR5D);
    avm_set_text_mode(AVM_TEXT_WHITE_TRANSPARENT);
    avm_draw_filled_rect_black(0, 0, 128, 64);
    rpg_render_world(g);
    clear_overlay();
}

static void close_overlay(Game const& g) {
    avm_draw_filled_rect_black(0, 0, 128, 64);
    rpg_render_world(g);
    avm_display(true);
}

static bool wait_for_input(uint8_t prompt) {
    avm_draw_text_P(LEFT, 62,
                    prompt == DIALOGUE_MORE     ? F("A more / B leave")
                    : prompt == DIALOGUE_CHOICE ? F("A yes / B no")
                                                : F("A / B close"));
    avm_display(false);
    dialogue_prompt = prompt;

    uint8_t pressed = input_wait_press(BTN_A | BTN_B);
    dialogue_prompt = DIALOGUE_CLOSED;
    return !(pressed & BTN_B);
}

static bool next_line() {
    if(dialogue_y + AVM_FONT_BR5D->line_height <= LAST_LINE) {
        dialogue_x = LEFT;
        dialogue_y += AVM_FONT_BR5D->line_height;
        return true;
    }

    if(!wait_for_input(DIALOGUE_MORE))
        return false;
    clear_overlay();
    return true;
}

// Scratch belongs to shallow helpers and is gone before paging/input/redrawing.
// Measure with the actual runtime font; do not duplicate or cache glyph metrics.
__attribute__((noinline)) static uint8_t word_width(char const PROGMEM* word) {
    char chunk[5];
    uint16_t width = 0;

    while(*word && !separator(*word)) {
        uint8_t count = 0;
        while(count < 4 && *word && !separator(*word))
            chunk[count++] = *word++;
        chunk[count] = 0;
        width += avm_text_width(chunk);
        if(width > RIGHT - LEFT)
            return RIGHT - LEFT + 1;
    }

    return (uint8_t)width;
}

__attribute__((noinline)) static uint8_t draw_chunk(char const PROGMEM* word) {
    char chunk[5];
    uint8_t count = 0;

    while(count < 4 && word[count] && !separator(word[count])) {
        chunk[count] = word[count];
        ++count;
    }
    chunk[count] = 0;

    // Split an oversized word at glyph boundaries instead of clipping it.
    while(count && dialogue_x + avm_text_width(chunk) > RIGHT)
        chunk[--count] = 0;
    if(count)
        dialogue_x = (uint8_t)avm_draw_text(dialogue_x, dialogue_y, chunk).x;
    return count;
}

static bool stream_text(char const PROGMEM* words) {
    while(*words) {
        if(separator(*words)) {
            char c = *words++;
            if(c == '\n') {
                char const PROGMEM* next = words;
                while(separator(*next))
                    ++next;
                if(!*next)
                    return true;
                if(!next_line())
                    return false;
            }
            continue;
        }

        if(dialogue_x != LEFT) {
            uint8_t space = (uint8_t)avm_text_width_P(F(" "));
            if(dialogue_x + space + word_width(words) > RIGHT) {
                if(!next_line())
                    return false;
            } else {
                dialogue_x += space;
            }
        }

        while(*words && !separator(*words)) {
            uint8_t count = draw_chunk(words);
            if(!count && !next_line())
                return false;
            words += count;
        }
    }

    return true;
}

bool dialogue_show(Game& g, char const PROGMEM* words) {
    if(!words)
        return true;
    while(separator(*words))
        ++words;
    if(!*words)
        return true;

    open_overlay(g);
    bool accepted = stream_text(words) && wait_for_input(DIALOGUE_DONE);
    close_overlay(g);
    return accepted;
}

bool dialogue_ask(Game& g, char const PROGMEM* question) {
    open_overlay(g);
    bool accepted = (!question || stream_text(question)) && wait_for_input(DIALOGUE_CHOICE);
    close_overlay(g);
    return accepted;
}
