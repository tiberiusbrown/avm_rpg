#ifndef BATTLE_REFERENCE_H
#define BATTLE_REFERENCE_H

#include "battle.h"
#include "battle_ai.h"

// Host-only reference for the search. It is an unpruned negamax that recurses, which production
// code never does. It shares the move order, the terminal and static scores, and the root tie-break
// with the production search, so the two must agree exactly on the action and the score.

// A recursive, unpruned reference search. It shares the move order, the terminal and static scores,
// and the root tie-break with the production search. It exists only in host tests, so production
// code keeps its explicit stack.
inline int16_t reference_value(BattleState const& s, BATTLE_TEAM side, uint8_t ply, uint8_t depth,
                               uint64_t& nodes) {
    if(s.ended || ply == depth)
        return battle_score(s, side, ply);
    BATTLE_TEAM foe = side == BATTLE_PARTY ? BATTLE_ENEMY : BATTLE_PARTY;
    int16_t best = -32000;
    bool any = false;
    for(int kind = BATTLE_ACTION_WIN; kind <= BATTLE_ACTION_MOVE; ++kind) {
        BattleActionCursor cursor = battle_actions(s);
        BattleAction action{};
        while(battle_next_action(s, cursor, action)) {
            if(battle_action_kind(s, action) != kind)
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

inline BattleSearchResult reference_search(BattleState const& root, uint8_t depth,
                                           uint64_t& nodes) {
    BattleSearchResult best{BattleAction{BATTLE_NO_UNIT, 0}, 0};
    int16_t best_key = -1;
    bool found = false;
    BATTLE_TEAM foe = root.turn == BATTLE_PARTY ? BATTLE_ENEMY : BATTLE_PARTY;
    for(int kind = BATTLE_ACTION_WIN; kind <= BATTLE_ACTION_MOVE; ++kind) {
        BattleActionCursor cursor = battle_actions(root);
        BattleAction action{};
        while(battle_next_action(root, cursor, action)) {
            if(battle_action_kind(root, action) != kind)
                continue;
            BattleState child = root;
            battle_apply(child, action);
            ++nodes;
            int16_t score = (int16_t)-reference_value(child, foe, 1, depth, nodes);
            int16_t key = battle_greedy_score(root, action);
            if(!found || score > best.score || (score == best.score && key > best_key)) {
                best = BattleSearchResult{action, score};
                best_key = key;
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
