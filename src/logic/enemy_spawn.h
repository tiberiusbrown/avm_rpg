#ifndef LOGIC_ENEMY_SPAWN_H
#define LOGIC_ENEMY_SPAWN_H

#include "battle.h"
#include "encounter.h"
#include "npc_policy.h"
#include "npc_routes.h"

enum ENEMY_BEHAVIOR : uint8_t { ENEMY_WANDER, ENEMY_ROUTE, ENEMY_CHASE };
enum ENEMY_SPEED : uint8_t { SPEED_FAST = 1, SPEED_NORMAL = 2 };
enum ENEMY_RESPAWN : uint8_t { RESPAWN_REMEMBER, RESPAWN_NEVER };

struct EnemyHooks {
    static bool available() {
        return true;
    }

    static bool before_battle() {
        return true;
    }

    static auto select_battle(BattleEncounter const PROGMEM* encounter) {
        return encounter;
    }

    static void victory() {}
    static void after_victory() {}
};

struct EnemySettings {
    DIRECTION facing = SOUTH;
    uint8_t wander_x = TILE, wander_y = TILE;
    uint8_t pause = 48, walk = 48;
    uint8_t detect = 3 * TILE, disengage = 5 * TILE, leash = 6 * TILE;
    uint8_t priority = 2;
};

template <TilePos POS, BattleEncounter const PROGMEM* BATTLE, uint8_t ROW,
          ENEMY_BEHAVIOR MODE = ENEMY_WANDER, ENEMY_SPEED SPEED = SPEED_NORMAL,
          uint8_t ROUTE = 0, ENEMY_RESPAWN RESPAWN = RESPAWN_REMEMBER,
          typename Hooks = EnemyHooks, EnemySettings SETTINGS = {}>
struct EnemySpawn {
    static_assert(valid_site(POS) && POS != NO_LOCATION, "invalid enemy spawn position");
    static_assert(BATTLE != nullptr, "enemy spawn needs a battle encounter");
    static_assert(ROW < ACTOR_ROW_COUNT, "invalid enemy sprite row");
    static_assert(MODE <= ENEMY_CHASE, "invalid enemy behavior");
    static_assert(SPEED == SPEED_NORMAL || SPEED == SPEED_FAST, "invalid enemy speed");
    static_assert((MODE == ENEMY_ROUTE && routes::area_for(ROUTE) == POS.area) ||
                  (MODE != ENEMY_ROUTE && ROUTE == 0), "enemy patrol needs a route in its area");
    static_assert(RESPAWN <= RESPAWN_NEVER, "invalid enemy respawn policy");
    static_assert(SETTINGS.facing <= WEST && SETTINGS.pause <= 240 && SETTINGS.walk <= 240,
                  "invalid enemy facing or wander duration");
    static_assert(MODE != ENEMY_CHASE ||
                  (SETTINGS.detect && SETTINGS.detect < SETTINGS.disengage &&
                   SETTINGS.wander_x + SETTINGS.wander_y < SETTINGS.leash),
                  "chase needs hysteresis and a leash beyond the wandering range");

    static constexpr TilePos position = POS;
    static constexpr auto battle = BATTLE;
    static constexpr auto respawn = RESPAWN;
    static constexpr auto settings = SETTINGS;
    using hooks = Hooks;
    static constexpr bool conditional = &Hooks::available != &EnemyHooks::available;

    static BattleEncounter const PROGMEM* prepare_battle() {
        if(!Hooks::before_battle())
            return nullptr;
        return Hooks::select_battle(BATTLE);
    }

    static constexpr NpcDefinition PROGMEM definition{
        MODE == ENEMY_ROUTE ? NPC_ROUTE : MODE == ENEMY_CHASE ? NPC_CHASE : NPC_WANDER,
        SETTINGS.facing, SPEED, SETTINGS.pause, SETTINGS.walk,
        SETTINGS.wander_x, SETTINGS.wander_y, SETTINGS.priority, ROUTE, ROW, false
    };
};

template <typename... Entries> consteval bool unique_enemy_positions() {
    constexpr TilePos positions[] = {NO_LOCATION, Entries::position...};
    for(unsigned i = 1; i < sizeof...(Entries) + 1; ++i)
        for(unsigned j = 0; j < i; ++j)
            if(positions[i] == positions[j])
                return false;
    return true;
}

template <typename Spawn, std::size_t INDEX> struct EnemySite {
    static constexpr TilePos position = Spawn::position;
    static constexpr NPC_ID id = (NPC_ID)(NPC_ENEMY_FIRST + INDEX);
    template <HandlerStage> static constexpr bool implemented = true;

    static TilePos current() {
        return !game.get_flag(enemy_defeated_flag(id)) && Spawn::hooks::available()
                   ? position : NO_LOCATION;
    }

    template <HandlerStage> static void run() {
#ifndef RPG_AVM
        ++npc_site_checks;
#endif
        if(current() == position)
            npc_offer_candidate(id, position);
    }
};

template <typename, typename> struct EnemyCatalog;

template <std::size_t... I, typename... Entries>
struct EnemyCatalog<std::index_sequence<I...>, TypeList<Entries...>> {
    static_assert(unique_enemy_positions<Entries...>(), "duplicate enemy spawn positions");
    using sites = TypeList<EnemySite<Entries, I>...>;
    static constexpr TilePos PROGMEM positions[] = {
        {Entries::position.area, Entries::position.x, Entries::position.y}..., {0, 0, 0}
    };

    static TilePos location(NPC_ID id) {
        if(game.get_flag(enemy_defeated_flag(id)) ||
           ((Entries::conditional && id == EnemySite<Entries, I>::id &&
             !Entries::hooks::available()) || ...))
            return NO_LOCATION;
        auto const& site = positions[id - NPC_ENEMY_FIRST];
        return {site.area, site.x, site.y};
    }
    static constexpr NpcDefinition const PROGMEM* const PROGMEM definitions[] = {
        &Entries::definition..., nullptr
    };
    static constexpr EnemyPursuit PROGMEM pursuit[] = {
        {Entries::settings.detect, Entries::settings.disengage, Entries::settings.leash}..., {}
    };
    static constexpr bool PROGMEM remembers[] = {(Entries::respawn == RESPAWN_REMEMBER)..., false};
};

#endif
