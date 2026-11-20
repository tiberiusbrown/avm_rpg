#ifndef PARTY_UI_H
#define PARTY_UI_H

#include "rpg.h"
#include "party_preview.h"
#include "text_wrap.h"

// The Party screen is one state machine with a page for each screen. A press moves one level down
// with A, one level up with B, or within the page with the D-pad. The state is transient: it lives
// in the screen's frame and is never saved. Pressing a button reads the party but never changes it.
enum PARTY_PAGE : uint8_t {
    PARTY_PAGE_OVERVIEW,        // the member's header and the three categories
    PARTY_PAGE_SPECIALS,        // the member's starting specials
    PARTY_PAGE_SPECIAL_DETAILS, // one special's cost, range, and effect
    PARTY_PAGE_PERKS,        // the perk index: two tracks, equipment perks, and learning
    PARTY_PAGE_PERK_LIST,    // a movement or attack track, or the equipment perks
    PARTY_PAGE_PERK_CHOICE,  // the next rank of each track, for learning
    PARTY_PAGE_PERK_DETAILS, // one perk or rank, reached from PERK_LIST or PERK_CHOICE
    PARTY_PAGE_PERK_CONFIRM,
    PARTY_PAGE_EQUIPMENT,    // the weapon, armor and relic slots
    PARTY_PAGE_ITEM_LIST,    // the preview items of one slot
    PARTY_PAGE_ITEM_DETAILS, // one preview item
    PARTY_PAGE_COUNT,
};

// What a press asks the screen to do. The screen plays the matching sound and closes on CLOSE.
enum PARTY_EVENT : uint8_t {
    PARTY_EVENT_NONE,
    PARTY_EVENT_TICK,   // the highlight moved, or a description scrolled
    PARTY_EVENT_SELECT, // a page opened
    PARTY_EVENT_CANCEL, // a page closed to the page above it
    PARTY_EVENT_EDGE,   // nothing moved, because the list or description has no further entry
    PARTY_EVENT_LEARN,  // the controller commits the confirmed choice
    PARTY_EVENT_CLOSE,  // the Party panel closes
};

// The overview's categories, in order.
enum PARTY_CATEGORY : uint8_t {
    CATEGORY_SPECIALS,
    CATEGORY_PERKS,
    CATEGORY_EQUIPMENT,
    CATEGORY_COUNT
};

// The rows of the perk index. The first three open lists, and the last opens the choice screen.
enum PARTY_PERK_ROW : uint8_t {
    PERK_ROW_MOVEMENT,
    PERK_ROW_ATTACK,
    PERK_ROW_EQUIPMENT,
    PERK_ROW_CHOICE,
    PERK_ROW_COUNT
};

// The equipment slots, in the order the preview items are listed.
enum PARTY_SLOT : uint8_t { SLOT_WEAPON, SLOT_ARMOR, SLOT_RELIC, SLOT_COUNT };

static_assert(SLOT_COUNT == PREVIEW_SLOT_COUNT, "every slot has a preview item list");

// A description is wrapped to this many pixels and shown a few lines at a time. The longest
// description is PARTY_TEXT_LINES lines, and native tests check the shipped descriptions.
inline constexpr uint8_t PARTY_TEXT_WIDTH = 110;
inline constexpr uint8_t PARTY_TEXT_ROWS = 4;
inline constexpr uint8_t PARTY_TEXT_LINES = 8;

// A perk code names one rank of a track, as track * PERK_TRACK_RANKS + rank, or one equipment
// effect, as PERK_EQUIPMENT + index. The rank component is zero-based. An item code names one preview item, as slot *
// PREVIEW_ITEMS_PER_SLOT + index.
inline constexpr uint8_t PARTY_PERK_TRACK_COUNT = MOVEMENT_CLASS_COUNT + (uint8_t)ATTACK_CLASS_COUNT;
inline constexpr uint8_t PERK_EQUIPMENT = PARTY_PERK_TRACK_COUNT * PERK_TRACK_RANKS;

constexpr uint8_t party_perk_code(uint8_t track, uint8_t rank) {
    return (uint8_t)(track * PERK_TRACK_RANKS + rank);
}

constexpr uint8_t party_item_code(uint8_t slot, uint8_t index) {
    return (uint8_t)(slot * PREVIEW_ITEMS_PER_SLOT + index);
}

constexpr uint8_t party_item_slot(uint8_t code) {
    return (uint8_t)(code / PREVIEW_ITEMS_PER_SLOT);
}

// The track of a rank code, or PARTY_NO_TRACK for an equipment effect.
inline constexpr uint8_t PARTY_NO_TRACK = 0xff;

struct PartyUi {
    PARTY_PAGE page;
    PARTY_PAGE back; // the page B returns to from a perk details page
    uint8_t member;  // the member shown, below party.count
    uint8_t entry;   // the perk, item, or special that a detail page shows
    uint8_t scroll;  // the first visible description line of a detail page
    uint8_t line_count;
    bool perk_learned; // feedback on returning to the choice page
    // The highlighted entry of each page. A page keeps its highlight while a page above it is
    // open, so B returns to the same entry.
    uint8_t selection[PARTY_PAGE_COUNT];
    uint8_t starts[PARTY_TEXT_LINES + 1]; // line starts of the wrapped description
};

// Opens the Party on the overview of the first member with every highlight at its first entry.
void party_ui_open(PartyUi& ui);
// Applies the buttons that became pressed this frame. `party` is read but never changed.
PARTY_EVENT party_ui_press(PartyUi& ui, Party const& party, uint8_t pressed);

// Whether a page shows a wrapped description, not a list.
constexpr bool party_ui_describes(PARTY_PAGE page) {
    return page == PARTY_PAGE_SPECIAL_DETAILS || page == PARTY_PAGE_PERK_DETAILS ||
           page == PARTY_PAGE_ITEM_DETAILS;
}

// The entries of the current list page, or 0 for a page with no list.
uint8_t party_ui_entries(PartyUi const& ui, Party const& party);
// The starting specials a member has. Each member has one, from its attack class.
uint8_t party_special_count(PartyMember const& member);
// The track selected for inspection or confirmation.
PERK_TRACK_KIND party_ui_choice_track(PartyUi const& ui);
void party_ui_perk_learned(PartyUi& ui);

enum PARTY_PERK_STATUS : uint8_t {
    PERK_STATUS_FUTURE, PERK_STATUS_LOCKED, PERK_STATUS_LEARNABLE, PERK_STATUS_EARNED
};
PARTY_PERK_STATUS party_perk_status(PartyMember const& member, PERK_TRACK_KIND track, uint8_t rank);

// The perk code of row `index` of a perk list or learning.
uint8_t party_ui_row_perk(PartyUi const& ui, Party const& party, uint8_t index);
// The description of a detail page, or null for any other page.
char const PROGMEM* party_ui_text(PartyUi const& ui, Party const& party);

// Wraps the description of the open detail page. Call it after a press changes the page to a detail
// page, so the scroll limits and the drawn lines are known. Other pages get no lines.
template <typename RangeWidth>
void party_ui_layout(PartyUi& ui, Party const& party, RangeWidth range_width) {
    char const PROGMEM* text = party_ui_text(ui, party);
    ui.line_count =
        text ? text_wrap(text, PARTY_TEXT_WIDTH, range_width, ui.starts, PARTY_TEXT_LINES) : 0;
}

// Class names and the starting special's name and description, from the battle's definitions.
char const PROGMEM* party_movement_name(uint8_t movement);
char const PROGMEM* party_attack_name(uint8_t attack);
char const PROGMEM* party_special_name(uint8_t attack);
char const PROGMEM* party_special_text(uint8_t attack);

// Perk codes. An equipment perk has no track.
char const PROGMEM* party_perk_name(uint8_t code);
char const PROGMEM* party_perk_text(uint8_t code);
char const PROGMEM* party_perk_source(uint8_t code);
uint8_t party_perk_track(uint8_t code);
char const PROGMEM* party_track_name(uint8_t track);

// Item codes and equipment slots.
char const PROGMEM* party_item_name(uint8_t code);
char const PROGMEM* party_item_text(uint8_t code);
char const PROGMEM* party_slot_name(uint8_t slot);
// The item equipped in a slot. No equipment exists yet, so every slot is empty.
char const PROGMEM* party_equipped(PartyMember const& member, uint8_t slot);

#endif
