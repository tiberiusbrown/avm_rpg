#include "party_ui.h"
#include "battle.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// These tests open Party directly and never enter world dialogue.
bool dialogue_show(char const PROGMEM*) { return false; }
bool dialogue_show(NPC_ID, char const PROGMEM*) { return false; }
bool dialogue_ask(char const PROGMEM*) { return false; }
bool dialogue_ask(NPC_ID, char const PROGMEM*) { return false; }

static unsigned checks;
#define CHECK(value)                                                                               \
    do {                                                                                           \
        ++checks;                                                                                  \
        if(!(value)) {                                                                             \
            fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #value);                        \
            exit(1);                                                                               \
        }                                                                                          \
    } while(0)

// The 5x7 font advances six pixels for every character, so a range's width is exact here.
struct SixPixels {
    uint16_t operator()(char const PROGMEM* text, uint8_t from, uint8_t to) const {
        (void)text;
        return (uint16_t)((to - from) * 6);
    }
};

// The room a list row gives its text: a track rank's name follows its number and starts 18 pixels
// in from the margin, and other rows start 10 pixels in. The panel's text ends at 124 pixels.
static constexpr uint16_t RANK_NAME_ROOM = 102;
static constexpr uint16_t ROW_TEXT_ROOM = 114;

static PartyMember make_member(NPC_ID id, uint8_t movement, uint8_t attack) {
    PartyMember member{};
    member.id = id;
    member.level = 1;
    member.movement_class = movement;
    member.attack_class = attack;
    member.hp = party_max_hp(member);
    member.ap = party_max_ap(member);
    return member;
}

// Four members with different movement and attack classes, so a switch changes what the track and
// special pages show.
static Party make_party(uint8_t count) {
    Party party{};
    party.members[0] = make_member(NPC_NONE, MOVEMENT_STEP, ATTACK_STRIKE);
    party.members[1] = make_member(NPC_ROWAN, MOVEMENT_LEAP, ATTACK_SHOOT);
    party.members[2] = make_member(NPC_NONE, MOVEMENT_CHARGE, ATTACK_REND);
    party.members[3] = make_member(NPC_NONE, MOVEMENT_STEP, ATTACK_REACH);
    party.count = count;
    return party;
}

// One frame of presses, measured the way the screen measures a description when a press opens one.
static PARTY_EVENT press(PartyUi& ui, Party const& party, uint8_t buttons) {
    PARTY_PAGE before = ui.page;
    PARTY_EVENT event = party_ui_press(ui, party, buttons);
    if(ui.page != before && party_ui_describes(ui.page))
        party_ui_layout(ui, party, SixPixels{});
    return event;
}

// Opens a category from the overview, which is where the Party starts.
static void open_category(PartyUi& ui, Party const& party, PARTY_CATEGORY category) {
    party_ui_open(ui);
    for(uint8_t i = 0; i < category; ++i)
        CHECK(press(ui, party, BTN_DOWN) == PARTY_EVENT_TICK);
    CHECK(press(ui, party, BTN_A) == PARTY_EVENT_SELECT);
}

// Opens the perk index, then the row, from the overview of the first member.
static void open_perk_row(PartyUi& ui, Party const& party, PARTY_PERK_ROW row) {
    open_category(ui, party, CATEGORY_PERKS);
    for(uint8_t i = 0; i < row; ++i)
        CHECK(press(ui, party, BTN_DOWN) == PARTY_EVENT_TICK);
    CHECK(press(ui, party, BTN_A) == PARTY_EVENT_SELECT);
}

static void test_open_state() {
    Party party = make_party(2);
    PartyUi ui{};
    party_ui_open(ui);
    CHECK(ui.page == PARTY_PAGE_OVERVIEW);
    CHECK(ui.member == 0);
    CHECK(ui.selection[PARTY_PAGE_OVERVIEW] == 0);
    CHECK(party_ui_entries(ui, party) == CATEGORY_COUNT);
    CHECK(party_ui_press(ui, party, 0) == PARTY_EVENT_NONE);
}

static void test_overview_categories_and_close() {
    Party party = make_party(1);
    PartyUi ui{};
    party_ui_open(ui);
    // Up from the first category wraps to the last, as the Journal's list does.
    CHECK(press(ui, party, BTN_UP) == PARTY_EVENT_TICK);
    CHECK(ui.selection[PARTY_PAGE_OVERVIEW] == CATEGORY_EQUIPMENT);
    CHECK(press(ui, party, BTN_DOWN) == PARTY_EVENT_TICK);
    CHECK(ui.selection[PARTY_PAGE_OVERVIEW] == CATEGORY_SPECIALS);
    CHECK(press(ui, party, BTN_DOWN) == PARTY_EVENT_TICK);
    // A opens the category, and the Party stays open.
    CHECK(press(ui, party, BTN_A) == PARTY_EVENT_SELECT);
    CHECK(ui.page == PARTY_PAGE_PERKS);
    CHECK(press(ui, party, BTN_B) == PARTY_EVENT_CANCEL);
    CHECK(ui.page == PARTY_PAGE_OVERVIEW);
    // The highlight stays on the category that was opened.
    CHECK(ui.selection[PARTY_PAGE_OVERVIEW] == CATEGORY_PERKS);
    CHECK(press(ui, party, BTN_B) == PARTY_EVENT_CLOSE);
    // A single member has no other page to turn to, so the press answers with the edge.
    CHECK(press(ui, party, BTN_RIGHT) == PARTY_EVENT_EDGE);
    CHECK(ui.member == 0);
}

static void test_member_switching_wraps() {
    Party party = make_party(3);
    PartyUi ui{};
    party_ui_open(ui);
    CHECK(press(ui, party, BTN_RIGHT) == PARTY_EVENT_TICK && ui.member == 1);
    CHECK(press(ui, party, BTN_RIGHT) == PARTY_EVENT_TICK && ui.member == 2);
    CHECK(press(ui, party, BTN_RIGHT) == PARTY_EVENT_TICK && ui.member == 0);
    CHECK(press(ui, party, BTN_LEFT) == PARTY_EVENT_TICK && ui.member == 2);
    CHECK(press(ui, party, BTN_LEFT) == PARTY_EVENT_TICK && ui.member == 1);
    // Switching members keeps the overview's category.
    CHECK(ui.page == PARTY_PAGE_OVERVIEW && ui.selection[PARTY_PAGE_OVERVIEW] == CATEGORY_SPECIALS);
}

static void test_one_member_party() {
    Party party = make_party(1);
    PartyUi ui{};
    party_ui_open(ui);
    CHECK(press(ui, party, BTN_LEFT) == PARTY_EVENT_EDGE && ui.member == 0);
    CHECK(press(ui, party, BTN_RIGHT) == PARTY_EVENT_EDGE && ui.member == 0);
    open_category(ui, party, CATEGORY_SPECIALS);
    CHECK(press(ui, party, BTN_RIGHT) == PARTY_EVENT_EDGE && ui.member == 0);
    CHECK(ui.page == PARTY_PAGE_SPECIALS);
}

static void test_member_switch_keeps_section() {
    Party party = make_party(3);
    PartyUi ui{};
    open_category(ui, party, CATEGORY_SPECIALS);
    CHECK(press(ui, party, BTN_RIGHT) == PARTY_EVENT_TICK);
    CHECK(ui.page == PARTY_PAGE_SPECIALS && ui.member == 1);
    CHECK(press(ui, party, BTN_B) == PARTY_EVENT_CANCEL);
    CHECK(press(ui, party, BTN_DOWN) == PARTY_EVENT_TICK);
    CHECK(press(ui, party, BTN_A) == PARTY_EVENT_SELECT);
    CHECK(ui.page == PARTY_PAGE_PERKS);
    CHECK(press(ui, party, BTN_LEFT) == PARTY_EVENT_TICK && ui.member == 0);
    CHECK(ui.page == PARTY_PAGE_PERKS);
    CHECK(press(ui, party, BTN_B) == PARTY_EVENT_CANCEL);
    CHECK(press(ui, party, BTN_DOWN) == PARTY_EVENT_TICK);
    CHECK(press(ui, party, BTN_A) == PARTY_EVENT_SELECT);
    CHECK(ui.page == PARTY_PAGE_EQUIPMENT);
    CHECK(press(ui, party, BTN_RIGHT) == PARTY_EVENT_TICK && ui.member == 1);
    CHECK(ui.page == PARTY_PAGE_EQUIPMENT);
}

// The track, details, choice and item pages belong to one member, so a switch does nothing there.
static void test_no_member_switch_inside_lists_and_details() {
    Party party = make_party(3);
    PartyUi ui{};
    open_perk_row(ui, party, PERK_ROW_MOVEMENT);
    CHECK(ui.page == PARTY_PAGE_PERK_LIST);
    CHECK(press(ui, party, BTN_RIGHT) == PARTY_EVENT_NONE && ui.member == 0);
    CHECK(press(ui, party, BTN_A) == PARTY_EVENT_SELECT);
    CHECK(ui.page == PARTY_PAGE_PERK_DETAILS);
    CHECK(press(ui, party, BTN_LEFT) == PARTY_EVENT_NONE && ui.member == 0);
    CHECK(press(ui, party, BTN_B) == PARTY_EVENT_CANCEL);
    CHECK(press(ui, party, BTN_B) == PARTY_EVENT_CANCEL);
    CHECK(ui.page == PARTY_PAGE_PERKS);

    open_category(ui, party, CATEGORY_EQUIPMENT);
    CHECK(press(ui, party, BTN_A) == PARTY_EVENT_SELECT);
    CHECK(ui.page == PARTY_PAGE_ITEM_LIST);
    CHECK(press(ui, party, BTN_RIGHT) == PARTY_EVENT_NONE && ui.member == 0);
}

// After a member change, every highlight is clamped to the entries that exist, even when a stale
// value is left behind.
static void test_member_change_clamps_highlight() {
    Party party = make_party(2);
    PartyUi ui{};
    open_category(ui, party, CATEGORY_EQUIPMENT);
    ui.selection[PARTY_PAGE_EQUIPMENT] = 7;
    CHECK(press(ui, party, BTN_RIGHT) == PARTY_EVENT_TICK);
    CHECK(ui.selection[PARTY_PAGE_EQUIPMENT] == SLOT_COUNT - 1);
    CHECK(press(ui, party, BTN_B) == PARTY_EVENT_CANCEL);

    open_category(ui, party, CATEGORY_SPECIALS);
    ui.selection[PARTY_PAGE_SPECIALS] = 3;
    CHECK(press(ui, party, BTN_RIGHT) == PARTY_EVENT_TICK);
    CHECK(ui.selection[PARTY_PAGE_SPECIALS] == 0);
}

static void test_list_bounds_and_wrap() {
    Party party = make_party(2);
    PartyUi ui{};
    open_category(ui, party, CATEGORY_SPECIALS);
    // The one special leaves up and down nowhere to go.
    CHECK(press(ui, party, BTN_DOWN) == PARTY_EVENT_EDGE && ui.selection[PARTY_PAGE_SPECIALS] == 0);
    CHECK(press(ui, party, BTN_UP) == PARTY_EVENT_EDGE && ui.selection[PARTY_PAGE_SPECIALS] == 0);
    CHECK(press(ui, party, BTN_B) == PARTY_EVENT_CANCEL);

    open_perk_row(ui, party, PERK_ROW_EQUIPMENT);
    CHECK(party_ui_entries(ui, party) == 0);
    CHECK(press(ui, party, BTN_A) == PARTY_EVENT_EDGE);
    CHECK(press(ui, party, BTN_B) == PARTY_EVENT_CANCEL);
    CHECK(press(ui, party, BTN_UP) == PARTY_EVENT_TICK);
    CHECK(press(ui, party, BTN_A) == PARTY_EVENT_SELECT);
    CHECK(party_ui_entries(ui, party) == PERK_TRACK_RANKS);
    CHECK(press(ui, party, BTN_UP) == PARTY_EVENT_TICK && ui.selection[PARTY_PAGE_PERK_LIST] == PERK_TRACK_RANKS - 1);
    CHECK(press(ui, party, BTN_DOWN) == PARTY_EVENT_TICK &&
          ui.selection[PARTY_PAGE_PERK_LIST] == 0);
}

static void test_specials_for_each_attack_class() {
    static char const* const NAMES[ATTACK_CLASS_COUNT] = {
        "Power Strike", "Opening Shot", "Driving Thrust", "Crushing Bash", "Wound Hunter",
    };
    for(uint8_t attack = 0; attack < ATTACK_CLASS_COUNT; ++attack) {
        Party party = make_party(1);
        party.members[0] = make_member(NPC_NONE, MOVEMENT_STEP, attack);
        PartyUi ui{};
        open_category(ui, party, CATEGORY_SPECIALS);
        CHECK(party_special_count(party.members[0]) == 1);
        CHECK(press(ui, party, BTN_A) == PARTY_EVENT_SELECT);
        CHECK(ui.page == PARTY_PAGE_SPECIAL_DETAILS && ui.scroll == 0);

        CHECK(strcmp(party_special_name(attack), NAMES[attack]) == 0);
        CHECK(SPECIAL_INFO[attack].ap == 2);
        char const* text = party_ui_text(ui, party);
        CHECK(text != nullptr);
        // The bonus against a full-HP victim, and the bonus against any other victim when it
        // differs.
        char number[16];
        snprintf(number, sizeof number, "Adds %u", SPECIAL_INFO[attack].bonus_full);
        CHECK(strstr(text, number) != nullptr);
        if(SPECIAL_INFO[attack].bonus_injured != SPECIAL_INFO[attack].bonus_full) {
            snprintf(number, sizeof number, "and %u", SPECIAL_INFO[attack].bonus_injured);
            CHECK(strstr(text, number) != nullptr);
        }
        CHECK((strstr(text, "knocks") != nullptr) ==
              (SPECIAL_INFO[attack].effect == BATTLE_EFFECT_KNOCKBACK));
        if(attack == ATTACK_REND) {
            // Rend's normal attack is the one that depends on the victim's HP.
            snprintf(number, sizeof number, "deals %u", ATTACK_INFO[ATTACK_REND].damage);
            CHECK(strstr(text, number) != nullptr);
            snprintf(number, sizeof number, "%u below half",
                     ATTACK_INFO[ATTACK_REND].injured_damage);
            CHECK(strstr(text, number) != nullptr);
        }

        // A detail page has no action for A, and B climbs back to the list, then to the overview.
        CHECK(press(ui, party, BTN_A) == PARTY_EVENT_EDGE && ui.page == PARTY_PAGE_SPECIAL_DETAILS);
        CHECK(press(ui, party, BTN_B) == PARTY_EVENT_CANCEL && ui.page == PARTY_PAGE_SPECIALS);
        CHECK(press(ui, party, BTN_B) == PARTY_EVENT_CANCEL && ui.page == PARTY_PAGE_OVERVIEW);
    }
}

static void test_perk_tracks_follow_classes() {
    Party party = make_party(4);
    PartyUi ui{};
    // Member 0 moves by Step and strikes, so its movement track is Step.
    open_perk_row(ui, party, PERK_ROW_MOVEMENT);
    CHECK(ui.page == PARTY_PAGE_PERK_LIST);
    CHECK(party_ui_entries(ui, party) == PERK_TRACK_RANKS);
    for(uint8_t rank = 0; rank < PERK_TRACK_RANKS; ++rank) {
        uint8_t code = party_ui_row_perk(ui, party, rank);
        CHECK(party_perk_track(code) == MOVEMENT_STEP);
        CHECK(party_perk_source(code) == nullptr);
    }
    CHECK(strcmp(party_track_name(MOVEMENT_STEP), "Step") == 0);

    // Member 1 moves by Leap and shoots, so its movement track is Leap and its attack track is
    // Shoot.
    PartyUi other{};
    open_category(other, party, CATEGORY_PERKS);
    CHECK(press(other, party, BTN_RIGHT) == PARTY_EVENT_TICK && other.member == 1);
    CHECK(press(other, party, BTN_A) == PARTY_EVENT_SELECT);
    CHECK(party_perk_track(party_ui_row_perk(other, party, 0)) == MOVEMENT_LEAP);
    CHECK(press(other, party, BTN_B) == PARTY_EVENT_CANCEL);
    CHECK(press(other, party, BTN_DOWN) == PARTY_EVENT_TICK);
    CHECK(press(other, party, BTN_A) == PARTY_EVENT_SELECT);
    uint8_t attack_track = party_perk_track(party_ui_row_perk(other, party, 0));
    CHECK(attack_track == (uint8_t)MOVEMENT_CLASS_COUNT + ATTACK_SHOOT);
    CHECK(strcmp(party_track_name(attack_track), "Shoot") == 0);
    CHECK(party_perk_code(attack_track, 0) == party_ui_row_perk(other, party, 0));
}

static void test_perk_index_keeps_rows_on_return() {
    Party party = make_party(1);
    party.members[0].equipment[SLOT_WEAPON] = equipment_encode(ITEM_IRON_FANG);
    party.members[0].equipment[SLOT_RELIC] = equipment_encode(ITEM_VITAL_PENDANT);
    PartyUi ui{};
    open_perk_row(ui, party, PERK_ROW_EQUIPMENT);
    CHECK(ui.page == PARTY_PAGE_PERK_LIST);
    CHECK(press(ui, party, BTN_DOWN) == PARTY_EVENT_TICK);
    CHECK(ui.selection[PARTY_PAGE_PERK_LIST] == 1);
    CHECK(press(ui, party, BTN_A) == PARTY_EVENT_SELECT);
    CHECK(ui.page == PARTY_PAGE_PERK_DETAILS && ui.back == PARTY_PAGE_PERK_LIST);
    CHECK(ui.entry == PERK_EQUIPMENT + ITEM_VITAL_PENDANT);
    CHECK(party_perk_track(ui.entry) == PARTY_NO_TRACK);
    CHECK(party_perk_source(ui.entry) != nullptr);
    // Returning from the details keeps the entry, and returning from the list keeps the row.
    CHECK(press(ui, party, BTN_B) == PARTY_EVENT_CANCEL);
    CHECK(ui.page == PARTY_PAGE_PERK_LIST && ui.selection[PARTY_PAGE_PERK_LIST] == 1);
    CHECK(press(ui, party, BTN_B) == PARTY_EVENT_CANCEL);
    CHECK(ui.page == PARTY_PAGE_PERKS && ui.selection[PARTY_PAGE_PERKS] == PERK_ROW_EQUIPMENT);
}

static void test_perk_selection() {
    Party party = make_party(4);
    PartyUi ui{};
    // Member 2 moves by Charge and rends.
    PartyMember& member = party.members[2];
    CHECK(party_gain_experience(member, 10) == 1);
    CHECK(party_unspent_perks(member) == 1);
    CHECK(party_perk_status(member, PERK_MOVEMENT, 1) == PERK_STATUS_LEARNABLE);
    CHECK(party_perk_status(member, PERK_MOVEMENT, 2) == PERK_STATUS_FUTURE);
    open_category(ui, party, CATEGORY_PERKS);
    CHECK(press(ui, party, BTN_RIGHT) == PARTY_EVENT_TICK && ui.member == 1);
    CHECK(press(ui, party, BTN_RIGHT) == PARTY_EVENT_TICK && ui.member == 2);
    CHECK(press(ui, party, BTN_UP) == PARTY_EVENT_TICK && ui.selection[PARTY_PAGE_PERKS] == 3);
    CHECK(press(ui, party, BTN_A) == PARTY_EVENT_SELECT);
    CHECK(ui.page == PARTY_PAGE_PERK_CHOICE);
    CHECK(party_ui_entries(ui, party) == 2);
    CHECK(party_ui_row_perk(ui, party, 0) == party_perk_code(MOVEMENT_CHARGE, 0));
    CHECK(party_ui_row_perk(ui, party, 1) ==
          party_perk_code((uint8_t)MOVEMENT_CLASS_COUNT + ATTACK_REND, 0));

    // Inspection and cancellation leave the selection unspent.
    CHECK(press(ui, party, BTN_DOWN) == PARTY_EVENT_TICK &&
          ui.selection[PARTY_PAGE_PERK_CHOICE] == 1);
    CHECK(press(ui, party, BTN_DOWN) == PARTY_EVENT_TICK &&
          ui.selection[PARTY_PAGE_PERK_CHOICE] == 0);
    CHECK(press(ui, party, BTN_DOWN) == PARTY_EVENT_TICK &&
          ui.selection[PARTY_PAGE_PERK_CHOICE] == 1);
    CHECK(press(ui, party, BTN_A) == PARTY_EVENT_SELECT);
    CHECK(ui.page == PARTY_PAGE_PERK_DETAILS && ui.back == PARTY_PAGE_PERK_CHOICE);
    CHECK(ui.entry == party_perk_code((uint8_t)MOVEMENT_CLASS_COUNT + ATTACK_REND, 0));
    CHECK(press(ui, party, BTN_A) == PARTY_EVENT_SELECT && ui.page == PARTY_PAGE_PERK_CONFIRM);
    CHECK(press(ui, party, 0) == PARTY_EVENT_NONE);
    CHECK(press(ui, party, BTN_B | BTN_A) == PARTY_EVENT_CANCEL);
    CHECK(ui.page == PARTY_PAGE_PERK_DETAILS && party_unspent_perks(member) == 1);
    CHECK(press(ui, party, BTN_A) == PARTY_EVENT_SELECT);
    CHECK(press(ui, party, BTN_A) == PARTY_EVENT_LEARN);
    CHECK(party_unspent_perks(member) == 1);
    CHECK(party_learn_perk(member, party_ui_choice_track(ui)));
    party_ui_perk_learned(ui);
    CHECK(ui.page == PARTY_PAGE_PERK_CHOICE && ui.perk_learned);
    CHECK(party_perk_status(member, PERK_ATTACK, 1) == PERK_STATUS_EARNED);
    CHECK(party_perk_status(member, PERK_MOVEMENT, 1) == PERK_STATUS_LOCKED);
    CHECK(party_unspent_perks(member) == 0);
    CHECK(press(ui, party, BTN_A) == PARTY_EVENT_EDGE);

    CHECK(party_gain_experience(member, 35) == 2 && member.level == 4);
    CHECK(party_unspent_perks(member) == 1);
    CHECK(press(ui, party, BTN_A) == PARTY_EVENT_EDGE);
    CHECK(press(ui, party, BTN_UP) == PARTY_EVENT_TICK);
    CHECK(press(ui, party, BTN_A) == PARTY_EVENT_SELECT);
    CHECK(press(ui, party, BTN_A) == PARTY_EVENT_SELECT);
    CHECK(press(ui, party, BTN_A) == PARTY_EVENT_LEARN);
    CHECK(party_learn_perk(member, party_ui_choice_track(ui)));
    party_ui_perk_learned(ui);
    party_gain_experience(member, 255);
    CHECK(member.level == 10 && party_unspent_perks(member) == 3);
    CHECK(press(ui, party, BTN_A) == PARTY_EVENT_EDGE);
    CHECK(press(ui, party, BTN_DOWN) == PARTY_EVENT_TICK);
    CHECK(press(ui, party, BTN_A) == PARTY_EVENT_EDGE);
    CHECK(press(ui, party, BTN_B) == PARTY_EVENT_CANCEL && ui.page == PARTY_PAGE_PERKS);
    CHECK(press(ui, party, BTN_LEFT) == PARTY_EVENT_TICK && ui.member == 1);
    CHECK(party_unspent_perks(party.members[ui.member]) == 0);
}

static void test_equipment_flow() {
    game_start();
    game.party = make_party(2);
    Party const& party = game.party;
    PartyUi ui{};
    open_category(ui, party, CATEGORY_EQUIPMENT);
    CHECK(ui.page == PARTY_PAGE_EQUIPMENT);
    for(uint8_t slot = 0; slot < SLOT_COUNT; ++slot)
        CHECK(strcmp(party_equipped(party.members[0], slot), "(None)") == 0);

    CHECK(press(ui, party, BTN_DOWN) == PARTY_EVENT_TICK);
    CHECK(press(ui, party, BTN_A) == PARTY_EVENT_SELECT);
    CHECK(party_ui_entries(ui, party) == 1);
    CHECK(press(ui, party, BTN_A) == PARTY_EVENT_UNEQUIP);
    CHECK(!equipment_unequip(ui.member, party_ui_slot(ui)));
    CHECK(press(ui, party, BTN_B) == PARTY_EVENT_CANCEL);
    CHECK(press(ui, party, BTN_UP) == PARTY_EVENT_TICK);

    for(uint8_t member = 0; member < party.count; ++member) {
        CHECK(ui.member == member);
        CHECK(press(ui, party, BTN_A) == PARTY_EVENT_SELECT);
        CHECK(party_ui_entries(ui, party) == 2);
        CHECK(party_ui_row_item(ui, 0) == ITEM_NONE);
        CHECK(party_ui_row_item(ui, 1) == ITEM_IRON_FANG);
        CHECK(press(ui, party, BTN_DOWN) == PARTY_EVENT_TICK);
        CHECK(press(ui, party, BTN_A) == PARTY_EVENT_SELECT);
        CHECK(ui.page == PARTY_PAGE_ITEM_DETAILS && ui.entry == ITEM_IRON_FANG);
        CHECK(strstr(party_ui_text(ui, party), "+1 damage"));
        CHECK(press(ui, party, BTN_A) == PARTY_EVENT_EQUIP);
        CHECK(equipment_equip(ui.member, (ITEM_ID)ui.entry));
        CHECK(equipment_wearer(ITEM_IRON_FANG) == member);
        CHECK(strcmp(party_equipped(party.members[member], SLOT_WEAPON), "Iron Fang") == 0);
        CHECK(press(ui, party, BTN_B) == PARTY_EVENT_CANCEL);
        CHECK(ui.selection[PARTY_PAGE_ITEM_LIST] == 1);
        CHECK(press(ui, party, BTN_B) == PARTY_EVENT_CANCEL);
        CHECK(press(ui, party, BTN_RIGHT) == PARTY_EVENT_TICK);
    }

    CHECK(press(ui, party, BTN_RIGHT) == PARTY_EVENT_TICK);
    CHECK(press(ui, party, BTN_B) == PARTY_EVENT_CANCEL);
    CHECK(press(ui, party, BTN_UP) == PARTY_EVENT_TICK);
    CHECK(press(ui, party, BTN_A) == PARTY_EVENT_SELECT);
    CHECK(press(ui, party, BTN_DOWN) == PARTY_EVENT_TICK);
    CHECK(press(ui, party, BTN_DOWN) == PARTY_EVENT_TICK);
    CHECK(press(ui, party, BTN_A) == PARTY_EVENT_SELECT);
    CHECK(party_ui_entries(ui, party) == 1);
    CHECK(party_ui_row_perk(ui, party, 0) == PERK_EQUIPMENT + ITEM_IRON_FANG);
    CHECK(equipment_unequip(ui.member, SLOT_WEAPON));
    CHECK(party_ui_entries(ui, party) == 0);
}

static void test_catalog_entries_are_complete() {
    for(uint8_t id = 0; id < ITEM_DEFINED_COUNT; ++id) {
        auto item = equipment_item((ITEM_ID)id);
        CHECK(item && item->slot < SLOT_COUNT);
        CHECK(*item->name && *item->description);
        CHECK(strlen(item->name) * 6 <= ROW_TEXT_ROOM);
    }
    for(uint8_t slot = 0; slot < SLOT_COUNT; ++slot)
        CHECK(party_slot_name(slot) != nullptr && *party_slot_name(slot) != 0);
    for(uint8_t track = 0; track < PARTY_PERK_TRACK_COUNT; ++track) {
        CHECK(party_track_name(track) != nullptr && *party_track_name(track) != 0);
        for(uint8_t rank = 0; rank < PERK_TRACK_RANKS; ++rank) {
            uint8_t code = party_perk_code(track, rank);
            CHECK(party_perk_track(code) == track);
            CHECK(party_perk_name(code) != nullptr && *party_perk_name(code) != 0);
            CHECK(party_perk_text(code) != nullptr && *party_perk_text(code) != 0);
            CHECK(strlen(party_perk_name(code)) * 6 <= RANK_NAME_ROOM);
        }
    }
    for(uint8_t index = 0; index < ITEM_DEFINED_COUNT; ++index) {
        uint8_t code = (uint8_t)(PERK_EQUIPMENT + index);
        CHECK(party_perk_track(code) == PARTY_NO_TRACK);
        CHECK(party_perk_name(code) != nullptr && *party_perk_name(code) != 0);
        CHECK(party_perk_text(code) != nullptr && *party_perk_text(code) != 0);
        CHECK(party_perk_source(code) != nullptr && *party_perk_source(code) != 0);
        CHECK(strlen(party_perk_name(code)) * 6 <= ROW_TEXT_ROOM);
        CHECK(strlen(party_perk_source(code)) * 6 <= ROW_TEXT_ROOM);
    }
    for(uint8_t attack = 0; attack < ATTACK_CLASS_COUNT; ++attack) {
        CHECK(party_special_name(attack) != nullptr && *party_special_name(attack) != 0);
        CHECK(strlen(party_special_name(attack)) * 6 <= ROW_TEXT_ROOM);
    }
}

// Every shipped description wraps within the panel and none is truncated. The same width is used
// on the screen, so a long description cannot spill past the border.
static void check_wrap(char const* text) {
    uint8_t starts[PARTY_TEXT_LINES + 1];
    uint8_t count = text_wrap(text, PARTY_TEXT_WIDTH, SixPixels{}, starts, PARTY_TEXT_LINES);
    CHECK(count >= 1 && count <= PARTY_TEXT_LINES);
    CHECK(starts[count] == strlen(text));
    for(uint8_t line = 0; line < count; ++line) {
        uint8_t to = starts[line + 1];
        while(to > starts[line] && text[to - 1] == ' ')
            --to;
        CHECK(SixPixels{}(text, starts[line], to) <= PARTY_TEXT_WIDTH);
    }
}

static void test_perk_descriptions() {
    static constexpr char const* NAMES[] = {
        "Steadfast", "Renewal", "Momentum", "Keen Edge", "Longshot", "Lunge", "Heavy Blow",
        "Blood Sense",
    };
    static constexpr char const* EFFECTS[] = {
        "Maximum HP increases by 3", "1 additional AP after each victory",
        "two-cell move deals 1 extra engagement damage", "at full HP", "at range 4",
        "at range 2", "deal 1 extra damage", "below full HP",
    };
    for(uint8_t track = 0; track < PARTY_PERK_TRACK_COUNT; ++track) {
        uint8_t code = party_perk_code(track, 0);
        CHECK(strcmp(party_perk_name(code), NAMES[track]) == 0);
        CHECK(strstr(party_perk_text(code), EFFECTS[track]));
        CHECK(strcmp(party_perk_name(party_perk_code(track, 1)), "Future rank") == 0);
    }
}

static void test_descriptions_wrap() {
    for(uint8_t track = 0; track < PARTY_PERK_TRACK_COUNT; ++track)
        for(uint8_t rank = 0; rank < PERK_TRACK_RANKS; ++rank)
            check_wrap(party_perk_text(party_perk_code(track, rank)));
    for(uint8_t index = 0; index < ITEM_DEFINED_COUNT; ++index)
        check_wrap(party_perk_text((uint8_t)(PERK_EQUIPMENT + index)));
    for(uint8_t attack = 0; attack < ATTACK_CLASS_COUNT; ++attack)
        check_wrap(party_special_text(attack));
}

// Navigation emits requests and leaves the party unchanged.
static void test_navigation_leaves_party_unchanged() {
    Party party = make_party(4);
    party.members[0].level = 2;
    Party before = party;
    PartyUi ui{};
    open_perk_row(ui, party, PERK_ROW_CHOICE);
    CHECK(press(ui, party, BTN_A) == PARTY_EVENT_SELECT);
    CHECK(press(ui, party, BTN_B) == PARTY_EVENT_CANCEL);
    CHECK(press(ui, party, BTN_DOWN) == PARTY_EVENT_TICK);
    CHECK(press(ui, party, BTN_A) == PARTY_EVENT_SELECT);
    CHECK(press(ui, party, BTN_B) == PARTY_EVENT_CANCEL);
    CHECK(press(ui, party, BTN_B) == PARTY_EVENT_CANCEL);
    CHECK(press(ui, party, BTN_B) == PARTY_EVENT_CANCEL);
    open_category(ui, party, CATEGORY_EQUIPMENT);
    CHECK(press(ui, party, BTN_A) == PARTY_EVENT_SELECT);
    CHECK(press(ui, party, BTN_A) == PARTY_EVENT_UNEQUIP);
    CHECK(press(ui, party, BTN_B) == PARTY_EVENT_CANCEL);
    CHECK(press(ui, party, BTN_B) == PARTY_EVENT_CANCEL);
    CHECK(memcmp(&before, &party, sizeof(Party)) == 0);
}

// Random presses, as a player might make them, keep every index in bounds and never change the
// party. Closing reopens the Party, which must start in its predictable state.
static void test_random_presses_stay_in_bounds() {
    static constexpr uint8_t BUTTONS[] = {BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT, BTN_A, BTN_B};
    srand(20261119);
    for(uint16_t trial = 0; trial < 40; ++trial) {
        uint8_t count = (uint8_t)(1 + trial % PARTY_CAPACITY);
        Party party = make_party(count);
        Party before = party;
        PartyUi ui{};
        party_ui_open(ui);
        for(uint16_t step = 0; step < 600; ++step) {
            uint8_t pressed = BUTTONS[rand() % (int)(sizeof BUTTONS / sizeof BUTTONS[0])];
            if(press(ui, party, pressed) == PARTY_EVENT_CLOSE) {
                CHECK(pressed == BTN_B);
                party_ui_open(ui);
                CHECK(ui.page == PARTY_PAGE_OVERVIEW && ui.member == 0);
                continue;
            }
            CHECK(ui.member < party.count);
            CHECK(ui.page < PARTY_PAGE_COUNT);
            CHECK(ui.back < PARTY_PAGE_COUNT);
            uint8_t entries = party_ui_entries(ui, party);
            if(entries)
                CHECK(ui.selection[ui.page] < entries);
            if(party_ui_describes(ui.page)) {
                CHECK(ui.line_count <= PARTY_TEXT_LINES);
                CHECK(ui.scroll == 0 || ui.scroll + PARTY_TEXT_ROWS <= ui.line_count);
            }
            CHECK(memcmp(&before, &party, sizeof(Party)) == 0);
        }
    }
}

int main() {
    test_open_state();
    test_overview_categories_and_close();
    test_member_switching_wraps();
    test_one_member_party();
    test_member_switch_keeps_section();
    test_no_member_switch_inside_lists_and_details();
    test_member_change_clamps_highlight();
    test_list_bounds_and_wrap();
    test_specials_for_each_attack_class();
    test_perk_tracks_follow_classes();
    test_perk_index_keeps_rows_on_return();
    test_perk_selection();
    test_equipment_flow();
    test_catalog_entries_are_complete();
    test_perk_descriptions();
    test_descriptions_wrap();
    test_navigation_leaves_party_unchanged();
    test_random_presses_stay_in_bounds();
    printf("party_ui: %u checks passed\n", checks);
    return 0;
}
