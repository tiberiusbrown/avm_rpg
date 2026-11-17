#include "rpg.h"
#include "npc.h"
#include "sfx.h"

#include <avm.h>

// Overlay geometry. Changing the visible line count recomputes every position below.
static constexpr uint8_t DISPLAY_WIDTH = 128, DISPLAY_HEIGHT = 64;
static constexpr uint8_t DIALOGUE_VISIBLE_LINES = 3;
static constexpr uint8_t BORDER = 1, PADDING = 1, GAP_ABOVE = 1;

// AVM_FONT_BR5D: seven-pixel line spacing, five pixels above the baseline, and
// descenders that reach the baseline row.
static constexpr uint8_t LINE_HEIGHT = 7, ASCENT = 5;
static constexpr uint8_t PROMPT_GAP = 1, PROMPT_ROWS = ASCENT + 1;
static constexpr uint8_t NAME_HEIGHT = 2 * BORDER + 2 * PADDING + ASCENT + 1;

static constexpr uint8_t OVERLAY_HEIGHT =
    2 * BORDER + 2 * PADDING + DIALOGUE_VISIBLE_LINES * LINE_HEIGHT + PROMPT_GAP + PROMPT_ROWS;
static constexpr uint8_t OVERLAY_TOP = DISPLAY_HEIGHT - OVERLAY_HEIGHT;

static constexpr uint8_t LEFT = BORDER + PADDING;
static constexpr uint8_t RIGHT = DISPLAY_WIDTH - BORDER - PADDING;
static constexpr uint8_t FIRST_LINE = OVERLAY_TOP + BORDER + PADDING + ASCENT;
static constexpr uint8_t LAST_LINE = FIRST_LINE + (DIALOGUE_VISIBLE_LINES - 1) * LINE_HEIGHT;
static constexpr uint8_t PROMPT_BASELINE = DISPLAY_HEIGHT - BORDER - PADDING - 1;

// Frames run at 50 Hz. Each letter appears on its own frame and blips for that frame.
// A prompt ignores A for half a second so a press that finished typing cannot advance.
static constexpr uint8_t ADVANCE_GUARD_FRAMES = 25;
static constexpr uint8_t BLIP_AMPLITUDE = 15;
static constexpr int8_t BLIP_OFFSET = -12;
static uint8_t const PROGMEM BLIP_NOTES[8] = {
    BLIP_OFFSET + 67, BLIP_OFFSET + 69, BLIP_OFFSET + 71, BLIP_OFFSET + 74,
    BLIP_OFFSET + 76, BLIP_OFFSET + 79, BLIP_OFFSET + 81, BLIP_OFFSET + 83,
};

static uint8_t dialogue_x, dialogue_y;
static bool dialogue_skip;
bool dialogue_panel_open;

// Retain only flash pointers for the visible page, so it can move during closing.
// A second segment supports the recruitment name followed by its announcement.
static struct DialoguePage {
    char const PROGMEM* first;
    char const PROGMEM* second;
    char const PROGMEM* end;
    char const PROGMEM* lines[DIALOGUE_VISIBLE_LINES - 1];
    NPC_ID speaker;
    DIALOGUE_PROMPT prompt;
} dialogue_page;

static void animate_overlay(bool closing);

// Exposed for debugger/device QA; no dialogue state lives in Game or its save.
volatile DIALOGUE_PROMPT dialogue_prompt;

static bool separator(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static void draw_overlay(uint8_t offset) {
    avm_draw_filled_rect_black(0, OVERLAY_TOP + offset - GAP_ABOVE, DISPLAY_WIDTH, GAP_ABOVE);
    avm_draw_filled_rect_white(0, OVERLAY_TOP + offset, DISPLAY_WIDTH, OVERLAY_HEIGHT);
    avm_draw_filled_rect_black(BORDER, OVERLAY_TOP + offset + BORDER, DISPLAY_WIDTH - 2 * BORDER,
                               OVERLAY_HEIGHT - 2 * BORDER);
}

static void clear_overlay() {
    draw_overlay(0);
    dialogue_page.first = dialogue_page.second = dialogue_page.end = nullptr;
    for(auto& line : dialogue_page.lines)
        line = nullptr;
    dialogue_page.prompt = DIALOGUE_CLOSED;
    dialogue_x = LEFT;
    dialogue_y = FIRST_LINE;
    dialogue_skip = false;
    dialogue_prompt = DIALOGUE_TYPING;
}

static void draw_name(NPC_ID id, uint8_t offset = 0) {
    char const PROGMEM* name = npc_name(id);
    if(!name || !*name)
        return;

    uint8_t top = OVERLAY_TOP - GAP_ABOVE - NAME_HEIGHT + offset;
    if(top >= DISPLAY_HEIGHT)
        return;
    uint16_t width = avm_text_width(name) + 2 * LEFT;
    if(width > DISPLAY_WIDTH)
        width = DISPLAY_WIDTH;
    avm_draw_filled_rect_black(0, top - GAP_ABOVE, width + (width < DISPLAY_WIDTH),
                               NAME_HEIGHT + GAP_ABOVE);
    avm_draw_filled_rect_white(0, top, width, NAME_HEIGHT);
    avm_draw_filled_rect_black(BORDER, top + BORDER, width - 2 * BORDER, NAME_HEIGHT - 2 * BORDER);
    avm_draw_text(LEFT, top + BORDER + PADDING + ASCENT, name);
}

static void open_overlay(NPC_ID id = NPC_NONE) {
    // The press that opened the dialogue must not finish or advance its first page.
    input_wait_release(BTN_A | BTN_B);
    avm_set_text_font(AVM_FONT_BR5D);
    avm_set_text_mode(AVM_TEXT_WHITE_TRANSPARENT);
    dialogue_page = {};
    dialogue_page.speaker = id;
    dialogue_prompt = DIALOGUE_TYPING;
    if(!dialogue_panel_open) {
        dialogue_panel_open = true;
        animate_overlay(false);
    }
    // Rebuild the uncovered world, removing the previous speaker's name if it changed.
    avm_draw_filled_rect_black(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    rpg_render_world(OVERLAY_HEIGHT);
    clear_overlay();
    draw_name(id);
}

static void draw_prompt(DIALOGUE_PROMPT prompt, uint8_t offset = 0) {
    avm_draw_text(LEFT, PROMPT_BASELINE + offset,
                  prompt == DIALOGUE_MORE     ? F("A more / B leave")
                  : prompt == DIALOGUE_CHOICE ? F("A yes / B no")
                                              : F("A / B close"));
}

static bool wait_for_input(DIALOGUE_PROMPT prompt) {
    dialogue_page.prompt = prompt;
    draw_prompt(prompt);
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
__attribute__((noinline)) static bool reveal_letter(char const PROGMEM* words, bool blips) {
    char c = *words;
    char letter[2] = {c, 0};
    if(dialogue_x + avm_text_width(letter) > RIGHT && !next_line())
        return false;

    if(!dialogue_page.first)
        dialogue_page.first = words;
    if(dialogue_x == LEFT && dialogue_y != FIRST_LINE)
        dialogue_page.lines[(dialogue_y - FIRST_LINE) / LINE_HEIGHT - 1] = words;
    dialogue_page.end = words + 1;
    dialogue_x = (uint8_t)avm_draw_text(dialogue_x, dialogue_y, letter).x;
    avm_display(false);
    if(dialogue_skip)
        return true;

    // Each letter blips for its own frame, with a pitch chosen from its character
    // code. Zero duration latches until the explicit stop after that frame.
    if(blips && (c & 1))
        avm_audio_tone(BLIP_NOTES[((uint8_t)c >> 1) & 7], BLIP_AMPLITUDE, 32);
    type_frame();
    return true;
}

static bool stream_text(char const PROGMEM* words, bool blips = true) {
    if(dialogue_page.first)
        dialogue_page.second = words;
    while(*words) {
        if(separator(*words)) {
            char c = *words++;
            if(dialogue_page.first)
                dialogue_page.end = words;
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
            if(!reveal_letter(words++, blips))
                return false;
        }
    }

    return true;
}

// Line starts are recorded during typing. Replay uses those same breaks without
// measuring the page again, keeping the close animation within its frame budget.
static uint8_t page_line(char const PROGMEM* words) {
    for(uint8_t line = 0; line < DIALOGUE_VISIBLE_LINES - 1; ++line)
        if(dialogue_page.lines[line] == words)
            return line + 1;
    return 0;
}

static void draw_segment(char const PROGMEM* words, char const PROGMEM* end, uint8_t offset,
                         uint8_t& x, uint8_t& y) {
    bool space = x != LEFT;
    while(*words && words != end) {
        char chunk[25];
        uint8_t count = 0;
        while(count < 24 && *words && words != end) {
            if(uint8_t line = page_line(words)) {
                if(count)
                    break;
                x = LEFT;
                y = FIRST_LINE + line * LINE_HEIGHT;
                space = false;
            }
            if(y + offset >= DISPLAY_HEIGHT + ASCENT)
                return;
            if(separator(*words)) {
                ++words;
                space = true;
                continue;
            }
            if(space && (x != LEFT || count))
                chunk[count++] = ' ';
            space = false;
            if(count < 24)
                chunk[count++] = *words++;
        }
        chunk[count] = 0;
        if(count)
            x = (uint8_t)avm_draw_text(x, y + offset, chunk).x;
    }
}

static void draw_transition(uint8_t offset) {
    avm_draw_filled_rect_black(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    uint8_t covered = offset < OVERLAY_HEIGHT ? OVERLAY_HEIGHT - offset : 0;
    rpg_render_world(covered);
    draw_name(dialogue_page.speaker, offset);
    if(offset < OVERLAY_HEIGHT) {
        draw_overlay(offset);
        uint8_t x = LEFT, y = FIRST_LINE;
        if(dialogue_page.first)
            draw_segment(dialogue_page.first, dialogue_page.second ? nullptr : dialogue_page.end,
                         offset, x, y);
        if(dialogue_page.second)
            draw_segment(dialogue_page.second, dialogue_page.end, offset, x, y);
        if(dialogue_page.prompt != DIALOGUE_CLOSED)
            draw_prompt(dialogue_page.prompt, offset);
    }
}

static void animate_overlay(bool closing) {
    uint8_t travel = OVERLAY_HEIGHT;
    if(npc_name(dialogue_page.speaker))
        travel += GAP_ABOVE + NAME_HEIGHT;
    // Use the same normalized smoothstep and frame count in both directions,
    // including panels with names. Only the total travel distance varies.
    static constexpr uint16_t PROGMEM PROGRESS[] = {19, 66, 128, 190, 237, 256};
    for(uint16_t progress : PROGRESS) {
        input_wait_frame();
        uint8_t position = (uint8_t)((travel * progress + 128) >> 8);
        draw_transition(closing ? position : travel - position);
        avm_display(false);
    }
}

void dialogue_close() {
    if(!dialogue_panel_open)
        return;
    animate_overlay(true);
    dialogue_panel_open = false;
    dialogue_page = {};
    dialogue_prompt = DIALOGUE_CLOSED;
    input_sync();
}

bool dialogue_show(char const PROGMEM* words) {
    return dialogue_show(NPC_NONE, words);
}

bool dialogue_show(NPC_ID id, char const PROGMEM* words) {
    if(!words)
        return true;
    while(separator(*words))
        ++words;
    if(!*words)
        return true;

    open_overlay(id);
    bool accepted = stream_text(words) && wait_for_input(DIALOGUE_DONE);
    return accepted;
}

bool dialogue_ask(char const PROGMEM* question) {
    open_overlay();
    bool accepted = (!question || stream_text(question)) && wait_for_input(DIALOGUE_CHOICE);
    return accepted;
}

void party_joined(NPC_ID id) {
    char const PROGMEM* name = npc_name(id);
    if(!name)
        return;

    open_overlay();
    sfx_play(SFX_PARTY_JOIN);
    // Typing blips share the jingle's voice, so let the jingle play uninterrupted.
    if(stream_text(name, false) && stream_text(F("has joined the party!"), false))
        wait_for_input(DIALOGUE_DONE);
}
