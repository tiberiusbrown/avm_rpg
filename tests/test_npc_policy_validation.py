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
    'enemy_position': ('using Bad = EnemySpawn<TilePos{255, 0, 0}, &BATTLE_GROVE_WOLVES, 16>;', 'invalid enemy spawn position'),
    'enemy_battle': ('using Bad = EnemySpawn<positions::GroveWolf, nullptr, 16>;', 'enemy spawn needs a battle'),
    'enemy_duplicate': ('using Entry = EnemySpawn<positions::GroveWolf, &BATTLE_GROVE_WOLVES, 16>; using Bad = EnemyCatalog<std::index_sequence<0, 1>, TypeList<Entry, Entry>>;', 'duplicate enemy spawn positions'),
    'enemy_sprite': ('using Bad = EnemySpawn<positions::GroveWolf, &BATTLE_GROVE_WOLVES, 32>;', 'invalid enemy sprite row'),
    'enemy_route_area': ('using Bad = EnemySpawn<positions::GroveWolf, &BATTLE_GROVE_WOLVES, 16, ENEMY_ROUTE, SPEED_NORMAL, routes::VaultPatrol>;', 'enemy patrol needs a route in its area'),
    'enemy_route_missing': ('using Bad = EnemySpawn<positions::GroveWolf, &BATTLE_GROVE_WOLVES, 16, ENEMY_ROUTE>;', 'enemy patrol needs a route in its area'),
    'enemy_speed': ('using Bad = EnemySpawn<positions::GroveWolf, &BATTLE_GROVE_WOLVES, 16, ENEMY_WANDER, (ENEMY_SPEED)3>;', 'invalid enemy speed'),
    'enemy_behavior': ('using Bad = EnemySpawn<positions::GroveWolf, &BATTLE_GROVE_WOLVES, 16, (ENEMY_BEHAVIOR)9>;', 'invalid enemy behavior'),
    'enemy_respawn': ('using Bad = EnemySpawn<positions::GroveWolf, &BATTLE_GROVE_WOLVES, 16, ENEMY_WANDER, SPEED_NORMAL, 0, (ENEMY_RESPAWN)9>;', 'invalid enemy respawn'),
}
cases.update(enemy_cases)
for name, (body, diagnostic) in cases.items():
    path = work / (name + '.cpp')
    header = 'logic/enemy_spawns.h' if name in enemy_cases else 'npc_policy.h'
    path.write_text(f'#include "{header}"\n' + body + '\nstatic_assert(sizeof(Bad) > 0);\n')
    if Path(compiler).stem.lower() in ('cl', 'clang-cl'):
        command = [compiler, '/nologo', '/std:c++20', '/Zs', '/I' + str(root / 'src'),
                   '/I' + str(root / 'generated'), str(path)]
    else:
        command = [compiler, '-std=c++20', '-fsyntax-only', '-I' + str(root / 'src'),
                   '-I' + str(root / 'generated'), str(path)]
    result = subprocess.run(command,
                            text=True, capture_output=True)
    diagnostics = result.stdout + result.stderr
    assert result.returncode and diagnostic in diagnostics, (name, diagnostics)
print('invalid policy diagnostics passed')
