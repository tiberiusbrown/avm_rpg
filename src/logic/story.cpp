#include "story.h"

#include "chunk.h"
#include "enemy_catalog.h"
#include "map_positions.h"
#include "npc.h"
#include "quest.h"
#include "sfx.h"

namespace {

constexpr uint8_t GO_HOME_EXPERIENCE = 3;
constexpr uint8_t GIANT_RAT_EXPERIENCE = 5, GIANT_RAT_GOLD = 10;
// How near the traveler comes before noticing the rat, in pixels along both axes together.
constexpr uint16_t RAT_SIGHT = 3 * TILE;

static_assert(enemy_defeated_flag(NPC_GIANT_RAT) == FLAG_RAT_DEFEATED,
              "the giant rat's catalog index is its defeat flag");

void greet() {
    game.set_flag(FLAG_ELIZA_GREETED);
    dialogue_show(
        NPC_ELIZA,
        F("Alden! I've been looking for you everywhere! Please, I really need your help!"));
}

bool near_rat() {
    Npc const* rat = npc_find(NPC_GIANT_RAT);
    if(!rat)
        return false;
    uint16_t x = rat->actor.x, y = rat->actor.y;
    uint16_t dx = x > game.player.x ? x - game.player.x : game.player.x - x;
    uint16_t dy = y > game.player.y ? y - game.player.y : game.player.y - y;
    return dx + dy <= RAT_SIGHT;
}

// The way home, from waking to the front door. Returns true when it moved the story on, so that
// a quest panel can appear before anything else happens.
bool follow_the_way_home() {
    if(!game.get_flag(FLAG_HOME_STARTED)) {
        // Wandering away from the nap spot sets the traveler looking for the way home.
        if(game.area != positions::PlayerStart.area ||
           game.player_is_in_region(positions::NapAreaRegion))
            return false;
        dialogue_show(NPC_ALDEN, F("Now which way was home again?"));
        quest_start(QUEST_GO_HOME);
        quest_select(QUEST_GO_HOME);
        return true;
    }
    // The quest's panel appears before this frame, so the Journal hint follows it.
    if(!game.get_flag(FLAG_MENU_JOURNAL)) {
        game.set_flag(FLAG_MENU_JOURNAL);
        dialogue_show(F("Press B to pause the game and access your journal."));
        return true;
    }
    if(game.get_flag(FLAG_HOME_REACHED))
        return false;
    if(game.player_is_in_region(positions::AldenRoomsRegion)) {
        game.set_flag(FLAG_HOME_REACHED);
        quest_award_reward(QUEST_GO_HOME, GO_HOME_EXPERIENCE, 0);
        return true;
    }
    if(game.get_flag(FLAG_LEFT_CLEARING))
        return false;
    if(game.player_is_in_region(positions::PathEastRegion)) {
        game.set_flag(FLAG_LEFT_CLEARING);
        return true;
    }
    if(game.get_flag(FLAG_BOULDERS_NOTICED) ||
       !game.player_is_in_region(positions::BoulderSightRegion))
        return false;
    game.set_flag(FLAG_BOULDERS_NOTICED);
    dialogue_show(NPC_ALDEN, F("Hey! Those boulders must have rolled while I slept. The way out is "
                               "blocked! Maybe I can push them back?"));
    dialogue_show(F("Press A while facing something to interact with it."));
    return true;
}

// Eliza's plea, the rat in her bedroom, and the choice of how to fight it.
void follow_the_giant_rat() {
    if(!game.get_flag(FLAG_HOME_REACHED))
        return;
    if(!game.get_flag(FLAG_ELIZA_GREETED)) {
        if(game.player_is_in_region(positions::AldenRoomsRegion) && npc_find(NPC_ELIZA))
            greet();
        return;
    }
    if(!game.get_flag(FLAG_RAT_ACCEPTED) || game.get_flag(FLAG_RAT_DEFEATED))
        return;
    if(!game.get_flag(FLAG_RAT_SIGHTED)) {
        if(game.player_is_in_region(positions::ElizaBedroomRegion) && near_rat()) {
            game.set_flag(FLAG_RAT_SIGHTED);
            dialogue_show(NPC_ALDEN,
                          F("There's the giant rat Eliza mentioned. It sure is big. I wonder how "
                            "effective my fighting style would be against a giant rat...."));
        }
        return;
    }
    if(game.get_flag(FLAG_CLASSES_CHOSEN) && !game.get_flag(FLAG_MENU_PARTY)) {
        game.set_flag(FLAG_MENU_PARTY);
        dialogue_show(F("Press B to access the Party menu, which gives information on each party "
                        "member's classes, perks, equipment, and special abilities."));
        npc_start_pursuit(NPC_GIANT_RAT);
    }
}

} // namespace

RPG_NOINLINE void story_update() {
    if(!game.get_flag(FLAG_INTRO_SHOWN)) {
        game.set_flag(FLAG_INTRO_SHOWN);
        dialogue_show(NPC_ALDEN, F("Wow, I sure slept a while...."));
        return;
    }
    if(!follow_the_way_home())
        follow_the_giant_rat();
}

// Out of line: the exploration loop asks every frame, and inlining would grow the frame that
// every deep path stands on.
RPG_NOINLINE bool story_class_choice_due() {
    return game.get_flag(FLAG_RAT_SIGHTED) && !game.get_flag(FLAG_CLASSES_CHOSEN) &&
           !game.get_flag(FLAG_RAT_DEFEATED);
}

void story_choose_classes(MOVEMENT_CLASS movement, ATTACK_CLASS attack) {
    PartyMember& traveler = game.party.members[0];
    traveler.movement_class = movement;
    traveler.attack_class = attack;
    game.set_flag(FLAG_CLASSES_CHOSEN);
}

uint16_t story_locked_menu_items() {
    uint16_t locked = 0;
    if(!game.get_flag(FLAG_MENU_PARTY))
        locked |= 1u << PAUSE_PARTY;
    if(!game.get_flag(FLAG_MENU_SUPPLIES))
        locked |= 1u << PAUSE_SUPPLIES;
    if(!game.get_flag(FLAG_MENU_JOURNAL))
        locked |= 1u << PAUSE_JOURNAL;
    return locked;
}

void story_talk_eliza() {
    if(!game.get_flag(FLAG_HOME_REACHED))
        return;
    if(!game.get_flag(FLAG_ELIZA_GREETED)) {
        greet();
        return;
    }
    if(game.get_flag(FLAG_RAT_REPORTED))
        return;
    if(!game.get_flag(FLAG_RAT_ACCEPTED)) {
        if(dialogue_ask(NPC_ELIZA,
                        F("Alden... you have to help me! There's a giant rat inside my home... "
                          "bigger than anything I've ever seen! Will you please get rid of it?"))) {
            dialogue_show(NPC_ELIZA, F("Thank you so much! I'll just wait here.... You're a much "
                                       "better fighter than I am."));
            quest_start(QUEST_GIANT_RAT);
            quest_select(QUEST_GIANT_RAT);
        } else {
            dialogue_show(NPC_ELIZA, F("You won't help me? Oh dear.... What am I going to do?"));
        }
        return;
    }
    if(!game.get_flag(FLAG_RAT_DEFEATED)) {
        dialogue_show(NPC_ELIZA,
                      F("I'll just wait here.... You're a much better fighter than I am."));
        return;
    }
    dialogue_show(NPC_ALDEN, F("The rat is gone, Eliza."));
    dialogue_show(NPC_ELIZA, F("Oh, thank goodness! I knew you could do it."));
    game.set_flag(FLAG_RAT_REPORTED);
    quest_award_reward(QUEST_GIANT_RAT, GIANT_RAT_EXPERIENCE, GIANT_RAT_GOLD);
}

void story_push_boulder(NPC_ID id) {
    sfx_play(npc_push(id, game.player.facing, positions::BoulderGateRegion) ? SFX_BOULDER_ROLL
                                                                            : SFX_ERROR);
}
