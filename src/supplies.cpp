#include "rpg.h"

uint8_t supply_count(Supplies const& supplies, SUPPLY_ID id) {
    return supply_valid(id) ? supplies.quantity[id] : 0;
}

bool supplies_can_add(Supplies const& supplies, SUPPLY_ID id, uint8_t amount) {
    return supply_valid(id) && supplies.quantity[id] + amount <= SUPPLY_LIMIT;
}

bool supplies_add(Supplies& supplies, SUPPLY_ID id, uint8_t amount) {
    if(!supplies_can_add(supplies, id, amount))
        return false;
    supplies.quantity[id] += amount;
    return true;
}

static bool injured(PartyMember const& member) {
    return member.hp && member.hp < party_max_hp(member);
}

bool supply_can_use(SUPPLY_ID id, Party const& party, uint8_t target) {
    if(id == SUPPLY_ELIXIR) {
        if(target == SUPPLY_NO_TARGET)
            for(uint8_t i = 0; i < party.count; ++i)
                if(injured(party.members[i]))
                    return true;
        return false;
    }
    if(target >= party.count)
        return false;
    PartyMember const& member = party.members[target];
    switch(id) {
        case SUPPLY_POTION:
            return injured(member);
        case SUPPLY_REVIVE:
            return !member.hp && party_max_hp(member);
        case SUPPLY_ETHER:
            return member.ap < party_max_ap(member);
        default:
            return false;
    }
}

static uint8_t restore(uint8_t current, uint8_t maximum, uint8_t amount) {
    return current + amount < maximum ? current + amount : maximum;
}

bool supply_use(Supplies& supplies, Party& party, SUPPLY_ID id, uint8_t target) {
    if(!supply_count(supplies, id) || !supply_can_use(id, party, target))
        return false;
    if(id == SUPPLY_ELIXIR) {
        for(uint8_t i = 0; i < party.count; ++i) {
            PartyMember& member = party.members[i];
            if(injured(member))
                member.hp = restore(member.hp, party_max_hp(member), 4);
        }
    } else {
        PartyMember& member = party.members[target];
        if(id == SUPPLY_ETHER)
            member.ap = restore(member.ap, party_max_ap(member), 3);
        else
            member.hp = restore(member.hp, party_max_hp(member), id == SUPPLY_POTION ? 8 : 5);
    }
    --supplies.quantity[id];
    return true;
}

void party_rest(Party& party) {
    for(uint8_t i = 0; i < party.count; ++i) {
        PartyMember& member = party.members[i];
        member.hp = party_max_hp(member);
        member.ap = party_max_ap(member);
    }
}
