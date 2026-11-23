#ifndef QUEST_H
#define QUEST_H

#include "rpg.h"
#include "quest_ids.h"

#include <type_traits>

// Derived by each quest from game state on every query. No generic status is stored.
enum QUEST_STATUS : uint8_t { QUEST_NOT_STARTED, QUEST_ACTIVE, QUEST_COMPLETED, QUEST_FAILED };

// Event subscriptions. A FlagChanged subscription marks its quest for evaluate() at the next
// settle, when a flag actually changes. An NpcTalked subscription calls on(NpcTalked<ID>) once for
// each processed conversation, at the dialogue's safe point.
template <FLAG FLAG_ID> struct FlagChanged {
    static_assert(FLAG_ID > FLAG_RESERVED && FLAG_ID < FLAG_COUNT, "invalid flag subscription");
    static constexpr bool is_event = true;
    static constexpr bool flag_event = true;
    static constexpr uint16_t key = FLAG_ID;
};

template <NPC_ID ID> struct NpcTalked {
    static_assert(ID > NPC_NONE && ID < NPC_ID_LIMIT, "invalid NPC identity for conversation event");
    static constexpr bool is_event = true;
    static constexpr bool flag_event = false;
    static constexpr uint16_t key = ID;
};

// Naming a member of the event type instantiates it, so its own checks run during validation.
template <typename> inline constexpr bool is_quest_event = false;
template <FLAG FLAG_ID> inline constexpr bool is_quest_event<FlagChanged<FLAG_ID>> = FlagChanged<FLAG_ID>::is_event;
template <NPC_ID ID> inline constexpr bool is_quest_event<NpcTalked<ID>> = NpcTalked<ID>::is_event;

template <typename...> inline constexpr bool all_unique = true;
template <typename FIRST, typename... REST>
inline constexpr bool all_unique<FIRST, REST...> =
    ((!std::is_same_v<FIRST, REST>) && ...) && all_unique<REST...>;

template <typename... EVENTS> struct Watch {
    static_assert((is_quest_event<EVENTS> && ...),
                  "unsupported event subscription; use FlagChanged or NpcTalked");
    static_assert(all_unique<EVENTS...>, "duplicate event subscription");

    static constexpr bool watches_flag(FLAG flag) {
        return ((EVENTS::flag_event && EVENTS::key == flag) || ...);
    }
};

// Each quest specializes Quest<ID> in its own header under src/quests. The primary template is
// never defined. A definition provides these members, with the signatures shown:
//   static QUEST_STATUS status();              required, derived from game state
//   static char const PROGMEM* title();        required, the Journal name
//   static char const PROGMEM* summary();      required, progress text for the current status
//   static char const PROGMEM* objective();    optional, nullptr when there is no goal
//   static bool marker(uint8_t area, TilePos& out);  optional map marker for the objective
//   static void start();                       optional, records the start fact
//   static bool consistent();                  optional save validation of quest state
//   static void evaluate();                    optional, for FlagChanged subscriptions
//   static void on(NpcTalked<ID>);             one per NpcTalked subscription
// Subscriptions are declared as `using Watches = Watch<...>;`. Quests without one never receive
// events. Queries are read-only; only start(), handlers and evaluate() may change game state.
template <QUEST_ID ID> struct Quest;

// The public API takes runtime IDs, and the compiled dispatch reaches only defined quests.
// QUEST_NONE and out-of-range IDs are never quests: queries answer as not started, and mutators
// refuse.
QUEST_STATUS quest_status(QUEST_ID id);
// Starts a not-started quest that defines start(). A started, completed or failed quest is left
// unchanged. Its start notice offers tracking.
bool quest_start(QUEST_ID id);
char const PROGMEM* quest_title(QUEST_ID id);
char const PROGMEM* quest_summary(QUEST_ID id);
char const PROGMEM* quest_objective(QUEST_ID id);
// The quest's marker for an area, valid only while the quest is active and the position is in
// bounds for that area.
bool quest_marker(QUEST_ID id, uint8_t area, TilePos& out);

// Copy a fixed marker as immediate coordinates, with no stored map position.
template <TilePos POS> bool quest_fixed_marker(uint8_t area, TilePos& out) {
    if(area != POS.area)
        return false;
    out.area = POS.area;
    out.x = POS.x;
    out.y = POS.y;
    return true;
}

// Tracking is separate from status and from the Journal's highlight. Only an active quest can be
// tracked, and at most one quest is tracked at a time.
QUEST_ID quest_selected();
bool quest_select(QUEST_ID id);
void quest_deselect();
// The tracked quest's marker in an area. It is false when no quest is tracked.
bool quest_map_marker(uint8_t area, TilePos& out);
// The tracked quest's local objective, or the exit toward an objective in another area.
bool quest_exploration_marker(TilePos& out);

// Journal entries are quests that have started. They are listed in ID order, and a scan answers
// each lookup, so no list is stored.
bool quest_listed(QUEST_ID id);
uint8_t quest_journal_count();
QUEST_ID quest_journal_entry(uint8_t index);

// Records that a flag's value changed. Called by Game when a mutation changes a flag. Evaluation
// waits for quest_settle.
void quest_flag_changed(FLAG flag);
// Delivers a conversation to its subscribers at once, and keeps the tracked quest valid.
void quest_npc_talked(NPC_ID id);
template <NPC_ID ID> void quest_emit(NpcTalked<ID>) {
    quest_npc_talked(ID);
}
// Evaluates the pending quests in bounded passes. Anything left over stays pending for the next
// settle, and the tracked quest is cleared if it is no longer active.
void quest_settle();

struct QuestNotice {
    QUEST_ID started; // QUEST_NONE identifies a completion notice.
    char const PROGMEM* objective;
    uint8_t experience, gold;
};

// Grants XP, credits as much gold as the wallet can hold, and queues a completion notice.
// Call once, after a successful completion transition.
void quest_award_reward(uint8_t experience, uint8_t gold);
bool quest_take_notice(QuestNotice& out);
LevelUpNotice quest_take_level_ups();
void render_quest_notice(QuestNotice const& notice);
// Clears transient evaluations and notices on a new game or a loaded save.
void quest_reset();
// Saved quest state is valid when the selection names a defined ID and every quest is consistent.
bool quest_saved_state_valid();
// Clears a stale selection after a successful load.
void quest_restore();

#ifndef RPG_AVM
// Host-only probe: the number of evaluate() calls made by quest_settle.
extern unsigned quest_evaluations;
#endif

#endif
