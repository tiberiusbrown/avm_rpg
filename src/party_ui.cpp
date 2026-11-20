#include "party_ui.h"

#include "battle.h"

// Each choice row is a track's next rank: the member's movement track, then its attack track.
static constexpr uint8_t PERK_CHOICE_COUNT = 2;

static uint8_t& highlight(PartyUi& ui) {
    return ui.selection[ui.page];
}

// Whether a page moves to another member. The pages that list a member's entries stay on their
// page when the member changes. The detail pages and tracks keep the member they opened for.
static bool switches_member(PARTY_PAGE page) {
    return page == PARTY_PAGE_OVERVIEW || page == PARTY_PAGE_SPECIALS || page == PARTY_PAGE_PERKS ||
           page == PARTY_PAGE_EQUIPMENT;
}

// After a member change, a highlight beyond the new entries moves to the last entry.
static void clamp_highlight(PartyUi& ui, Party const& party) {
    uint8_t count = party_ui_entries(ui, party);
    uint8_t& selected = highlight(ui);
    if(!count)
        selected = 0;
    else if(selected >= count)
        selected = (uint8_t)(count - 1);
}

// Enters a page from the page above it. The new page's highlight starts at its first entry.
static PARTY_EVENT open(PartyUi& ui, PARTY_PAGE page) {
    ui.perk_learned = false;
    ui.page = page;
    highlight(ui) = 0;
    ui.scroll = 0;
    return PARTY_EVENT_SELECT;
}

// Enters a detail page that B returns to. The page above keeps its highlight.
static PARTY_EVENT open_details(PartyUi& ui, PARTY_PAGE page, PARTY_PAGE back_page) {
    ui.back = back_page;
    ui.page = page;
    ui.scroll = 0;
    return PARTY_EVENT_SELECT;
}

// Returns to the page above without changing the highlight it keeps.
static PARTY_EVENT leave(PartyUi& ui, PARTY_PAGE page) {
    ui.page = page;
    ui.scroll = 0;
    return PARTY_EVENT_CANCEL;
}

static PARTY_EVENT back(PartyUi& ui) {
    switch(ui.page) {
        case PARTY_PAGE_OVERVIEW:
            return PARTY_EVENT_CLOSE;
        case PARTY_PAGE_SPECIALS:
        case PARTY_PAGE_PERKS:
        case PARTY_PAGE_EQUIPMENT:
            return leave(ui, PARTY_PAGE_OVERVIEW);
        case PARTY_PAGE_SPECIAL_DETAILS:
            return leave(ui, PARTY_PAGE_SPECIALS);
        case PARTY_PAGE_PERK_LIST:
        case PARTY_PAGE_PERK_CHOICE:
            return leave(ui, PARTY_PAGE_PERKS);
        case PARTY_PAGE_PERK_CONFIRM:
            ui.page = PARTY_PAGE_PERK_DETAILS;
            return PARTY_EVENT_CANCEL;
        case PARTY_PAGE_PERK_DETAILS:
            return leave(ui, ui.back);
        case PARTY_PAGE_ITEM_LIST:
            return leave(ui, PARTY_PAGE_EQUIPMENT);
        case PARTY_PAGE_ITEM_DETAILS:
            return leave(ui, PARTY_PAGE_ITEM_LIST);
        default:
            return PARTY_EVENT_CLOSE;
    }
}

static PARTY_EVENT activate(PartyUi& ui, Party const& party) {
    switch(ui.page) {
        case PARTY_PAGE_OVERVIEW:
            if(highlight(ui) == CATEGORY_SPECIALS)
                return open(ui, PARTY_PAGE_SPECIALS);
            if(highlight(ui) == CATEGORY_PERKS)
                return open(ui, PARTY_PAGE_PERKS);
            return open(ui, PARTY_PAGE_EQUIPMENT);
        case PARTY_PAGE_SPECIALS:
            ui.entry = highlight(ui);
            return open_details(ui, PARTY_PAGE_SPECIAL_DETAILS, PARTY_PAGE_SPECIALS);
        case PARTY_PAGE_PERKS:
            return open(ui, highlight(ui) == PERK_ROW_CHOICE ? PARTY_PAGE_PERK_CHOICE
                                                             : PARTY_PAGE_PERK_LIST);
        case PARTY_PAGE_PERK_CHOICE:
            if(!party_can_learn_perk(party.members[ui.member], party_ui_choice_track(ui)))
                return PARTY_EVENT_EDGE;
            ui.perk_learned = false;
            [[fallthrough]];
        case PARTY_PAGE_PERK_LIST:
            ui.entry = party_ui_row_perk(ui, party, highlight(ui));
            return open_details(ui, PARTY_PAGE_PERK_DETAILS, ui.page);
        case PARTY_PAGE_PERK_DETAILS:
            if(ui.back != PARTY_PAGE_PERK_CHOICE ||
               !party_can_learn_perk(party.members[ui.member], party_ui_choice_track(ui)))
                return PARTY_EVENT_EDGE;
            ui.page = PARTY_PAGE_PERK_CONFIRM;
            return PARTY_EVENT_SELECT;
        case PARTY_PAGE_PERK_CONFIRM:
            return PARTY_EVENT_LEARN;
        case PARTY_PAGE_EQUIPMENT:
            return open(ui, PARTY_PAGE_ITEM_LIST);
        case PARTY_PAGE_ITEM_LIST:
            ui.entry = party_item_code(ui.selection[PARTY_PAGE_EQUIPMENT], highlight(ui));
            return open_details(ui, PARTY_PAGE_ITEM_DETAILS, PARTY_PAGE_ITEM_LIST);
        default:
            // The detail pages' actions, such as equipping, are not available yet.
            return PARTY_EVENT_EDGE;
    }
}

static PARTY_EVENT switch_member(PartyUi& ui, Party const& party, bool forward) {
    if(!switches_member(ui.page))
        return PARTY_EVENT_NONE;
    if(party.count <= 1)
        return PARTY_EVENT_EDGE;
    ui.member = (uint8_t)((ui.member + (forward ? 1 : party.count - 1)) % party.count);
    clamp_highlight(ui, party);
    return PARTY_EVENT_TICK;
}

static PARTY_EVENT move(PartyUi& ui, Party const& party, bool down) {
    if(party_ui_describes(ui.page)) {
        if(!down && ui.scroll) {
            --ui.scroll;
            return PARTY_EVENT_TICK;
        }
        if(down && ui.scroll + PARTY_TEXT_ROWS < ui.line_count) {
            ++ui.scroll;
            return PARTY_EVENT_TICK;
        }
        return PARTY_EVENT_EDGE;
    }
    uint8_t count = party_ui_entries(ui, party);
    if(count == 0)
        return PARTY_EVENT_NONE;
    if(count == 1)
        return PARTY_EVENT_EDGE;
    uint8_t& selected = highlight(ui);
    selected = (uint8_t)((selected + (down ? 1 : count - 1)) % count);
    return PARTY_EVENT_TICK;
}

PERK_TRACK_KIND party_ui_choice_track(PartyUi const& ui) {
    return ui.selection[PARTY_PAGE_PERK_CHOICE] == 0 ? PERK_MOVEMENT : PERK_ATTACK;
}

void party_ui_perk_learned(PartyUi& ui) {
    leave(ui, PARTY_PAGE_PERK_CHOICE);
    ui.perk_learned = true;
}

PARTY_PERK_STATUS party_perk_status(PartyMember const& member, PERK_TRACK_KIND track, uint8_t rank) {
    if(rank > PERK_IMPLEMENTED_RANKS)
        return PERK_STATUS_FUTURE;
    if(rank <= party_perk_rank(member, track))
        return PERK_STATUS_EARNED;
    return party_can_learn_perk(member, track) ? PERK_STATUS_LEARNABLE : PERK_STATUS_LOCKED;
}

void party_ui_open(PartyUi& ui) {
    ui = PartyUi{};
}

PARTY_EVENT party_ui_press(PartyUi& ui, Party const& party, uint8_t pressed) {
    if(pressed & BTN_B)
        return back(ui);
    if(pressed & BTN_A)
        return activate(ui, party);
    if(pressed & (BTN_LEFT | BTN_RIGHT))
        return switch_member(ui, party, (pressed & BTN_RIGHT) != 0);
    if(pressed & (BTN_UP | BTN_DOWN))
        return move(ui, party, (pressed & BTN_DOWN) != 0);
    return PARTY_EVENT_NONE;
}

uint8_t party_ui_entries(PartyUi const& ui, Party const& party) {
    switch(ui.page) {
        case PARTY_PAGE_OVERVIEW:
            return CATEGORY_COUNT;
        case PARTY_PAGE_SPECIALS:
            return party_special_count(party.members[ui.member]);
        case PARTY_PAGE_PERKS:
            return PERK_ROW_COUNT;
        case PARTY_PAGE_PERK_LIST:
            return ui.selection[PARTY_PAGE_PERKS] == PERK_ROW_EQUIPMENT ? PREVIEW_EQUIPMENT_COUNT
                                                                        : PERK_TRACK_RANKS;
        case PARTY_PAGE_PERK_CHOICE:
            return PERK_CHOICE_COUNT;
        case PARTY_PAGE_EQUIPMENT:
            return SLOT_COUNT;
        case PARTY_PAGE_ITEM_LIST:
            return PREVIEW_ITEMS_PER_SLOT;
        default:
            return 0;
    }
}

uint8_t party_special_count(PartyMember const&) {
    // Only the starting special exists. Later specials will come from the member's unlocks.
    return 1;
}

uint8_t party_ui_row_perk(PartyUi const& ui, Party const& party, uint8_t index) {
    PartyMember const& member = party.members[ui.member];
    uint8_t attack_track = (uint8_t)(MOVEMENT_CLASS_COUNT + member.attack_class);
    if(ui.page == PARTY_PAGE_PERK_CHOICE)
        return party_perk_code(index == 0 ? member.movement_class : attack_track,
                               party_perk_rank(member, index == 0 ? PERK_MOVEMENT : PERK_ATTACK));
    uint8_t row = ui.selection[PARTY_PAGE_PERKS];
    if(row == PERK_ROW_EQUIPMENT)
        return (uint8_t)(PERK_EQUIPMENT + index);
    return party_perk_code(row == PERK_ROW_MOVEMENT ? member.movement_class : attack_track, index);
}

char const PROGMEM* party_ui_text(PartyUi const& ui, Party const& party) {
    switch(ui.page) {
        case PARTY_PAGE_SPECIAL_DETAILS:
            return party_special_text(party.members[ui.member].attack_class);
        case PARTY_PAGE_PERK_DETAILS:
            return party_perk_text(ui.entry);
        case PARTY_PAGE_ITEM_DETAILS:
            return party_item_text(ui.entry);
        default:
            return nullptr;
    }
}

char const PROGMEM* party_movement_name(uint8_t movement) {
    static char const PROGMEM* const PROGMEM NAMES[MOVEMENT_CLASS_COUNT] = {
        F("Step"),
        F("Leap"),
        F("Charge"),
    };
    return NAMES[movement];
}

char const PROGMEM* party_attack_name(uint8_t attack) {
    static char const PROGMEM* const PROGMEM NAMES[ATTACK_CLASS_COUNT] = {
        F("Strike"), F("Shoot"), F("Reach"), F("Impact"), F("Rend"),
    };
    return NAMES[attack];
}

char const PROGMEM* party_special_name(uint8_t attack) {
    return BATTLE_SPECIAL_NAMES[attack];
}

// The starting specials' descriptions. Their numbers are the bonuses in SPECIAL_INFO and the
// normal damage in ATTACK_INFO, and native tests check both against those tables. The cost and
// range come from the tables at display time.
char const PROGMEM* party_special_text(uint8_t attack) {
    static char const PROGMEM* const PROGMEM TEXTS[ATTACK_CLASS_COUNT] = {
        F("Targets an enemy in a straight line. Adds 3 damage to the normal attack against any "
          "enemy."),
        F("Targets an enemy in a straight line. Adds 4 damage against an enemy at full HP, and 1 "
          "otherwise."),
        F("Straight-line target. Adds 2 damage, and knocks an enemy that survives back one cell if "
          "the cell behind it is empty."),
        F("Straight-line target. Adds 3 damage, and knocks an enemy that survives back one cell if "
          "the cell behind it is empty."),
        F("Adds to Rend's normal attack, which deals 2 damage, or 4 below half HP. Adds 1 at full "
          "HP, and 3 below full HP."),
    };
    return TEXTS[attack];
}

char const PROGMEM* party_perk_name(uint8_t code) {
    if(code < PERK_EQUIPMENT) {
        if(code % PERK_TRACK_RANKS)
            return F("Future rank");
        static char const PROGMEM* const PROGMEM NAMES[PARTY_PERK_TRACK_COUNT] = {
#define MOVEMENT_PERK(cls, id, amount, range, name, text) F(name),
#define ATTACK_PERK MOVEMENT_PERK
#include "perks.def"
#undef ATTACK_PERK
#undef MOVEMENT_PERK
        };
        return NAMES[code / PERK_TRACK_RANKS];
    }
    return preview_equipment_name(code - PERK_EQUIPMENT);
}

char const PROGMEM* party_perk_text(uint8_t code) {
    if(code < PERK_EQUIPMENT) {
        if(code % PERK_TRACK_RANKS)
            return F("This rank is reserved for a future version.");
        static char const PROGMEM* const PROGMEM TEXTS[PARTY_PERK_TRACK_COUNT] = {
#define MOVEMENT_PERK(cls, id, amount, range, name, text) F(text),
#define ATTACK_PERK MOVEMENT_PERK
#include "perks.def"
#undef ATTACK_PERK
#undef MOVEMENT_PERK
        };
        return TEXTS[code / PERK_TRACK_RANKS];
    }
    return preview_equipment_text(code - PERK_EQUIPMENT);
}

char const PROGMEM* party_perk_source(uint8_t code) {
    return code < PERK_EQUIPMENT ? nullptr : preview_equipment_source(code - PERK_EQUIPMENT);
}

uint8_t party_perk_track(uint8_t code) {
    return code < PERK_EQUIPMENT ? (uint8_t)(code / PERK_TRACK_RANKS) : PARTY_NO_TRACK;
}

char const PROGMEM* party_track_name(uint8_t track) {
    if(track < MOVEMENT_CLASS_COUNT)
        return party_movement_name(track);
    return party_attack_name((uint8_t)(track - MOVEMENT_CLASS_COUNT));
}

char const PROGMEM* party_item_name(uint8_t code) {
    return preview_item_name(party_item_slot(code), code % PREVIEW_ITEMS_PER_SLOT);
}

char const PROGMEM* party_item_text(uint8_t code) {
    return preview_item_text(party_item_slot(code), code % PREVIEW_ITEMS_PER_SLOT);
}

char const PROGMEM* party_slot_name(uint8_t slot) {
    static char const PROGMEM* const PROGMEM NAMES[SLOT_COUNT] = {
        F("Weapon"),
        F("Armor"),
        F("Relic"),
    };
    return NAMES[slot];
}

char const PROGMEM* party_equipped(PartyMember const&, uint8_t) {
    return nullptr;
}
