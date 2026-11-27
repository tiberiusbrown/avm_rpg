"""Compile-only diagnostics: invalid quest definitions must fail before runtime."""
from pathlib import Path
import subprocess
import sys

compiler, source, build = sys.argv[1:]
root = Path(source)
work = Path(build) / 'quest-validation'
work.mkdir(exist_ok=True)

# Each case is a complete translation unit. The ID limit is set per case so that the catalog spans
# only the quests the case defines.
HEADER = '#define RPG_QUEST_ID_LIMIT {limit}\n#include "quest_dispatch.h"\n'
VALID_QUEST = ('template <> struct Quest<(QUEST_ID)1> {{\n'
               '    static QUEST_STATUS status() {{ return QUEST_NOT_STARTED; }}\n'
               '    static char const PROGMEM* title() {{ return F("Q"); }}\n'
               '    static char const PROGMEM* summary() {{ return F("S"); }}\n'
               '{extra}}};\n')
CHECK = 'static_assert(AllQuests::valid, "catalog check");\n'

cases = {
    'missing': (HEADER.format(limit=3) + VALID_QUEST.format(extra='') + CHECK,
                'missing quest definition for a registered QUEST_ID'),
    'status_signature': (HEADER.format(limit=2) + 'template <> struct Quest<(QUEST_ID)1> {\n'
                         '    static bool status() { return true; }\n'
                         '    static char const PROGMEM* title() { return F("Q"); }\n'
                         '    static char const PROGMEM* summary() { return F("S"); }\n};\n' + CHECK,
                         'quest must define static QUEST_STATUS status()'),
    'title_signature': (HEADER.format(limit=2) + 'template <> struct Quest<(QUEST_ID)1> {\n'
                        '    static QUEST_STATUS status() { return QUEST_ACTIVE; }\n'
                        '    static int title() { return 0; }\n'
                        '    static char const PROGMEM* summary() { return F("S"); }\n};\n' + CHECK,
                        'quest must define static char const PROGMEM* title()'),
    'marker_signature': (HEADER.format(limit=2) + VALID_QUEST.format(
        extra='    static int marker(uint8_t, TilePos&) { return 0; }\n') + CHECK,
        'quest marker() must be static bool marker(uint8_t area, TilePos& out)'),
    'start_signature': (HEADER.format(limit=2) + VALID_QUEST.format(
        extra='    static bool start() { return true; }\n') + CHECK,
        'quest start() must be static void start()'),
    'subscription_npc': (HEADER.format(limit=2) + VALID_QUEST.format(
        extra='    using Watches = Watch<NpcTalked<(NPC_ID)NPC_ID_LIMIT>>;\n    static void on(NpcTalked<(NPC_ID)NPC_ID_LIMIT>) {}\n') + CHECK,
        'invalid NPC identity for conversation event'),
    'subscription_flag': (HEADER.format(limit=2) + VALID_QUEST.format(
        extra='    using Watches = Watch<FlagChanged<FLAG_COUNT>>;\n    static void evaluate() {}\n') + CHECK,
        'invalid flag subscription'),
    'subscription_reserved_flag': (HEADER.format(limit=2) + VALID_QUEST.format(
        extra='    using Watches = Watch<FlagChanged<FLAG_RESERVED>>;\n    static void evaluate() {}\n') + CHECK,
        'invalid flag subscription'),
    'subscription_unsupported': (HEADER.format(limit=2) + VALID_QUEST.format(
        extra='    using Watches = Watch<int>;\n') + CHECK,
        'unsupported event subscription'),
    'subscription_duplicate': (HEADER.format(limit=2) + VALID_QUEST.format(
        extra='    using Watches = Watch<NpcTalked<NPC_TESSA>, NpcTalked<NPC_TESSA>>;\n'
              '    static void on(NpcTalked<NPC_TESSA>) {}\n') + CHECK,
        'duplicate event subscription'),
    'handler_missing': (HEADER.format(limit=2) + VALID_QUEST.format(
        extra='    using Watches = Watch<NpcTalked<NPC_TESSA>>;\n') + CHECK,
        'a subscribed event needs its handler'),
    'flag_handler_missing': (HEADER.format(limit=2) + VALID_QUEST.format(
        extra='    using Watches = Watch<FlagChanged<FLAG_CAVE_GATE>>;\n') + CHECK,
        'a subscribed event needs its handler'),
    'valid_minimal': (HEADER.format(limit=2) + VALID_QUEST.format(extra='') + CHECK, None),
    'valid_shipping_catalog': ('#include "quest_dispatch.h"\n#include "quest_registry.h"\n' + CHECK, None),
    'valid_subscriptions': (HEADER.format(limit=2) + VALID_QUEST.format(
        extra='    using Watches = Watch<FlagChanged<FLAG_CAVE_GATE>, NpcTalked<NPC_TESSA>>;\n'
              '    static void evaluate() {}\n    static void on(NpcTalked<NPC_TESSA>) {}\n') + CHECK, None),
}

failures = []
for name, (body, diagnostic) in cases.items():
    path = work / (name + '.cpp')
    path.write_text(body if body.startswith('#') else '#include "quest_dispatch.h"\n' + body)
    if Path(compiler).stem.lower() in ('cl', 'clang-cl'):
        command = [compiler, '/nologo', '/std:c++20', '/Zs', '/I' + str(root / 'src'),
                   '/I' + str(root / 'generated'), str(path)]
    else:
        command = [compiler, '-std=c++20', '-fsyntax-only', '-I' + str(root / 'src'),
                   '-I' + str(root / 'generated'), str(path)]
    result = subprocess.run(command, text=True, capture_output=True)
    diagnostics = result.stdout + result.stderr
    if diagnostic is None:
        if result.returncode:
            failures.append((name, diagnostics))
    else:
        if not (result.returncode and diagnostic in diagnostics):
            failures.append((name, diagnostics))
if failures:
    for name, diagnostics in failures:
        print('FAILED', name, diagnostics[:2000], file=sys.stderr)
    sys.exit(1)
print('quest definition diagnostics passed')
