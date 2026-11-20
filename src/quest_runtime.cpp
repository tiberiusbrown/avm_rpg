#include "quest.h"
#include "quest_dispatch.h"

// The catalog is included once, here. Engine code reaches quests only through quest.h.
#ifndef RPG_QUEST_REGISTRY_HEADER
#define RPG_QUEST_REGISTRY_HEADER "quest_registry.h"
#endif
#include RPG_QUEST_REGISTRY_HEADER

#ifndef RPG_AVM
unsigned quest_evaluations;
#endif

static_assert(AllQuests::valid, "invalid quest catalog");

namespace {

// Settle passes per frame. A quest whose evaluation keeps notifying itself stays pending across
// frames, so a cycle cannot stall the frame and no notification is dropped.
constexpr uint8_t SETTLE_PASSES = 4;

// Transient evaluation bits, one per QUEST_ID. They never reach the save.
uint8_t pending[QUEST_BYTES];
LevelUpNotice pending_level_ups;

using StatusFn = QUEST_STATUS (*)();
using TextFn = char const PROGMEM* (*)();
using MarkerFn = bool (*)(uint8_t, TilePos&);
using BoolFn = bool (*)();
using VoidFn = void (*)();
using TalkFn = void (*)();

// One table per operation, indexed by QUEST_ID. Entry 0 is QUEST_NONE, which has no quest.
template <typename SEQUENCE> struct QuestTables;

template <std::size_t... I> struct QuestTables<std::index_sequence<I...>> {
    static constexpr StatusFn PROGMEM status[] = {nullptr, &QuestOps<(QUEST_ID)(I + 1)>::status...};
    static constexpr TextFn PROGMEM title[] = {nullptr, &QuestOps<(QUEST_ID)(I + 1)>::title...};
    static constexpr TextFn PROGMEM summary[] = {nullptr, &QuestOps<(QUEST_ID)(I + 1)>::summary...};
    static constexpr TextFn PROGMEM objective[] = {nullptr,
                                                   &QuestOps<(QUEST_ID)(I + 1)>::objective...};
    static constexpr MarkerFn PROGMEM marker[] = {nullptr, &QuestOps<(QUEST_ID)(I + 1)>::marker...};
    static constexpr BoolFn PROGMEM start[] = {nullptr, &QuestOps<(QUEST_ID)(I + 1)>::start...};
    static constexpr BoolFn PROGMEM consistent[] = {nullptr,
                                                    &QuestOps<(QUEST_ID)(I + 1)>::consistent...};
    static constexpr VoidFn PROGMEM evaluate[] = {nullptr, &QuestOps<(QUEST_ID)(I + 1)>::evaluate...};
};

using Tables = QuestTables<std::make_index_sequence<QUEST_ID_LIMIT - 1>>;

// For each flag, the quests whose FlagChanged subscriptions name it. A flag that no quest watches
// has an all-zero row, so its notification touches nothing.
struct FlagWatchTable {
    uint8_t bits[FLAG_COUNT][QUEST_BYTES];
};

template <QUEST_ID ID> constexpr void mark_flag_watchers(FlagWatchTable& table) {
    for(uint16_t flag = 0; flag < FLAG_COUNT; ++flag)
        if(WatchOf<ID>::watches_flag((FLAG)flag))
            table.bits[flag][ID / 8] = (uint8_t)(table.bits[flag][ID / 8] | (1u << (ID % 8)));
}

template <std::size_t... I> constexpr FlagWatchTable make_flag_watch_table(std::index_sequence<I...>) {
    FlagWatchTable table{};
    (mark_flag_watchers<(QUEST_ID)(I + 1)>(table), ...);
    return table;
}

constexpr FlagWatchTable PROGMEM FLAG_WATCHERS =
    make_flag_watch_table(std::make_index_sequence<QUEST_ID_LIMIT - 1>{});

// A conversation calls only the quests whose Watch names that NPC. Each NPC's delivery is a fixed
// sequence of direct calls, so an event never scans the catalog for subscribers at run time.
template <NPC_ID NPC, QUEST_ID ID> void deliver_talk_to() {
    if constexpr(watches_npc<WatchOf<ID>, NPC>)
        Quest<ID>::on(NpcTalked<NPC>{});
}

template <NPC_ID NPC, std::size_t... I> void deliver_talk_all(std::index_sequence<I...>) {
    (deliver_talk_to<NPC, (QUEST_ID)(I + 1)>(), ...);
}

template <NPC_ID NPC> void deliver_talk() {
    deliver_talk_all<NPC>(std::make_index_sequence<QUEST_ID_LIMIT - 1>{});
}

template <typename SEQUENCE> struct TalkTable;

template <std::size_t... N> struct TalkTable<std::index_sequence<N...>> {
    static constexpr TalkFn PROGMEM entries[] = {nullptr, &deliver_talk<(NPC_ID)(N + 1)>...};
};

using Talks = TalkTable<std::make_index_sequence<NPC_ID_LIMIT - 1>>;

bool valid_quest(QUEST_ID id) {
    return id > QUEST_NONE && id < QUEST_ID_LIMIT;
}

bool valid_npc(NPC_ID id) {
    return id > NPC_NONE && id < NPC_ID_LIMIT;
}

bool pending_any() {
    for(uint8_t i = 0; i < QUEST_BYTES; ++i)
        if(pending[i])
            return true;
    return false;
}

// A tracked quest that is no longer active, such as one that just completed, stops being tracked.
void validate_selection() {
    if(game.selected_quest != QUEST_NONE && quest_status(game.selected_quest) != QUEST_ACTIVE)
        game.selected_quest = QUEST_NONE;
}

} // namespace

QUEST_STATUS quest_status(QUEST_ID id) {
    return valid_quest(id) ? Tables::status[id]() : QUEST_NOT_STARTED;
}

bool quest_start(QUEST_ID id) {
    // start() runs only for a not-started quest, so a second request changes nothing.
    if(!valid_quest(id) || Tables::status[id]() != QUEST_NOT_STARTED || !Tables::start[id]())
        return false;
    if(Tables::status[id]() != QUEST_ACTIVE)
        return false;
    if(game.selected_quest == QUEST_NONE)
        game.selected_quest = id;
    return true;
}

char const PROGMEM* quest_title(QUEST_ID id) {
    return valid_quest(id) ? Tables::title[id]() : F("");
}

char const PROGMEM* quest_summary(QUEST_ID id) {
    return valid_quest(id) ? Tables::summary[id]() : F("");
}

char const PROGMEM* quest_objective(QUEST_ID id) {
    return valid_quest(id) ? Tables::objective[id]() : nullptr;
}

bool quest_marker(QUEST_ID id, uint8_t area, TilePos& out) {
    if(!valid_quest(id) || Tables::status[id]() != QUEST_ACTIVE || !Tables::marker[id](area, out))
        return false;
    // A marker must name the requested area and lie inside it, whatever the quest computed.
    Area const PROGMEM* a = world_area(area);
    return a && out.area == area && out.x < a->width && out.y < a->height;
}

QUEST_ID quest_selected() {
    return game.selected_quest;
}

bool quest_select(QUEST_ID id) {
    if(!valid_quest(id) || quest_status(id) != QUEST_ACTIVE)
        return false;
    game.selected_quest = id;
    return true;
}

void quest_deselect() {
    game.selected_quest = QUEST_NONE;
}

bool quest_map_marker(uint8_t area, TilePos& out) {
    return quest_marker(game.selected_quest, area, out);
}

bool quest_listed(QUEST_ID id) {
    return valid_quest(id) && quest_status(id) != QUEST_NOT_STARTED;
}

uint8_t quest_journal_count() {
    uint8_t count = 0;
    for(uint8_t id = 1; id < QUEST_ID_LIMIT; ++id)
        if(quest_listed((QUEST_ID)id))
            ++count;
    return count;
}

QUEST_ID quest_journal_entry(uint8_t index) {
    uint8_t seen = 0;
    for(uint8_t id = 1; id < QUEST_ID_LIMIT; ++id) {
        if(!quest_listed((QUEST_ID)id))
            continue;
        if(seen == index)
            return (QUEST_ID)id;
        ++seen;
    }
    return QUEST_NONE;
}

void quest_flag_changed(FLAG flag) {
    if(flag >= FLAG_COUNT)
        return;
    for(uint8_t i = 0; i < QUEST_BYTES; ++i)
        pending[i] |= FLAG_WATCHERS.bits[flag][i];
}

void quest_npc_talked(NPC_ID id) {
    if(valid_npc(id))
        Talks::entries[id]();
    validate_selection();
}

void quest_settle() {
    for(uint8_t pass = 0; pass < SETTLE_PASSES && pending_any(); ++pass) {
        // Notifications raised while a pass runs belong to the next pass, so the snapshot is taken
        // and cleared first.
        uint8_t work[QUEST_BYTES];
        for(uint8_t i = 0; i < QUEST_BYTES; ++i) {
            work[i] = pending[i];
            pending[i] = 0;
        }
        for(uint8_t id = 1; id < QUEST_ID_LIMIT; ++id) {
            if(!(work[id / 8] & (1u << (id % 8))))
                continue;
            // Only active quests progress. A quest that is not active has nothing to advance.
            if(Tables::status[id]() != QUEST_ACTIVE)
                continue;
#ifndef RPG_AVM
            ++quest_evaluations;
#endif
            Tables::evaluate[id]();
        }
    }
    validate_selection();
}

void quest_award_experience(uint8_t experience) {
    for(uint8_t i = 0; i < game.party.count; ++i)
        pending_level_ups.award(game.party, i, experience);
}

LevelUpNotice quest_take_level_ups() {
    LevelUpNotice notice = pending_level_ups;
    pending_level_ups = {};
    return notice;
}

void quest_reset() {
    pending_level_ups = {};
    for(uint8_t i = 0; i < QUEST_BYTES; ++i)
        pending[i] = 0;
}

bool quest_saved_state_valid() {
    if(game.selected_quest >= QUEST_ID_LIMIT)
        return false;
    for(uint8_t id = 1; id < QUEST_ID_LIMIT; ++id)
        if(!Tables::consistent[id]())
            return false;
    return true;
}

void quest_restore() {
    quest_reset();
    validate_selection();
}
