#include "battle.h"

#include "tile_ids.h"

#include <utility>

namespace {
struct Terrain {
    uint8_t tiles[BATTLE_CELLS];
};

// The original clearing also anchors the fixed rules and search fixtures.
constexpr Terrain GROVE{{
    TILE_GRASS_06, TILE_GRASS_07, TILE_GRASS_06, TILE_FLOWERS_26, TILE_TREE, TILE_GRASS_07, TILE_TREE_ROUND, TILE_GRASS_06,
    TILE_GRASS_07, TILE_GRASS_06, TILE_GRASS_16, TILE_GRASS_06, TILE_GRASS_06, TILE_FLOWERS_27, TILE_GRASS_06, TILE_GRASS_07,
    TILE_GRASS_06, TILE_GRASS_16, TILE_GRASS_07, TILE_GRASS_06, TILE_FLOWERS_26, TILE_GRASS_06, TILE_GRASS_06, TILE_GRASS_07,
    TILE_GRASS_07, TILE_ROCKS, TILE_GRASS_06, TILE_GRASS_06, TILE_BOULDER, TILE_GRASS_16, TILE_FLOWERS_27, TILE_GRASS_06,
}};

// Each row uses '.' for floor, '+' for an accent tile, and '#' for an obstacle.
consteval Terrain terrain(uint8_t floor, uint8_t accent, uint8_t obstacle,
                          char const (&top)[9], char const (&upper)[9],
                          char const (&lower)[9], char const (&bottom)[9]) {
    char const* rows[] = {top, upper, lower, bottom};
    Terrain result{};
    for(uint8_t y = 0; y < BATTLE_ROWS; ++y)
        for(uint8_t x = 0; x < BATTLE_COLUMNS; ++x) {
            char symbol = rows[y][x];
            result.tiles[battle_cell(x, y)] = symbol == '#' ? obstacle : symbol == '+' ? accent : floor;
        }
    return result;
}

template <Terrain Layout, typename Indices> struct TerrainStorage;

template <Terrain Layout, std::size_t... I>
struct TerrainStorage<Layout, std::index_sequence<I...>> {
    static constexpr uint8_t PROGMEM tiles[] = {Layout.tiles[I]...};
};

constexpr bool blocked(uint8_t tileset, uint8_t tile) {
    return TILESET_BLOCKED[(unsigned)tileset * 32 + tile / 8] & (1u << (tile % 8));
}

constexpr bool valid_rect(BattleRect area) {
    return area.width && area.height && area.x + area.width <= BATTLE_COLUMNS &&
           area.y + area.height <= BATTLE_ROWS;
}

template <uint8_t Tileset, Terrain Layout>
consteval uint8_t open_cells(BattleRect area) {
    uint8_t count = 0;
    for(uint8_t cell = 0; cell < BATTLE_CELLS; ++cell)
        if(area.contains(battle_column(cell), battle_row(cell)) && !blocked(Tileset, Layout.tiles[cell]))
            ++count;
    return count;
}

template <uint8_t Tileset, Terrain Layout, BattleRect Party, BattleRect Enemy>
consteval Battlefield battlefield() {
    static_assert(valid_rect(Party) && valid_rect(Enemy), "battlefield deployment rectangle is out of bounds");
    static_assert(Party.x + Party.width <= Enemy.x || Enemy.x + Enemy.width <= Party.x ||
                  Party.y + Party.height <= Enemy.y || Enemy.y + Enemy.height <= Party.y,
                  "battlefield deployment rectangles overlap");

    static_assert(open_cells<Tileset, Layout>(Party) >= BATTLE_SIDE_CAPACITY &&
                  open_cells<Tileset, Layout>(Enemy) >= BATTLE_SIDE_CAPACITY,
                  "battlefield deployment rectangles must each fit a full team");

    using Storage = TerrainStorage<Layout, std::make_index_sequence<BATTLE_CELLS>>;
    Battlefield result{Tileset, Storage::tiles, {}, {},
                       {Party.x, Party.y, Party.width, Party.height},
                       {Enemy.x, Enemy.y, Enemy.width, Enemy.height}};
    for(uint8_t cell = 0; cell < BATTLE_CELLS; ++cell)
        if(blocked(Tileset, Layout.tiles[cell]))
            result.blocked[battle_row(cell)] |= (uint8_t)(1u << battle_column(cell));

    for(uint8_t a = 0; a < BATTLE_CELLS; ++a)
        for(uint8_t b = 0; b < BATTLE_CELLS; ++b) {
            if(a == b)
                continue;
            int step = 0;
            if(battle_row(a) == battle_row(b))
                step = a < b ? 1 : -1;
            else if(battle_column(a) == battle_column(b))
                step = a < b ? BATTLE_COLUMNS : -BATTLE_COLUMNS;
            if(!step)
                continue;
            for(int cell = a + step; cell != b; cell += step)
                if(blocked(Tileset, Layout.tiles[cell])) {
                    result.screened[a][b / 8] |= (uint8_t)(1u << (b % 8));
                    break;
                }
        }
    return result;
}

constexpr BattleRect WEST_WING{0, 0, 3, 4}, EAST_WING{5, 0, 3, 4};
constexpr BattleRect NORTH_WEST{0, 0, 3, 2}, SOUTH_EAST{5, 2, 3, 2};
constexpr BattleRect SOUTH_WEST{0, 2, 3, 2}, NORTH_EAST{5, 0, 3, 2};
} // namespace

// Entries follow BATTLEFIELD_ID order.
constexpr Battlefield PROGMEM BATTLEFIELDS[BATTLEFIELD_COUNT] = {
    battlefield<TILESET_WORLD, GROVE, WEST_WING, EAST_WING>(),
    battlefield<TILESET_WORLD, terrain(TILE_GRASS_06, TILE_FLOWERS_26, TILE_TREE_ROUND,
                "..+....#", ".+...+..", "..#.....", "+...+..."), NORTH_WEST, SOUTH_EAST>(),
    battlefield<TILESET_WORLD, terrain(TILE_GRASS_17, TILE_FLOWERS_27, TILE_ROCKS,
                ".+....+.", "...##...", "+.......", "....#..+"), SOUTH_WEST, NORTH_EAST>(),

    battlefield<TILESET_CAVE, terrain(CAVE_TILE_FLOOR_A, CAVE_TILE_FLOOR_B, CAVE_TILE_BOULDER,
                ".+.#..+.", "....+...", "..+....#", "...#.+.."), WEST_WING, EAST_WING>(),
    battlefield<TILESET_CAVE, terrain(CAVE_TILE_FLOOR_B, CAVE_TILE_FLOOR_C, CAVE_TILE_STALAGMITE,
                "+..#....", "..+..#..", "....+...", ".#....+."), NORTH_WEST, SOUTH_EAST>(),
    battlefield<TILESET_CAVE, terrain(CAVE_TILE_FLOOR_C, CAVE_TILE_FLOOR_MOSS, CAVE_TILE_STALAGMITES,
                "#.......", ".+..#+..", "....#...", "+.+....#"), SOUTH_WEST, NORTH_EAST>(),

    battlefield<TILESET_DUNGEON, terrain(DUNGEON_TILE_FLOOR_A, DUNGEON_TILE_FLOOR_B, DUNGEON_TILE_COLUMN,
                "..+.....", "...#...+", "+....#..", ".....+.."), WEST_WING, EAST_WING>(),
    battlefield<TILESET_DUNGEON, terrain(DUNGEON_TILE_FLOOR_B, DUNGEON_TILE_FLOOR_CRACKED, DUNGEON_TILE_RUBBLE,
                ".+..#...", "......+.", "....#...", "#+....+."), NORTH_WEST, SOUTH_EAST>(),
    battlefield<TILESET_DUNGEON, terrain(DUNGEON_TILE_FLOOR_A, DUNGEON_TILE_FLOOR_GRATE, DUNGEON_TILE_POT,
                ".....+.#", "...#....", "#...+...", "..+....."), SOUTH_WEST, NORTH_EAST>(),

    battlefield<TILESET_HOUSE, terrain(HOUSE_TILE_FLOOR_WOOD_A, HOUSE_TILE_FLOOR_WOOD_B, HOUSE_TILE_TABLE,
                "....#...", ".+......", "......+.", "..#....."), WEST_WING, EAST_WING>(),
    battlefield<TILESET_HOUSE, terrain(HOUSE_TILE_FLOOR_STONE_A, HOUSE_TILE_FLOOR_STONE_B, HOUSE_TILE_CRATE,
                "+.......", "...#..#.", ".+....+.", "....#..."), NORTH_WEST, SOUTH_EAST>(),
    battlefield<TILESET_HOUSE, terrain(HOUSE_TILE_FLOOR_TILE_A, HOUSE_TILE_FLOOR_TILE_B, HOUSE_TILE_TABLE_CANDLE,
                ".....+..", "...##...", "..+.....", ".......+"), SOUTH_WEST, NORTH_EAST>(),
};
