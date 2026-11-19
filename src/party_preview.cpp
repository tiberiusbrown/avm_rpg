#include "party_preview.h"

// Sample perk ranks, one row per track. Rows 0 to 2 are the movement classes (Step, Leap, Charge)
// and rows 3 to 7 the attack classes (Strike, Shoot, Reach, Impact, Rend). Every rank is a
// placeholder, so the Party's perk pages must mark them as preview entries that are not earned.
char const PROGMEM* preview_rank_name(uint8_t track, uint8_t rank) {
    static char const PROGMEM* const PROGMEM NAMES[PREVIEW_TRACK_COUNT][PREVIEW_RANK_COUNT] = {
        {F("Endurance"), F("Engagement"), F("Recovery"), F("Footwork"), F("Vanguard")},
        {F("Long Jump"), F("Landing"), F("Vault"), F("Aerial Guard"), F("Soar")},
        {F("Momentum"), F("Trample"), F("Bulwark"), F("Overrun"), F("Battering Run")},
        {F("Keen Edge"), F("Follow-Through"), F("Riposte"), F("Cleave"), F("Finisher")},
        {F("Steady Aim"), F("Long Reach"), F("Quick Draw"), F("Piercing"), F("Marksman")},
        {F("Lunge"), F("Spacing"), F("Pike Wall"), F("Sweep"), F("Skewer")},
        {F("Heavy Blow"), F("Stagger"), F("Shockwave"), F("Crush"), F("Quake")},
        {F("Bleed"), F("Tear"), F("Opportunist"), F("Hemorrhage"), F("Butcher")},
    };
    return NAMES[track][rank];
}

char const PROGMEM* preview_rank_text(uint8_t track, uint8_t rank) {
    static char const PROGMEM* const PROGMEM TEXTS[PREVIEW_TRACK_COUNT][PREVIEW_RANK_COUNT] = {
        {F("Each move costs 1 less HP, but never less than 1 HP."),
         F("A move that ends beside an enemy also attacks it for 1 damage."),
         F("A move that ends beside an ally restores 1 AP to this member."),
         F("Movement may pass over allies without stopping on them."),
         F("Adds 1 cell to movement range while this member is at full HP.")},
        {F("Leaps may reach one cell farther."),
         F("A leap that ends beside an enemy deals 1 damage to it."),
         F("Leaps may pass over one unit as well as over terrain."),
         F("Takes 1 less damage in the enemy phase after a leap."),
         F("The first leap in each battle costs no HP.")},
        {F("Each cell of a charge beyond the first adds 1 damage to its attack."),
         F("A charge knocks back the first enemy it strikes."),
         F("Takes 1 less damage in the enemy phase after a charge."),
         F("A charge that defeats an enemy restores 1 AP."),
         F("Charge range rises by 1 while this member is at full HP.")},
        {F("Normal attacks deal +1 damage against enemies at full HP."),
         F("After a Strike defeats an enemy, this member may act once more."),
         F("An adjacent enemy that attacks this member takes 1 damage."),
         F("Strike also hits the enemy directly behind its target for 1 damage."),
         F("Strikes against enemies below 3 HP deal +2 damage.")},
        {F("Shots at range 2 deal +1 damage."), F("Shot range extends by one cell."),
         F("The first shot of each battle costs no AP."),
         F("A shot that hits also deals 1 damage to the enemy behind its target."),
         F("Opening Shot's bonus also applies to enemies below full HP.")},
        {F("Reach attacks at range 2 deal +1 damage."),
         F("An enemy that ends its move beside this member takes 1 damage."),
         F("Allies directly behind this member take 1 less damage."),
         F("Reach attacks also hit an enemy beside their target for 1 damage."),
         F("Driving Thrust pushes its victim two cells instead of one.")},
        {F("Impact attacks deal +1 damage."),
         F("An enemy knocked back by Impact loses its next move."),
         F("Impact knockback also pushes an enemy beside the target."),
         F("Impact attacks against enemies at full HP deal +1 damage."),
         F("Crushing Bash also deals 1 damage to each enemy beside the target.")},
        {F("Rend's normal attack deals +1 damage against any injured enemy."),
         F("Rend attacks on an enemy at 2 HP or less deal +1 damage."),
         F("Wound Hunter costs 1 AP against an injured enemy."),
         F("An enemy injured by Rend loses 1 HP at the start of its turn."),
         F("Each enemy this member defeats restores 1 AP.")},
    };
    return TEXTS[track][rank];
}

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
