#ifndef QUEST_DISPATCH_H
#define QUEST_DISPATCH_H

// Compile-time machinery behind quest.h. Quest headers never include this file, and only
// quest_runtime.cpp instantiates it, so every quest is reached through generated tables.

#include "quest.h"
#include "npc_ids.h"

#include <stddef.h>
#include <type_traits>
#include <utility>

// Quests without subscriptions inherit an empty Watch.
template <typename QUEST, typename = void> struct QuestWatches {
    using type = Watch<>;
};

template <typename QUEST> struct QuestWatches<QUEST, std::void_t<typename QUEST::Watches>> {
    using type = typename QUEST::Watches;
};

template <QUEST_ID ID> using WatchOf = typename QuestWatches<Quest<ID>>::type;

template <QUEST_ID ID> concept defined_quest = requires { sizeof(Quest<ID>); };

// Member signatures are compared exactly. An optional member that is declared with the wrong
// signature fails validation instead of being ignored.
template <typename QUEST, typename = void> inline constexpr bool status_ok = false;
template <typename QUEST>
inline constexpr bool status_ok<QUEST, std::void_t<decltype(&QUEST::status)>> =
    std::is_same_v<decltype(&QUEST::status), QUEST_STATUS (*)()>;

template <typename QUEST, typename = void> inline constexpr bool title_ok = false;
template <typename QUEST>
inline constexpr bool title_ok<QUEST, std::void_t<decltype(&QUEST::title)>> =
    std::is_same_v<decltype(&QUEST::title), char const PROGMEM* (*)()>;
template <typename QUEST, typename = void> inline constexpr bool summary_ok = false;
template <typename QUEST>
inline constexpr bool summary_ok<QUEST, std::void_t<decltype(&QUEST::summary)>> =
    std::is_same_v<decltype(&QUEST::summary), char const PROGMEM* (*)()>;

template <typename QUEST, typename = void> inline constexpr bool objective_ok = true;
template <typename QUEST>
inline constexpr bool objective_ok<QUEST, std::void_t<decltype(&QUEST::objective)>> =
    std::is_same_v<decltype(&QUEST::objective), char const PROGMEM* (*)()>;

template <typename QUEST, typename = void> inline constexpr bool marker_ok = true;
template <typename QUEST>
inline constexpr bool marker_ok<QUEST, std::void_t<decltype(&QUEST::marker)>> =
    std::is_same_v<decltype(&QUEST::marker), bool (*)(uint8_t, TilePos&)>;

template <typename QUEST, typename = void> inline constexpr bool start_ok = true;
template <typename QUEST>
inline constexpr bool start_ok<QUEST, std::void_t<decltype(&QUEST::start)>> =
    std::is_same_v<decltype(&QUEST::start), void (*)()>;

template <typename QUEST, typename = void> inline constexpr bool consistent_ok = true;
template <typename QUEST>
inline constexpr bool consistent_ok<QUEST, std::void_t<decltype(&QUEST::consistent)>> =
    std::is_same_v<decltype(&QUEST::consistent), bool (*)()>;

template <typename QUEST, typename = void> inline constexpr bool evaluate_ok = true;
template <typename QUEST>
inline constexpr bool evaluate_ok<QUEST, std::void_t<decltype(&QUEST::evaluate)>> =
    std::is_same_v<decltype(&QUEST::evaluate), void (*)()>;

// A subscription needs its handler, with the expected return type.
template <typename QUEST, typename EVENT> inline constexpr bool handles = false;
template <typename QUEST, FLAG FLAG_ID>
inline constexpr bool handles<QUEST, FlagChanged<FLAG_ID>> = requires { QUEST::evaluate(); };
template <typename QUEST, NPC_ID ID>
inline constexpr bool handles<QUEST, NpcTalked<ID>> = requires { QUEST::on(NpcTalked<ID>{}); };

template <typename QUEST, typename WATCH> inline constexpr bool handles_all = false;
template <typename QUEST, typename... EVENTS>
inline constexpr bool handles_all<QUEST, Watch<EVENTS...>> = (handles<QUEST, EVENTS> && ...);

// Whether a Watch subscribes a quest to a conversation with NPC.
template <typename WATCH, NPC_ID NPC> inline constexpr bool watches_npc = false;
template <NPC_ID NPC, typename... EVENTS>
inline constexpr bool watches_npc<Watch<EVENTS...>, NPC> =
    (std::is_same_v<EVENTS, NpcTalked<NPC>> || ...);

template <typename WATCH> struct IsWatch : std::false_type {};
template <typename... EVENTS> struct IsWatch<Watch<EVENTS...>> : std::true_type {};

// The validation for one registered quest. An undefined ID reports the missing definition, and
// a defined quest reports each violated interface requirement.
template <QUEST_ID ID, bool DEFINED = defined_quest<ID>> struct CheckQuest {
    static_assert(DEFINED, "missing quest definition for a registered QUEST_ID");
    static constexpr bool valid = false;
};

template <QUEST_ID ID> struct CheckQuest<ID, true> {
    using Definition = Quest<ID>;
    using Watches = WatchOf<ID>;

    static_assert(status_ok<Definition>,
                  "quest must define static QUEST_STATUS status() with that exact signature");
    static_assert(title_ok<Definition>,
                  "quest must define static char const PROGMEM* title() with that exact signature");
    static_assert(summary_ok<Definition>, "quest must define static char const PROGMEM* summary()");
    static_assert(objective_ok<Definition>,
                  "quest objective() must return char const PROGMEM* and take no arguments");
    static_assert(marker_ok<Definition>,
                  "quest marker() must be static bool marker(uint8_t area, TilePos& out)");
    static_assert(start_ok<Definition>, "quest start() must be static void start()");
    static_assert(consistent_ok<Definition>, "quest consistent() must be static bool consistent()");
    static_assert(evaluate_ok<Definition>, "quest evaluate() must be static void evaluate()");
    static_assert(IsWatch<Watches>::value, "quest Watches must be a Watch<...> subscription list");
    // Completing the Watch type runs its own event checks, such as duplicates and unknown events.
    static_assert(sizeof(Watches) > 0, "quest Watches must be complete");
    static_assert(handles_all<Definition, Watches>,
                  "a subscribed event needs its handler: evaluate() for FlagChanged, "
                  "on(NpcTalked<ID>) for NpcTalked");

    static constexpr bool valid = true;
};

// The full catalog: every ID from 1 through QUEST_ID_LIMIT - 1 is defined and valid.
template <typename SEQUENCE> struct QuestSet;

template <std::size_t... I> struct QuestSet<std::index_sequence<I...>> {
    static constexpr bool valid = (CheckQuest<(QUEST_ID)(I + 1)>::valid && ...);
};

using AllQuests = QuestSet<std::make_index_sequence<QUEST_ID_LIMIT - 1>>;

// Runtime-ID entry points. Each quest's members are reached through one function per operation,
// and the optional members return their documented defaults.
template <QUEST_ID ID> struct QuestOps {
    using Definition = Quest<ID>;

    static QUEST_STATUS status() {
        return Definition::status();
    }

    static char const PROGMEM* title() {
        return Definition::title();
    }

    static char const PROGMEM* summary() {
        return Definition::summary();
    }

    static char const PROGMEM* objective() {
        if constexpr(requires { Definition::objective(); })
            return Definition::objective();
        else
            return nullptr;
    }

    static bool marker(uint8_t area, TilePos& out) {
        if constexpr(requires { Definition::marker(area, out); })
            return Definition::marker(area, out);
        else
            return false;
    }

    // A quest without start() cannot be started through quest_start.
    static bool start() {
        if constexpr(requires { Definition::start(); }) {
            Definition::start();
            return true;
        } else
            return false;
    }

    static bool consistent() {
        if constexpr(requires { Definition::consistent(); })
            return Definition::consistent();
        else
            return true;
    }

    static void evaluate() {
        if constexpr(requires { Definition::evaluate(); })
            Definition::evaluate();
    }
};

#endif
