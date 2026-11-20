#include "equipment.h"

static constexpr EquipmentItem PROGMEM ITEMS[] = {
    {SLOT_WEAPON, F("Iron Fang"), F("+1 Attack"),
     F("Direct attacks and specials deal +1 damage. Movement engagement damage is unchanged.")},
    {SLOT_ARMOR, F("Traveler's Boots"), F("+2 Max HP"), F("Raises maximum HP by 2.")},
    {SLOT_RELIC, F("Dawn Pendant"), F("+1 AP Recovery"),
     F("Recover 1 extra AP after each victory, up to maximum AP.")},
    {SLOT_RELIC, F("Vital Pendant"), F("+3 Max HP"), F("Raises maximum HP by 3.")},
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

bool equipment_equip(uint8_t member, ITEM_ID item) {
    if(member >= game.party.count || !equipment_owned(item))
        return false;
    uint8_t previous = equipment_wearer(item);
    if(previous == member)
        return false;
    EQUIPMENT_SLOT slot = ITEMS[item].slot;
    if(previous != PARTY_CAPACITY)
        set_slot(game.party.members[previous], slot, ITEM_NONE);
    set_slot(game.party.members[member], slot, item);
    return true;
}

bool equipment_unequip(uint8_t member, EQUIPMENT_SLOT slot) {
    if(member >= game.party.count || equipment_get(game.party.members[member], slot) == ITEM_NONE)
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
