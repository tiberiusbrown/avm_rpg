#ifndef BATTLE_REFERENCE_H
#define BATTLE_REFERENCE_H

#include "battle.h"
#include "battle_ai.h"

// Host-only reference for the search. It is an unpruned negamax that recurses, which production
// code never does. It uses the same terminal and utility scores, walks the root's actions in the
// search's root order, and keeps the first root action that reaches the best score, so the two must
// agree exactly on the action and the score of a complete fixed-depth search.

// The legal actions of a position in search order. Below the root, order changes only which
// branches are pruned, not any value, so only the root's order has to match the search's: approach
// ordering applies there unless the root's children are all leaves.
struct ReferenceActions {
    uint8_t count;
    uint8_t actions[BATTLE_ACTION_CAPACITY];
};

inline ReferenceActions reference_actions(BattleState const& s, bool approach) {
    ReferenceActions list{};
    uint8_t attacks = 0;
    list.count = battle_actions(s, list.actions, attacks);
    battle_order_actions(s, list.actions, list.count, attacks, approach);
    return list;
}

// A recursive, unpruned reference search for the side to act at `ply`.
inline int16_t reference_value(BattleState const& s, BATTLE_TEAM side, uint8_t ply, uint8_t depth,
                               uint64_t& nodes) {
    if(s.ended || ply == depth)
        return battle_score(s, side, ply);
    BATTLE_TEAM foe = side == BATTLE_PARTY ? BATTLE_ENEMY : BATTLE_PARTY;
    ReferenceActions list = reference_actions(s, true);
    if(list.count == 0) {
        BattleState child = s;
        battle_pass(child);
        ++nodes;
        return (int16_t)-reference_value(child, foe, (uint8_t)(ply + 1), depth, nodes);
    }
    int16_t best = -32000;
    for(uint8_t i = 0; i < list.count; ++i) {
        BattleState child = s;
        battle_apply(child, battle_unpack(list.actions[i]));
        ++nodes;
        int16_t value = (int16_t)-reference_value(child, foe, (uint8_t)(ply + 1), depth, nodes);
        if(value > best)
            best = value;
    }
    return best;
}

// The value of one root action at `depth`: the action's child searched to the remaining depth.
inline int16_t reference_action_value(BattleState const& root, BattleAction action, uint8_t depth) {
    uint64_t nodes = 0;
    BATTLE_TEAM foe = root.turn == BATTLE_PARTY ? BATTLE_ENEMY : BATTLE_PARTY;
    BattleState child = root;
    if(action.unit_index == BATTLE_NO_UNIT)
        battle_pass(child);
    else
        battle_apply(child, action);
    return (int16_t)-reference_value(child, foe, 1, depth, nodes);
}

// The unpruned reference at the root. The first action in search order with the best score wins, so
// a tie never replaces an earlier action.
inline BattleSearchResult reference_search(BattleState const& root, uint8_t depth, uint64_t& nodes) {
    BattleSearchResult best{BattleAction{BATTLE_NO_UNIT, 0}, 0};
    if(root.ended) {
        best.score = battle_score(root, root.turn, 0);
        return best;
    }
    BATTLE_TEAM foe = root.turn == BATTLE_PARTY ? BATTLE_ENEMY : BATTLE_PARTY;
    ReferenceActions list = reference_actions(root, depth > 1);
    if(list.count == 0) {
        BattleState child = root;
        battle_pass(child);
        ++nodes;
        best.score = (int16_t)-reference_value(child, foe, 1, depth, nodes);
        return best;
    }
    for(uint8_t i = 0; i < list.count; ++i) {
        BattleAction action = battle_unpack(list.actions[i]);
        BattleState child = root;
        battle_apply(child, action);
        ++nodes;
        int16_t score = (int16_t)-reference_value(child, foe, 1, depth, nodes);
        if(i == 0 || score > best.score)
            best = BattleSearchResult{action, score};
    }
    return best;
}

#endif
