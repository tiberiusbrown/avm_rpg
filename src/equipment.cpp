#include "equipment.h"

static constexpr EquipmentItem PROGMEM ITEMS[] = {
    {SLOT_WEAPON, F("Firststrike Sword"), F("First attack +2"),
     F("The first normal attack each battle deals +2 damage.")},
    {SLOT_WEAPON, F("Hunter's Blade"), F("First wound +3"),
     F("The first normal hit against a previously injured target deals +3 damage.")},
    {SLOT_WEAPON, F("Needle Spear"), F("Full target +1"),
     F("Normal hits against enemies at full HP deal +1 damage.")},
    {SLOT_WEAPON, F("Cracked Saber"), F("Low HP hit +3"),
     F("Normal hits deal +1. At half HP or less, the first normal hit deals +3 instead. Max HP -3.")},
    {SLOT_WEAPON, F("Brass Knuckle"), F("Surrounded +1"),
     F("Normal hits deal +1 damage while at least two enemies are adjacent to the wielder.")},
    {SLOT_WEAPON, F("Stonebreaker"), F("Target in cover +1"),
     F("Normal hits deal +1 damage if the target is adjacent to a solid obstacle.")},
    {SLOT_WEAPON, F("Wolf Fang"), F("Ally support +1"),
     F("Normal hits deal +1 damage if another living party member is adjacent to the target.")},
    {SLOT_WEAPON, F("Thorn Pike"), F("Engage one +1"),
     F("Each move deals +1 engagement damage to one adjacent enemy.")},
    {SLOT_WEAPON, F("Pathfinder's Knife"), F("Leave cover +2"),
     F("Move from beside an obstacle to beside none: +2 damage to the lowest-HP adjacent enemy. "
       "Once per battle.")},
    {SLOT_WEAPON, F("Woodcut Axe"), F("Board edge +1"),
     F("Normal hits deal +1 damage to a target on the battle board's outer edge.")},
    {SLOT_WEAPON, F("Mud Sling"), F("Range and cover +2"),
     F("Normal hits deal +2 at range 3 or more while standing beside an obstacle.")},
    {SLOT_WEAPON, F("Barn Fork"), F("At full HP +1"),
     F("Normal hits deal +1 damage while the wielder is at full HP.")},
    {SLOT_ARMOR, F("Guard Vest"), F("First hit -2"),
     F("Reduce the first enemy hit each battle by 2 damage.")},
    {SLOT_ARMOR, F("Leather Jerkin"), F("Max HP +3"), F("+3 maximum HP.")},
    {SLOT_ARMOR, F("Oak Shield"), F("Low HP guard"),
     F("Reduce enemy hits by 1 damage while the wearer is at or below half HP.")},
    {SLOT_ARMOR, F("Reed Armor"), F("Solo heavy hit -2"),
     F("While the wearer has no adjacent ally, reduce the first enemy hit that would deal at least "
       "3 "
       "damage by 2.")},
    {SLOT_ARMOR, F("River Cloak"), F("First ranged hit -3"),
     F("The first enemy hit from at least three cells away deals 3 less damage.")},
    {SLOT_ARMOR, F("Ash Cloak"), F("Free quiet move"),
     F("+1 maximum HP. The first move without engagement costs 1 less HP.")},
    {SLOT_ARMOR, F("Scout Sandals"), F("Free engage move"),
     F("+1 maximum HP. The first move that ends with engagement costs 1 less HP.")},
    {SLOT_ARMOR, F("Soft Bandages"), F("Potion HP +2"),
     F("Potions heal the wearer for up to 2 extra HP.")},
    {SLOT_ARMOR, F("Padded Shawl"), F("First ally guard -3"),
     F("The first enemy hit taken while adjacent to a living ally deals 3 less damage.")},
    {SLOT_ARMOR, F("Lead Boots"), F("Hits -1, move HP +1"),
     F("Reduce enemy hits by 1 damage and increase each move's HP cost by 1.")},
    {SLOT_ARMOR, F("Turnshoe"), F("First edge hit -3"),
     F("Reduce the first enemy hit taken on a board edge each battle by 3 damage.")},
    {SLOT_ARMOR, F("Hopper Vest"), F("HP +1, Elixir +1"),
     F("+1 maximum HP. Elixirs heal the wearer for up to 1 extra HP.")},
    {SLOT_RELIC, F("Moon Shard"), F("Max AP +1"), F("+1 maximum AP.")},
    {SLOT_RELIC, F("Survivor's Ribbon"), F("Win HP +1"),
     F("Restore 1 HP to the conscious wearer after each victory.")},
    {SLOT_RELIC, F("Ferry Bead"), F("Win heal ally"),
     F("Win while conscious to heal the adjacent living ally missing the most HP for 2 HP. "
       "Ties use party order.")},
    {SLOT_RELIC, F("Thorn Ring"), F("First crowd +1"),
     F("The first move that engages at least two enemies deals +1 damage to each adjacent enemy.")},
    {SLOT_RELIC, F("Grave Token"), F("At 1 HP +3"),
     F("At exactly 1 HP, each hit on the wearer's action deals +3 damage, including engagement and "
       "secondary hits.")},
    {SLOT_RELIC, F("Sluice Tally"), F("Win gold +1"),
     F("Gain 1 extra G after every victory while equipped, up to wallet capacity.")},
    {SLOT_RELIC, F("Shared Cup"), F("Potion heals wearer"),
     F("When the party uses a Potion on another member, the conscious wearer also heals up to 1 "
       "HP.")},
    {SLOT_RELIC, F("Lantern Wick"), F("Guard ally once"),
     F("The first enemy hit against a conscious ally adjacent to the wearer deals 2 less damage, "
       "once "
       "per battle.")},
    {SLOT_RELIC, F("Tessa's Rosette"), F("HP +1, Revive 7"),
     F("+1 maximum HP. A Revive used on the wearer returns them to up to 7 HP.")},
    {SLOT_RELIC, F("Split Button"), F("Win at 1 HP: +4"),
     F("If the wearer finishes a victory at exactly 1 HP, restore 4 HP to them.")},
};

static_assert(sizeof(ITEMS) / sizeof(*ITEMS) == ITEM_DEFINED_COUNT);

EquipmentItem const PROGMEM* equipment_item(ITEM_ID item) {
    return equipment_valid_item(item) ? &ITEMS[item] : nullptr;
}

bool equipment_owned(ITEM_ID item) {
    return equipment_valid_item(item) && game.get_flag(equipment_flag(item));
}

bool equipment_acquire(ITEM_ID item) {
    if(!equipment_valid_item(item) || equipment_owned(item))
        return false;
    game.set_flag(equipment_flag(item));
    return true;
}

uint8_t equipment_wearer(ITEM_ID item) {
    if(equipment_valid_item(item)) {
        EQUIPMENT_SLOT slot = ITEMS[item].slot;
        for(uint8_t i = 0; i < game.party.count; ++i)
            if(equipment_get(game.party.members[i], slot) == item)
                return i;
    }
    return PARTY_CAPACITY;
}

static void set_slot(PartyMember& member, EQUIPMENT_SLOT slot, ITEM_ID item) {
    member.equipment[slot] = equipment_encode(item);
    uint8_t hp = party_max_hp(member);
    uint8_t ap = party_max_ap(member);
    if(member.hp > hp)
        member.hp = hp;
    if(member.ap > ap)
        member.ap = ap;
}

bool equipment_can_change(uint8_t member) {
    return member < game.party.count && game.party.members[member].hp;
}

bool equipment_can_equip(uint8_t member, ITEM_ID item) {
    if(!equipment_can_change(member) || !equipment_owned(item))
        return false;
    uint8_t previous = equipment_wearer(item);
    return previous != member && (previous == PARTY_CAPACITY || equipment_can_change(previous));
}

bool equipment_equip(uint8_t member, ITEM_ID item) {
    if(!equipment_can_equip(member, item))
        return false;
    uint8_t previous = equipment_wearer(item);
    EQUIPMENT_SLOT slot = ITEMS[item].slot;
    if(previous != PARTY_CAPACITY)
        set_slot(game.party.members[previous], slot, ITEM_NONE);
    set_slot(game.party.members[member], slot, item);
    return true;
}

bool equipment_unequip(uint8_t member, EQUIPMENT_SLOT slot) {
    if(!equipment_can_change(member) ||
       equipment_get(game.party.members[member], slot) == ITEM_NONE)
        return false;
    set_slot(game.party.members[member], slot, ITEM_NONE);
    return true;
}

bool equipment_saved_state_valid() {
    for(uint8_t i = 0; i < PARTY_CAPACITY; ++i) {
        PartyMember const& member = game.party.members[i];
        for(uint8_t slot = 0; slot < SLOT_COUNT; ++slot) {
            uint8_t stored = member.equipment[slot];
            if(!stored)
                continue;
            ITEM_ID item = equipment_decode(stored);
            if(i >= game.party.count || !equipment_owned(item) || ITEMS[item].slot != slot ||
               equipment_wearer(item) != i)
                return false;
        }
    }
    return true;
}
