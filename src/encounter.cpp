#include "encounter.h"

#include "logic/enemy_catalog.h"

namespace {
NPC_ID pending_enemy;
bool remember_due;

bool defeated_respawnable() {
    for(unsigned i = 0; i < ENEMY_SPAWN_COUNT; ++i)
        if(AuthoredEnemies::remembers[i] && game.get_flag(enemy_defeated_flag((NPC_ID)(NPC_ENEMY_FIRST + i))))
            return true;
    return false;
}

template <typename> struct EncounterHooks;

template <typename... Spawns, std::size_t... I>
struct EncounterHooks<TypeList<EnemySite<Spawns, I>...>> {
    static BattleEncounter const PROGMEM* before(NPC_ID id) {
        BattleEncounter const PROGMEM* battle = nullptr;
        (void)((id == EnemySite<Spawns, I>::id && (battle = Spawns::prepare_battle(), true)) || ...);
        return battle;
    }

    static void victory(NPC_ID id) {
        (void)((id == EnemySite<Spawns, I>::id && (Spawns::hooks::victory(), true)) || ...);
    }

    static void after(NPC_ID id) {
        (void)((id == EnemySite<Spawns, I>::id && (Spawns::hooks::after_victory(), true)) || ...);
    }
};

using Hooks = EncounterHooks<AuthoredEnemies::sites>;
} // namespace

EnemyPursuit enemy_pursuit(NPC_ID id) {
    auto const& rules = AuthoredEnemies::pursuit[id - NPC_ENEMY_FIRST];
    return {rules.detect, rules.disengage, rules.leash};
}

void encounter_contact(NPC_ID id) {
    if(npc_is_enemy(id) && (!pending_enemy || id < pending_enemy))
        pending_enemy = id;
}

void encounter_restore() {
    pending_enemy = NPC_NONE;
    remember_due = !game.remember_timer && defeated_respawnable();
}

bool remember_save_valid() {
    return game.remember_timer <= REMEMBER_DELAY_FRAMES &&
           (!game.remember_timer || defeated_respawnable());
}

bool remember_tick() {
    if(game.remember_timer && !--game.remember_timer)
        remember_due = defeated_respawnable();
    return remember_due;
}

void remember_reset() {
    game.remember_timer = 0;
    for(unsigned i = 0; i < ENEMY_SPAWN_COUNT; ++i)
        if(AuthoredEnemies::remembers[i])
            game.clear_flag(enemy_defeated_flag((NPC_ID)(NPC_ENEMY_FIRST + i)));
    remember_due = false;
}

RPG_NOINLINE bool encounter_run() {
    NPC_ID id = pending_enemy;
    pending_enemy = NPC_NONE;
    if(!id || !npc_find(id) || npc_location(id).area != game.area)
        return true;

    auto battle = Hooks::before(id);
    dialogue_close();
    if(!battle) {
        npc_retire_enemy(id);
        return true;
    }
    if(!npc_find(id) || npc_location(id).area != game.area)
        return true;
    if(!battle_encounter_valid(battle)) {
        npc_retire_enemy(id);
        return true;
    }

    if(battle_show(battle) == BATTLE_DEFEAT) {
        shop_requested = false;
        encounter_restore();
        return false;
    }

    game.set_flag(enemy_defeated_flag(id));
    if(AuthoredEnemies::remembers[id - NPC_ENEMY_FIRST]) {
        game.remember_timer = REMEMBER_DELAY_FRAMES;
        remember_due = false;
    }
    npc_retire_enemy(id);
    Hooks::victory(id);
    world_reclaim_workspace();
    fade_world_in();
    Hooks::after(id);
    return true;
}
