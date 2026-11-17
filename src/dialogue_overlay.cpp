#include "rpg.h"
#include "npc.h"
#include "sfx.h"

#include <avm.h>

// Overlay geometry. Changing the visible line count recomputes every position below.
static constexpr uint8_t DISPLAY_WIDTH = 128, DISPLAY_HEIGHT = 64;
static constexpr uint8_t DIALOGUE_VISIBLE_LINES = 4;
static constexpr uint8_t BORDER = 1, PADDING = 1, GAP_ABOVE = 1;

// AVM_FONT_BR5D: seven-pixel line spacing, five pixels above the baseline, and
// descenders that reach the baseline row.
static constexpr uint8_t LINE_HEIGHT = 7, ASCENT = 5;
// Rows from the top of a letter to its baseline row, which descenders reach.
static constexpr uint8_t GLYPH_ROWS = ASCENT + 1;
static constexpr uint8_t NAME_HEIGHT = 2 * BORDER + 2 * PADDING + GLYPH_ROWS;

static constexpr uint8_t OVERLAY_HEIGHT =
    2 * BORDER + 2 * PADDING + (DIALOGUE_VISIBLE_LINES - 1) * LINE_HEIGHT + GLYPH_ROWS;
static constexpr uint8_t OVERLAY_TOP = DISPLAY_HEIGHT - OVERLAY_HEIGHT;

// A speaker with a portrait shows it in a tab above the left end of the panel, and the name panel
// moves beside it. The tab leaves the panel, the text width and the paging untouched. The name
// panel starts one pixel past the portrait, inside a black outline that joins the portrait's edge.
static constexpr uint8_t FACE_SIZE = 32;
static constexpr int8_t FACE_TOP = OVERLAY_TOP - GAP_ABOVE - FACE_SIZE;
static constexpr uint8_t NAME_LEFT_BESIDE_FACE = FACE_SIZE + 1;

// While a speaker types, the mouth flips between open and closed every few letters.
static constexpr uint8_t MOUTH_LETTERS = 5;

static constexpr uint8_t LEFT = BORDER + PADDING;
static constexpr uint8_t RIGHT = DISPLAY_WIDTH - BORDER - PADDING;
static constexpr uint8_t FIRST_LINE = OVERLAY_TOP + BORDER + PADDING + ASCENT;
static constexpr uint8_t LAST_LINE = FIRST_LINE + (DIALOGUE_VISIBLE_LINES - 1) * LINE_HEIGHT;

// The last line carries the prompt at its right end, so text on that line stops short of it. A page
// waiting for A shows a 7x7 indicator, and a question shows its legend. Easing in and out draws
// the indicator in its first frame.
static constexpr uint8_t PROMPT_SIZE = 7, PROMPT_GAP = 2;
static constexpr uint8_t PROMPT_X = RIGHT - PROMPT_SIZE;
static constexpr uint8_t PROMPT_Y = LAST_LINE - ASCENT - 1;
static constexpr uint8_t PROMPT_TEXT_RIGHT = PROMPT_X - PROMPT_GAP;
static constexpr uint8_t PROMPT_BLINK_FRAMES = 25;

// Frames run at 50 Hz. Each letter appears on its own frame and blips for that frame.
// A prompt ignores A for half a second so a press that finished typing cannot advance. Its
// indicator appears when A starts to work.
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

static bool has_face(NPC_ID id) {
    return id < NPC_ID_LIMIT && FACE_PRESENT[id];
}

// The portrait tab sits above the panel and moves with it, so it takes the same offset.
__attribute__((noinline)) static void draw_face(NPC_ID id, bool open, uint8_t offset = 0) {
    int16_t top = FACE_TOP + offset;
    if(top >= DISPLAY_HEIGHT)
        return;
    avm_draw_sprite_overwrite(0, top, FACE_SPRITES, id * 2 + open);
}

// What typing keeps on its caller's stack. last_right is where the last line stops, which leaves
// room for the prompt. While a speaker with a portrait types, the mouth flips between open and
// closed every few letters; the caller starts it closed. The mouth never moves without text
// arriving, so a caller closes it before the panel waits for input. That call sits beside
// wait_for_input, not inside it, to stay off the deepest call chain. Easing in and out always
// draws the mouth closed.
struct Typing {
    bool face;
    bool mouth_open;
    uint8_t mouth_letters;
    uint8_t last_right;
};

__attribute__((noinline)) static void set_mouth(Typing& typing, bool open) {
    if(!typing.face || open == typing.mouth_open)
        return;
    typing.mouth_open = open;
    draw_face(dialogue_page.speaker, open);
}

__attribute__((noinline)) static void tick_mouth(Typing& typing) {
    if(!typing.face || ++typing.mouth_letters < MOUTH_LETTERS)
        return;
    typing.mouth_letters = 0;
    set_mouth(typing, !typing.mouth_open);
}

static uint8_t right_edge(Typing const& typing) {
    return dialogue_y == LAST_LINE ? typing.last_right : RIGHT;
}

__attribute__((noinline)) static void draw_name(NPC_ID id, uint8_t offset = 0) {
    char const PROGMEM* name = npc_name(id);
    if(!name || !*name)
        return;

    uint8_t top = OVERLAY_TOP - GAP_ABOVE - NAME_HEIGHT + offset;
    if(top >= DISPLAY_HEIGHT)
        return;
    uint8_t left = has_face(id) ? NAME_LEFT_BESIDE_FACE : 0;
    uint8_t outline = left ? left - 1 : 0;
    uint16_t width = avm_text_width(name) + 2 * LEFT;
    if(left + width > DISPLAY_WIDTH)
        width = DISPLAY_WIDTH - left;
    avm_draw_filled_rect_black(outline, top - GAP_ABOVE,
                               left - outline + width + (left + width < DISPLAY_WIDTH),
                               NAME_HEIGHT + GAP_ABOVE);
    avm_draw_filled_rect_white(left, top, width, NAME_HEIGHT);
    avm_draw_filled_rect_black(left + BORDER, top + BORDER, width - 2 * BORDER,
                               NAME_HEIGHT - 2 * BORDER);
    avm_draw_text(left + LEFT, top + BORDER + PADDING + ASCENT, name);
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
    if(has_face(id))
        draw_face(id, false);
}

static char const PROGMEM* choice_legend() {
    return F("A yes  B no");
}

// A question shows its legend right-aligned on the last line. The other prompts show an arrow
// that bobs while more text follows, or the A button, which blinks, after the last page.
// The phase picks the arrow's height or the button's fill.
__attribute__((noinline)) static void draw_prompt(DIALOGUE_PROMPT prompt, uint8_t offset = 0,
                                                  bool phase = false) {
    if(prompt == DIALOGUE_CHOICE) {
        char const PROGMEM* legend = choice_legend();
        avm_draw_text(RIGHT - avm_text_width(legend), LAST_LINE + offset, legend);
        return;
    }
    avm_draw_sprite_overwrite(PROMPT_X, PROMPT_Y + offset, PROMPT_SPRITES,
                              (prompt == DIALOGUE_MORE ? 2 : 0) + phase);
}

static bool wait_for_input(DIALOGUE_PROMPT prompt) {
    dialogue_page.prompt = prompt;
    bool indicator = prompt != DIALOGUE_CHOICE;
    // A legend is there at once. An indicator appears when A starts to work.
    if(!indicator) {
        draw_prompt(prompt);
        avm_display(false);
    }
    dialogue_prompt = prompt;

    input_wait_release(BTN_A | BTN_B);
    uint8_t guard = ADVANCE_GUARD_FRAMES, blink = 0;
    bool accepted, phase = false;
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
        if(guard) {
            if(!--guard && indicator) {
                draw_prompt(prompt);
                avm_display(false);
            }
        } else if(indicator && ++blink == PROMPT_BLINK_FRAMES) {
            blink = 0;
            phase = !phase;
            draw_prompt(prompt, 0, phase);
            avm_display(false);
        }
    }
    dialogue_prompt = DIALOGUE_CLOSED;
    return accepted;
}

static bool next_line(Typing& typing) {
    if(dialogue_y + LINE_HEIGHT <= LAST_LINE) {
        dialogue_x = LEFT;
        dialogue_y += LINE_HEIGHT;
        return true;
    }

    set_mouth(typing, false);
    if(!wait_for_input(DIALOGUE_MORE))
        return false;
    clear_overlay();
    // The first letter of the next page opens the mouth.
    typing.mouth_letters = MOUTH_LETTERS - 1;
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
__attribute__((noinline)) static bool reveal_letter(char const PROGMEM* words, bool blips,
                                                    Typing& typing) {
    char c = *words;
    char letter[2] = {c, 0};
    if(dialogue_x + avm_text_width(letter) > right_edge(typing) && !next_line(typing))
        return false;

    if(!dialogue_page.first)
        dialogue_page.first = words;
    if(dialogue_x == LEFT && dialogue_y != FIRST_LINE)
        dialogue_page.lines[(dialogue_y - FIRST_LINE) / LINE_HEIGHT - 1] = words;
    dialogue_page.end = words + 1;
    dialogue_x = (uint8_t)avm_draw_text(dialogue_x, dialogue_y, letter).x;
    if(!dialogue_skip)
        tick_mouth(typing);
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

static bool stream_text(char const PROGMEM* words, bool blips, Typing& typing) {
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
                if(!next_line(typing))
                    return false;
            }
            continue;
        }

        if(dialogue_x != LEFT) {
            uint8_t space = (uint8_t)avm_text_width(F(" "));
            if(dialogue_x + space + word_width(words) > right_edge(typing)) {
                if(!next_line(typing))
                    return false;
            } else {
                dialogue_x += space;
            }
        }

        // Word placement above has already decided this word's line.
        while(*words && !separator(*words)) {
            if(!reveal_letter(words++, blips, typing))
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
    if(has_face(dialogue_page.speaker))
        draw_face(dialogue_page.speaker, false, offset);
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
    if(has_face(dialogue_page.speaker))
        travel += GAP_ABOVE + FACE_SIZE;
    else if(npc_name(dialogue_page.speaker))
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
    // The mouth starts closed, and the first letter opens it.
    Typing typing = {has_face(id), false, MOUTH_LETTERS - 1, PROMPT_TEXT_RIGHT};
    bool accepted = false;
    if(stream_text(words, true, typing)) {
        set_mouth(typing, false);
        accepted = wait_for_input(DIALOGUE_DONE);
    }
    return accepted;
}

bool dialogue_ask(char const PROGMEM* question) {
    open_overlay();
    // The legend sits at the right end of the last line, so the question stops short of it.
    Typing typing = {false, false, 0,
                     (uint8_t)(RIGHT - avm_text_width(choice_legend()) - PROMPT_GAP)};
    bool accepted =
        (!question || stream_text(question, true, typing)) && wait_for_input(DIALOGUE_CHOICE);
    return accepted;
}

void party_joined(NPC_ID id) {
    char const PROGMEM* name = npc_name(id);
    if(!name)
        return;

    open_overlay();
    sfx_play(SFX_PARTY_JOIN);
    // Typing blips share the jingle's voice, so let the jingle play uninterrupted.
    Typing typing = {false, false, 0, PROMPT_TEXT_RIGHT};
    if(stream_text(name, false, typing) && stream_text(F("has joined the party!"), false, typing))
        wait_for_input(DIALOGUE_DONE);
}
