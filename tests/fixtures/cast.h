#ifndef FIXTURE_CAST_H
#define FIXTURE_CAST_H

#include "rpg.h"

// A synthetic cast for engine tests, independent of the game's characters. Engine tests compile
// with RPG_NPC_ID_LIMIT=16, so these identities follow the game's.
inline constexpr NPC_ID NPC_VILLAGER = (NPC_ID)8;
inline constexpr NPC_ID NPC_WANDERER = (NPC_ID)9;
inline constexpr NPC_ID NPC_ARCHER = (NPC_ID)10;
inline constexpr NPC_ID NPC_INNKEEPER = (NPC_ID)11;
inline constexpr NPC_ID NPC_SMITH = (NPC_ID)12;
inline constexpr NPC_ID NPC_WATCHER = (NPC_ID)13;
inline constexpr NPC_ID NPC_MERCHANT = (NPC_ID)14;
inline constexpr NPC_ID NPC_SURVEY = (NPC_ID)15;

// Flags that engine tests toggle to move fixture policies. Equipment ownership flags have no other
// effect while nobody wears the item.
inline constexpr FLAG TEST_FLAG_A = (FLAG)(FLAG_ITEM_FIRST + 30);
inline constexpr FLAG TEST_FLAG_B = (FLAG)(FLAG_ITEM_FIRST + 31);
inline constexpr FLAG TEST_FLAG_C = (FLAG)(FLAG_ITEM_FIRST + 32);

// Tiles of the fixture maps that fixtures/field.cpp installs.
namespace sites {
inline constexpr TilePos PLAYER_START{AREA_OUTDOOR, 27, 18};
inline constexpr TilePos SIGN{AREA_OUTDOOR, 27, 17};
inline constexpr TilePos TEST_FIELD{AREA_OUTDOOR, 50, 3};
inline constexpr TilePos WELL{AREA_OUTDOOR, 31, 18};
inline constexpr TilePos ORCHARD{AREA_OUTDOOR, 14, 12};
inline constexpr TilePos LANE{AREA_OUTDOOR, 25, 15};
inline constexpr TilePos HOME{AREA_OUTDOOR, 29, 19};
inline constexpr TilePos YARD{AREA_OUTDOOR, 41, 24};
inline constexpr TilePos LOOKOUT{AREA_OUTDOOR, 28, 7};
inline constexpr TilePos GROVE_WOLF{AREA_OUTDOOR, 23, 3};
// The second area is one open floor.
inline constexpr TilePos INN{AREA_INTERIORS, 42, 5};
inline constexpr TilePos SHOP{AREA_INTERIORS, 61, 4};
inline constexpr TilePos SCOUT{AREA_INTERIORS, 19, 5};
inline constexpr TilePos GUARD{AREA_INTERIORS, 30, 5};
inline constexpr TilePos FAR_ROOM{AREA_INTERIORS, 52, 5};
} // namespace sites

#endif
