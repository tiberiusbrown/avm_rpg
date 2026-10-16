# AVM Open-World RPG — Game Design & Technical Specification

**Version:** 0.1 (discussion draft)  
**Date:** October 14, 2026  
**Status:** Proposed; intentionally open to revision  
**Target:** Arduboy Virtual Machine (AVM), running on Arduboy-class AVR hardware  
**Primary audience:** AI design/development agents, with human code review and playtesting  
**Project working title:** *Untitled AVM RPG* (the agent will propose a final title)

> **Core thesis:** Build a small-scale but genuine Elder Scrolls-style open-world RPG, with a main quest, optional quests, freely explorable towns and dungeons, named characters, ambient wandering NPCs, consequential choices, and a world entirely designed and authored by AI. Implement game behavior and quest logic in native C/C++, not a runtime scripting language. Use AVM's expanded code/content storage to make the world substantially richer than a conventional tiny handheld RPG, while strictly bounding guest RAM and stack.

## 1. Executive summary

The game is a top-down, tile-based, single-player fantasy RPG for a 128 × 64 monochrome display and six controls (D-pad plus A and B). The player can largely ignore the main storyline, travel around the world, explore settlements and wilderness, enter dungeons, meet people, fight enemies, find secrets, improve equipment, and follow side quests. Significant events change conversations and local world state. The world should feel authored, interconnected, and worth exploring rather than generated merely to fill space.

AI agents are responsible for **all creative content**: world premise and lore, geography, named places, town layouts, interiors, dungeon layouts, NPC identities and personalities, dialogue, quests, encounters, items, monsters, environmental storytelling, and game balance. The human's role is to evaluate code and playable builds, not to supply quest premises or manually design levels.

Development is **incremental, not one-shot**. The team first proves AVM integration and a playable engine, then produces a complete small region, then expands in tested regional increments toward a finished main story and meaningful optional content. The agent may iterate on creative work autonomously. Human feedback is primarily code review and playtesting.

### 1.1 Design pillars

1. **Freedom first.** Main-quest progress is optional during most of the game; wandering should reliably uncover interesting discoveries.
2. **A living-looking world, not a full simulation.** Townsfolk wander, pause, face directions, and respond to context; offscreen NPCs need no simulated schedules.
3. **Content density over raw size.** Memorable places, meaningful quests, and distinct settlements outrank map dimensions or quest counts.
4. **Native compiled rules.** Distinct quest outcomes and bespoke interactions are coded directly in C/C++; text and maps are compiled as read-only assets.
5. **Deterministic, testable state.** Core rules and important quest branches must have automated state-based tests independent of rendering.
6. **AVM-native discipline.** ≤1,024 bytes of guest static RAM, ≤256 bytes of guest stack, six buttons, host-assisted display/audio where available, and bounded frame workload.
7. **Agent creative ownership.** The agent fills in unspecified design details, records its choices, and revises weak material without shifting creative decisions to the human.

### 1.2 Explicit non-goals

- A faithful reproduction of any particular Elder Scrolls game or copyrighted setting.
- Real-time simulation of every NPC, complex daily schedules, physics-heavy action combat, or large active crowds.
- Multiplayer, user-generated quests, a runtime scripting VM, general-purpose scene editor on-device, or procedural generation as a substitute for handcrafted-feeling content.
- A vast but empty map, an infinite quest generator, or an obligation to complete all side content before the ending.
- Fully voice-acted dialogue, colored artwork, large cutscenes, or unbounded free-form text input.

## 2. Project decisions and ownership

| Topic | Current design direction | Owner / status |
|---|---|---|
| Setting, lore, plot, geography, characters | AI invents and authors all | **Locked principle; content undecided** |
| Camera/presentation | Top-down 8×8 tile world, smooth or tile-stepped scrolling depending on performance | Proposed |
| Primary movement | Real-time tile-to-tile walking | Proposed |
| Combat | Turn-based or low-frequency tactical combat; prefer turn-based first | Proposed |
| Dialogue/quests | Native C/C++ handlers and read-only text; no runtime quest scripting VM | **Locked principle** |
| NPC activity | Active-area-only wandering and short patrols; important NPCs retain persistent identity | **Locked principle** |
| World structure | One connected overworld, entrances to local interior/dungeon maps | Proposed |
| Saved progress | Durable, versioned save with persistent quest/world changes | Required; API unverified |
| AVM budgets | 1 KiB guest static RAM and 256 B guest stack | **Hard requirements from project brief** |
| Art and music | AI-authored monochrome assets and optional host-generated audio | Proposed |
| Human workflow | Human reviews code and playtests; agents own content design | **Locked principle** |

**Verification caveat:** Exact AVM runtime APIs, guest ABI, display ownership, data access, save storage, and profiler behavior have not been verified against the current repository for this draft. Phase 0 must inspect the actual codebase and replace every provisional assumption with an established interface before implementation commitments are made. AVM repository: https://github.com/tiberiusbrown/avm.

## 3. Player experience and core loop

### 3.1 Desired feel

A first-time player begins in a recognizable settlement or nearby wilderness, meets a few characters, learns the basic controls, hears a rumor about the central mystery, and is immediately free to choose where to go. Roads lead to other communities; branching paths reveal caves, ruins, hidden clearings, and travelers. The world contains small personal stories as well as a larger conflict. Key plot characters exist in the world, but the game should not feel like a chain of mandatory quest markers.

**Typical loop:** explore → discover person/place/problem → investigate or fight or negotiate → gain information/item/reputation → observe a changed world → select a new destination. Exploration, not grinding, should be the main source of rewards.

### 3.2 Navigation and discovery

- The player walks four-directionally across solid/passable tiles, with visual facing direction.
- Landscape contains roads, water, vegetation, ruins, landmarks, town gates, bridges, and dungeon entrances.
- The overworld is mostly contiguous; entering buildings or dungeons transitions to compact local maps.
- Wilderness paths should permit alternate routes and accidental discoveries, not funnel every journey through a required checkpoint.
- Reward close observation with optional chests, secret routes, hidden NPCs, lore, rare enemies, and environmental puzzles.
- Provide an accessible world-map view and a compact journal; do **not** default to intrusive quest arrows at every moment.
- Landmarks, rumors, and written directions should allow progress without constant waypoint guidance.

### 3.3 Progression principles

- Player starts modestly capable; exploration supplies gear, experience, money, and new capabilities.
- Early regions are forgiving. Danger increases in certain wilderness pockets or dungeons, with warnings before major difficulty jumps.
- Quest rewards include world access, information, reputation, items, and changed relationships—not only currency and experience.
- At least some important quests have alternative solutions (combat, discovery, diplomacy, item use, or choice).
- The player can finish the main storyline without resolving most side quests.

## 4. Controls, display, and UX

### 4.1 Six-button control model

| Context | D-pad | A | B |
|---|---|---|---|
| Overworld | Move/facing | Talk, inspect, open, use, confirm context | Open main menu |
| Main menu | Navigate tabs/entries | Select | Back/close |
| Dialogue | Change choice / advance selection | Advance / choose | Back if permitted; otherwise skip animation only |
| Inventory | Move cursor | Use/equip/select | Back |
| Battle | Navigate command/target | Execute/confirm | Cancel to previous step |
| Map/journal | Scroll or navigate | Select detail | Return |

Where ambiguous, display a small contextual prompt when near a person or interactable tile. Avoid requiring button chords; if a secondary shortcut is helpful, add it only after usability tests.

### 4.2 Screen layout

- Native display is **128×64 monochrome**. Primary world view shows up to **16×8 tiles at 8×8 pixels**, subject to camera and optional overlays.
- Avoid reserving a permanent large HUD that sacrifices map visibility. Show health and location briefly on transitions or in a small unobtrusive overlay; detailed stats live in the menu.
- Dialogue may cover much of the display and pause the simulation; use a readable bitmap font, bounded lines, and page transitions.
- Character names should be legible; avoid large portraits that reduce dialogue space unless testing demonstrates benefit.
- Support visual distinction among terrain through high-contrast silhouettes and textures, not subtle grayscale patterns.
- Draw only what is necessary; measure guest CPU and storage reads before committing to smooth pixel-scrolling.

### 4.3 Usability requirements

- No player-critical information should require audio.
- Every interactable needs a discoverable affordance: facing, A prompt, distinctive tile, or NPC feedback.
- Menu navigation must remain predictable with two action buttons.
- The game must tolerate save/reload from any safe out-of-combat location and must never strand the player behind a quest-locked entrance.
- Prefer a concise journal with **objective, place, and clues**, rather than a long transcript.

## 5. World model and content structure

### 5.1 Proposed initial scale and later expansion

**Vertical slice:** 1 town, adjacent wilderness, 1 dungeon, 8–12 locally active/named NPCs, 1 main-story hook, and 3–5 meaningful side quests. This is the first human-playtested build.

**Candidate final scope (stretch targets, not fixed quotas):** a roughly 128×128 overworld, 5–8 settlements, 8–12 dungeons/ruins, 60–100 named NPCs across all areas, 25–40 substantial side quests (expand only if quality stays high), 8–12 main-quest stages, 3–5 factions, and 2–3 meaningful ending variants. Smaller is acceptable if dense and polished.

At four bits per tile, a 128×128 map requires 8,192 bytes of read-only tile indices before layering, collision metadata, or compression. The agent must benchmark actual content access through AVM; this is a storage estimate, not proof of suitability.

### 5.2 Regions, areas, and entrances

- **Region:** narrative/geographic grouping (coast, forest, frontier, mountain pass, etc.), not necessarily a separately loaded runtime unit.
- **Area:** one contiguous map with a stable ID. The overworld may be one large area; interiors and dungeons have separate area IDs.
- **Portal:** pair of source tile and destination area/tile, used for doorways, stairs, and special travel.
- **Persistent changes:** global flags or bounded area-specific overrides, never a full copied world map in RAM.
- **Area entry:** determine NPC spawns, enemies, object state, quest changes, and entrance positions from static definitions plus saved flags.
- **Area exit:** discard transient entities and cache; persist only authored consequences and relevant persistent states.

### 5.3 Town design rubric for the content agent

Every settlement should have a distinct local economy or purpose, landmark, visual identity, history, social tension, memorable residents, links to other regions, and at least one discovery unrelated to the main plot. Include ambient people who are not quest dispensers. Allow some consequences from other regions to show up in conversations or services.

### 5.4 Dungeon design rubric

Each dungeon has an explicit premise and reason to exist, a readable internal visual language, at least one memorable mechanic or twist, meaningful optional branches, and a satisfying discovery or conclusion. Avoid indiscriminate repetitive corridors and combat. Provide recovery/exit safeguards and test every transition. Distinct dungeon mechanics may use bespoke native code where needed.

## 6. NPC simulation: a living world on tiny RAM

### 6.1 NPC categories

1. **Ambient:** anonymous or lightly named villagers, travelers, guards, animals, etc. They wander, pause, turn, and optionally speak a short contextual line. Positions are not persisted globally.
2. **Named non-quest:** merchants, elders, craftsmen, witnesses, rivals, and other authored personalities with location-specific dialogue or services. Their identity persists in the static world database.
3. **Quest-important:** named characters with branching conversations, custom quest interactions, changed availability or location, and optionally direct consequences when killed/rescued/recruited (if these mechanics are enabled).
4. **Hostile/creature:** map-based enemies using simpler patrol, pursuit, or battle-entry behavior.

### 6.2 Lightweight movement algorithm

Only NPCs in the **current active area** exist in live memory. Proposed cap: **12 active entities** initially, subject to measurement. An NPC has a stable ID or definition index, current tile position, facing, movement state, home/anchor position or short patrol route, and small cooldown/behavior bits.

- Each NPC receives a movement opportunity on a coarse cadence (e.g., every 8–16 player movement steps or a bounded simulation timer); cadence is tunable.
- On an opportunity, choose among stand, turn, or step to a nearby passable unoccupied tile. Allow several seconds of inactivity to avoid jitter.
- For ambient characters, prefer constrained roaming around an anchor or small radius; for guards, use 2–5 authored patrol waypoints.
- Never enter the player's tile, leave allowed bounds, block critical one-tile exits indefinitely, or walk through solid objects.
- If blocked, stand or choose another direction; do not perform expensive pathfinding for casual wandering.
- When dialog begins, freeze world movement or at least the speaking NPC. On exit/reentry, regenerate ambient positions near their home area.
- Important NPCs must remain **findable**: anchor them to recognizable spaces, provide a fallback location, and never wander into inaccessible tiles.
- Changes from quests, such as an NPC moving towns, are explicit native rules evaluated when an area loads.

**Determinism:** Use a seeded tiny PRNG with independent or reproducible NPC-update behavior so replay tests can reproduce movement. Ambient randomness must never change quest outcomes unpredictably.

### 6.3 Ambient dialogue

Ambient speech uses short authored snippets selected from a person's identity, area context, and relevant world flags. It may mention rumors, local dangers, current faction tensions, weather flavor, or recent quest outcomes. Repetition should be limited. Substantial branching dialogue is reserved for named characters.

### 6.4 NPC acceptance tests

No illegal moves; no overlapping solid NPCs; no permanently blocked doorway; movement remains within configured bounds; quest NPC is always accessible; entering/exiting area safely reconstructs NPCs; conversation reliably starts/stops; moving NPCs do not invalidate interactions in progress.

## 7. Quest, dialogue, and choice system — native C/C++

### 7.1 Architectural rule

**There will be no runtime quest scripting language or event bytecode interpreter.** Each quest can use ordinary C/C++ functions, shared helper APIs, persistent flags/phases, and compile-time data. A common dispatch mechanism may route a stable NPC/object/quest ID to a handler; dispatch is not a script VM. Complex quests are free to contain unique code and mechanics.

Dialogue text and selectable option labels should generally be read-only, generated asset data. Choices invoke directly compiled handlers. On-device UI is an incremental state machine rather than a blocking function that expects to preserve a deep stack across player input.

### 7.2 Shared native APIs (conceptual, not AVM ABI)

```cpp
// Illustrative interfaces; actual names/types to be established in Phase 0/1.
struct GameState;
struct DialogueView;

bool has_flag(const GameState&, uint16_t flag);
void set_flag(GameState&, uint16_t flag);
bool has_item(const GameState&, uint8_t item_id);
bool take_item(GameState&, uint8_t item_id, uint8_t quantity);
bool give_item(GameState&, uint8_t item_id, uint8_t quantity);
void start_dialogue(uint16_t text_id, uint16_t choice_set_id);
void start_battle(uint8_t encounter_id);
void move_named_npc(GameState&, uint16_t npc_id, uint16_t area_id);
void mark_journal_event(GameState&, uint16_t event_id);

// Called in response to player interactions, not held open on the stack.
void on_npc_interact(GameState&, uint16_t npc_id);
void on_object_interact(GameState&, uint16_t object_id);
void on_dialogue_choice(GameState&, uint16_t choice_id);
```

### 7.3 Persistent quest representation

- Assign stable IDs to quest flags, phases, major outcomes, and named entities.
- Prefer one canonical flag for a world fact: e.g., `BRIDGE_REPAIRED`, not separate redundant flags in three quests.
- Use quest-phase bytes only where the current stage cannot be derived efficiently and reliably from world flags.
- Reserve irreversible state changes for explicit commit points, especially item consumption, allegiance changes, NPC relocation, and endings.
- Maintain a journal view derived from flags/phases and read-only strings; do not store full prose in RAM.
- Do not let the order of source files or asset generation change saved-game ID values.

### 7.4 Quest quality rubric

Every meaningful side quest should include most of: a strong human or world-specific hook, at least one discovery or complication, a memorable location/NPC interaction, a meaningful resolution, and a visible persistent consequence. Vary activity types: investigation, exploration, puzzle, combat, diplomacy, delivery where narratively appropriate, rescue, trade, or environmental manipulation. Reject copy-paste errands unless they serve a distinct narrative purpose.

Use **2–3 major multi-stage questlines** alongside smaller independent quests. Faction membership may create mutually exclusive outcomes, but should not silently break the critical path. Avoid requiring the player to retain an item they could reasonably sell or consume without warning.

### 7.5 Example native quest (illustrative only)

```cpp
void talk_to_watch_captain(GameState& g) {
    if (has_flag(g, FLAG_GATE_REOPENED)) {
        start_dialogue(TEXT_CAPTAIN_AFTER, CHOICES_NONE);
    } else if (has_flag(g, FLAG_SMUGGLER_EVIDENCE)) {
        start_dialogue(TEXT_CAPTAIN_EVIDENCE, CHOICES_REPORT_OR_CONCEAL);
    } else {
        start_dialogue(TEXT_CAPTAIN_RUMOR, CHOICES_ACCEPT_LEAD);
    }
}

void captain_choice(GameState& g, uint16_t choice) {
    switch (choice) {
    case CHOICE_REPORT_SMUGGLERS:
        set_flag(g, FLAG_SMUGGLERS_EXPOSED);
        mark_journal_event(g, JOURNAL_WATCH_HELPED);
        break;
    case CHOICE_CONCEAL_EVIDENCE:
        set_flag(g, FLAG_SMUGGLERS_PROTECTED);
        mark_journal_event(g, JOURNAL_HIDDEN_AGREEMENT);
        break;
    }
}
```

This is not prescribed story content: the agent invents the actual characters, quests, and outcomes.

## 8. Main quest, factions, side quests, and narrative coherence

### 8.1 Main quest structure

The narrative agent chooses an original central conflict with a clear mystery, emerging stakes, and a credible antagonist or competing interests. The main quest should have an opening invitation, several independently approachable leads, a midgame reversal, a commitment stage, and a finale. The first half should allow significant deviation and regional quest order variation; the final stretch can narrow to a more linear conclusion.

Suggested structure: **8–12 named stages**, grouped into three acts. Include at least one branching resolution of a major dilemma and 2–3 overall ending variants. The main story must make sense if the player completes few optional quests, while side-quest outcomes should materially color relationships, aid, locations, or epilogues.

### 8.2 Side content

Start with 3–5 distinct quests in the vertical slice. Expand to **25–40 substantive side quests** only if review shows the content is distinct and good. Some quests should be discovered through overheard rumors, unusual landmarks, books/inscriptions, or exploring an unmarked place. Not all quests need an explicit NPC quest giver.

### 8.3 Factions

Prefer 3–5 groups with consistent interests, relationships, and localized influence. Represent reputation compactly: a few signed byte values or discrete relationship states. Reputation changes dialogue, prices, help, hostility, or access. Do not build a fully simulated diplomacy engine.

### 8.4 World bible, controlled by AI

Maintain `design/world_bible.md` containing lore, geography, faction motivations, important chronology, character identity, region motifs, and global story facts. Maintain a `quest_graph` and `entity_registry` as development assets. An independent editorial pass should check consistency across regions before code is accepted.

The agent must not invent contradictory geography or NPC biographies while adding new regions. When content changes, update the bible and impacted tests in the same change.

## 9. Exploration, combat, and character progression

### 9.1 Combat model

**Preferred initial implementation:** short turn-based encounters initiated by contact or direct interaction with visible enemies. A separate battle view (or compact overlay) handles one player and typically one to three opponents. The game should never require high-frequency collision or projectile simulation to work correctly.

Proposed actions: Attack, Guard, Use Item, Special/Magic, Run where allowed. Combat calculations use deterministic integer arithmetic plus a seeded PRNG. Keep status effects small and bounded. Hostile creatures may move on the overworld slowly, but complex chase AI is not required.

### 9.2 Character development

Start with a compact system: HP, energy/magic (optional), attack, defense, speed (optional), and a small set of status conditions. Use equipment slots such as weapon, armor, and accessory. Add more attributes only when they meaningfully affect decisions. Equipment and consumables should have clear effects and avoid a large inventory sorting burden on a six-button device.

### 9.3 Encounter and difficulty design

- First encounters teach systems and must be forgiving.
- Dangerous areas communicate risk through NPC warnings, signage, visuals, and enemy appearance.
- Difficulty should reward better equipment and understanding rather than unavoidable random grind.
- Bosses may use bespoke state machines; routine enemies should share compact behavior code.
- Player defeat should have an explicit, tested consequence (recommended: reload last save or return to a safe location with a small penalty). No irrevocable permadeath by default.

### 9.4 Economy and shops

Keep the economy deterministic and simple: a single currency, compact item database, understandable purchase/sell behavior, and region/faction discounts only where narratively useful. Do not store individual merchant stock inventories persistently unless quests require them; derive availability from static lists and flags.

## 10. Content authoring pipeline (offline; no runtime script VM)

1. **Invent:** agent develops setting, region design, quest and character outline, map sketches, level structure, dialogue, and art motifs.
2. **Structure:** write editable source assets such as JSON/TOML/CSV, ASCII tilemaps, dialogue Markdown, and C++ quest modules. These are **development assets**, not a scripting language executed on the AVR.
3. **Validate source content:** check unique IDs, map bounds, collision/accessibility, entrance pairs, referenced dialogue IDs, quest prerequisites, spelling, and basic narrative consistency.
4. **Compile assets:** offline generator emits compact static tables, packed maps, bitmap sprites, string banks, and stable IDs in C/C++ headers or other AVM-compatible read-only resources.
5. **Build game:** compile native C/C++ systems, per-quest handlers, and generated assets to the AVM guest target.
6. **Test:** run native logic tests, state-machine quest tests, map checks, full-playthrough fixtures, AVM integration tests, and emulator performance checks.
7. **Review/revise:** a critic agent identifies repetition, barren places, weak character voices, and confusing quest clues; implement revisions before declaring region complete.

The authoring pipeline should preserve human-readable, version-controlled source files; generated headers should be reproducible and not hand-edited.

## 11. AVM memory, CPU, and storage architecture

### 11.1 Proposed subsystem layout

- **Guest C/C++ core:** movement, collision, NPC state, quest handlers, dialogue state, inventory, battle rules, save serialization.
- **Read-only content:** overworld maps, interior maps, strings, sprites, encounter tables, NPC definitions, item definitions, authored event/quest code.
- **Host-facing services:** pixels/tiles/sprites, button input, elapsed ticks, optional audio, save/load, and flash access, as confirmed by AVM implementation.
- **Native test executable:** same pure game-rules code with stubbed host services for deterministic test runs.

Avoid heap allocation, exceptions, RTTI, large standard-library containers, recursion, and deep call chains unless measured to fit comfortably.

### 11.2 Provisional 1,024-byte guest static RAM budget

| Category | Bytes | Notes |
|---|---:|---|
| Player stats and status | 48 | Position-independent character state |
| Persistent world flags | 96 | Up to 768 bits if purely bit-packed |
| Quest phases and faction states | 64 | Sparse or compact encoding as appropriate |
| Inventory and equipment | 96 | Fixed capacity |
| Current world position, area and timers | 16 | Area ID, coordinates, RNG/ticks |
| Active NPC/enemy entities | 144 | Example: 12 entities × 12 bytes |
| Current tile/visibility cache | 128 | Optional; benchmark direct reads |
| Local dynamic overrides | 48 | Changes to interactive local tiles/objects |
| Dialogue and menu state | 64 | IDs/cursor/page indices, not text copies |
| Combat working state | 64 | Encounter-local, possibly overlaid with other state |
| Shared scratch | 128 | Reused; never stack-allocated |
| **Target subtotal** | **896** | **128 bytes remain for compiler/runtime needs** |

This is a planning allocation, **not a measured binary**. The final compiler may add globals, guards, libc runtime state, or alignment padding. Measure the actual linked guest image before every release. If save size exceeds host persistence capacity, compress/bit-pack the durable state or provide a supported external-save arrangement; do not silently drop quest history.

### 11.3 Guest stack and execution

- **Hard guest stack limit:** 256 bytes. Aim for **≤160–192 bytes measured peak** under representative worst-case tests, leaving margin.
- Require call-depth review including compiler spills, interrupt/host-call ABI effects where applicable, and indirect dispatch.
- No recursive pathfinding or dialogue evaluation; use fixed arrays and iterative traversals.
- NPC updates are budgeted/coarsened independently from rendering; avoid recomputing offscreen AI.
- Benchmark worst-case market square with all active NPCs, text paging, collision checks, battle transitions, area loads, and save serialization.
- Prefer dirty redraws, tile caches, or host drawing calls **only when profiling proves value**.

### 11.4 Storage and persistence

The initial integration study must establish: guest executable and asset size ceilings; how the guest addresses larger read-only data; random-access cost; save location and capacity; reset behavior; and compatibility with the current sound subsystem. Do not assume ordinary SRAM, EEPROM, or FX-flash semantics without checking AVM code.

A durable save must include a version/header/checksum, stable player position, stats, inventory, world flags, quest phases, and relationship state. Loading invalid or incompatible data must fail safely. If multiple save slots are not feasible, provide one dependable save slot with safe overwrite behavior. Prefer migration support when IDs or layouts change; otherwise clearly mark a development build as save-breaking.

## 12. Test strategy and release gates

### 12.1 Separate logic from display

The same state-transition functions should compile into a host-native test target and the AVM guest. All game outcomes must be driven by explicit inputs and a controlled random seed. A test should be able to specify starting state, execute actions, and compare resulting canonical state without rendering a single pixel.

### 12.2 Automated test categories

| Test class | Examples |
|---|---|
| Build/static | Guest link succeeds; static RAM fits; no forbidden allocations; stable asset IDs |
| Map structure | Valid tiles, entrances, return paths, reachable exits, object coordinates, no accidental impassable spawn |
| NPC dynamics | Legal movement, bounded wandering, no permanently blocked critical path, quest NPC remains findable |
| Dialogue | Every text/choice ID resolves; long strings paginate; choices return correctly |
| Quest logic | Prerequisites, each authored branch, item consumption, flags, rewards, persistent consequences |
| Story graph | Main quest completable through supported choices; side quests may be ignored; no required unique item softlocks |
| Economy/inventory | Capacity edges, equip/unequip, shop cost, currency underflow/overflow, quest-item safety |
| Combat | Win/loss/run, statuses, deterministic seeded outcomes, boss-specific transitions |
| Persistence | Save/load equivalence, invalid data handling, mid-quest resume, version changes |
| Integration | All six inputs, actual dialogue UI, map transitions, rendering, audio if implemented |
| Performance | Worst-case active NPC frame, display workload, storage access, guest stack high-water |
| Editorial | Distinct locations, no contradictory facts, meaningful quest variety, readable clues |

### 12.3 Quest scenario fixtures

For every substantial quest, author at least one happy-path scenario plus tests of all consequential decision branches. Include edge cases: quest ignored, destination visited before acceptance, NPC relocated, unique item lost or unavailable, entering/exiting during a quest, saving halfway through, and completing another quest first.

Development-only quest manifests may describe prerequisites, outcomes, and test coverage, but they **do not run on the device** and do not replace the native C++ quest functions.

### 12.4 End-to-end playthroughs

Automated scripted playthroughs should cover: main quest with minimal side content; a side-quest-heavy playthrough; two major story/faction branches; exploration out of intended order; a death/reload sequence; and late-game reentry into early towns. Use concrete state assertions rather than only checking the final ending flag.

### 12.5 Human acceptance

Deliver reproducible builds and concise change reports for code review and playtesting. Human reports such as “town feels empty” or “quest clues are confusing” are creative/UX bugs for agents to solve without requiring the human to write the replacement content. Automated tests do not substitute for playability and editorial review.

## 13. Multi-agent development model

Agents may work sequentially or in parallel as tooling permits, but all tasks operate against shared canonical design and registry files. Suggested roles:

- **Technical lead:** AVM ABI, limits, build/test framework, interfaces and integration.
- **Creative director:** coherent original vision, narrative themes, world bible, editorial standards.
- **World/level author:** geography, maps, collision, entrances, environmental details and secrets.
- **Narrative/quest author:** NPCs, dialogue, quest logic in C++, alternative resolutions and journal strings.
- **Systems programmer:** movement, UI, NPC simulation, combat, save, tools, asset packing.
- **Independent QA agent:** state-based fixtures, path validation, contradictions, resource measurement, exploratory failure cases.

No agent may alter the public state schema, stable content IDs, save layout, or core engine interfaces without a reviewed migration note and regression tests. Region batches should be separate pull requests/commits, with reproducible generated artifacts.

### 13.1 Definition of done for every content batch

A region is complete only when: its map and interiors are connected and reachable; named/ambient NPCs behave; quests have authored dialogue and tested resolutions; rewards and consequences persist; the journal provides enough clues; a build remains within budgets; all global regression tests pass; a reviewer can play the content end-to-end; and the world bible reflects the region.

## 14. Milestone plan and stopping points

### M0 — AVM reconnaissance / contract

Inspect actual repository and build pipeline; inventory drawing/input/audio/save APIs; compile a minimal guest; establish RAM, stack, CPU, asset access, emulator instrumentation, and native-test strategy. **Deliverable:** `docs/avm_contract.md`, proof-of-concept build, measured baseline. **Exit:** no unresolved assumption about the basic target interface.

### M1 — Playable technical core

Implement player movement, tile collision, viewport, entrances, active NPC spawn/wander, interaction, menu, text paging, persistence prototype, and native tests. Use temporary maps. **Exit:** can wander, talk, transition areas, save, reload, and run at target speed on the AVM runtime.

### M2 — AI-authored world charter

Agent invents original setting, regions, factions, central mystery, main-quest arc, character roster, and visual motifs. Establish stable identifiers and region-level maps. **Exit:** world bible, quest graph, region inventory, editorial consistency review. Creative content is not sent to the human for mandatory preapproval.

### M3 — Complete vertical slice

One town, wilderness, one dungeon, ambient and important NPCs, battle and inventory, shops/loot, main hook, and 3–5 distinctive side quests. **Exit:** one enjoyable hour-scale area of play, all cases tested, first human playtest checkpoint.

### M4 — Content production pipeline

Make map/text/asset generators robust; add lint, reachability, reference validation, deterministic fixtures, content reviews, and repeatable CI. **Exit:** agents can deliver a region without manually patching generated code or invalidating old saves.

### M5 — Main quest end-to-end skeleton

Implement all central story stages and an ending using placeholder-level side-region detail as necessary. **Exit:** the main quest is fully playable from beginning to end; no compulsory side quests; at least two major outcomes; tests verify full progression.

### M6 — Regional expansions

Add 2–3 regions per batch, with unique geography, NPCs, interiors, secrets, 4–8 side quests across the batch, and cross-region reactions. **Exit per batch:** full test suite passes; regression playthrough remains valid; human playtest checkpoints at meaningful boundaries.

### M7 — Content and systems polish

Rewrite weak quests and dialogue, diversify towns/dungeons, improve clue delivery and NPC ambient behaviors, tune economy and combat, add music/SFX if feasible, and refine endings. **Exit:** stable release candidate and measured resource margins across worst-case scenarios.

**Ordering note:** M4 tooling should begin during M1–M3; milestones identify gates, not strict sequential silos. M5 ensures the complete narrative exists before extensive side-content expansion, reducing the risk of a beautiful but unfinished world.

## 15. Risks and mitigations

| Risk | Mitigation |
|---|---|
| RAM/stack overrun | Static state budget, measured binary/link map, stack high-water tests, no unbounded allocation |
| AVR/VM CPU overload | Coarse NPC updates, local-only simulation, bounded entity counts, turn-based combat, per-frame profiling |
| World feels generic or empty | Authored landmarks, settlement rubrics, content density reviews, distinct regional themes |
| Main story never finished | Implement full critical path by M5, before broad side-content expansion |
| Quest softlocks | Stable flags, branch fixtures, reachability/choice tests, save/reload tests, multiple end-to-end routes |
| NPCs block paths / disappear | Roaming radius, occupancy checks, fallback spawn and accessibility assertions |
| Repetitive generated dialogue | Distinct character voices, contextual lines, independent editorial pass, repetition checks |
| Save compatibility broken by new content | Frozen IDs, versioned serialization, migration review and fixture saves |
| AI team invents conflicting canon | Shared world bible, ID registry, dependency graph, editorial merge gate |
| Unverified AVM capabilities | M0 hardware/software contract and working instrumentation before core architecture freezes |
| Game too ambitious | Vertical slice gate, main-story-first skeleton, expansion conditional on density/quality |

## 16. Iteration workflow with the human

The agent should deliver periodic **playable builds**, not only design prose. Each delivery includes: build artifact, revision/commit, concise change summary, noteworthy places/quests to test, test results, exact measured memory/stack/performance where possible, save compatibility status, and known limitations.

The human can give high-level reactions (“quests feel repetitive,” “more people should wander through the marketplace,” “this dungeon is confusing”), report bugs, and review code. The agent chooses the design solution, updates code/content, adds regression tests, and explains the change. Unless explicitly requested, it should not ask the human to design maps, name characters, write dialogue, or choose story branches for the authoring process.

### 16.1 Source-of-truth documents

- `docs/game_design.md` — this specification, updated through review.
- `design/world_bible.md` — AI-owned setting and canon.
- `design/region_index.md` — geography, areas, entrances, completion status.
- `design/quest_graph.md` — quest dependencies, branching outcomes, critical-path proof.
- `design/entity_registry.*` — stable IDs for quests, NPCs, items, areas, texts, flags.
- `docs/avm_contract.md` — verified technical API/budget details from M0.
- `docs/decisions.md` — architecture/design decision log with rationale.
- `tests/` — native and AVM integration tests, saved-state fixtures, playthrough traces.

### 16.2 Proposed repository outline

```text
rpg/
  docs/                 design specification, AVM contract, decisions
  design/               world bible, quest graph, region/NPC registers
  assets_src/           editable maps, text, sprites, audio source
  tools/                packers, map validators, content linters
  src/core/             pure game state, movement, inventory, combat
  src/world/            area loading, NPC spawning and movement
  src/quests/           bespoke compiled quest C++ modules
  src/ui/               renderer-facing UI, journal, dialogue, menus
  src/platform/         verified AVM host interface and native stubs
  generated/            reproducibly generated C/C++ read-only assets
  tests/unit/           rules, quest branches, save/load
  tests/integration/    simulated input sequences and AVM tests
  tests/playthroughs/   complete game traces across story branches
```

The final layout should conform to the existing AVM toolchain; this tree is a proposed organization, not a demand to refactor AVM itself.

## 17. Acceptance criteria for the eventual game

**Functional:** A player can start a new game, freely visit multiple regions, enter and exit towns and dungeons, encounter walking ambient NPCs, talk to named NPCs, acquire and finish optional quests, track goals, fight encounters, acquire/equip items, save/load, and complete the main quest. At least two meaningful major resolutions have different durable world or ending consequences.

**Creative:** Settlements have identifiable cultures and conflicts; main quest coheres; side quests show varied motivations and actions; notable decisions produce visible reactions; exploration yields unmarked rewards; world feels populated rather than simply decorated.

**Technical:** Builds reproducibly for the current AVM target; linked static RAM ≤1,024 bytes; guest stack peak ≤256 bytes with safety margin target; all assets accessible; no known critical softlock; all essential functionality usable with D-pad/A/B; runtime remains responsive on actual target conditions; persistence is reliable; critical branch and full-playthrough tests pass.

**Process:** All creative game content is AI-authored, committed as maintainable source, and covered by relevant automated tests. Human can review and playtest without being required to supply world designs.

## 18. Open design questions and revision queue

These questions are intentionally left open for testing or agent-driven design. **They are not requests for the human to design game content.**

| ID | Question | Proposed default | How to decide |
|---|---|---|---|
| O-01 | Turn-based vs light real-time combat? | Turn-based | AVM speed and gameplay prototype |
| O-02 | Pixel scrolling vs tile stepping? | Tile stepping initially | Profile display/input response |
| O-03 | How much overworld state can persist? | Compact flags and limited object overrides | Confirm save capacity in M0 |
| O-04 | How many active NPCs? | 12 | Worst-case profiling in a busy town |
| O-05 | How do visible enemies initiate combat? | Contact or interaction, with escape options | Vertical-slice playtest |
| O-06 | How does defeat work? | Safe restart from last save | UX and persistence reliability |
| O-07 | Is there a minimap? | World map in menu; no always-on minimap | Screen readability tests |
| O-08 | How much dialog is visible per page? | 2–3 short lines | Font and on-device legibility |
| O-09 | Is music part of first release? | Optional after core engine | Verify native host audio services |
| O-10 | How large is the final world? | Quality-driven, 128×128 as planning example | Region density and performance |
| O-11 | Any companion system? | Defer | RAM and content complexity |
| O-12 | Are essential NPCs killable? | No, at least in initial release | Softlock risk and narrative value |
| O-13 | Automatic vs manual saving? | Manual at safe points, optional autosave at area changes | Verified save API and latency |

## 19. Immediate next agent assignment

**Goal:** Complete M0 and initiate M1, without designing detailed quest content yet.

1. Inspect the actual AVM repository and document callable host services, guest build/link workflow, read-only data handling, guest RAM/stack accounting, audio hooks, and persistence.
2. Build and execute a minimal guest that reads D-pad/A/B, draws a tile and sprite, and transitions between two static areas.
3. Set up native/guest shared game logic and deterministic unit tests for movement and collision.
4. Prototype an active-area NPC that walks, waits, avoids walls, and can be talked to.
5. Measure static RAM, guest stack high-water where available, host CPU use, and the cost of tile drawing/data loads.
6. Deliver code, tests, executable artifacts, `docs/avm_contract.md`, and recommended corrections to the provisional technical sections of this design.

**Success condition:** We possess a reproducible, measured playable technical foundation. Only then should the creative agent scale up world authoring.

---

### Revision history

| Version | Date | Change |
|---|---|---|
| 0.1 | 2026-10-14 | Initial complete AI-authored AVM open-world RPG design, native quest architecture, wandering NPCs, resource budgets, phased development, testing and review workflow |
