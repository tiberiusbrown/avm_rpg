#include "rpg.h"

#include <avm.h>

// Overlay geometry. Changing the visible line count recomputes every position below.
static constexpr uint8_t DISPLAY_WIDTH = 128, DISPLAY_HEIGHT = 64;
static constexpr uint8_t DIALOGUE_VISIBLE_LINES = 3;
static constexpr uint8_t BORDER = 1, PADDING = 1, GAP_ABOVE = 1;

// AVM_FONT_BR5D: seven-pixel line spacing, five pixels above the baseline, and
// descenders that reach the baseline row.
static constexpr uint8_t LINE_HEIGHT = 7, ASCENT = 5;
static constexpr uint8_t PROMPT_GAP = 1, PROMPT_ROWS = ASCENT + 1;

static constexpr uint8_t OVERLAY_HEIGHT =
    2 * BORDER + 2 * PADDING + DIALOGUE_VISIBLE_LINES * LINE_HEIGHT + PROMPT_GAP + PROMPT_ROWS;
static constexpr uint8_t OVERLAY_TOP = DISPLAY_HEIGHT - OVERLAY_HEIGHT;

static constexpr uint8_t LEFT = BORDER + PADDING;
static constexpr uint8_t RIGHT = DISPLAY_WIDTH - BORDER - PADDING;
static constexpr uint8_t FIRST_LINE = OVERLAY_TOP + BORDER + PADDING + ASCENT;
static constexpr uint8_t LAST_LINE = FIRST_LINE + (DIALOGUE_VISIBLE_LINES - 1) * LINE_HEIGHT;
static constexpr uint8_t PROMPT_BASELINE = DISPLAY_HEIGHT - BORDER - PADDING - 1;

// Frames run at 30 Hz. Each letter appears on its own frame and blips for that frame.
// A prompt ignores A for half a second so a press that finished typing cannot advance.
static constexpr uint8_t ADVANCE_GUARD_FRAMES = 15;
static constexpr uint8_t BLIP_AMPLITUDE = 18;
static uint8_t const PROGMEM BLIP_NOTES[8] = {67, 69, 71, 74, 76, 79, 81, 83};

static uint8_t dialogue_x, dialogue_y;
static bool dialogue_skip;

// Exposed for debugger/device QA; no dialogue state lives in Game or its save.
volatile DIALOGUE_PROMPT dialogue_prompt;

static bool separator(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static void clear_overlay() {
    avm_draw_filled_rect_black(0, OVERLAY_TOP - GAP_ABOVE, DISPLAY_WIDTH, GAP_ABOVE);
    avm_draw_filled_rect_white(0, OVERLAY_TOP, DISPLAY_WIDTH, OVERLAY_HEIGHT);
    avm_draw_filled_rect_black(BORDER, OVERLAY_TOP + BORDER, DISPLAY_WIDTH - 2 * BORDER,
                               OVERLAY_HEIGHT - 2 * BORDER);
    dialogue_x = LEFT;
    dialogue_y = FIRST_LINE;
    dialogue_skip = false;
    dialogue_prompt = DIALOGUE_TYPING;
}

static void open_overlay() {
    // The press that opened the dialogue must not finish or advance its first page.
    input_wait_release(BTN_A | BTN_B);
    avm_set_text_font(AVM_FONT_BR5D);
    avm_set_text_mode(AVM_TEXT_WHITE_TRANSPARENT);
    avm_draw_filled_rect_black(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    rpg_render_world();
    clear_overlay();
}

static void close_overlay() {
    avm_draw_filled_rect_black(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    rpg_render_world();
    avm_display(true);
}

static bool wait_for_input(DIALOGUE_PROMPT prompt) {
    avm_draw_text(LEFT, PROMPT_BASELINE,
                  prompt == DIALOGUE_MORE     ? F("A more / B leave")
                  : prompt == DIALOGUE_CHOICE ? F("A yes / B no")
                                              : F("A / B close"));
    avm_display(false);
    dialogue_prompt = prompt;

    input_wait_release(BTN_A | BTN_B);
    uint8_t guard = ADVANCE_GUARD_FRAMES;
    bool accepted;
    for(;;) {
        input_wait_frame();
        uint8_t pressed = input_pressed();
        if(pressed & BTN_B) {
            accepted = false;
            break;
        }
        if((pressed & BTN_A) && !guard) {
            accepted = true;
            break;
        }
        if(guard)
            --guard;
    }
    dialogue_prompt = DIALOGUE_CLOSED;
    return accepted;
}

static bool next_line() {
    if(dialogue_y + LINE_HEIGHT <= LAST_LINE) {
        dialogue_x = LEFT;
        dialogue_y += LINE_HEIGHT;
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

// One typing frame. An A press here finishes the current page without blips.
static void type_frame() {
    input_wait_frame();
    if(input_pressed() & BTN_A)
        dialogue_skip = true;
}

// Draws one letter at the pen with its blip. A letter that would pass the right
// edge starts a new line first, so oversized words still break at glyph boundaries.
__attribute__((noinline)) static bool reveal_letter(char c) {
    char letter[2] = {c, 0};
    if(dialogue_x + avm_text_width(letter) > RIGHT && !next_line())
        return false;

    dialogue_x = (uint8_t)avm_draw_text(dialogue_x, dialogue_y, letter).x;
    avm_display(false);
    if(dialogue_skip)
        return true;

    // Each letter blips for its own frame as a triangle, with a pitch chosen from its character
    // code.
    avm_audio_tone_triangle(BLIP_NOTES[(uint8_t)c & 7], BLIP_AMPLITUDE);
    type_frame();
    avm_audio_stop_sfx();
    return true;
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
            uint8_t space = (uint8_t)avm_text_width(F(" "));
            if(dialogue_x + space + word_width(words) > RIGHT) {
                if(!next_line())
                    return false;
            } else {
                dialogue_x += space;
            }
        }

        // Word placement above has already decided this word's line.
        while(*words && !separator(*words)) {
            if(!reveal_letter(*words++))
                return false;
        }
    }

    return true;
}

bool dialogue_show(char const PROGMEM* words) {
    if(!words)
        return true;
    while(separator(*words))
        ++words;
    if(!*words)
        return true;

    open_overlay();
    bool accepted = stream_text(words) && wait_for_input(DIALOGUE_DONE);
    close_overlay();
    return accepted;
}

bool dialogue_ask(char const PROGMEM* question) {
    open_overlay();
    bool accepted = (!question || stream_text(question)) && wait_for_input(DIALOGUE_CHOICE);
    close_overlay();
    return accepted;
}
