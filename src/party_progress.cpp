#include "rpg.h"

PartyMember party_new_member(NPC_ID id, MOVEMENT_CLASS movement, ATTACK_CLASS attack) {
    PartyMember member{0, 0, movement, attack, id, 1, 0, 0, {}};
    member.hp = party_max_hp(member);
    member.ap = party_max_ap(member);
    return member;
}

uint8_t party_gain_experience(PartyMember& member, uint8_t amount) {
    uint16_t experience = member.experience + amount;
    uint8_t gained = 0;
    while(member.level < PARTY_MAX_LEVEL && experience >= party_experience_needed(member.level)) {
        experience -= party_experience_needed(member.level);
        ++member.level;
        ++gained;
    }
    member.experience = member.level < PARTY_MAX_LEVEL ? (uint8_t)experience : 0;
    return gained;
}

bool party_can_learn_perk(PartyMember const& member, PERK_TRACK_KIND track) {
    return (track == PERK_MOVEMENT || track == PERK_ATTACK) && party_progress_valid(member) &&
           member.movement_class < MOVEMENT_CLASS_COUNT && member.attack_class < ATTACK_CLASS_COUNT &&
           party_unspent_perks(member) && party_perk_rank(member, track) < PERK_IMPLEMENTED_RANKS;
}

bool party_learn_perk(PartyMember& member, PERK_TRACK_KIND track) {
    if(!party_can_learn_perk(member, track))
        return false;
    member.perk_ranks += track == PERK_MOVEMENT ? 1 : 8;
    return true;
}

void LevelUpNotice::award(Party& party, uint8_t slot, uint8_t experience) {
    PartyMember& member = party.members[slot];
    uint8_t previous_choices = member.level / 2;
    if(party_gain_experience(member, experience)) {
        members |= (uint8_t)(1 << slot);
        perk_earned |= member.level / 2 > previous_choices;
    }
}

bool party_has_unspent_perks(Party const& party) {
    for(uint8_t i = 0; i < party.count; ++i)
        if(party_unspent_perks(party.members[i]))
            return true;
    return false;
}
