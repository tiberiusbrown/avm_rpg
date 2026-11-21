#include "battle.h"

#include "tile_ids.h"

#include <utility>

namespace {
struct Terrain {
    uint8_t tileset;
    uint8_t tiles[BATTLE_CELLS];
};

struct Palette {
    uint8_t floor[4], accent[2], obstacle[4];
};

constexpr Palette PALETTES[TILESET_COUNT] = {
    {{TILE_GRASS_08, TILE_GRASS_09, TILE_GRASS_0A, TILE_GRASS_59},
     {TILE_FLOWERS_26, TILE_FLOWERS_27},
     {TILE_TREE_ROUND, TILE_BOULDER, TILE_ROCKS, TILE_BOULDER_CRACKED}},
    {{CAVE_TILE_FLOOR_A, CAVE_TILE_FLOOR_B, CAVE_TILE_FLOOR_C, CAVE_TILE_FLOOR_B},
     {CAVE_TILE_FLOOR_MOSS, CAVE_TILE_FLOOR_C},
     {CAVE_TILE_BOULDER, CAVE_TILE_STALAGMITE, CAVE_TILE_STALAGMITES, CAVE_TILE_CRYSTALS}},
    {{DUNGEON_TILE_FLOOR_A, DUNGEON_TILE_FLOOR_B, DUNGEON_TILE_FLOOR_A, DUNGEON_TILE_FLOOR_B},
     {DUNGEON_TILE_FLOOR_CRACKED, DUNGEON_TILE_FLOOR_GRATE},
     {DUNGEON_TILE_COLUMN, DUNGEON_TILE_RUBBLE, DUNGEON_TILE_POT, DUNGEON_TILE_CRATE}},
    {{HOUSE_TILE_FLOOR_WOOD_A, HOUSE_TILE_FLOOR_WOOD_B, HOUSE_TILE_FLOOR_WOOD_C, HOUSE_TILE_FLOOR_WOOD_D},
     {HOUSE_TILE_RUG_SMALL, HOUSE_TILE_FLOOR_WOOD_D},
     {HOUSE_TILE_TABLE, HOUSE_TILE_CRATE, HOUSE_TILE_STOOL, HOUSE_TILE_POT}},
};
constexpr uint8_t DENSE_GRASS[] = {TILE_GRASS_06, TILE_GRASS_07, TILE_GRASS_16, TILE_GRASS_17};
static_assert(TILESET_WORLD == 0 && TILESET_CAVE == 1 && TILESET_DUNGEON == 2 && TILESET_HOUSE == 3);

constexpr bool blocked(uint8_t tileset, uint8_t tile) {
    return TILESET_BLOCKED[(unsigned)tileset * 32 + tile / 8] & (1u << (tile % 8));
}

static_assert([] {
    for(uint8_t tileset = 0; tileset < TILESET_COUNT; ++tileset) {
        auto const& palette = PALETTES[tileset];
        for(uint8_t tile : palette.floor)
            if(blocked(tileset, tile))
                return false;
        for(uint8_t tile : palette.accent)
            if(blocked(tileset, tile))
                return false;
        for(uint8_t tile : palette.obstacle)
            if(!blocked(tileset, tile))
                return false;
    }
    for(uint8_t tile : DENSE_GRASS)
        if(blocked(TILESET_WORLD, tile))
            return false;
    return true;
}(), "battlefield palettes must preserve authored floor and obstacle collision");

constexpr uint32_t terrain_hash(uint32_t key) {
    key = (key ^ (key >> 16)) * UINT32_C(0x7feb352d);
    key = (key ^ (key >> 15)) * UINT32_C(0x846ca68b);
    return key ^ (key >> 16);
}

// Author only floor ('.') and obstacle ('#') cells. The board ID gives each layout its own palette
// rolls. Hashing coarse patches biases nearby grass cells toward the same density.
consteval Terrain terrain(uint8_t tileset, BATTLEFIELD_ID id,
                          char const (&top)[9], char const (&upper)[9],
                          char const (&lower)[9], char const (&bottom)[9]) {
    char const* rows[] = {top, upper, lower, bottom};
    auto const& palette = PALETTES[tileset];
    uint32_t seed = ((uint32_t)id + 1) << 8;
    Terrain result{tileset, {}};
    uint8_t floor_index = 0;
    for(uint8_t y = 0; y < BATTLE_ROWS; ++y)
        for(uint8_t x = 0; x < BATTLE_COLUMNS; ++x) {
            uint8_t cell = battle_cell(x, y);
            uint32_t roll = terrain_hash(seed + cell);
            if(rows[y][x] == '#') {
                uint8_t tile = palette.obstacle[roll & 3];
                if(tileset == TILESET_WORLD && tile == TILE_TREE_ROUND &&
                   ((x && rows[y][x - 1] == '#') || (x + 1 < BATTLE_COLUMNS && rows[y][x + 1] == '#') ||
                    (y && rows[y - 1][x] == '#') || (y + 1 < BATTLE_ROWS && rows[y + 1][x] == '#')))
                    tile = palette.obstacle[1 + (roll >> 2) % 3];
                result.tiles[cell] = tile;
                continue;
            }

            // Pick one accent per eight floor cells, with a different position in each group.
            uint8_t accent = terrain_hash(seed + 32 + floor_index / 8) & 7;
            if((floor_index++ & 7) == accent) {
                result.tiles[cell] = palette.accent[roll & 1];
                continue;
            }

            uint32_t patch = terrain_hash(seed + 64 + x / 3 + 3 * (y / 2));
            uint8_t dense_chance = (patch & 1) ? 5 : 2;
            auto const& floor = tileset == TILESET_WORLD && ((roll >> 2) & 7) < dense_chance
                                    ? DENSE_GRASS : palette.floor;
            result.tiles[cell] = floor[roll & 3];
        }
    return result;
}

template <Terrain Layout, typename Indices> struct TerrainStorage;

template <Terrain Layout, std::size_t... I>
struct TerrainStorage<Layout, std::index_sequence<I...>> {
    static constexpr uint8_t PROGMEM tiles[] = {Layout.tiles[I]...};
};

constexpr bool valid_rect(BattleRect area) {
    return area.width && area.height && area.x + area.width <= BATTLE_COLUMNS &&
           area.y + area.height <= BATTLE_ROWS;
}

template <Terrain Layout>
consteval uint8_t open_cells(BattleRect area) {
    uint8_t count = 0;
    for(uint8_t cell = 0; cell < BATTLE_CELLS; ++cell)
        if(area.contains(battle_column(cell), battle_row(cell)) && !blocked(Layout.tileset, Layout.tiles[cell]))
            ++count;
    return count;
}

template <Terrain Layout, BattleRect Party, BattleRect Enemy>
consteval Battlefield battlefield() {
    static_assert(valid_rect(Party) && valid_rect(Enemy), "battlefield deployment rectangle is out of bounds");
    static_assert(Party.x + Party.width <= Enemy.x || Enemy.x + Enemy.width <= Party.x ||
                  Party.y + Party.height <= Enemy.y || Enemy.y + Enemy.height <= Party.y,
                  "battlefield deployment rectangles overlap");

    static_assert(open_cells<Layout>(Party) >= BATTLE_SIDE_CAPACITY &&
                  open_cells<Layout>(Enemy) >= BATTLE_SIDE_CAPACITY,
                  "battlefield deployment rectangles must each fit a full team");

    using Storage = TerrainStorage<Layout, std::make_index_sequence<BATTLE_CELLS>>;
    Battlefield result{Layout.tileset, Storage::tiles, {}, {},
                       {Party.x, Party.y, Party.width, Party.height},
                       {Enemy.x, Enemy.y, Enemy.width, Enemy.height}};
    for(uint8_t cell = 0; cell < BATTLE_CELLS; ++cell)
        if(blocked(Layout.tileset, Layout.tiles[cell]))
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
                if(blocked(Layout.tileset, Layout.tiles[cell])) {
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
    battlefield<terrain(TILESET_WORLD, BATTLEFIELD_GROVE,
        "....#.#.",
        "........",
        "........",
        ".#..#..."), WEST_WING, EAST_WING>(),
    battlefield<terrain(TILESET_WORLD, BATTLEFIELD_MEADOW,
        ".......#",
        "........",
        "..#.....",
        "........"), NORTH_WEST, SOUTH_EAST>(),
    battlefield<terrain(TILESET_WORLD, BATTLEFIELD_RIVERBANK,
        "........",
        "...##...",
        "........",
        "....#..."), SOUTH_WEST, NORTH_EAST>(),

    battlefield<terrain(TILESET_CAVE, BATTLEFIELD_MINE_GALLERY,
        "...#....",
        "........",
        ".......#",
        "...#...."), WEST_WING, EAST_WING>(),
    battlefield<terrain(TILESET_CAVE, BATTLEFIELD_MINE_CROSSING,
        "...#....",
        ".....#..",
        "........",
        ".#......"), NORTH_WEST, SOUTH_EAST>(),
    battlefield<terrain(TILESET_CAVE, BATTLEFIELD_MINE_CHAMBER,
        "#.......",
        "....#...",
        "....#...",
        ".......#"), SOUTH_WEST, NORTH_EAST>(),

    battlefield<terrain(TILESET_DUNGEON, BATTLEFIELD_VAULT_HALL,
        "........",
        "...#....",
        ".....#..",
        "........"), WEST_WING, EAST_WING>(),
    battlefield<terrain(TILESET_DUNGEON, BATTLEFIELD_VAULT_CRYPT,
        "....#...",
        "........",
        "....#...",
        "#......."), NORTH_WEST, SOUTH_EAST>(),
    battlefield<terrain(TILESET_DUNGEON, BATTLEFIELD_VAULT_COURT,
        ".......#",
        "...#....",
        "#.......",
        "........"), SOUTH_WEST, NORTH_EAST>(),

    battlefield<terrain(TILESET_HOUSE, BATTLEFIELD_HOUSE_TAVERN,
        "....#...",
        "........",
        "........",
        "..#....."), WEST_WING, EAST_WING>(),
    battlefield<terrain(TILESET_HOUSE, BATTLEFIELD_HOUSE_STORE,
        "........",
        "...#..#.",
        "........",
        "....#..."), NORTH_WEST, SOUTH_EAST>(),
    battlefield<terrain(TILESET_HOUSE, BATTLEFIELD_HOUSE_HALL,
        "........",
        "...##...",
        "........",
        "........"), SOUTH_WEST, NORTH_EAST>(),
};
