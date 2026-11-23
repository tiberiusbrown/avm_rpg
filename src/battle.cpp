#include "battle.h"
#include "npc.h"
#include "runtime_workspace.h"
#include "tile_ids.h"

// The guest inlines the marked rules helpers into the evaluation, so they add no frame at the
// search's deepest point. Host compilers decide for themselves.
#if defined(__clang__)
#define BATTLE_ALWAYS_INLINE __attribute__((always_inline))
#else
#define BATTLE_ALWAYS_INLINE
#endif

bool battle_blocked(uint8_t field, uint8_t cell) {
    return (BATTLEFIELDS[field].blocked[battle_row(cell)] >> battle_column(cell)) & 1;
}

constexpr EnemyInfo PROGMEM ENEMY_INFO[ENEMY_COUNT] = {
    {10, MOVEMENT_STEP, ATTACK_STRIKE, 2}, {10, MOVEMENT_LEAP, ATTACK_STRIKE, 1},
    {9, MOVEMENT_STEP, ATTACK_SHOOT, 0},    {10, MOVEMENT_STEP, ATTACK_REACH, 3},
    {9, MOVEMENT_CHARGE, ATTACK_REND, 17},    {12, MOVEMENT_STEP, ATTACK_IMPACT, 25},
};
static_assert([] {
    for(auto const& enemy : ENEMY_INFO)
        if(!enemy.max_hp || enemy.movement_class >= MOVEMENT_CLASS_COUNT ||
           enemy.attack_class >= ATTACK_CLASS_COUNT || enemy.sprite_row >= ACTOR_ROW_COUNT)
            return false;
    return true;
}());

// Check every independent class combination, including those not used by an enemy preset.
constexpr bool battle_classes_fit() {
    for(auto const& rules : BATTLE_RULES.classes) {
        if(!rules.attack_min || rules.attack_min > rules.attack_max ||
           (rules.geometry == BATTLE_ATTACK_DIAGONAL && rules.attack_max != 1) ||
           rules.damage > 15 || rules.injured_damage > 15)
            return false;
    }
    return true;
}

static_assert(battle_classes_fit());

RPG_NOINLINE bool battle_encounter_valid(BattleEncounter const PROGMEM* encounter) {
    if(!encounter || !encounter->fields || (encounter->fields & ~BATTLEFIELDS_ALL) ||
       !encounter->enemy_count || encounter->enemy_count > BATTLE_SIDE_CAPACITY)
        return false;

    for(uint8_t i = 0; i < encounter->enemy_count; ++i)
        if(encounter->enemies[i] >= ENEMY_COUNT)
            return false;
    return true;
}

// The nonzero xorshift has 65,535 outputs. Reject the incomplete final bucket
// so every eligible field or cell has the same chance of being selected.
static uint8_t random_choice(uint8_t count) {
    uint16_t limit = UINT16_MAX - UINT16_MAX % count;
    uint16_t roll;
    do {
        roll = (uint16_t)(rng_next() - 1);
    } while(roll >= limit);
    return (uint8_t)(roll % count);
}

static uint8_t select_battlefield(BattlefieldSelection selection) {
    uint8_t count = 0;
    for(auto bits = selection; bits; bits >>= 1)
        count += bits & 1;
    uint8_t pick = random_choice(count);
    for(uint8_t field = 0; ; ++field, selection >>= 1)
        if((selection & 1) && pick-- == 0)
            return field;
}

RPG_NOINLINE static void deploy(BattleState& state, BattleEncounter const PROGMEM* encounter,
                                BattleRect area, BATTLE_TEAM team) {
    uint8_t cells[BATTLE_CELLS];
    uint8_t available = 0;
    for(uint8_t y = area.y; y < area.y + area.height; ++y)
        for(uint8_t x = area.x; x < area.x + area.width; ++x) {
            uint8_t cell = battle_cell(x, y);
            if(!battle_blocked(state.field, cell))
                cells[available++] = cell;
        }

    uint8_t count = team == BATTLE_PARTY ? game.party.count : encounter->enemy_count;
    for(uint8_t i = 0; i < count; ++i) {
        uint8_t id = team == BATTLE_PARTY ? battle_party_id(i) : encounter->enemies[i];
        uint8_t hp = team == BATTLE_PARTY ? game.party.members[i].hp : ENEMY_INFO[id].max_hp;
        if(!hp)
            continue;
        uint8_t pick = random_choice(available);
        state.units[state.unit_count++] = {cells[pick], hp, id, team};
        cells[pick] = cells[--available];
    }
}

void battle_begin(BattleState& state, BattleEncounter const PROGMEM* encounter) {
    state = {};
    state.result = BATTLE_DEFEAT;
    if(!battle_encounter_valid(encounter)) {
        state.ended = true;
        return;
    }

    // New and loaded games already provide a nonzero seed.
    if(!game.rng)
        game.rng = 0xbabe;
    state.field = select_battlefield(encounter->fields);
    auto const& field = BATTLEFIELDS[state.field];
    auto const& party = field.party_area;
    auto const& enemy = field.enemy_area;
    deploy(state, encounter, {party.x, party.y, party.width, party.height}, BATTLE_PARTY);
    deploy(state, encounter, {enemy.x, enemy.y, enemy.width, enemy.height}, BATTLE_ENEMY);
    if(!battle_living(state, BATTLE_PARTY))
        state.ended = true;
}

BattleRewards battle_finish(BattleState const& state) {
    BattleRewards rewards;
    static_assert(BATTLE_UNIT_CAPACITY * 3 <= UINT8_MAX);
    uint8_t gold = 0;
    for(uint8_t i = 0; i < state.unit_count; ++i) {
        BattleUnit const& unit = state.units[i];
        if(unit.team == BATTLE_PARTY && (unit.id & BATTLE_PARTY_SOURCE))
            game.party.members[unit.id & ~BATTLE_PARTY_SOURCE].hp = unit.hp;
        // Defeated units stay in the roster, so this counts every enemy deployed.
        if(unit.team == BATTLE_ENEMY)
            gold += 3;
    }
    if(!state.ended || state.result != BATTLE_VICTORY)
        return rewards;
    uint16_t wallet_room = UINT16_MAX - game.gold;
    rewards.gold_gained = gold < wallet_room ? gold : (uint8_t)wallet_room;
    game.gold += rewards.gold_gained;
    // Every member recovers and earns experience, including one that fell or did not deploy.
    for(uint8_t i = 0; i < game.party.count; ++i) {
        PartyMember& member = game.party.members[i];
        uint16_t ap = member.ap + party_victory_ap(member);
        uint8_t maximum = party_max_ap(member);
        member.ap = ap < maximum ? (uint8_t)ap : maximum;
        rewards.levels.award(game.party, i, party_victory_experience(member));
    }
    return rewards;
}

uint8_t battle_max_hp(BattleUnit const& unit) {
    return unit.id & BATTLE_PARTY_SOURCE
               ? party_max_hp(game.party.members[unit.id & ~BATTLE_PARTY_SOURCE])
               : ENEMY_INFO[unit.id].max_hp;
}

uint8_t battle_unit_at(BattleState const& state, uint8_t cell) {
    for(uint8_t i = 0; i < state.unit_count; ++i) {
        BattleUnit const& unit = state.units[i];
        if(unit.hp && unit.cell == cell)
            return i;
    }
    return BATTLE_NO_UNIT;
}

bool battle_neighbor(uint8_t cell, DIRECTION dir, uint8_t& neighbor) {
    uint8_t x = cell % BATTLE_COLUMNS, y = cell / BATTLE_COLUMNS;
    switch(dir) {
        case NORTH:
            if(!y)
                return false;
            neighbor = (uint8_t)(cell - BATTLE_COLUMNS);
            return true;
        case EAST:
            if(x + 1 == BATTLE_COLUMNS)
                return false;
            neighbor = (uint8_t)(cell + 1);
            return true;
        case SOUTH:
            if(y + 1 == BATTLE_ROWS)
                return false;
            neighbor = (uint8_t)(cell + BATTLE_COLUMNS);
            return true;
        case WEST:
            if(!x)
                return false;
            neighbor = (uint8_t)(cell - 1);
            return true;
        default:
            return false;
    }
}

DIRECTION battle_direction(uint8_t from, uint8_t to) {
    int8_t dx = (int8_t)(battle_column(to) - battle_column(from));
    int8_t dy = (int8_t)(battle_row(to) - battle_row(from));
    int8_t ax = dx < 0 ? (int8_t)-dx : dx, ay = dy < 0 ? (int8_t)-dy : dy;
    if(ax >= ay && ax)
        return dx > 0 ? EAST : WEST;
    return dy > 0 ? SOUTH : NORTH;
}

static bool at_range(uint8_t from, uint8_t to, uint8_t distance) {
    uint8_t delta = from < to ? to - from : from - to;
    return (delta == distance && battle_row(from) == battle_row(to)) ||
           delta == distance * BATTLE_COLUMNS;
}

static uint8_t difference(uint8_t a, uint8_t b) {
    return a > b ? (uint8_t)(a - b) : (uint8_t)(b - a);
}

static uint8_t diagonal_distance(uint8_t a, uint8_t b) {
    uint8_t dx = difference(battle_column(a), battle_column(b));
    uint8_t dy = difference(battle_row(a), battle_row(b));
    return dx == dy ? dx : 0;
}

RPG_NOINLINE static uint8_t adjacent_allies(BattleState const& state, uint8_t ally, uint8_t cell,
                                            bool first_only) {
    BATTLE_TEAM team = state.units[ally].team;
    uint8_t count = 0;
    for(uint8_t i = 0; i < state.unit_count; ++i) {
        BattleUnit const& unit = state.units[i];
        if(i != ally && unit.hp && unit.team == team && battle_cells_adjacent(unit.cell, cell)) {
            if(first_only)
                return 1;
            ++count;
        }
    }
    return count;
}

RPG_NOINLINE bool battle_flank_supported(BattleState const& state, uint8_t attacker,
                                         uint8_t victim) {
    return adjacent_allies(state, attacker, state.units[victim].cell, true);
}

static uint8_t attack_perk_rank(BattleUnit const& actor) {
    return actor.id & BATTLE_PARTY_SOURCE
               ? party_attack_perk_rank(game.party.members[actor.id & ~BATTLE_PARTY_SOURCE])
               : 0;
}

static uint8_t movement_perk_rank(BattleUnit const& actor) {
    return actor.id & BATTLE_PARTY_SOURCE
               ? party_movement_perk_rank(game.party.members[actor.id & ~BATTLE_PARTY_SOURCE])
               : 0;
}

static bool battle_open_cell(BattleState const& state, uint8_t cell) {
    return !battle_blocked(state.field, cell) && battle_unit_at(state, cell) == BATTLE_NO_UNIT;
}

static uint8_t displacement_cell(BattleState const& state, uint8_t from, DIRECTION away) {
    uint8_t cell;
    if(battle_neighbor(from, away, cell) && battle_open_cell(state, cell))
        return cell;
    return BATTLE_CELLS;
}

static uint8_t arc_perk_bonus(BattleState const& state, uint8_t attacker, uint8_t victim,
                              uint8_t rank) {
    auto const& from = state.units[attacker];
    auto const& to = state.units[victim];
    uint8_t a = battle_cell(battle_column(from.cell), battle_row(to.cell));
    uint8_t b = battle_cell(battle_column(to.cell), battle_row(from.cell));
    uint8_t first = battle_unit_at(state, a), second = battle_unit_at(state, b);
    uint8_t bonus = rank >= CROSSCUT_RANK ? CROSSCUT_BONUS : 0;
    if(first == BATTLE_NO_UNIT && second == BATTLE_NO_UNIT && !battle_blocked(state.field, a) &&
       !battle_blocked(state.field, b))
        bonus += OPEN_ANGLE_BONUS;
    if(rank >= INSIDE_ANGLE_RANK &&
       ((first < BATTLE_NO_UNIT && state.units[first].team != from.team) ||
        (second < BATTLE_NO_UNIT && state.units[second].team != from.team)))
        bonus += INSIDE_ANGLE_BONUS;
    return bonus;
}

// The retreat cell, the unit behind the victim, or the Arc damage bonus.
RPG_NOINLINE static uint8_t strike_geometry(BattleState const& state, uint8_t attacker,
                                            uint8_t victim, uint8_t attack) {
    auto const& actor = state.units[attacker];
    uint8_t to = state.units[victim].cell;
    if(attack == ATTACK_ARC) {
        uint8_t rank = attack_perk_rank(actor);
        return rank ? arc_perk_bonus(state, attacker, victim, rank) : 0;
    }
    if(attack == ATTACK_RECOIL) {
        DIRECTION away = battle_direction(to, actor.cell);
        uint8_t landing = displacement_cell(state, actor.cell, away);
        if(landing < BATTLE_CELLS && attack_perk_rank(actor) >= SPRINGBACK_RANK) {
            uint8_t farther = displacement_cell(state, landing, away);
            if(farther < BATTLE_CELLS)
                return farther;
        }
        return landing;
    }
    if((attack == ATTACK_IMPACT && attack_perk_rank(actor) >= BODY_CHECK_RANK) ||
       (attack == ATTACK_SHOOT && attack_perk_rank(actor) >= PIERCING_SHOT_RANK)) {
        uint8_t behind;
        if(battle_neighbor(to, battle_direction(actor.cell, to), behind))
            return battle_unit_at(state, behind);
    }
    return BATTLE_NO_UNIT;
}

// Results encode class-specific geometry and formation checks in one byte.
BATTLE_ALWAYS_INLINE static inline uint8_t
attack_position(BattleState const& state, uint8_t attacker, uint8_t victim, uint8_t attack) {
    if(attack == ATTACK_BLAST)
        return battle_splash_targets(state, attacker, victim);
    if(attack == ATTACK_FLANK)
        return adjacent_allies(state, attacker, state.units[victim].cell,
                               attack_perk_rank(state.units[attacker]) < CONVERGING_BLADES_RANK);
    if(attack == ATTACK_STRIKE)
        return attack_perk_rank(state.units[attacker]) >= DUELIST_RANK &&
               adjacent_allies(state, victim, state.units[victim].cell, true);
    return strike_geometry(state, attacker, victim, attack);
}

RPG_NOINLINE static uint8_t primary_damage(BattleState const& state, uint8_t attacker_index,
                                           uint8_t victim_index, uint8_t attack, uint8_t position) {
    BattleUnit const& attacker = state.units[attacker_index];
    BattleUnit const& victim = state.units[victim_index];
    uint8_t rank = attack_perk_rank(attacker);
    AttackInfo const& rules = ATTACK_INFO[attack];
    uint8_t damage = rules.damage;
    if(rules.injured_damage != rules.damage) {
        uint8_t maximum = battle_max_hp(victim);
        // Round the fractional threshold up so an integer HP must be strictly below it.
        uint8_t threshold =
            (uint8_t)(maximum - (rank >= BLOODLETTING_RANK ? maximum / 4 : maximum / 2));
        if(victim.hp < threshold)
            damage = rules.injured_damage;
    }
    if(attack == ATTACK_FLANK && position)
        damage += 2;
    if(attacker.id & BATTLE_PARTY_SOURCE) {
        PartyMember const& member = game.party.members[attacker.id & ~BATTLE_PARTY_SOURCE];
        damage += party_attack_bonus(member);
        if(equipment_get(member, SLOT_WEAPON) == ITEM_IRON_FANG)
            damage += IRON_FANG_DAMAGE;
        if(rank) {
            switch(attack) {
                case ATTACK_STRIKE:
                    if(victim.hp == battle_max_hp(victim))
                        damage += KEEN_EDGE_BONUS;
                    if(rank >= SWORD_MASTERY_RANK)
                        damage += SWORD_MASTERY_BONUS;
                    if(rank >= DUELIST_RANK && !position)
                        damage += DUELIST_BONUS;
                    break;
                case ATTACK_SHOOT:
                    if(at_range(attacker.cell, victim.cell, LONGSHOT_RANGE))
                        damage += LONGSHOT_BONUS;
                    if(rank >= SHARPSHOOTER_RANK)
                        damage += SHARPSHOOTER_BONUS;
                    break;
                case ATTACK_REACH:
                    if(at_range(attacker.cell, victim.cell, LUNGE_RANGE))
                        damage += LUNGE_BONUS;
                    if(rank >= SHORT_GRIP_RANK &&
                       at_range(attacker.cell, victim.cell, SHORT_GRIP_RANGE))
                        damage += SHORT_GRIP_BONUS;
                    break;
                case ATTACK_IMPACT:
                    if(position < BATTLE_NO_UNIT)
                        damage += BODY_CHECK_BONUS;
                    if(rank >= HEAVY_BLOW_RANK)
                        damage += HEAVY_BLOW_BONUS;
                    break;
                case ATTACK_REND:
                    if(rank >= BLOOD_SENSE_RANK && victim.hp < battle_max_hp(victim))
                        damage += BLOOD_SENSE_BONUS;
                    if(rank >= BLOOD_RUSH_RANK && attacker.hp <= battle_max_hp(attacker) / 2)
                        damage += BLOOD_RUSH_BONUS;
                    break;
                case ATTACK_ARC:
                    damage += position;
                    break;
                case ATTACK_DRAIN:
                    if(rank >= DEEP_CUT_RANK)
                        damage += DEEP_CUT_BONUS;
                    break;
                case ATTACK_RECOIL:
                    if(position < BATTLE_CELLS)
                        damage += PARTING_BLOW_BONUS;
                    if(rank >= COUNTERFORCE_RANK)
                        damage += COUNTERFORCE_BONUS;
                    break;
                case ATTACK_FLANK:
                    if(position)
                        damage += ENCIRCLE_BONUS;
                    if(rank >= TACTICAL_EDGE_RANK)
                        damage += TACTICAL_EDGE_BONUS;
                    if(rank >= CONVERGING_BLADES_RANK)
                        damage += position * CONVERGING_BLADES_BONUS;
                    break;
                case ATTACK_BLAST:
                    if(position & (position - 1))
                        damage += CLUSTER_BURST_BONUS;
                    break;
            }
        }
    }
    return damage;
}

static uint8_t special_damage_bonus(uint8_t attack, BattleUnit const& victim) {
    SpecialInfo const& info = SPECIAL_INFO[attack];
    if(info.bonus_full == info.bonus_injured || victim.hp == battle_max_hp(victim))
        return info.bonus_full;
    return info.bonus_injured;
}

BATTLE_ALWAYS_INLINE static inline uint8_t raw_attack_damage(BattleState const& state,
                                                             uint8_t attacker, uint8_t victim,
                                                             uint8_t attack, uint8_t position,
                                                             bool special) {
    uint8_t damage = primary_damage(state, attacker, victim, attack, position);
    if(special)
        damage += special_damage_bonus(attack, state.units[victim]);
    return damage;
}

// Keep the defender's formation scan off the primary damage calculation's frame.
RPG_NOINLINE static uint8_t guarded_damage(BattleState const& state, uint8_t victim_index,
                                           uint8_t damage, uint8_t& reflected) {
    BattleUnit const& victim = state.units[victim_index];
    if(damage > HOLD_TOGETHER_BONUS && (victim.id & BATTLE_PARTY_SOURCE)) {
        auto const& member = game.party.members[victim.id & ~BATTLE_PARTY_SOURCE];
        if(member.movement_class == HOLD_TOGETHER_CLASS && party_movement_perk_rank(member) &&
           adjacent_allies(state, victim_index, victim.cell, true)) {
            uint8_t reduction = HOLD_TOGETHER_BONUS;
            uint8_t rank = party_movement_perk_rank(member);
            if(rank >= SHIELD_WALL_RANK)
                reduction += SHIELD_WALL_BONUS;
            if(rank >= SHIELD_REBOUND_RANK) {
                reflected = (uint8_t)(damage / 2);
                damage -= reflected;
            }
            return damage > reduction ? (uint8_t)(damage - reduction) : 1;
        }
    }
    return damage;
}

uint8_t battle_attack_damage(BattleState const& state, uint8_t attacker, uint8_t victim,
                             bool special) {
    uint8_t attack = battle_attack_class(state.units[attacker].id);
    uint8_t reflected = 0;
    uint8_t damage = raw_attack_damage(state, attacker, victim, attack,
                                       attack_position(state, attacker, victim, attack), special);
    return guarded_damage(state, victim, damage, reflected);
}

RPG_NOINLINE uint8_t battle_splash_targets(BattleState const& state, uint8_t attacker,
                                           uint8_t victim) {
    BattleUnit const& primary = state.units[victim];
    bool diagonal = attack_perk_rank(state.units[attacker]) >= SHOCKWAVE_RANK;
    uint8_t targets = 0;
    for(uint8_t i = 0; i < state.unit_count; ++i) {
        BattleUnit const& unit = state.units[i];
        if(unit.hp && unit.team == primary.team &&
           (battle_cells_adjacent(unit.cell, primary.cell) ||
            (diagonal && diagonal_distance(unit.cell, primary.cell) == 1)))
            targets |= (uint8_t)(1 << i);
    }
    return targets;
}

enum SECONDARY_KIND : uint8_t { SECONDARY_FLAT, SECONDARY_SPLASH, SECONDARY_SHOT, SECONDARY_OPENING };

struct SecondaryDamage {
    uint8_t targets, damage;
    SECONDARY_KIND kind = SECONDARY_FLAT;
    uint8_t line_end = BATTLE_CELLS;
};

// Throughshot continues across open cells. Wrecking Line takes only the contiguous enemy run,
// with each survivor shifted by the same amount into the space that was open before the attack.
BATTLE_ALWAYS_INLINE static inline SecondaryDamage attack_line(BattleState const& state,
                                                               uint8_t attacker, uint8_t victim,
                                                               bool throughshot) {
    auto const& actor = state.units[attacker];
    uint8_t cell = state.units[victim].cell, next;
    DIRECTION direction = battle_direction(actor.cell, cell);
    SecondaryDamage splash{0, COLLISION_BONUS};
    while(battle_neighbor(cell, direction, next) && !battle_blocked(state.field, next)) {
        uint8_t occupant = battle_unit_at(state, next);
        if(occupant == BATTLE_NO_UNIT) {
            if(!throughshot)
                break;
        } else if(state.units[occupant].team != actor.team) {
            splash.targets |= (uint8_t)(1 << occupant);
        } else {
            break;
        }
        cell = next;
    }
    splash.line_end = cell;
    return splash;
}

RPG_NOINLINE static uint8_t corner_targets(BattleState const& state, uint8_t attacker,
                                            uint8_t victim) {
    auto const& actor = state.units[attacker];
    uint8_t to = state.units[victim].cell;
    uint8_t a = battle_cell(battle_column(actor.cell), battle_row(to));
    uint8_t b = battle_cell(battle_column(to), battle_row(actor.cell));
    uint8_t first = battle_unit_at(state, a), second = battle_unit_at(state, b);
    uint8_t targets = 0;
    if(first < BATTLE_NO_UNIT && state.units[first].team != actor.team)
        targets |= (uint8_t)(1 << first);
    if(second < BATTLE_NO_UNIT && state.units[second].team != actor.team)
        targets |= (uint8_t)(1 << second);
    return targets;
}

RPG_NOINLINE static SecondaryDamage attack_splash(BattleState const& state, uint8_t attacker,
                                                   uint8_t victim, uint8_t position,
                                                   uint8_t raw, bool special) {
    auto const& actor = state.units[attacker];
    uint8_t attack = battle_attack_class(actor.id);
    if(attack == ATTACK_BLAST) {
        uint8_t damage = attack_perk_rank(actor) >= SHRAPNEL_RANK ? 1 + SHRAPNEL_BONUS : 1;
        return {position, damage, SECONDARY_SPLASH};
    }
    uint8_t rank = attack_perk_rank(actor);
    if(rank >= PERK_TRACK_RANKS) {
        if(attack == ATTACK_SHOOT) {
            SecondaryDamage shot = attack_line(state, attacker, victim, true);
            auto const& target = state.units[victim];
            uint8_t bonus = at_range(actor.cell, target.cell, LONGSHOT_RANGE) ? LONGSHOT_BONUS : 0;
            if(special)
                bonus += special_damage_bonus(attack, target);
            shot.damage = (uint8_t)(raw - bonus);
            shot.kind = special ? SECONDARY_OPENING : SECONDARY_SHOT;
            return shot;
        }
        if(attack == ATTACK_IMPACT)
            return attack_line(state, attacker, victim, false);
        if(attack == ATTACK_ARC)
            return {corner_targets(state, attacker, victim), raw};
    }
    if((attack == ATTACK_SHOOT || attack == ATTACK_IMPACT) && position < BATTLE_NO_UNIT &&
       state.units[position].team != actor.team) {
        if(attack == ATTACK_SHOOT)
            return {(uint8_t)(1 << position), PIERCING_SHOT_BONUS};
        if(rank >= COLLISION_RANK)
            return {(uint8_t)(1 << position), COLLISION_BONUS};
    }
    return {};
}

static uint8_t splash_damage(BattleState const& state, SecondaryDamage const& splash,
                             uint8_t origin, uint8_t center, uint8_t target) {
    auto const& unit = state.units[target];
    if(splash.kind == SECONDARY_SPLASH && !battle_cells_adjacent(center, unit.cell))
        return SHOCKWAVE_BONUS;
    uint8_t damage = splash.damage;
    if(splash.kind >= SECONDARY_SHOT) {
        if(at_range(origin, unit.cell, LONGSHOT_RANGE))
            damage += LONGSHOT_BONUS;
        if(splash.kind == SECONDARY_OPENING)
            damage += special_damage_bonus(ATTACK_SHOOT, unit);
    }
    return damage;
}

RPG_NOINLINE BattleAttackImpact battle_attack_impact(BattleState const& state, uint8_t attacker,
                                                     uint8_t victim) {
    uint8_t attack = battle_attack_class(state.units[attacker].id);
    uint8_t secondary = attack_position(state, attacker, victim, attack);
    uint8_t raw = primary_damage(state, attacker, victim, attack, secondary);
    uint8_t reflected = 0;
    uint8_t damage = guarded_damage(state, victim, raw, reflected);
    uint8_t hp = state.units[victim].hp;
    BattleAttackImpact impact{(uint8_t)(hp <= damage), hp < damage ? hp : damage, reflected};
    SecondaryDamage splash = attack_splash(state, attacker, victim, secondary, raw, false);
    for(uint8_t i = 0; splash.targets; ++i, splash.targets >>= 1) {
        if(splash.targets & 1) {
            hp = state.units[i].hp;
            damage = splash_damage(state, splash, state.units[attacker].cell,
                                   state.units[victim].cell, i);
            impact.kills += hp <= damage;
            impact.damage += hp < damage ? hp : damage;
        }
    }
    return impact;
}

uint8_t battle_sprite_row(BattleUnit const& unit) {
    if(unit.id & BATTLE_PARTY_SOURCE) {
        NPC_ID id = game.party.members[unit.id & ~BATTLE_PARTY_SOURCE].id;
        auto definition = npc_definition(id);
        return definition ? definition->sprite_row : 0;
    }
    return ENEMY_INFO[unit.id].sprite_row;
}

// The distance from `a` to `b` when they share a row or column, or zero when they share neither
// (or are the same cell).
static uint8_t battle_line_distance(uint8_t a, uint8_t b) {
    uint8_t ax = battle_column(a), ay = battle_row(a), bx = battle_column(b), by = battle_row(b);
    if(ay == by)
        return (uint8_t)(ax > bx ? ax - bx : bx - ax);
    if(ax == bx)
        return (uint8_t)(ay > by ? ay - by : by - ay);
    return 0;
}

static uint8_t attack_distance(BattleUnitRules const& rules, uint8_t a, uint8_t b) {
    return rules.geometry == BATTLE_ATTACK_DIAGONAL ? diagonal_distance(a, b)
                                                    : battle_line_distance(a, b);
}

RPG_NOINLINE static uint8_t capstone_attack_range(BattleState const& state, uint8_t attacker,
                                                  uint8_t victim, uint8_t distance) {
    auto const& actor = state.units[attacker];
    auto const& target = state.units[victim];
    switch(battle_attack_class(actor.id)) {
        case ATTACK_SHOOT:
            return distance >= ATTACK_INFO[ATTACK_SHOOT].attack_min ? distance : 0;
        case ATTACK_REND:
            return distance == BLOOD_HUNT_RANGE &&
                           target.hp < (battle_max_hp(target) + 1) / 2
                       ? distance : 0;
        case ATTACK_FLANK:
            if(distance == 2 || diagonal_distance(actor.cell, target.cell) == 1) {
                for(uint8_t i = 0; i < state.unit_count; ++i) {
                    auto const& ally = state.units[i];
                    if(ally.hp && ally.team == actor.team &&
                       battle_cells_adjacent(ally.cell, actor.cell) &&
                       battle_cells_adjacent(ally.cell, target.cell))
                        return 1; // The supporting ally opens the attack, including through its cell.
                }
            }
            break;
        default:
            break;
    }
    return 0;
}

// Whether blocking terrain lies strictly between two cells on one row or column: one lookup in the
// battlefield's compile-time screening bits. Inlined, so it adds no frame below the evaluation.
static inline __attribute__((always_inline)) bool battle_terrain_between(uint8_t field, uint8_t a,
                                                                         uint8_t b) {
    return (BATTLEFIELDS[field].screened[a][b >> 3] >> (b & 7)) & 1;
}

// Terrain first, from the battlefield's masks; then the units, so the cost does not grow with the
// distance.
RPG_NOINLINE bool battle_line_blocked(BattleState const& state, uint8_t a, uint8_t b) {
    if(battle_terrain_between(state.field, a, b))
        return true;
    uint8_t lo = a < b ? a : b, hi = a < b ? b : a;
    bool same_row = battle_row(a) == battle_row(b);
    for(uint8_t i = 0; i < state.unit_count; ++i) {
        BattleUnit const& unit = state.units[i];
        uint8_t c = unit.cell;
        if(!unit.hp || c <= lo || c >= hi)
            continue;
        // Between two cells of a row lies every cell with a larger index and a smaller one; on a
        // column only the cells of the same column count.
        if(same_row || battle_column(c) == battle_column(a))
            return true;
    }
    return false;
}

// Only party members learn perks. A mask is enough to reuse the living guards for a whole list.
RPG_NOINLINE static uint8_t movement_guards(BattleState const& state, BATTLE_TEAM team) {
    uint8_t guards = 0;
    for(uint8_t i = 0; i < state.unit_count; ++i) {
        auto const& unit = state.units[i];
        if(!unit.hp || unit.team == team || !(unit.id & BATTLE_PARTY_SOURCE) ||
           attack_perk_rank(unit) < PERK_TRACK_RANKS)
            continue;
        uint8_t attack = battle_attack_class(unit.id);
        if(attack == BLADE_LOCK_CLASS || attack == PIKE_FENCE_CLASS)
            guards |= (uint8_t)(1 << i);
    }
    return guards;
}

static bool reach_covers(BattleState const& state, BattleUnit const& guard, uint8_t mover,
                         uint8_t cell) {
    if(battle_cells_adjacent(guard.cell, cell))
        return true;
    if(!at_range(guard.cell, cell, THROUGH_THE_RANKS_RANGE))
        return false;
    uint8_t middle = (uint8_t)((guard.cell + cell) / 2);
    if(battle_blocked(state.field, middle))
        return false;
    uint8_t blocker = battle_unit_at(state, middle);
    return blocker == BATTLE_NO_UNIT || blocker == mover || state.units[blocker].team == guard.team;
}

RPG_NOINLINE static uint8_t active_pike_fences(BattleState const& state, uint8_t mover,
                                             uint8_t guards) {
    uint8_t fences = 0;
    auto const& actor = state.units[mover];
    for(uint8_t i = 0; guards; ++i, guards >>= 1) {
        if(!(guards & 1))
            continue;
        auto const& guard = state.units[i];
        if(battle_attack_class(guard.id) == PIKE_FENCE_CLASS) {
            if(!reach_covers(state, guard, mover, actor.cell))
                fences |= (uint8_t)(1 << i);
        } else if(battle_cells_adjacent(actor.cell, guard.cell) &&
                  !adjacent_allies(state, mover, actor.cell, true)) {
            return 0xFF; // Blade Lock bars every voluntary move.
        }
    }
    return fences;
}

RPG_NOINLINE static bool crosses_pike_fence(BattleState const& state, uint8_t mover,
                                           uint8_t destination, uint8_t fences) {
    for(uint8_t i = 0; fences; ++i, fences >>= 1)
        if((fences & 1) && reach_covers(state, state.units[i], mover, destination))
            return true;
    return false;
}

static bool trample_enemy(BattleState const& state, BattleUnit const& actor, uint8_t cell) {
    uint8_t crossed = battle_unit_at(state, cell);
    return crossed != BATTLE_NO_UNIT && state.units[crossed].team != actor.team &&
           !battle_blocked(state.field, cell);
}

bool battle_can_move_to(BattleState const& state, uint8_t unit_index, uint8_t destination) {
    if(unit_index >= state.unit_count || destination >= BATTLE_CELLS)
        return false;
    BattleUnit const& actor = state.units[unit_index];
    if(!actor.hp || !battle_open_cell(state, destination))
        return false;
    uint8_t guards = movement_guards(state, actor.team);
    if(guards) {
        uint8_t fences = active_pike_fences(state, unit_index, guards);
        if(fences == 0xFF || (fences && crosses_pike_fence(state, unit_index, destination, fences)))
            return false;
    }
    BattleUnitRules const& rules = battle_rules(actor.id);
    if(rules.movement_type != BATTLE_MOVEMENT_ORTHOGONAL) {
        uint8_t dx = difference(battle_column(actor.cell), battle_column(destination));
        uint8_t dy = difference(battle_row(actor.cell), battle_row(destination));
        if(rules.movement_type == BATTLE_MOVEMENT_VAULT && !(dx && dy) &&
           dx + dy == HIGH_VAULT_RANGE && movement_perk_rank(actor) >= HIGH_VAULT_RANK) {
            uint8_t usual;
            return battle_neighbor(destination, battle_direction(destination, actor.cell), usual) &&
                   !battle_open_cell(state, usual);
        }
        if(!battle_move_reaches(rules.movement_type, rules.move_range, dx, dy))
            return false;
        if(rules.movement_type != BATTLE_MOVEMENT_GLIDE || dx == 1)
            return true;
        uint8_t middle = (uint8_t)((actor.cell + destination) / 2);
        return !battle_blocked(state.field, middle) &&
               battle_unit_at(state, middle) == BATTLE_NO_UNIT;
    }
    uint8_t distance = battle_line_distance(actor.cell, destination);
    if(!distance || distance > rules.move_range)
        return false;
    if(distance == TRAMPLE_RANGE && movement_perk_rank(actor) >= TRAMPLE_RANK) {
        uint8_t middle = (uint8_t)((actor.cell + destination) / 2);
        if(trample_enemy(state, actor, middle))
            return true;
    }
    return !battle_line_blocked(state, actor.cell, destination);
}

RPG_NOINLINE bool battle_reach_through_ally(BattleState const& state, BattleUnit const& attacker,
                                            uint8_t destination) {
    if(!(attacker.id & BATTLE_PARTY_SOURCE))
        return false;
    auto const& member = game.party.members[attacker.id & ~BATTLE_PARTY_SOURCE];
    if(member.attack_class != THROUGH_THE_RANKS_CLASS ||
       party_attack_perk_rank(member) < THROUGH_THE_RANKS_RANK)
        return false;
    uint8_t middle = (uint8_t)((attacker.cell + destination) / 2);
    if(battle_blocked(state.field, middle))
        return false;
    uint8_t blocker = battle_unit_at(state, middle);
    return blocker != BATTLE_NO_UNIT && state.units[blocker].team == attacker.team;
}

bool battle_can_attack(BattleState const& state, uint8_t attacker, uint8_t victim) {
    if(attacker >= state.unit_count || victim >= state.unit_count)
        return false;
    BattleUnit const& actor = state.units[attacker];
    BattleUnit const& target = state.units[victim];
    if(!actor.hp || !target.hp || actor.team == target.team)
        return false;
    BattleUnitRules const& rules = battle_rules(actor.id);
    uint8_t distance = attack_distance(rules, actor.cell, target.cell);
    if(!battle_in_attack_range(rules, distance)) {
        if(attack_perk_rank(actor) < PERK_TRACK_RANKS)
            return false;
        distance = capstone_attack_range(state, attacker, victim, distance);
        if(!distance)
            return false;
    }
    return distance == 1 || !battle_line_blocked(state, actor.cell, target.cell) ||
           (distance == THROUGH_THE_RANKS_RANGE &&
            battle_reach_through_ally(state, actor, target.cell));
}

bool battle_legal(BattleState const& state, BattleAction action) {
    if(state.ended || action.unit_index >= state.unit_count || action.destination >= BATTLE_CELLS)
        return false;
    BattleUnit const& actor = state.units[action.unit_index];
    if(!actor.hp || actor.team != state.turn)
        return false;
    uint8_t occupant = battle_unit_at(state, action.destination);
    if(occupant == BATTLE_NO_UNIT)
        return battle_can_move_to(state, action.unit_index, action.destination);
    return battle_can_attack(state, action.unit_index, occupant);
}

// Whether a cell holds a living unit, from one occupancy mask per row: bit x of rows[y].
static bool battle_occupied(uint8_t const* rows, uint8_t cell) {
    return (rows[battle_row(cell)] >> battle_column(cell)) & 1;
}

// Appends the empty cells a ground move reaches, nearest first, stopping at the first occupied
// cell.
static inline __attribute__((always_inline)) uint8_t battle_ray(uint8_t const* rows, uint8_t* out,
                                                                uint8_t count, uint8_t unit_bits,
                                                                uint8_t cell, int8_t delta,
                                                                uint8_t steps) {
    while(steps--) {
        cell = (uint8_t)(cell + delta);
        if(battle_occupied(rows, cell))
            break;
        out[count++] = (uint8_t)(unit_bits | cell);
    }
    return count;
}

// Appends one jump of (dx, dy) when it lands on the board on an empty cell.
static inline __attribute__((always_inline)) uint8_t battle_jump(uint8_t const* rows, uint8_t* out,
                                                                 uint8_t count, uint8_t unit_bits,
                                                                 uint8_t x, uint8_t y, int8_t dx,
                                                                 int8_t dy) {
    // Unsigned wrap turns a step off the left or top edge into a large value.
    uint8_t nx = (uint8_t)(x + dx), ny = (uint8_t)(y + dy);
    if(nx >= BATTLE_COLUMNS || ny >= BATTLE_ROWS)
        return count;
    uint8_t cell = battle_cell(nx, ny);
    if(!battle_occupied(rows, cell))
        out[count++] = (uint8_t)(unit_bits | cell);
    return count;
}

static uint8_t smaller(uint8_t a, uint8_t b) {
    return a < b ? a : b;
}

RPG_NOINLINE static uint8_t battle_vault_or_glide(BattleUnitRules const& rules, uint8_t const* rows,
                                                  uint8_t* out, uint8_t count, uint8_t unit_bits,
                                                  uint8_t cell) {
    uint8_t x = battle_column(cell), y = battle_row(cell);
    if(rules.movement_type == BATTLE_MOVEMENT_VAULT) {
        int8_t range = (int8_t)rules.move_range;
        count = battle_jump(rows, out, count, unit_bits, x, y, 0, (int8_t)-range);
        count = battle_jump(rows, out, count, unit_bits, x, y, range, 0);
        count = battle_jump(rows, out, count, unit_bits, x, y, 0, range);
        return battle_jump(rows, out, count, unit_bits, x, y, (int8_t)-range, 0);
    }

    uint8_t north = smaller(y, rules.move_range);
    uint8_t east = smaller((uint8_t)(BATTLE_COLUMNS - 1 - x), rules.move_range);
    uint8_t south = smaller((uint8_t)(BATTLE_ROWS - 1 - y), rules.move_range);
    uint8_t west = smaller(x, rules.move_range);
    count = battle_ray(rows, out, count, unit_bits, cell, 1 - BATTLE_COLUMNS, smaller(north, east));
    count = battle_ray(rows, out, count, unit_bits, cell, 1 + BATTLE_COLUMNS, smaller(south, east));
    count =
        battle_ray(rows, out, count, unit_bits, cell, -1 + BATTLE_COLUMNS, smaller(south, west));
    return battle_ray(rows, out, count, unit_bits, cell, -1 - BATTLE_COLUMNS, smaller(north, west));
}

RPG_NOINLINE static uint8_t capstone_ground_moves(BattleState const& state, uint8_t actor,
                                                  uint8_t const* rows, uint8_t* out, bool vault) {
    uint8_t count = 0;
    constexpr DIRECTION directions[] = {NORTH, EAST, SOUTH, WEST};
    for(DIRECTION dir : directions) {
        auto const& unit = state.units[actor];
        uint8_t first, landing;
        if(!battle_neighbor(unit.cell, dir, first))
            continue;
        if(!vault) {
            if(!battle_occupied(rows, first))
                out[count++] = battle_pack({actor, first});
            else if(!trample_enemy(state, unit, first))
                continue;
        }
        if(!battle_neighbor(first, dir, landing))
            continue;
        if(battle_occupied(rows, landing) && (!vault || !battle_neighbor(landing, dir, landing)))
            continue;
        if(!battle_occupied(rows, landing))
            out[count++] = battle_pack({actor, landing});
    }
    return count;
}

RPG_NOINLINE static uint8_t append_moves(BattleState const& state, uint8_t a,
                                         uint8_t const* rows, uint8_t* out) {
    auto const& actor = state.units[a];
    uint8_t count = 0;
    uint8_t x = battle_column(actor.cell), y = battle_row(actor.cell);
    uint8_t unit_bits = (uint8_t)(a << 5);
    BattleUnitRules const& rules = battle_rules(actor.id);
    if(rules.movement_type == BATTLE_MOVEMENT_LEAP) {
        // Clockwise from north-north-east.
        count = battle_jump(rows, out, count, unit_bits, x, y, 1, -2);
        count = battle_jump(rows, out, count, unit_bits, x, y, 2, -1);
        count = battle_jump(rows, out, count, unit_bits, x, y, 2, 1);
        count = battle_jump(rows, out, count, unit_bits, x, y, 1, 2);
        count = battle_jump(rows, out, count, unit_bits, x, y, -1, 2);
        count = battle_jump(rows, out, count, unit_bits, x, y, -2, 1);
        count = battle_jump(rows, out, count, unit_bits, x, y, -2, -1);
        count = battle_jump(rows, out, count, unit_bits, x, y, -1, -2);
    } else if(rules.movement_type != BATTLE_MOVEMENT_ORTHOGONAL) {
        count = battle_vault_or_glide(rules, rows, out, count, unit_bits, actor.cell);
    } else {
        // North, east, south, and west, as far as range and the board edge allow.
        uint8_t range = rules.move_range;
        count = battle_ray(rows, out, count, unit_bits, actor.cell, -(int8_t)BATTLE_COLUMNS,
                           smaller(y, range));
        count = battle_ray(rows, out, count, unit_bits, actor.cell, 1,
                           smaller((uint8_t)(BATTLE_COLUMNS - 1 - x), range));
        count = battle_ray(rows, out, count, unit_bits, actor.cell, BATTLE_COLUMNS,
                           smaller((uint8_t)(BATTLE_ROWS - 1 - y), range));
        count = battle_ray(rows, out, count, unit_bits, actor.cell, -1, smaller(x, range));
    }
    return count;
}

// The enumeration behind battle_actions and battle_unit_actions, for units first to end - 1. The
// row occupancy masks are four bytes on the stack, built once per call.
static uint8_t battle_enumerate(BattleState const& state, uint8_t first, uint8_t end, uint8_t* out,
                                uint8_t& attacks) {
    uint8_t count = 0;
    attacks = 0;
    if(state.ended)
        return 0;
    // Blocking terrain starts the occupancy, so moves, shots, and leap landings treat it as
    // occupied.
    uint8_t rows[BATTLE_ROWS];
    uint8_t const PROGMEM* blocked = BATTLEFIELDS[state.field].blocked;
    for(uint8_t row = 0; row < BATTLE_ROWS; ++row)
        rows[row] = blocked[row];
    for(uint8_t i = 0; i < state.unit_count; ++i) {
        BattleUnit const& unit = state.units[i];
        if(unit.hp)
            rows[battle_row(unit.cell)] =
                (uint8_t)(rows[battle_row(unit.cell)] | (1 << battle_column(unit.cell)));
    }

    // Attacks pair each acting unit with each opponent, so there are at most four pair checks. A
    // shot longer than one cell needs every cell between to be empty.
    for(uint8_t a = first; a < end; ++a) {
        BattleUnit const& actor = state.units[a];
        if(!actor.hp || actor.team != state.turn)
            continue;
        BattleUnitRules const& rules = battle_rules(actor.id);
        uint8_t lo = rules.attack_min, hi = rules.attack_max;
        bool capstone = attack_perk_rank(actor) >= PERK_TRACK_RANKS;
        for(uint8_t v = 0; v < state.unit_count; ++v) {
            BattleUnit const& target = state.units[v];
            if(!target.hp || target.team == actor.team)
                continue;
            uint8_t distance = attack_distance(rules, actor.cell, target.cell);
            if(distance < lo || distance > hi) {
                if(!capstone)
                    continue;
                distance = capstone_attack_range(state, a, v, distance);
                if(!distance)
                    continue;
            }
            if(distance > 1) {
                int8_t delta =
                    (int8_t)(battle_row(actor.cell) == battle_row(target.cell) ? 1
                                                                               : BATTLE_COLUMNS);
                if(target.cell < actor.cell)
                    delta = (int8_t)-delta;
                uint8_t c = (uint8_t)(actor.cell + delta);
                while(c != target.cell && !battle_occupied(rows, c))
                    c = (uint8_t)(c + delta);
                if(c != target.cell && (distance != THROUGH_THE_RANKS_RANGE ||
                                        !battle_reach_through_ally(state, actor, target.cell)))
                    continue;
            }
            out[count++] = (uint8_t)(a << 5 | target.cell);
        }
    }
    attacks = count;

    uint8_t guards = movement_guards(state, state.turn);
    for(uint8_t a = first; a < end; ++a) {
        BattleUnit const& actor = state.units[a];
        if(!actor.hp || actor.team != state.turn)
            continue;
        uint8_t fences = guards ? active_pike_fences(state, a, guards) : 0;
        if(fences == 0xFF)
            continue;
        // The ordering scratch is idle until enumeration completes. Stage only fenced moves.
        static_assert(sizeof(runtime_workspace.battle.order_rows) >= BATTLE_MOVE_LIMIT);
        uint8_t* moves = fences ? runtime_workspace.battle.order_rows : out + count;
        auto const& rules = battle_rules(actor.id);
        bool vault = rules.movement_type == BATTLE_MOVEMENT_VAULT;
        bool ground_perk = rules.move_range == 2 &&
                           (vault || rules.movement_type == BATTLE_MOVEMENT_ORTHOGONAL) &&
                           movement_perk_rank(actor) >= PERK_TRACK_RANKS;
        uint8_t size = ground_perk ? capstone_ground_moves(state, a, rows, moves, vault)
                                   : append_moves(state, a, rows, moves);
        if(!fences) {
            count += size;
            continue;
        }
        for(uint8_t i = 0; i < size; ++i)
            if(!crosses_pike_fence(state, a, battle_unpack(moves[i]).destination, fences))
                out[count++] = moves[i];
    }
    return count;
}

uint8_t battle_actions(BattleState const& state, uint8_t* out, uint8_t& attacks) {
    return battle_enumerate(state, 0, state.unit_count, out, attacks);
}

uint8_t battle_unit_actions(BattleState const& state, uint8_t unit_index, uint8_t* out,
                            uint8_t& attacks) {
    if(unit_index >= state.unit_count) {
        attacks = 0;
        return 0;
    }
    return battle_enumerate(state, unit_index, (uint8_t)(unit_index + 1), out, attacks);
}

// One unit at a time, stopping at the first unit that can act. Kept out of line, so its loop stays
// off the battle screen's frames.
RPG_NOINLINE bool battle_can_act(BattleState const& state) {
    uint8_t attacks = 0;
    for(uint8_t u = 0; u < state.unit_count; ++u)
        if(state.units[u].team == state.turn &&
           battle_unit_actions(state, u, battle_action_scratch, attacks))
            return true;
    return false;
}

struct EngagementDamage {
    uint8_t adjacent = BATTLE_ENGAGEMENT_DAMAGE;
    uint8_t bonus_cell = BATTLE_CELLS;
    uint8_t crossed_cell = BATTLE_CELLS;
    uint8_t extra = PASSING_CUT_BONUS;

    uint8_t at(uint8_t cell, bool beside_landing) const {
        static_assert(SPEARHEAD_BONUS == VAULT_STRIKE_BONUS);
        static_assert(SHOCK_LANDING_BONUS == PASSING_CUT_BONUS &&
                      VAULT_STRIKE_BONUS == SHOCK_LANDING_BONUS);
        if(!beside_landing)
            return extra;
        return (uint8_t)(adjacent +
                         ((cell == bonus_cell || cell == crossed_cell) ? VAULT_STRIKE_BONUS : 0));
    }
};

static EngagementDamage engagement_damage(BattleUnit const& actor, uint8_t destination,
                                          uint8_t engaged) {
    EngagementDamage damage;
    if(!(actor.id & BATTLE_PARTY_SOURCE))
        return damage;
    PartyMember const& member = game.party.members[actor.id & ~BATTLE_PARTY_SOURCE];
    uint8_t rank = party_movement_perk_rank(member);
    if(!rank)
        return damage;
    switch(member.movement_class) {
        case MOVEMENT_LEAP: {
            uint8_t rest = (uint8_t)(engaged & (engaged - 1));
            if(!rest)
                damage.adjacent += LANDING_STRIKE_BONUS;
            else if(rank >= WIDE_LANDING_RANK && !(rest & (rest - 1)))
                damage.adjacent += WIDE_LANDING_BONUS;
            if(rank >= SKYFALL_RANK)
                damage.adjacent += SKYFALL_BONUS;
            break;
        }
        case MOVEMENT_CHARGE:
            battle_neighbor(destination, battle_direction(actor.cell, destination),
                            damage.bonus_cell);
            if(rank >= MOMENTUM_RANK && at_range(actor.cell, destination, MOMENTUM_RANGE)) {
                damage.adjacent += MOMENTUM_BONUS;
                if(rank >= TRAMPLE_RANK)
                    damage.crossed_cell = (uint8_t)((actor.cell + destination) / 2);
            }
            break;
        case MOVEMENT_VAULT:
            battle_neighbor(destination, battle_direction(destination, actor.cell),
                            damage.bonus_cell);
            if(rank >= VAULT_FORCE_RANK)
                damage.adjacent += VAULT_FORCE_BONUS;
            break;
        case MOVEMENT_GLIDE: {
            uint8_t distance = diagonal_distance(actor.cell, destination);
            if(distance == CUTBACK_RANGE && engaged && !(engaged & (engaged - 1)))
                damage.adjacent += CUTBACK_BONUS;
            if(distance == SLIPSTREAM_RANGE && rank >= SLIPSTREAM_RANK) {
                damage.adjacent += SLIPSTREAM_BONUS;
                if(rank >= DIAMOND_WAKE_RANK)
                    damage.extra = damage.adjacent;
            }
            break;
        }
    }
    return damage;
}

uint8_t battle_move_cost(BattleUnit const& actor, bool engages) {
    if(engages && (actor.id & BATTLE_PARTY_SOURCE)) {
        auto const& member = game.party.members[actor.id & ~BATTLE_PARTY_SOURCE];
        if(member.movement_class == FOOTWORK_CLASS &&
           party_movement_perk_rank(member) >= FOOTWORK_RANK)
            return FOOTWORK_BONUS;
    }
    return BATTLE_MOVE_COST;
}

// Damage away from the orthogonal landing neighbors stays separate from the landing bonuses.
RPG_NOINLINE uint8_t battle_extra_engagement(BattleState const& state, BattleUnit const& actor,
                                             uint8_t destination) {
    if(!(actor.id & BATTLE_PARTY_SOURCE))
        return 0;
    auto const& member = game.party.members[actor.id & ~BATTLE_PARTY_SOURCE];
    bool vault = member.movement_class == SHOCK_LANDING_CLASS &&
                 party_movement_perk_rank(member) >= SHOCK_LANDING_RANK;
    bool glide = member.movement_class == PASSING_CUT_CLASS &&
                 party_movement_perk_rank(member) >= PASSING_CUT_RANK;
    if(!(vault || glide))
        return 0;
    bool long_glide = glide && diagonal_distance(actor.cell, destination) == SLIPSTREAM_RANGE;
    uint8_t middle = long_glide ? (uint8_t)((actor.cell + destination) / 2) : actor.cell;
    uint8_t rank = party_movement_perk_rank(member);
    bool diamond = long_glide && rank >= DIAMOND_WAKE_RANK;
    bool high_vault = vault && rank >= HIGH_VAULT_RANK &&
                      at_range(actor.cell, destination, HIGH_VAULT_RANGE);
    uint8_t extra = 0;
    for(uint8_t i = 0; i < state.unit_count; ++i) {
        auto const& unit = state.units[i];
        if(!unit.hp || unit.team == actor.team || battle_cells_adjacent(unit.cell, destination))
            continue;
        if(vault ? diagonal_distance(unit.cell, destination) == 1 ||
                       (high_vault && battle_cells_adjacent(unit.cell, actor.cell) &&
                        at_range(unit.cell, destination, 2))
                 : battle_cells_adjacent(unit.cell, actor.cell) ||
                       battle_cells_adjacent(unit.cell, middle) ||
                       (diamond && (diagonal_distance(unit.cell, actor.cell) == 1 ||
                                    diagonal_distance(unit.cell, middle) == 1)))
            extra |= (uint8_t)(1 << i);
    }
    return extra;
}

static uint8_t landing_targets(BattleState const& state, BattleUnit const& actor,
                               uint8_t destination) {
    uint8_t engaged = 0;
    for(uint8_t i = 0; i < state.unit_count; ++i) {
        BattleUnit const& unit = state.units[i];
        if(unit.hp && unit.team != actor.team && battle_cells_adjacent(unit.cell, destination))
            engaged |= (uint8_t)(1 << i);
    }
    return engaged;
}

RPG_NOINLINE BattleEngagement battle_engagement(BattleState const& state, uint8_t unit_index,
                                                uint8_t destination, uint8_t extra) {
    BattleEngagement engagement{};
    BattleUnit const& actor = state.units[unit_index];
    uint8_t engaged = landing_targets(state, actor, destination);
    if(!(engaged | extra))
        return engagement;
    EngagementDamage damage = engagement_damage(actor, destination, engaged);
    for(uint8_t i = 0; engaged | extra; ++i, engaged >>= 1, extra >>= 1) {
        if(!((engaged | extra) & 1))
            continue;
        BattleUnit const& unit = state.units[i];
        uint8_t amount = damage.at(unit.cell, engaged & 1);
        ++engagement.hits;
        engagement.damage += unit.hp < amount ? unit.hp : amount;
        engagement.kills += unit.hp <= amount;
    }
    return engagement;
}

BattleEngagement battle_engagement(BattleState const& state, uint8_t unit_index,
                                   uint8_t destination) {
    uint8_t extra = battle_extra_engagement(state, state.units[unit_index], destination);
    return battle_engagement(state, unit_index, destination, extra);
}

BATTLE_ACTION_KIND battle_action_kind(BattleState const& state, BattleAction action) {
    BattleUnit const& actor = state.units[action.unit_index];
    uint8_t opponents =
        battle_living(state, actor.team == BATTLE_PARTY ? BATTLE_ENEMY : BATTLE_PARTY);
    uint8_t occupant = battle_unit_at(state, action.destination);
    uint8_t kills;
    if(occupant == BATTLE_NO_UNIT) {
        BattleEngagement engagement =
            battle_engagement(state, action.unit_index, action.destination);
        if(!engagement.hits)
            return BATTLE_ACTION_MOVE;
        kills = engagement.kills;
    } else {
        kills = battle_attack_impact(state, action.unit_index, occupant).kills;
    }
    if(!kills)
        return BATTLE_ACTION_ATTACK;
    return kills == opponents ? BATTLE_ACTION_WIN : BATTLE_ACTION_KILL;
}

uint8_t battle_living(BattleState const& state, BATTLE_TEAM team) {
    uint8_t count = 0;
    for(uint8_t i = 0; i < state.unit_count; ++i)
        if(state.units[i].hp && state.units[i].team == team)
            ++count;
    return count;
}

// Ends the battle once a side has no living units, after `acting` has resolved an action or part of
// one. The acting side wins when no opponent lives; otherwise its opponent wins when the acting
// side has none left. Returns whether the battle has ended.
static bool battle_check(BattleState& state, BATTLE_TEAM acting) {
    uint8_t living[2] = {};
    for(uint8_t i = 0; i < state.unit_count; ++i)
        if(state.units[i].hp)
            ++living[state.units[i].team];
    BATTLE_TEAM opposing = acting == BATTLE_PARTY ? BATTLE_ENEMY : BATTLE_PARTY;
    BATTLE_TEAM winner;
    if(!living[opposing])
        winner = acting;
    else if(!living[acting])
        winner = opposing;
    else
        return false;
    state.ended = true;
    state.result = winner == BATTLE_PARTY ? BATTLE_VICTORY : BATTLE_DEFEAT;
    return true;
}

static void battle_next_turn(BattleState& state) {
    state.turn = state.turn == BATTLE_PARTY ? BATTLE_ENEMY : BATTLE_PARTY;
}

// Removes HP from a unit, stopping at zero. Returns whether the unit fell.
static bool battle_damage(BattleUnit& unit, uint8_t amount) {
    unit.hp = unit.hp > amount ? (uint8_t)(unit.hp - amount) : 0;
    return !unit.hp;
}

static uint8_t concussion_landing(uint8_t center, uint8_t cell) {
    uint8_t x = (uint8_t)(2 * battle_column(cell) - battle_column(center));
    uint8_t y = (uint8_t)(2 * battle_row(cell) - battle_row(center));
    return x < BATTLE_COLUMNS && y < BATTLE_ROWS ? battle_cell(x, y) : BATTLE_CELLS;
}

RPG_NOINLINE static uint8_t concussion_targets(BattleState const& state, uint8_t center,
                                               uint8_t targets) {
    uint8_t push = 0;
    for(uint8_t i = 0; targets; ++i, targets >>= 1) {
        if(targets & 1) {
            uint8_t landing = concussion_landing(center, state.units[i].cell);
            if(landing < BATTLE_CELLS && battle_open_cell(state, landing))
                push |= (uint8_t)(1 << i);
        }
    }
    return push;
}

RPG_NOINLINE static bool apply_secondary_damage(BattleState& state, uint8_t attacker,
                                                SecondaryDamage const& splash, uint8_t center,
                                                uint8_t push) {
    bool fell = false;
    for(uint8_t i = 0, targets = splash.targets; targets; ++i, targets >>= 1, push >>= 1) {
        if(!(targets & 1))
            continue;
        auto& unit = state.units[i];
        if(battle_damage(unit, splash_damage(state, splash, state.units[attacker].cell, center, i)))
            fell = true;
        else if(push & 1)
            unit.cell = concussion_landing(center, unit.cell);
    }
    return fell;
}

RPG_NOINLINE static void wrecking_push(BattleState& state, uint8_t origin, uint8_t end,
                                       uint8_t targets) {
    // Damage affected only units inside the original line, whose end has already been recorded.
    DIRECTION direction = battle_direction(origin, end);
    uint8_t far = end, next;
    while(battle_neighbor(far, direction, next) && battle_open_cell(state, next))
        far = next;
    if(far == end)
        return;
    int8_t shift = (int8_t)(far - end);
    for(uint8_t i = 0; targets; ++i, targets >>= 1)
        if((targets & 1) && state.units[i].hp)
            state.units[i].cell = (uint8_t)(state.units[i].cell + shift);
}

static void battle_heal(BattleUnit& unit, uint8_t amount) {
    uint16_t healed = (uint16_t)unit.hp + amount;
    uint8_t maximum = battle_max_hp(unit);
    unit.hp = healed < maximum ? (uint8_t)healed : maximum;
}

RPG_NOINLINE static void drain_healing(BattleState& state, BattleUnit& actor, bool finished,
                                       uint8_t hp_lost, uint8_t rank) {
    if(finished && rank >= SOUL_DIVIDEND_RANK) {
        for(uint8_t i = 0; i < state.unit_count; ++i) {
            auto& unit = state.units[i];
            if(unit.hp && unit.team == actor.team)
                battle_heal(unit, SOUL_DIVIDEND_BONUS);
        }
        return;
    }
    uint8_t recovery = rank >= LIFE_TAP_RANK ? 1 + LIFE_TAP_BONUS : 1;
    if(rank >= DEEP_SIPHON_RANK)
        recovery += DEEP_SIPHON_BONUS;
    if(!finished && recovery >= hp_lost)
        recovery = (uint8_t)(hp_lost - 1);
    battle_heal(actor, recovery);
}

RPG_NOINLINE static bool battle_resolve_attack(BattleState& state, uint8_t attacker, uint8_t victim,
                                               bool special) {
    BattleUnit& actor = state.units[attacker];
    BattleUnit& target = state.units[victim];
    uint8_t attack = battle_attack_class(actor.id);
    uint8_t rank = attack_perk_rank(actor);
    uint8_t position = attack_position(state, attacker, victim, attack);
    uint8_t raw = raw_attack_damage(state, attacker, victim, attack, position, special);
    uint8_t reflected = 0;
    uint8_t primary = guarded_damage(state, victim, raw, reflected);
    SecondaryDamage splash = attack_splash(state, attacker, victim, position, raw, special);
    uint8_t push = attack == ATTACK_BLAST && rank >= CONCUSSION_RANK
                       ? concussion_targets(state, target.cell, splash.targets) : 0;
    uint8_t hp_lost = target.hp < primary ? target.hp : primary;
    bool fell = battle_damage(target, primary);
    if(splash.targets)
        fell = apply_secondary_damage(state, attacker, splash, target.cell, push) || fell;
    if(reflected)
        fell = battle_damage(actor, reflected) || fell;

    BATTLE_ATTACK_EFFECT effect = ATTACK_INFO[attack].effect;
    if(effect == BATTLE_EFFECT_DRAIN && hp_lost)
        drain_healing(state, actor, !target.hp, hp_lost, rank);
    if(effect == BATTLE_EFFECT_RECOIL) {
        if(position < BATTLE_CELLS) {
            if(target.hp && rank >= DRAGBACK_RANK)
                target.cell = actor.cell;
            actor.cell = position;
        }
    } else if(attack == ATTACK_IMPACT && rank >= WRECKING_LINE_RANK) {
        wrecking_push(state, actor.cell, splash.line_end, (uint8_t)(splash.targets | (1 << victim)));
    } else if(target.hp && (attack != ATTACK_IMPACT || position == BATTLE_NO_UNIT) &&
              (effect == BATTLE_EFFECT_KNOCKBACK ||
               (special && SPECIAL_INFO[attack].effect == BATTLE_EFFECT_KNOCKBACK))) {
        // An enemy hit by Collision occupies the push cell for this attack, even if it falls.
        uint8_t landing =
            displacement_cell(state, target.cell, battle_direction(actor.cell, target.cell));
        if(landing < BATTLE_CELLS)
            target.cell = landing;
    }
    return fell;
}

RPG_NOINLINE static bool battle_resolve_move(BattleState& state, BattleUnit& actor,
                                             uint8_t destination) {
    uint8_t engaged = landing_targets(state, actor, destination);
    uint8_t extra = battle_extra_engagement(state, actor, destination);
    EngagementDamage damage;
    if(engaged | extra)
        damage = engagement_damage(actor, destination, engaged);
    uint8_t move_cost = battle_move_cost(actor, (engaged | extra) != 0);
    uint8_t origin = actor.cell;
    bool skyhook = engaged && !(engaged & (engaged - 1)) &&
                   movement_perk_rank(actor) >= SKYHOOK_RANK &&
                   battle_movement_class(actor.id) == SKYHOOK_CLASS;
    actor.cell = destination;
    bool fell = false;
    for(uint8_t i = 0; engaged | extra; ++i, engaged >>= 1, extra >>= 1) {
        if((engaged | extra) & 1) {
            auto& unit = state.units[i];
            uint8_t amount = damage.at(unit.cell, engaged & 1);
            if(battle_damage(unit, amount))
                fell = true;
            else if(skyhook && (engaged & 1))
                unit.cell = origin;
        }
    }

    if(damage.bonus_cell < BATTLE_CELLS && battle_movement_class(actor.id) == OVERRUN_CLASS &&
       movement_perk_rank(actor) >= OVERRUN_RANK) {
        uint8_t target = battle_unit_at(state, damage.bonus_cell);
        if(target != BATTLE_NO_UNIT && state.units[target].team != actor.team) {
            uint8_t landing =
                displacement_cell(state, damage.bonus_cell, battle_direction(origin, destination));
            if(landing < BATTLE_CELLS)
                state.units[target].cell = landing;
        }
    }

    // Victory from engagement takes precedence over the movement cost.
    if(fell && battle_check(state, actor.team))
        return false;
    return battle_damage(actor, move_cost) || fell;
}

// Resolves a unit's move or attack. A special attack, which the caller has checked and paid for,
// uses the special's damage and effect against its victim.
static void battle_act(BattleState& state, BattleAction action, bool special) {
    BattleUnit& actor = state.units[action.unit_index];
    BATTLE_TEAM acting = actor.team;
    uint8_t destination = action.destination;
    uint8_t occupant = battle_unit_at(state, destination);
    // A running battle has living units on both sides, so only a defeat can end it: the sides are
    // counted only when some unit fell.
    bool fell = false;
    ++state.activations;
    if(occupant == BATTLE_NO_UNIT)
        fell = battle_resolve_move(state, actor, destination);
    else
        fell = battle_resolve_attack(state, action.unit_index, occupant, special);

    if(state.ended)
        return;
    if(fell && battle_check(state, acting))
        return;
    battle_next_turn(state);
}

void battle_apply(BattleState& state, BattleAction action) {
    if(action.unit_index == BATTLE_NO_UNIT) {
        battle_pass(state);
        return;
    }
    battle_act(state, action, false);
}

bool battle_special_legal(BattleState const& state, BattleAction action) {
    if(state.ended || action.unit_index >= state.unit_count || action.destination >= BATTLE_CELLS)
        return false;
    BattleUnit const& actor = state.units[action.unit_index];
    if(!(actor.id & BATTLE_PARTY_SOURCE) || !actor.hp || actor.team != state.turn)
        return false;
    uint8_t victim = battle_unit_at(state, action.destination);
    if(victim == BATTLE_NO_UNIT || !battle_can_attack(state, action.unit_index, victim))
        return false;
    PartyMember const& member = game.party.members[actor.id & ~BATTLE_PARTY_SOURCE];
    return member.ap >= SPECIAL_INFO[battle_attack_class(actor.id)].ap;
}

void battle_apply_special(BattleState& state, BattleAction action) {
    if(!battle_special_legal(state, action))
        return;
    uint8_t slot = state.units[action.unit_index].id & ~BATTLE_PARTY_SOURCE;
    game.party.members[slot].ap -=
        SPECIAL_INFO[battle_attack_class(state.units[action.unit_index].id)].ap;
    battle_act(state, action, true);
}

void battle_pass(BattleState& state) {
    if(state.ended)
        return;
    ++state.activations;
    // Apply the price to the entire team before checking defeat or handing over the turn.
    for(uint8_t i = 0; i < state.unit_count; ++i) {
        BattleUnit& unit = state.units[i];
        if(unit.hp && unit.team == state.turn)
            battle_damage(unit, BATTLE_MOVE_COST);
    }
    if(battle_check(state, state.turn))
        return;
    battle_next_turn(state);
}

uint16_t battle_settle(BattleState& state) {
    uint16_t passes = 0;
    while(!state.ended && !battle_can_act(state)) {
        battle_pass(state);
        ++passes;
    }
    return passes;
}
