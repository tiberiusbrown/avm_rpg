"""Compile-only diagnostics: invalid content must fail before runtime."""
from pathlib import Path
import subprocess
import sys

compiler, source, build = sys.argv[1:]
root = Path(source)
work = Path(build) / 'policy-validation'
work.mkdir(exist_ok=True)
cases = {
    'duplicate': ('using Bad = AtOneOf<positions::VillagerHome, When<positions::VillagerHome, [] { return true; }>>;', 'duplicate NPC policy location'),
    'invalid': ('using Bad = AtOneOf<TilePos{255, 0, 0}>;', 'invalid or duplicate'),
    'unsupported': ('struct Case { static constexpr TilePos position = positions::VillagerEast; static bool applies() { return true; } }; using Bad = AtOneOf<positions::VillagerHome, Case>;', 'unsupported NPC policy case'),
    'missing': ('using Bad = PolicyCatalog<std::make_index_sequence<NPC_ID_LIMIT - 1>>;', 'missing NpcPolicy'),
    'override': ('template <> struct NpcPolicy<NPC_VILLAGER> : AtOneOf<positions::VillagerHome> { static TilePos current() { return positions::VillagerEast; } }; using Bad = CheckedNpcPolicy<NPC_VILLAGER>;', 'do not override the derived location resolver'),
    'identity': ('auto bad() { return npc_location<NPC_NONE>(); } using Bad = AtOneOf<NO_LOCATION>;', 'invalid NPC identity'),
}
enemy_cases = {
    'enemy_position': ('using Bad = EnemySpawn<TilePos{255, 0, 0}, &FIXTURE_BATTLE, 16>;', 'invalid enemy spawn position'),
    'enemy_battle': ('using Bad = EnemySpawn<positions::GroveWolf, nullptr, 16>;', 'enemy spawn needs a battle'),
    'enemy_duplicate': ('using Entry = EnemySpawn<positions::GroveWolf, &FIXTURE_BATTLE, 16>; using Bad = EnemyCatalog<std::index_sequence<0, 1>, TypeList<Entry, Entry>>;', 'duplicate enemy spawn positions'),
    'enemy_sprite': ('using Bad = EnemySpawn<positions::GroveWolf, &FIXTURE_BATTLE, 32>;', 'invalid enemy sprite row'),
    'enemy_route_area': ('using Bad = EnemySpawn<positions::GroveWolf, &FIXTURE_BATTLE, 16, ENEMY_ROUTE, SPEED_NORMAL, routes::VaultPatrol>;', 'enemy patrol needs a route in its area'),
    'enemy_route_missing': ('using Bad = EnemySpawn<positions::GroveWolf, &FIXTURE_BATTLE, 16, ENEMY_ROUTE>;', 'enemy patrol needs a route in its area'),
    'enemy_speed': ('using Bad = EnemySpawn<positions::GroveWolf, &FIXTURE_BATTLE, 16, ENEMY_WANDER, (ENEMY_SPEED)3>;', 'invalid enemy speed'),
    'enemy_behavior': ('using Bad = EnemySpawn<positions::GroveWolf, &FIXTURE_BATTLE, 16, (ENEMY_BEHAVIOR)9>;', 'invalid enemy behavior'),
    'enemy_respawn': ('using Bad = EnemySpawn<positions::GroveWolf, &FIXTURE_BATTLE, 16, ENEMY_WANDER, SPEED_NORMAL, 0, (ENEMY_RESPAWN)9>;', 'invalid enemy respawn'),
}
def compile_source(name, source):
    path = work / (name + '.cpp')
    path.write_text(source)
    if Path(compiler).stem.lower() in ('cl', 'clang-cl'):
        command = [compiler, '/nologo', '/std:c++20', '/Zs', '/I' + str(root / 'src'),
                   '/I' + str(root / 'generated'), str(path)]
    else:
        command = [compiler, '-std=c++20', '-fsyntax-only', '-I' + str(root / 'src'),
                   '-I' + str(root / 'generated'), str(path)]
    return subprocess.run(command, text=True, capture_output=True)


cases.update(enemy_cases)
for name, (body, diagnostic) in cases.items():
    header = 'logic/enemy_spawn.h' if name in enemy_cases else 'npc_policy.h'
    battle = 'constexpr BattleEncounter FIXTURE_BATTLE{};\n' if name in enemy_cases else ''
    result = compile_source(name, f'#include "{header}"\n{battle}{body}\n'
                                  'static_assert(sizeof(Bad) > 0);\n')
    diagnostics = result.stdout + result.stderr
    assert result.returncode and diagnostic in diagnostics, (name, diagnostics)

standalone = {
    'generic_enemy_spawn': """
#include "logic/enemy_spawn.h"
constexpr BattleEncounter fixture{};
using Spawn = EnemySpawn<positions::GroveWolf, &fixture, 17>;
static_assert(Spawn::definition.sprite_row == 17);
""",
    'disabled_enemy_catalog': """
#define RPG_NPC_ID_LIMIT 65
#define RPG_DISABLE_ENEMY_SPAWNS
#include "logic/enemy_catalog.h"
static_assert(NPC_ENEMY_FIRST == 65 && ENEMY_SPAWN_COUNT == 0);
static_assert(std::is_same_v<AuthoredEnemies::sites, TypeList<>>);
""",
}
for name, source in standalone.items():
    result = compile_source(name, source)
    assert not result.returncode, (name, result.stdout + result.stderr)

print('NPC and enemy policy validation passed')
