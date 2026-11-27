#ifndef CLASS_CHOICE_H
#define CLASS_CHOICE_H

#include "rpg.h"
#include "text_wrap.h"

// The class choice is one state machine: a list of movement classes, a description of one, a list
// of attack classes, a description of one, and a confirmation. A moves forward and B moves back.
// The choice cannot be abandoned, so B does nothing on the first page. The state is transient, and
// pressing a button never changes the game.
enum CLASS_CHOICE_PAGE : uint8_t {
    CLASS_PAGE_MOVEMENT,
    CLASS_PAGE_MOVEMENT_DETAILS,
    CLASS_PAGE_ATTACK,
    CLASS_PAGE_ATTACK_DETAILS,
    CLASS_PAGE_CONFIRM,
};

// What a press asks the screen to do. The screen plays the matching sound and ends on CONFIRM.
enum CLASS_CHOICE_EVENT : uint8_t {
    CLASS_CHOICE_NONE,
    CLASS_CHOICE_TICK,   // the highlight moved, or a description scrolled
    CLASS_CHOICE_SELECT, // a page opened
    CLASS_CHOICE_CANCEL, // a page closed to the page above it
    CLASS_CHOICE_EDGE,   // nothing moved
    CLASS_CHOICE_CONFIRM,
};

// A list shows this many classes at a time.
inline constexpr uint8_t CLASS_LIST_ROWS = 5;
// A description is wrapped to this many pixels and shown a few lines at a time. The longest
// description is CLASS_TEXT_LINES lines, and native tests check the shipped descriptions.
inline constexpr uint8_t CLASS_TEXT_WIDTH = 110;
inline constexpr uint8_t CLASS_TEXT_ROWS = 4;
inline constexpr uint8_t CLASS_TEXT_LINES = 8;

struct ClassChoiceUi {
    CLASS_CHOICE_PAGE page;
    uint8_t movement, attack; // the highlighted class of each list
    uint8_t scroll;           // the first visible description line
    uint8_t line_count;
    uint8_t starts[CLASS_TEXT_LINES + 1]; // line starts of the wrapped description
};

// Opens the movement list with the traveler's current classes highlighted.
void class_choice_open(ClassChoiceUi& ui);
CLASS_CHOICE_EVENT class_choice_press(ClassChoiceUi& ui, uint8_t pressed);

constexpr bool class_choice_describes(CLASS_CHOICE_PAGE page) {
    return page == CLASS_PAGE_MOVEMENT_DETAILS || page == CLASS_PAGE_ATTACK_DETAILS;
}

constexpr MOVEMENT_CLASS class_choice_movement(ClassChoiceUi const& ui) {
    return (MOVEMENT_CLASS)ui.movement;
}

constexpr ATTACK_CLASS class_choice_attack(ClassChoiceUi const& ui) {
    return (ATTACK_CLASS)ui.attack;
}

// A class's mechanics and the direction its perks take.
char const PROGMEM* class_movement_text(uint8_t movement);
char const PROGMEM* class_attack_text(uint8_t attack);
// The description of a detail page, or null for any other page.
char const PROGMEM* class_choice_text(ClassChoiceUi const& ui);

// Wraps the description of the open detail page. Call it after a press changes the page to a
// detail page, so the scroll limits and the drawn lines are known.
template <typename RangeWidth> void class_choice_wrap(ClassChoiceUi& ui, RangeWidth range_width) {
    char const PROGMEM* text = class_choice_text(ui);
    ui.line_count =
        text ? text_wrap(text, CLASS_TEXT_WIDTH, range_width, ui.starts, CLASS_TEXT_LINES) : 0;
}

// The AVM screens measure with the runtime font.
void class_choice_layout(ClassChoiceUi& ui);
void render_class_choice(ClassChoiceUi const& ui);

#endif
