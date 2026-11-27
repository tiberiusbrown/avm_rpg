#ifndef QUEST_IDS_H
#define QUEST_IDS_H

#include <stdint.h>

// Quest identities are save and dispatch contracts. Journal order, activity and storage offsets
// never define them, and a quest keeps its ID for as long as it exists.
enum QUEST_ID : uint8_t { QUEST_NONE, QUEST_GO_HOME, QUEST_GIANT_RAT };

// One past the largest stable identity. Test catalogs override it to build larger registries.
#ifndef RPG_QUEST_ID_LIMIT
#define RPG_QUEST_ID_LIMIT 3
#endif
inline constexpr uint8_t QUEST_ID_LIMIT = RPG_QUEST_ID_LIMIT;

#endif
