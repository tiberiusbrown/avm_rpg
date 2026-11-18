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
for name, (body, diagnostic) in cases.items():
    path = work / (name + '.cpp')
    path.write_text('#include "npc_policy.h"\n' + body + '\nstatic_assert(sizeof(Bad) > 0);\n')
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
