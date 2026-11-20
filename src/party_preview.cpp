#include "party_preview.h"

char const PROGMEM* preview_equipment_name(uint8_t index) {
    static char const PROGMEM* const PROGMEM NAMES[PREVIEW_EQUIPMENT_COUNT] = {
        F("+1 Attack"),    F("+1 AP Recovery"), F("Boot Guard"),
        F("Thorn Strike"), F("Iron Hide"),      F("Vitality"),
    };
    return NAMES[index];
}

char const PROGMEM* preview_equipment_source(uint8_t index) {
    static char const PROGMEM* const PROGMEM SOURCES[PREVIEW_EQUIPMENT_COUNT] = {
        F("Iron Fang"),   F("Dawn Pendant"), F("Traveler's Boots"),
        F("Thorn Sword"), F("Iron Mail"),    F("Vital Pendant"),
    };
    return SOURCES[index];
}

char const PROGMEM* preview_equipment_text(uint8_t index) {
    static char const PROGMEM* const PROGMEM TEXTS[PREVIEW_EQUIPMENT_COUNT] = {
        F("Adds 1 damage to this member's normal attacks. Sample effect from a weapon."),
        F("Recovers 1 more AP after each victory. Sample effect from a relic."),
        F("The first movement HP cost is prevented in each battle. Sample effect from boots."),
        F("Each move that ends beside an enemy also attacks it automatically. Sample weapon "
          "effect."),
        F("Reduces incoming damage, but each move costs 1 extra HP. Sample effect from armor."),
        F("Increases maximum HP. Sample effect from a relic."),
    };
    return TEXTS[index];
}

char const PROGMEM* preview_item_name(uint8_t slot, uint8_t index) {
    static char const PROGMEM* const PROGMEM NAMES[PREVIEW_SLOT_COUNT][PREVIEW_ITEMS_PER_SLOT] = {
        {F("Iron Fang"), F("Thorn Sword")},
        {F("Traveler's Boots"), F("Iron Mail")},
        {F("Dawn Pendant"), F("Vital Pendant")},
    };
    return NAMES[slot][index];
}

char const PROGMEM* preview_item_text(uint8_t slot, uint8_t index) {
    static char const PROGMEM* const PROGMEM TEXTS[PREVIEW_SLOT_COUNT][PREVIEW_ITEMS_PER_SLOT] = {
        {F("Normal attacks deal +1 damage. Preview equipped by: Rowan (sample only)."),
         F("Each move that ends beside an enemy makes an automatic attack. Sample only.")},
        {F("The first movement HP cost is prevented in each battle. Sample only."),
         F("Reduces incoming damage. Drawback: each move costs 1 extra HP. Sample only.")},
        {F("Recovers 1 more AP after each victory. Preview equipped by: Traveler (sample only)."),
         F("Increases maximum HP. Sample only.")},
    };
    return TEXTS[slot][index];
}
