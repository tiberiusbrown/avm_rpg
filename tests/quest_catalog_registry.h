#ifndef QUEST_CATALOG_REGISTRY_H
#define QUEST_CATALOG_REGISTRY_H

// Registry for the catalog test build, which sets RPG_QUEST_ID_LIMIT to 40. Quest 1 is the
// shipping Go Home. Quests 2 to 39 are synthetic probes whose state lives in test arrays, because
// 40 quests cannot each own flags. They cover flag subscriptions, cascades, an evaluation cycle,
// conversation delivery, starts, markers, consistency and a long Journal.
#include "quests/go_home.h"
#include "cast.h"

// Flags that only the probes read. Equipment ownership flags have no other effect while nobody
// wears the item.
inline constexpr FLAG QUEST_FLAG_GATE = (FLAG)(FLAG_ITEM_FIRST + 20);
inline constexpr FLAG QUEST_FLAG_NOOK = (FLAG)(FLAG_ITEM_FIRST + 21);
inline constexpr FLAG QUEST_FLAG_BRIDGE = (FLAG)(FLAG_ITEM_FIRST + 22);
inline constexpr FLAG QUEST_FLAG_ORB = (FLAG)(FLAG_ITEM_FIRST + 23);
inline constexpr FLAG QUEST_FLAG_CHEST = (FLAG)(FLAG_ITEM_FIRST + 24);
inline constexpr FLAG QUEST_FLAG_UNWATCHED = (FLAG)(FLAG_ITEM_FIRST + 25);

inline QUEST_STATUS test_status[QUEST_ID_LIMIT];
inline unsigned test_talks[QUEST_ID_LIMIT];
inline unsigned test_evaluations[QUEST_ID_LIMIT];
inline unsigned test_starts[QUEST_ID_LIMIT];
inline unsigned test_completions[QUEST_ID_LIMIT];

// A quest that watches the gate, and cascades into the nook when it sees the gate set.
template <> struct Quest<(QUEST_ID)2> {
    using Watches = Watch<FlagChanged<QUEST_FLAG_GATE>>;
    static QUEST_STATUS status() { return test_status[2]; }
    static char const PROGMEM* title() { return F("Gate Watcher"); }
    static char const PROGMEM* summary() { return F("Watches the gate."); }
    static void evaluate() {
        ++test_evaluations[2];
        if(game.get_flag(QUEST_FLAG_GATE))
            game.set_flag(QUEST_FLAG_NOOK);
    }
};

// Cascade second stage: the nook sets the bridge.
template <> struct Quest<(QUEST_ID)3> {
    using Watches = Watch<FlagChanged<QUEST_FLAG_NOOK>>;
    static QUEST_STATUS status() { return test_status[3]; }
    static char const PROGMEM* title() { return F("Nook Watcher"); }
    static char const PROGMEM* summary() { return F("Watches the nook."); }
    static void evaluate() {
        ++test_evaluations[3];
        if(game.get_flag(QUEST_FLAG_NOOK))
            game.set_flag(QUEST_FLAG_BRIDGE);
    }
};

// Listens to the wanderer and counts each conversation without merging them.
template <> struct Quest<(QUEST_ID)4> {
    using Watches = Watch<NpcTalked<NPC_WANDERER>>;
    static QUEST_STATUS status() { return test_status[4]; }
    static char const PROGMEM* title() { return F("Wanderer Listener"); }
    static char const PROGMEM* summary() { return F("Talks with the wanderer."); }
    static void on(NpcTalked<NPC_WANDERER>) { ++test_talks[4]; }
};

// Completes on the villager's conversation, once, and only while active.
template <> struct Quest<(QUEST_ID)5> {
    using Watches = Watch<NpcTalked<NPC_VILLAGER>>;
    static QUEST_STATUS status() { return test_status[5]; }
    static char const PROGMEM* title() { return F("Villager Listener"); }
    static char const PROGMEM* summary() { return F("Talks with the villager."); }
    static void on(NpcTalked<NPC_VILLAGER>) {
        ++test_talks[5];
        if(test_status[5] == QUEST_ACTIVE) {
            test_status[5] = QUEST_COMPLETED;
            ++test_completions[5];
        }
    }
};

// Has a marker in the outdoor area only, and one objective while active.
template <> struct Quest<(QUEST_ID)6> {
    static QUEST_STATUS status() { return test_status[6]; }
    static char const PROGMEM* title() { return F("Sign Marker"); }
    static char const PROGMEM* summary() { return F("Read the sign."); }
    static char const PROGMEM* objective() {
        return status() == QUEST_ACTIVE ? F("Read the grove sign") : nullptr;
    }
    static bool marker(uint8_t area, TilePos& out) {
        if(area != AREA_OUTDOOR)
            return false;
        out = sites::SIGN;
        return true;
    }
};

// Startable: start() records the fact by making the quest active.
template <> struct Quest<(QUEST_ID)8> {
    static QUEST_STATUS status() { return test_status[8]; }
    static char const PROGMEM* title() { return F("Startable"); }
    static char const PROGMEM* summary() { return F("Starts on request."); }
    static void start() {
        ++test_starts[8];
        test_status[8] = QUEST_ACTIVE;
    }
};

// A marker outside the map. The catalog must reject it.
template <> struct Quest<(QUEST_ID)9> {
    static QUEST_STATUS status() { return test_status[9]; }
    static char const PROGMEM* title() { return F("Bad Marker"); }
    static char const PROGMEM* summary() { return F("Points off the map."); }
    static bool marker(uint8_t area, TilePos& out) {
        out = TilePos{area, 255, 255};
        return true;
    }
};

// A cycle: every evaluation toggles the flag it watches, so the notification never ends on its own.
template <> struct Quest<(QUEST_ID)10> {
    using Watches = Watch<FlagChanged<QUEST_FLAG_ORB>>;
    static QUEST_STATUS status() { return test_status[10]; }
    static char const PROGMEM* title() { return F("Cycle"); }
    static char const PROGMEM* summary() { return F("Toggles its own flag."); }
    static void evaluate() {
        ++test_evaluations[10];
        game.toggle_flag(QUEST_FLAG_ORB);
    }
};

// Shares the gate subscription with quest 2, so one change coalesces into two evaluations.
template <> struct Quest<(QUEST_ID)11> {
    using Watches = Watch<FlagChanged<QUEST_FLAG_GATE>>;
    static QUEST_STATUS status() { return test_status[11]; }
    static char const PROGMEM* title() { return F("Shared Gate"); }
    static char const PROGMEM* summary() { return F("Also watches the gate."); }
    static void evaluate() { ++test_evaluations[11]; }
};

// Consistency: completion needs the chest flag, so a saved completion without it is invalid.
template <> struct Quest<(QUEST_ID)12> {
    static QUEST_STATUS status() { return test_status[12]; }
    static char const PROGMEM* title() { return F("Checked"); }
    static char const PROGMEM* summary() { return F("Needs its chest."); }
    static bool consistent() {
        return test_status[12] != QUEST_COMPLETED || game.get_flag(QUEST_FLAG_CHEST);
    }
};

// Synthetic quests with only a status: they fill the Journal and the scaling checks.
#define RPG_TEST_PLAIN_QUEST(N)                                                                    \
    template <> struct Quest<(QUEST_ID)N> {                                                        \
        static QUEST_STATUS status() { return test_status[N]; }                                    \
        static char const PROGMEM* title() { return F("Catalog Quest"); }                          \
        static char const PROGMEM* summary() { return F("A synthetic quest."); }                   \
    };

RPG_TEST_PLAIN_QUEST(7)
RPG_TEST_PLAIN_QUEST(13)
RPG_TEST_PLAIN_QUEST(14)
RPG_TEST_PLAIN_QUEST(15)
RPG_TEST_PLAIN_QUEST(16)
RPG_TEST_PLAIN_QUEST(17)
RPG_TEST_PLAIN_QUEST(18)
RPG_TEST_PLAIN_QUEST(19)
RPG_TEST_PLAIN_QUEST(20)
RPG_TEST_PLAIN_QUEST(21)
RPG_TEST_PLAIN_QUEST(22)
RPG_TEST_PLAIN_QUEST(23)
RPG_TEST_PLAIN_QUEST(24)
RPG_TEST_PLAIN_QUEST(25)
RPG_TEST_PLAIN_QUEST(26)
RPG_TEST_PLAIN_QUEST(27)
RPG_TEST_PLAIN_QUEST(28)
RPG_TEST_PLAIN_QUEST(29)
RPG_TEST_PLAIN_QUEST(30)
RPG_TEST_PLAIN_QUEST(31)
RPG_TEST_PLAIN_QUEST(32)
RPG_TEST_PLAIN_QUEST(33)
RPG_TEST_PLAIN_QUEST(34)
RPG_TEST_PLAIN_QUEST(35)
RPG_TEST_PLAIN_QUEST(36)
RPG_TEST_PLAIN_QUEST(37)
RPG_TEST_PLAIN_QUEST(38)
RPG_TEST_PLAIN_QUEST(39)

#undef RPG_TEST_PLAIN_QUEST

#endif
