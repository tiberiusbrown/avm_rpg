#ifndef BATTLE_REFERENCE_H
#define BATTLE_REFERENCE_H

#include "battle.h"
#include "battle_ai.h"

// Host-only reference for the search. It is an unpruned negamax that recurses, which production code
// never does. It walks the same priority order, uses the same terminal and utility scores, and keeps
// the first action that reaches the best score, so the two must agree exactly on the action and the
// score.

// A recursive, unpruned reference search for the side to act at `ply`.
inline int16_t reference_value(BattleState const& s, BATTLE_TEAM side, uint8_t ply, uint8_t depth,
                               uint64_t& nodes) {
    if(s.ended || ply == depth)
        return battle_score(s, side, ply);
    BATTLE_TEAM foe = side == BATTLE_PARTY ? BATTLE_ENEMY : BATTLE_PARTY;
    int16_t best = -32000;
    bool any = false;
    for(int priority = BATTLE_PRIORITY_WIN; priority < BATTLE_PRIORITY_COUNT; ++priority) {
        BattleActionCursor cursor = battle_actions(s);
        BattleAction action{};
        while(battle_next_action(s, cursor, action)) {
            if(battle_search_priority(s, action) != priority)
                continue;
            any = true;
            BattleState child = s;
            battle_apply(child, action);
            ++nodes;
            int16_t value = (int16_t)-reference_value(child, foe, ply + 1, depth, nodes);
            if(value > best)
                best = value;
        }
    }
    if(!any) {
        BattleState child = s;
        battle_pass(child);
        ++nodes;
        best = (int16_t)-reference_value(child, foe, ply + 1, depth, nodes);
    }
    return best;
}

// The unpruned reference at the root. The first action in move order with the best score wins, so a
// tie never replaces an earlier action.
inline BattleSearchResult reference_search(BattleState const& root, uint8_t depth, uint64_t& nodes) {
    BattleSearchResult best{BattleAction{BATTLE_NO_UNIT, 0}, 0};
    bool found = false;
    BATTLE_TEAM foe = root.turn == BATTLE_PARTY ? BATTLE_ENEMY : BATTLE_PARTY;
    for(int priority = BATTLE_PRIORITY_WIN; priority < BATTLE_PRIORITY_COUNT; ++priority) {
        BattleActionCursor cursor = battle_actions(root);
        BattleAction action{};
        while(battle_next_action(root, cursor, action)) {
            if(battle_search_priority(root, action) != priority)
                continue;
            BattleState child = root;
            battle_apply(child, action);
            ++nodes;
            int16_t score = (int16_t)-reference_value(child, foe, 1, depth, nodes);
            if(!found || score > best.score) {
                best = BattleSearchResult{action, score};
                found = true;
            }
        }
    }
    if(!found) {
        BattleState child = root;
        battle_pass(child);
        ++nodes;
        best.action = BattleAction{BATTLE_NO_UNIT, 0};
        best.score = (int16_t)-reference_value(child, foe, 1, depth, nodes);
    }
    return best;
}

#endif
