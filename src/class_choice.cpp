#include "class_choice.h"

// Each description gives the class's rules, then the direction its perks take. Native tests keep
// them within CLASS_TEXT_LINES lines.
char const PROGMEM* class_movement_text(uint8_t movement) {
    switch(movement) {
        case MOVEMENT_STEP:
            return F("Step one cell up, down or sideways. Perks: stand with allies to shield each "
                     "other.");
        case MOVEMENT_LEAP:
            return F("Jump like a chess knight, over anything. Perks: strike hard where you land.");
        case MOVEMENT_CHARGE:
            return F(
                "Rush 1-2 cells in a clear line. Perks: slam, shove and trample what you charge.");
        case MOVEMENT_VAULT:
            return F("Spring exactly 2 cells in a line, over anything. Perks: strike foes you "
                     "cross and land by.");
        case MOVEMENT_GLIDE:
            return F(
                "Slide 1-2 cells diagonally along a clear path. Perks: cut every foe you pass.");
        default:
            return F("");
    }
}

char const PROGMEM* class_attack_text(uint8_t attack) {
    switch(attack) {
        case ATTACK_STRIKE:
            return F(
                "3 damage to a foe beside you. Perks: a duelist who punishes and pins lone foes.");
        case ATTACK_SHOOT:
            return F("3 damage 2-4 cells away in a line, never beside you. Perks: long, piercing "
                     "shots.");
        case ATTACK_REACH:
            return F(
                "3 damage 1-2 cells away in a line. Perks: a pike wall that strikes past allies.");
        case ATTACK_IMPACT:
            return F(
                "2 damage beside you, knocking the foe back. Perks: crash foes into one another.");
        case ATTACK_REND:
            return F(
                "2 damage beside you, 4 to a foe below half HP. Perks: hunt down the wounded.");
        case ATTACK_ARC:
            return F(
                "3 damage to a foe diagonal to you. Perks: sweep every foe around the corners.");
        case ATTACK_DRAIN:
            return F(
                "2 damage beside you, healing 1 HP. Perks: heal more and share life with allies.");
        case ATTACK_RECOIL:
            return F(
                "3 damage beside you, then step back. Perks: hit and run, dragging foes along.");
        case ATTACK_FLANK:
            return F("2 damage beside you, 4 if an ally also stands by the foe. Perks: surround "
                     "and strike.");
        case ATTACK_BLAST:
            return F("2 damage 2-3 cells away, splashing 1 onto neighbors. Perks: bigger, shoving "
                     "bursts.");
        default:
            return F("");
    }
}

char const PROGMEM* class_choice_text(ClassChoiceUi const& ui) {
    if(ui.page == CLASS_PAGE_MOVEMENT_DETAILS)
        return class_movement_text(ui.movement);
    if(ui.page == CLASS_PAGE_ATTACK_DETAILS)
        return class_attack_text(ui.attack);
    return nullptr;
}

void class_choice_open(ClassChoiceUi& ui) {
    PartyMember const& traveler = game.party.members[0];
    ui = {};
    ui.page = CLASS_PAGE_MOVEMENT;
    ui.movement = traveler.movement_class < MOVEMENT_CLASS_COUNT ? traveler.movement_class : 0;
    ui.attack = traveler.attack_class < ATTACK_CLASS_COUNT ? traveler.attack_class : 0;
}

static CLASS_CHOICE_EVENT turn(ClassChoiceUi& ui, CLASS_CHOICE_PAGE page,
                               CLASS_CHOICE_EVENT event) {
    ui.page = page;
    ui.scroll = 0;
    ui.line_count = 0;
    return event;
}

static CLASS_CHOICE_EVENT press_list(ClassChoiceUi& ui, uint8_t pressed, uint8_t& selected,
                                     uint8_t count) {
    if(!(pressed & (BTN_UP | BTN_DOWN)))
        return CLASS_CHOICE_NONE;
    if(pressed & BTN_DOWN)
        selected = selected + 1 < count ? (uint8_t)(selected + 1) : 0;
    else
        selected = selected ? (uint8_t)(selected - 1) : (uint8_t)(count - 1);
    ui.scroll = 0;
    return CLASS_CHOICE_TICK;
}

static CLASS_CHOICE_EVENT scroll(ClassChoiceUi& ui, uint8_t pressed) {
    if(!(pressed & (BTN_UP | BTN_DOWN)))
        return CLASS_CHOICE_NONE;
    if((pressed & BTN_DOWN) && ui.scroll + CLASS_TEXT_ROWS < ui.line_count) {
        ++ui.scroll;
        return CLASS_CHOICE_TICK;
    }
    if((pressed & BTN_UP) && ui.scroll) {
        --ui.scroll;
        return CLASS_CHOICE_TICK;
    }
    return CLASS_CHOICE_EDGE;
}

CLASS_CHOICE_EVENT class_choice_press(ClassChoiceUi& ui, uint8_t pressed) {
    bool forward = pressed & BTN_A, back = !forward && (pressed & BTN_B);
    switch(ui.page) {
        case CLASS_PAGE_MOVEMENT:
            if(forward)
                return turn(ui, CLASS_PAGE_MOVEMENT_DETAILS, CLASS_CHOICE_SELECT);
            if(back)
                return CLASS_CHOICE_EDGE;
            return press_list(ui, pressed, ui.movement, MOVEMENT_CLASS_COUNT);
        case CLASS_PAGE_MOVEMENT_DETAILS:
            if(forward)
                return turn(ui, CLASS_PAGE_ATTACK, CLASS_CHOICE_SELECT);
            if(back)
                return turn(ui, CLASS_PAGE_MOVEMENT, CLASS_CHOICE_CANCEL);
            return scroll(ui, pressed);
        case CLASS_PAGE_ATTACK:
            if(forward)
                return turn(ui, CLASS_PAGE_ATTACK_DETAILS, CLASS_CHOICE_SELECT);
            if(back)
                return turn(ui, CLASS_PAGE_MOVEMENT, CLASS_CHOICE_CANCEL);
            return press_list(ui, pressed, ui.attack, ATTACK_CLASS_COUNT);
        case CLASS_PAGE_ATTACK_DETAILS:
            if(forward)
                return turn(ui, CLASS_PAGE_CONFIRM, CLASS_CHOICE_SELECT);
            if(back)
                return turn(ui, CLASS_PAGE_ATTACK, CLASS_CHOICE_CANCEL);
            return scroll(ui, pressed);
        case CLASS_PAGE_CONFIRM:
            if(forward)
                return CLASS_CHOICE_CONFIRM;
            if(back)
                return turn(ui, CLASS_PAGE_ATTACK, CLASS_CHOICE_CANCEL);
            return CLASS_CHOICE_NONE;
    }
    return CLASS_CHOICE_NONE;
}
