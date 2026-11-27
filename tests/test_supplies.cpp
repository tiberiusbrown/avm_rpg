#include "pause_menu.h"
#include "supplies_ui.h"
#include "shop_ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if(!(condition)) {                                                                         \
            fprintf(stderr, "FAIL %d: %s\n", __LINE__, #condition);                                \
            exit(1);                                                                               \
        }                                                                                          \
    } while(0)

static Party make_party() {
    Party party{};
    party.count = PARTY_CAPACITY;
    for(auto& member : party.members)
        member = party_new_member(NPC_ALDEN, MOVEMENT_STEP, ATTACK_STRIKE);
    return party;
}

static void supplies_model() {
    Supplies supplies = STARTING_SUPPLIES;
    Party party = make_party();
    auto invalid = (SUPPLY_ID)255;
    CHECK(!supply_valid(invalid) && supply_count(supplies, invalid) == 0);
    CHECK(!supplies_add(supplies, invalid, 1));
    CHECK(!supplies_add(supplies, SUPPLY_POTION, 255));
    CHECK(supplies_add(supplies, SUPPLY_POTION, 13));
    CHECK(!supplies_can_add(supplies, SUPPLY_POTION, 1));
    CHECK(!supplies_add(supplies, SUPPLY_POTION, 1));
    CHECK(supply_count(supplies, SUPPLY_POTION) == SUPPLY_LIMIT);
    CHECK(!supply_use(supplies, party, invalid, 0));
    CHECK(!supply_use(supplies, party, SUPPLY_POTION, SUPPLY_NO_TARGET));

    auto& first = party.members[0];
    auto& second = party.members[1];
    auto& third = party.members[2];
    auto& fourth = party.members[3];
    CHECK(!supply_use(supplies, party, SUPPLY_POTION, 0));
    first.hp = 0;
    CHECK(!supply_use(supplies, party, SUPPLY_POTION, 0));
    CHECK(supply_count(supplies, SUPPLY_POTION) == SUPPLY_LIMIT);
    first.hp = 1;
    CHECK(supply_use(supplies, party, SUPPLY_POTION, 0) && first.hp == 9);
    first.hp = party_max_hp(first) - 1;
    CHECK(supply_use(supplies, party, SUPPLY_POTION, 0) && first.hp == party_max_hp(first));

    first.hp = 1;
    second.hp = party_max_hp(second) - 1;
    third.hp = 0;
    CHECK(!supply_use(supplies, party, SUPPLY_ELIXIR, 0));
    CHECK(supply_use(supplies, party, SUPPLY_ELIXIR, SUPPLY_NO_TARGET));
    CHECK(first.hp == 5 && second.hp == party_max_hp(second) && !third.hp);
    CHECK(fourth.hp == party_max_hp(fourth));
    CHECK(supplies_add(supplies, SUPPLY_ELIXIR, 1));
    first.hp = party_max_hp(first);
    CHECK(!supply_use(supplies, party, SUPPLY_ELIXIR, SUPPLY_NO_TARGET));
    CHECK(supply_count(supplies, SUPPLY_ELIXIR) == 1);

    third.ap = 0;
    CHECK(!supply_use(supplies, party, SUPPLY_REVIVE, 0));
    CHECK(supply_use(supplies, party, SUPPLY_REVIVE, 2));
    CHECK(third.hp == 5 && third.ap == 0);
    third.hp = 0;
    for(unsigned attempt = 0; attempt < 20; ++attempt)
        CHECK(!supply_use(supplies, party, SUPPLY_REVIVE, 2));
    CHECK(supply_count(supplies, SUPPLY_REVIVE) == 0 && third.hp == 0);
    CHECK(!supply_use(supplies, party, SUPPLY_ETHER, 0));
    CHECK(supply_use(supplies, party, SUPPLY_ETHER, 2));
    CHECK(third.hp == 0 && third.ap == (party_max_ap(third) < 3 ? party_max_ap(third) : 3));
    CHECK(supplies_add(supplies, SUPPLY_ETHER, 1));
    third.ap = party_max_ap(third) - 1;
    CHECK(supply_use(supplies, party, SUPPLY_ETHER, 2));
    CHECK(third.hp == 0 && third.ap == party_max_ap(third));
}

static void equipment_supply_modifiers() {
    Supplies supplies = STARTING_SUPPLIES;
    Party party = make_party();
    auto& target = party.members[0];
    auto& cup = party.members[1];
    auto& vest = party.members[2];
    target.equipment[SLOT_ARMOR] = equipment_encode(ITEM_SOFT_BANDAGES);
    cup.equipment[SLOT_RELIC] = equipment_encode(ITEM_SHARED_CUP);
    vest.equipment[SLOT_ARMOR] = equipment_encode(ITEM_HOPPER_VEST);
    target.hp = 1;
    cup.hp = 2;
    vest.hp = 1;
    CHECK(supply_use(supplies, party, SUPPLY_POTION, 0));
    CHECK(target.hp == 11 && cup.hp == 3);
    CHECK(supply_use(supplies, party, SUPPLY_POTION, 1));
    CHECK(cup.hp == 11);
    CHECK(supply_use(supplies, party, SUPPLY_ELIXIR, SUPPLY_NO_TARGET));
    CHECK(target.hp == 12 && cup.hp == 12 && vest.hp == 6 && party_max_hp(vest) == 13);
}

static void recovery() {
    Party party = make_party();
    for(uint8_t i = 0; i < party.count; ++i) {
        auto& member = party.members[i];
        member.level = 4 + i;
        member.experience = i;
        CHECK(party_learn_perk(member, PERK_MOVEMENT));
        CHECK(party_learn_perk(member, PERK_ATTACK));
        member.hp = i;
        member.ap = 0;
    }
    Party expected = party;
    for(auto& member : expected.members) {
        member.hp = party_max_hp(member);
        member.ap = party_max_ap(member);
    }
    party_rest(party);
    CHECK(!memcmp(&party, &expected, sizeof(Party)));
    party_rest(party);
    CHECK(!memcmp(&party, &expected, sizeof(Party)));
}

static void supplies_navigation() {
    for(uint8_t count = 1; count <= PARTY_CAPACITY; ++count) {
        for(uint8_t id = 0; id < SUPPLY_COUNT; ++id) {
            Supplies supplies = STARTING_SUPPLIES;
            Party party = make_party();
            party.count = count;
            auto& member = party.members[count - 1];
            if(id == SUPPLY_REVIVE)
                member.hp = 0;
            else if(id == SUPPLY_ETHER)
                member.ap = 0;
            else
                member.hp = 1;

            Party before = party;
            SuppliesUi ui{};
            auto press = [&](uint8_t pressed) {
                return supplies_press(ui, supplies, party, pressed);
            };
            CHECK(ui.page == SUPPLIES_LIST && ui.supply == SUPPLY_POTION);
            for(uint8_t step = 0; step < id; ++step)
                CHECK(press(BTN_DOWN) == SUPPLIES_MOVE);
            CHECK(ui.supply == id && press(BTN_A) == SUPPLIES_OPEN && ui.page == SUPPLIES_DETAILS);
            CHECK(press(0) == SUPPLIES_NONE && ui.page == SUPPLIES_DETAILS);
            ui.lines = SUPPLIES_DETAIL_LINES + 1;
            CHECK(press(BTN_DOWN) == SUPPLIES_MOVE && ui.scroll == 1);
            CHECK(press(BTN_DOWN) == SUPPLIES_EDGE);
            CHECK(press(BTN_UP) == SUPPLIES_MOVE && !ui.scroll);
            uint8_t owned = supply_count(supplies, ui.supply);
            CHECK(press(BTN_A) == SUPPLIES_OPEN);
            if(id == SUPPLY_ELIXIR)
                CHECK(ui.page == SUPPLIES_CONFIRM && ui.target == SUPPLY_NO_TARGET);
            else {
                CHECK(ui.page == SUPPLIES_TARGET && ui.target == count - 1);
                CHECK(press(BTN_UP) == SUPPLIES_EDGE && ui.target == count - 1);
            }
            CHECK(press(0) == SUPPLIES_NONE);
            CHECK(press(BTN_B) == SUPPLIES_BACK && ui.page == SUPPLIES_DETAILS);
            CHECK(!memcmp(&before, &party, sizeof(Party)));
            CHECK(supply_count(supplies, ui.supply) == owned);
            CHECK(press(BTN_B) == SUPPLIES_BACK && ui.page == SUPPLIES_LIST && ui.supply == id);
            supplies.quantity[id] = 0;
            CHECK(press(BTN_A) == SUPPLIES_OPEN);
            CHECK(press(BTN_A) == SUPPLIES_EDGE && ui.page == SUPPLIES_DETAILS);
            CHECK(!strcmp(ui.feedback, "None remaining"));
            supplies.quantity[id] = owned;
            CHECK(press(BTN_A) == SUPPLIES_OPEN);
            CHECK(press(BTN_A) == SUPPLIES_APPLY);
            CHECK(supply_use(supplies, party, ui.supply, ui.target));
            CHECK(supply_count(supplies, ui.supply) == owned - 1);
        }
    }

    SuppliesUi ui{};
    Party party = make_party();
    CHECK(supplies_press(ui, STARTING_SUPPLIES, party, BTN_A) == SUPPLIES_OPEN);
    CHECK(supplies_press(ui, STARTING_SUPPLIES, party, BTN_A) == SUPPLIES_EDGE);
    CHECK(ui.page == SUPPLIES_DETAILS);
    CHECK(supplies_press(ui, STARTING_SUPPLIES, party, BTN_B) == SUPPLIES_BACK);
    CHECK(supplies_press(ui, STARTING_SUPPLIES, party, BTN_B) == SUPPLIES_CLOSE);
}

static void pause_navigation() {
    auto battle = pause_open(PAUSE_SUPPLIES, true, true, true);
    CHECK(battle.count == 4 && !battle.selected && !battle.first);
    CHECK(pause_item(battle, 1) == PAUSE_RESIGN && pause_item(battle, 3) == PAUSE_PASS);
    auto indoors = pause_open(PAUSE_RESUME, false, false, false);
    CHECK(indoors.count == PAUSE_ROWS - 1);
    CHECK(pause_item(indoors, indoors.count - 1) == PAUSE_AUDIO);
    CHECK(pause_open(PAUSE_RESUME, true, false, false).count == PAUSE_ROWS);
    CHECK(pause_open(PAUSE_RESUME, false, false, true).count == PAUSE_ROWS);

    auto outdoor = pause_open(PAUSE_RESUME, true, false, true);
    CHECK(outdoor.count == PAUSE_ROWS + 1);
    pause_move(outdoor, false);
    CHECK(outdoor.first == 1 && pause_item(outdoor, outdoor.selected) == PAUSE_LOAD);
    pause_move(outdoor, true);
    CHECK(!outdoor.first && pause_item(outdoor, outdoor.selected) == PAUSE_RESUME);
    for(uint8_t row = 1; row < outdoor.count; ++row) {
        pause_move(outdoor, true);
        CHECK(outdoor.selected == row && row >= outdoor.first && row < outdoor.first + PAUSE_ROWS);
    }
    CHECK(outdoor.first == 1);
    for(uint8_t row = outdoor.count - 1; row; --row) {
        pause_move(outdoor, false);
        CHECK(outdoor.selected == row - 1 && outdoor.selected >= outdoor.first);
    }
    CHECK(!outdoor.first);
    auto reopened = pause_open(PAUSE_SUPPLIES, true, false, true);
    CHECK(!reopened.first && pause_item(reopened, reopened.selected) == PAUSE_SUPPLIES);
    CHECK(menu_opens_screen(MENU_SUPPLIES) && menu_opens_screen(MENU_MAP));
    CHECK(!menu_opens_screen(MENU_PASS));
}

static void shop_transactions() {
    constexpr uint8_t prices[] = {12, 22, 30, 18};
    for(uint8_t i = 0; i < SUPPLY_COUNT; ++i) {
        auto item = (SUPPLY_ID)i;
        Supplies supplies = STARTING_SUPPLIES;
        Supplies expected = supplies;
        uint16_t gold = prices[i] * 2;
        CHECK(shop_price(item) == prices[i]);
        CHECK(shop_can_buy(gold, supplies, item) == SHOP_SUCCESS);
        CHECK(shop_buy(gold, supplies, item) == SHOP_SUCCESS);
        ++expected.quantity[i];
        CHECK(gold == prices[i] && !memcmp(&supplies, &expected, sizeof(supplies)));
        CHECK(shop_buy(gold, supplies, item) == SHOP_SUCCESS);
        ++expected.quantity[i];
        CHECK(gold == 0 && !memcmp(&supplies, &expected, sizeof(supplies)));
        CHECK(shop_buy(gold, supplies, item) == SHOP_NOT_ENOUGH_GOLD);
        CHECK(gold == 0 && !memcmp(&supplies, &expected, sizeof(supplies)));

        supplies.quantity[i] = SUPPLY_LIMIT - 1;
        expected.quantity[i] = SUPPLY_LIMIT;
        gold = UINT16_MAX;
        CHECK(shop_buy(gold, supplies, item) == SHOP_SUCCESS);
        CHECK(gold == UINT16_MAX - prices[i] && !memcmp(&supplies, &expected, sizeof(supplies)));
        CHECK(shop_buy(gold, supplies, item) == SHOP_STACK_FULL);
        CHECK(gold == UINT16_MAX - prices[i] && !memcmp(&supplies, &expected, sizeof(supplies)));
        CHECK(shop_buy(gold, supplies, SUPPLY_COUNT) == SHOP_INVALID_ITEM);
        CHECK(shop_buy(gold, supplies, (SUPPLY_ID)255) == SHOP_INVALID_ITEM);
        CHECK(gold == UINT16_MAX - prices[i] && !memcmp(&supplies, &expected, sizeof(supplies)));
    }
}

static void shop_navigation() {
    ShopUi ui{};
    CHECK(ui.page == SHOP_LIST && ui.item == SUPPLY_POTION);
    CHECK(shop_press(ui, 0) == SHOP_NONE);
    CHECK(shop_press(ui, BTN_UP) == SHOP_MOVE && ui.item == SUPPLY_ETHER);
    CHECK(shop_press(ui, BTN_DOWN) == SHOP_MOVE && ui.item == SUPPLY_POTION);
    CHECK(shop_press(ui, BTN_DOWN) == SHOP_MOVE && ui.item == SUPPLY_ELIXIR);
    CHECK(shop_press(ui, BTN_A) == SHOP_OPEN && ui.page == SHOP_DETAILS && ui.scroll == 0);
    ui.lines = SUPPLIES_DETAIL_LINES + 1;
    CHECK(shop_press(ui, BTN_DOWN) == SHOP_MOVE && ui.scroll == 1);
    CHECK(shop_press(ui, BTN_DOWN) == SHOP_EDGE && ui.scroll == 1);
    CHECK(shop_press(ui, BTN_UP) == SHOP_MOVE && ui.scroll == 0);
    CHECK(shop_press(ui, BTN_B) == SHOP_BACK && ui.page == SHOP_LIST);
    CHECK(shop_press(ui, BTN_A) == SHOP_OPEN && ui.page == SHOP_DETAILS);
    CHECK(shop_press(ui, BTN_A) == SHOP_OPEN && ui.page == SHOP_CONFIRM);
    CHECK(shop_press(ui, BTN_DOWN) == SHOP_NONE);
    CHECK(shop_press(ui, BTN_A | BTN_B) == SHOP_BACK && ui.page == SHOP_DETAILS);
    CHECK(shop_press(ui, BTN_A) == SHOP_OPEN && ui.page == SHOP_CONFIRM);
    CHECK(shop_press(ui, 0) == SHOP_NONE);
    CHECK(shop_press(ui, BTN_A) == SHOP_PURCHASE);
    shop_feedback(ui, SHOP_NOT_ENOUGH_GOLD);
    CHECK(ui.page == SHOP_CONFIRM && !strcmp(ui.feedback, "Not enough gold"));
    shop_feedback(ui, SHOP_STACK_FULL);
    CHECK(ui.page == SHOP_CONFIRM && !strcmp(ui.feedback, "Already carrying 15"));
    shop_feedback(ui, SHOP_SUCCESS);
    CHECK(ui.page == SHOP_LIST && ui.item == SUPPLY_ELIXIR && !strcmp(ui.feedback, "Purchased!"));
    CHECK(shop_press(ui, BTN_B) == SHOP_CLOSE);
}

int main() {
    shop_transactions();
    shop_navigation();
    supplies_model();
    equipment_supply_modifiers();
    recovery();
    supplies_navigation();
    pause_navigation();
}
