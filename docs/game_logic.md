# Venlor — Game Logic Canon

**Save format:** `SAVE_VERSION = 22`  
**Implemented quests:** The Lantern Keeper and The Missing Surveyor.

## 0. Purpose and authority

This document is the canonical specification for Venlor’s concrete game rules,
content, persistent state, and dependencies. It contains no dialogue prose.

- **Update this file in the same change as any modification** to a quest, NPC, item, perk, special, encounter, reward, progression formula, permanent world interaction, or persistent state contract. Add entries before coding new content where practical.
- **Do not duplicate or fork these definitions** in new design notes. Other documents may discuss worldbuilding (`docs/world_bible.md`), presentation and density (`docs/content_authoring.md`), mechanics architecture (`docs/quests.md`), and map authoring (`assets/maps/AUTHORING*.md`); this file owns the logical inventories, prerequisites, effects, dependencies, and rewards.
- **Stable identities:** preserve already-shipped IDs, and audit save compatibility whenever the layout, enum values, NPC identities, enemy catalog order, or map coordinates change. Do not derive identity from source-file order except where the current enemy catalog explicitly does so.
- **Content style:** use identifiers, symbolic positions, gameplay conditions, effects and outcomes. Keep dialogue text, narrative descriptions, tile-art instructions and speculative story ideas out of this document.

### Entry conventions

- `F` / `!F` means a world flag is set / clear; `A && B` means both conditions are true.
- **Start** is the first event that makes a quest active. **Completion condition** is the actual status predicate, which may also be satisfied through free exploration before accepting a quest.
- **Reward** means an actual state change (gold, XP, items, access). A chest in the same area is **not** a quest completion reward unless quest code gates or grants it.
- **Position** means a symbolic `positions::Name` generated from Tiled's Logic layer; do not duplicate hardcoded coordinates in logic.

## 1. World, state and identity registry

### 1.1 Areas and passages

| Area | ID | Tileset | Implemented use |
|---|---:|---|---|
| `AREA_OUTDOOR` | 1 | World | Millcross village, surrounding paths, wolf and giant spiders, mine/vault entrances, maintenance hatch |
| `AREA_INTERIORS` | 2 | House | Ten linked house/service interiors; Mara and Ada reside here |
| `AREA_CAVE` | 3 | Cave | Hollow Mine: mechanisms, pursuit wolf, giant bat, tally, chest, second exit |
| `AREA_DUNGEON` | 4 | Dungeon | Old Vault: multi-mechanism puzzle, nonrespawning guardian, skeleton and floating eye, chest |

House doors/returns are paired by `handler_impl::HOUSE_LINKS` in `src/logic/area_village.h`: `Elder`, `Cottage`, `Weaver`, `Shop`, `Bakery`, `Farm`, `Inn`, `Smithy`, `Hut`, `Fisher` (`<Name>Door` ↔ `<Name>Exit`). Facing/pressing A enters; stepping onto the exit returns outside. `CaveMouth` ↔ `CaveExit`, `CaveHole` ↔ `CaveLadder`, and `MineEntrance` ↔ `DungeonExit` are passage pairs. The outdoor `CaveHole` is **inactive until `FLAG_CAVE_BRIDGE`**, when its tile becomes a ladder and its passage handler activates.

### 1.2 NPC identities and roles

NPC identities persist independently of the eight-slot live NPC/enemy pool.
`NPC_NONE = 0` identifies Alden, the traveler.

| `NPC_ID` | Value | Position policy / role in the two quests |
|---|---:|---|
| `NPC_TESSA` | 1 | Orchard keeper at `TessaWell`; reacts to village lantern restoration |
| `NPC_ORRIN` | 2 | Former sluice tender on the lane east of the inn; gives optional information about old lantern works and the mine; no required quest-completion trigger |
| `NPC_ROWAN` | 3 | Optional archer recruit; Step/Shoot; gives navigation and mine-safety hints; otherwise unchanged |
| `NPC_MARA` | 4 | Lantern Inn keeper and primary Lantern Keeper entry point; free rest service; reacts to outcome |
| `NPC_BRAM` | 5 | Smith at `BramYard`. Walks to `BramLantern` after arranging the repair meeting and waits there until restoration. |
| `NPC_NELL` | 6 | Mine watcher; primary surveyor lead, trail clues; recognizes surveyor after rescue; optional lantern clue |
| `NPC_ADA` | 7 | Supplies merchant; optional lantern/surveyor context, no mandatory quest gate |
| `NPC_SURVEYOR` | 8 | Missing surveyor; trapped in Hollow Mine until rescued, then permanently available at a safe village location near the inn; offers post-rescue mine knowledge |

`NPC_ID_LIMIT = 9`. Enemy identities start at 9. The surveyor’s location policy
uses `SurveyorTrapped` in Hollow Mine by default and selects `SurveyorReturned`
near the Lantern Inn when `FLAG_SURVEYOR_RESCUED` is set.

### 1.3 Quest IDs

| `QUEST_ID` | Numeric value | Quest definition | Completion predicate |
|---|---:|---|---|
| `QUEST_NONE` | 0 | Sentinel | — |
| `QUEST_LANTERN_KEEPER` | 1 | `src/quests/lantern_keeper.h` | `FLAG_LANTERN_RESTORED` |
| `QUEST_MISSING_SURVEYOR` | 2 | `src/quests/missing_surveyor.h` | `FLAG_SURVEYOR_RESCUED` |

`QUEST_ID_LIMIT = 3`. The Journal lists started and completed quests from this
registry. Status, tracking and map markers use the compiled quest framework.

### 1.4 Persistent flag ledger

The physical mechanisms retain their existing flags. All indices refer to save format 22.

| Bit | Flag | Set by | Read by / invariant |
|---:|---|---|---|
| 0 | `FLAG_RESERVED` | Never | Sentinel |
| 1 | `FLAG_CAVE_GATE` | Reversible `CaveLever` | East gate state and lantern relay access |
| 2 | `FLAG_CAVE_NOOK` | Reversible `CaveVaultLever` | West inspection gate |
| 3 | `FLAG_CAVE_BRIDGE` | Latching `CavePlate` | Permanent bridge and hatch; Surveyor rescue prerequisite |
| 4 | `FLAG_CAVE_CHEST` | Successful `CaveChest` claim | One-time chest |
| 5 | `FLAG_DUNGEON_SPIKES` | Reversible dungeon lever | Spike state |
| 6 | `FLAG_DUNGEON_CELL` | Dungeon cell door | Permanently open cell |
| 7 | `FLAG_DUNGEON_GATE` | Dungeon plate | Permanently open gate |
| 8 | `FLAG_DUNGEON_BLOCKS` | Dungeon block lever | Block state |
| 9 | `FLAG_DUNGEON_ORB` | Dungeon orb | Permanent orb, runes, vault gate |
| 10 | `FLAG_DUNGEON_CHEST` | Successful `DungeonChest` claim | One-time chest |
| 11 | `FLAG_LANTERN_STARTED` | Mara quest acceptance **or** first investigation of village lantern | Active Lantern Keeper if unfinished |
| 12 | `FLAG_LANTERN_DIAGNOSED` | Bram's mechanical inspection | Required for either repair outcome; obtainable before formal acceptance |
| 13 | `FLAG_LANTERN_RECORD_FOUND` | Inspect the old workshop record at Bram’s table | Required for either repair outcome; identifies possible ancient relay |
| 14 | `FLAG_LANTERN_RELAY_FOUND` | Inspect/secure old relay in the mine | Required for ancient restoration only; may be discovered early |
| 15 | `FLAG_LANTERN_RESTORED` | Final repair choice at village lantern with Bram's help | Quest completion, lighting/world reaction, XP + gold award exactly once |
| 16 | `FLAG_LANTERN_MODERN` | Select conventional replacement | Mutually exclusive outcome tag: set **only** with `FLAG_LANTERN_RESTORED`; clear at completion means ancient restoration |
| 17 | `FLAG_SURVEY_STARTED` | Read the inn notes, accept Nell’s request, or speak to the trapped surveyor | Active Missing Surveyor if not rescued |
| 18 | `FLAG_SURVEY_NOTES_READ` | Inspect surveyor notes | Optional route hints; not a hard rescue gate |
| 19 | `FLAG_SURVEY_TRAIL_FOUND` | Trace survey markers / find overlooked entrance clue | Optional alternative navigation clue; not a hard rescue gate |
| 20 | `FLAG_SURVEYOR_FOUND` | First talk to trapped surveyor | Requires the search to have started by then; persists across reload |
| 21 | `FLAG_SURVEYOR_RESCUED` | Confirm help to surveyor **after `FLAG_CAVE_BRIDGE`**, with safe return available | Completion, NPC relocation, XP + gold once; bridge/hatch are already permanently opened |
| 22–25 | `FLAG_ITEM_FIRST + ITEM_ID` | Equipment acquisition | Four existing equipment ownership bits |
| 26–41 | `FLAG_ENEMY_FIRST + spawn_index` | Victory against authored map enemy | 16 defeat slots; only authored respawnable bits reset |
| 42 | `FLAG_BRAM_LANTERN_MEETING` | Finish Bram's invitation after diagnosis and workshop evidence | Assigns Bram to `BramLantern` until either repair succeeds |

`FLAG_ITEM_FIRST = 22`, `FLAG_ENEMY_FIRST = 26`, and `FLAG_COUNT = 43`.
The flag set occupies six bytes. Section 6.3 records ownership of the encounter bits.

**Reward atomicity:** completion flags and reward grants must be guarded by a single successful state transition. For any optional supply payout, check stack capacity before committing or use an independently claimed reward flag; the two specified quests avoid this issue by awarding only XP and gold directly. `FLAG_LANTERN_MODERN` is set atomically with restoration for the modern branch; other branch is `RESTORED && !MODERN`. Quest completion must never clear encounter flags or force a new area load.

### 1.5 Other saved fields and transient-only fields

- **Saved `Game`:** save version, player actor and position, RNG seed, gold (`uint16_t`, max 65,535), world tick counter, world flags, World Remembers timer, current area, level field, party (identities/classes/current HP/AP/member level and XP/perk ranks/equipment references), supplies, area dimensions, previous tile, interaction readiness, NPC pool, audio settings, and `selected_quest`.
- **Rebuilt/transient:** tile cache/camera/workspaces, active NPC reconstruction on area load, dialogue panels/menu navigation, pending quest flag evaluations, pending level-up notices, combat search and battle scene state, and pending encounter contacts. Save restoration validates IDs, party data, flags, positions, and quest invariants; it reconstructs the view and NPC state.
- **Quest tracking:** only one **active** quest may be selected. Starting a quest automatically selects it if none is selected. A completed/inactive quest is automatically untracked. All started or completed quests appear in the Journal; selection does not affect quest progression.
- **Save compatibility.** Version 22 stores the number of ranks learned on each track. Existing
  saves retain their class identifiers, learned rank counts, and unspent choices. A learned rank
  uses its current effect below. A version 22 member at Step rank 1 can load with up to 3 HP above
  the current maximum from the former Steadfast perk. Loading clamps that HP to the current maximum.
  Earlier development save versions are rejected.

## 2. Implemented quests

Both shipped quests can be explored out of order and completed independently. The quest engine derives status from saved facts.

### 2.1 `QUEST_LANTERN_KEEPER` — The Lantern Keeper

**Type:** settlement investigation → records → short mine exploration → irreversible, visible repair choice.  
**Owning quest:** `src/quests/lantern_keeper.h`.  
**Area-specific logic:** `src/logic/area_village.h` and `src/logic/area_cave.h`.  
**NPCs:** `NPC_MARA` (initial lead, post-outcome witness), `NPC_BRAM` (diagnoses and enables repair choice), `NPC_ORRIN` (optional history/clue), `NPC_NELL` (optional cave lead), `NPC_TESSA` (world reaction).  
**Map markers:** `VillageLantern` (south edge of Well Square), `BramLantern` (the waiting spot north of it), `WorkshopRecord` (table inside the smithy), and `MineLanternRelay` (lamp bracket in the east winch room behind the east gate).

#### Start, stages and event contracts

| State or transition | Requirements | Action / result |
|---|---|---|
| **Not started** | `!FLAG_LANTERN_STARTED && !FLAG_LANTERN_RESTORED` | Journal excludes quest; ordinary lore NPC topics remain usable |
| **Start A** | Talk to Mara and accept investigation | `quest_start(QUEST_LANTERN_KEEPER)` sets `FLAG_LANTERN_STARTED` |
| **Start B** | Investigate `VillageLantern` before talking to Mara | Set `FLAG_LANTERN_STARTED` via quest start; must not require Mara conversation |
| **Active** | `FLAG_LANTERN_STARTED && !FLAG_LANTERN_RESTORED` | Goal derived from evidence facts; no manually stored current stage |
| **Mechanical diagnosis** | Speak to Bram on the lantern topic, whether or not the quest is active | Set `FLAG_LANTERN_DIAGNOSED` on successful completion of investigation; optional account from Orrin never required |
| **Workshop evidence** | Inspect `WorkshopRecord`, whether or not quest is active | Set `FLAG_LANTERN_RECORD_FOUND`; identifies mine relay as possible ancient source |
| **Ancient branch evidence** | Find and inspect `MineLanternRelay` | Set `FLAG_LANTERN_RELAY_FOUND`; accessed through the east gate controlled by `FLAG_CAVE_GATE` |
| **Arrange meeting** | After diagnosis and the workshop record, finish Bram's lantern invitation | Set `FLAG_BRAM_LANTERN_MEETING`. Bram walks toward the lantern and waits beside it. |
| **Modern repair** | `FLAG_LANTERN_DIAGNOSED && FLAG_LANTERN_RECORD_FOUND && !FLAG_LANTERN_RESTORED` | Choose **conventional replacement** at `VillageLantern`, with Bram's assistance; set `FLAG_LANTERN_MODERN` and `FLAG_LANTERN_RESTORED` atomically |
| **Ancient repair** | Modern prerequisites **plus** `FLAG_LANTERN_RELAY_FOUND` | Choose **ancient restoration** at `VillageLantern`, with Bram's assistance; set `FLAG_LANTERN_RESTORED`, leave `FLAG_LANTERN_MODERN` clear |
| **Completed** | `FLAG_LANTERN_RESTORED` | Status completed; choice is immutable and never reoffered |

**Choice presentation:** At the lantern, Bram offers conventional replacement
and, when the relay was recovered, ancient restoration. Confirm a repair with A,
or leave by canceling with B or selecting the back option. Bram’s assistance takes
place in dialogue at the lantern. Once the meeting is arranged, Bram walks while
visible and may finish relocating offscreen. Area entry and save restoration
rebuild him at the waiting spot. Either repair sends him back to his yard.
Later inspections describe the permanent repair.

**Objectives and markers:** Diagnose with Bram first for marker priority, then
inspect the workshop record. The outdoor marker points to the smithy door while
the interior query points to the record. After both facts, the outdoor marker
points to the lantern and the cave query points to the east gate lever or the
relay if it has yet to be found. The Journal gives the relay as an optional route.
Completion clears the marker.

**Reward:** **6 XP per current party member and 12 G**, once when restoration is
first set. Gold credits up to the wallet cap of 65,535 G. The lantern uses
distinct unlit, modern and ancient tiles. Bram, Mara, Tessa and Orrin respond
to the chosen repair. Orrin provides an optional historical clue after the
ancient restoration.

**Persistence and validation:**

- `RESTORED => STARTED && DIAGNOSED && RECORD_FOUND`.
- `RESTORED && !MODERN => RELAY_FOUND`; `MODERN => RESTORED`.
- The saved meeting requires diagnosis and the workshop record. Repair clears it. Earlier version 22 saves can arrange the meeting by speaking to Bram.
- Evidence may be found before quest start; avoid forcing a false state when that happens. If the player resolves the lantern through direct interaction while no quest is tracked, mark it started as part of the atomic completion path.
- Prevent repeat XP, G and branch switching; replay/restore must reconstruct the lantern's tile state from flags.
- No dependency on `FLAG_CAVE_BRIDGE`, `FLAG_SURVEYOR_RESCUED`, recruiting Rowan, or defeating either wolf.
- Validate choice cancellation, evidence in any order, both endings, save/reload, maximal gold, and post-resolution NPC interactions.

### 2.2 `QUEST_MISSING_SURVEYOR` — The Missing Surveyor

**Type:** navigation through scattered clues → mine mechanisms → find trapped NPC → rescue → permanent shortcut and relocation.  
**Owning quest:** `src/quests/missing_surveyor.h`.  
**Area-specific logic:** `src/logic/area_village.h`, `src/logic/area_cave.h`.  
**NPCs:** `NPC_NELL` (disappearance lead), `NPC_MARA` (inn note/information), `NPC_SURVEYOR` (trapped, then village resident), `NPC_ROWAN` (optional tactical advice), `NPC_ORRIN` (optional history).  
**Map markers:** `SurveyorNotes` (Lantern Inn table), `SurveyTrailMark` (north road), `SurveyorTrapped` (far landing), `SurveyorReturned` (outside near the inn). Existing markers `CaveMouth`, `CaveLever`, `CaveVaultLever`, `CavePlate`, `CaveBridgeRegion`, `CaveHole`, `CaveLadder` are reused.

#### Start, stages and event contracts

| State or transition | Requirements | Action / result |
|---|---|---|
| **Not started** | `!FLAG_SURVEY_STARTED && !FLAG_SURVEYOR_RESCUED` | Quest is absent from Journal; surveyor notes and mine remain discoverable |
| **Start A** | Read `SurveyorNotes` | The interaction starts `QUEST_MISSING_SURVEYOR` and sets `FLAG_SURVEY_NOTES_READ` |
| **Start B** | Ask Nell about the missing surveyor | `quest_start(QUEST_MISSING_SURVEYOR)` sets `FLAG_SURVEY_STARTED`; notes can be read later |
| **Optional trail** | Inspect `SurveyTrailMark` / locate secondary entrance hint | Set `FLAG_SURVEY_TRAIL_FOUND`; reveal directions; **not required** to reach/rescue surveyor |
| **Navigate mine** | Enter via `CaveMouth`, work through `CaveLever` and `CaveVaultLever` as necessary | Existing reversible `FLAG_CAVE_GATE` and `FLAG_CAVE_NOOK` control gates, not separate quest-stage flags |
| **Open crossing** | Step onto `CavePlate` | Latch `FLAG_CAVE_BRIDGE`, extend bridge and activate west-ridge maintenance hatch (`CaveHole`) permanently |
| **Find surveyor** | Speak with `NPC_SURVEYOR` at `SurveyorTrapped` | Set `FLAG_SURVEYOR_FOUND`; if this occurs before starting the quest, set `FLAG_SURVEY_STARTED` automatically |
| **Rescue** | `FLAG_SURVEYOR_FOUND && FLAG_CAVE_BRIDGE && !FLAG_SURVEYOR_RESCUED`; confirm assistance when talking to the surveyor | Set `FLAG_SURVEYOR_RESCUED` and award XP/gold once; actor retires at the end of the farewell and reappears at `SurveyorReturned` on the next appropriate area admission |
| **Completed** | `FLAG_SURVEYOR_RESCUED` | Status completed; progress remains in Journal; village surveyor offers informational topics |

**Rescue model:** The restored bridge and linked hatch provide the return route.
After the farewell, the surveyor leaves the scene and the policy reconstructs
them near the inn when that location is admitted.

**Objectives and markers:** Outdoors, the marker points to the mine mouth
until the bridge opens, then to the hatch. The inn notes can be marked in
interiors while neither optional clue is known and the bridge is down. In the
mine the marker follows the east lever, winch, plate and trapped surveyor
as relevant. The Journal prioritizes rescue confirmation once the surveyor has
been found. Completion clears the marker.

**Reward:** **8 XP per current party member and 15 G**, once on rescue. Gold
credits up to 65,535 G. The bridge and `CaveHole` ↔ `CaveLadder` shortcut
were unlocked by the plate and remain open. The chest is independently
claimable. Any wolf victory earns standard battle rewards.

**Persistence and validation:**

- `SURVEYOR_RESCUED => SURVEYOR_FOUND && SURVEY_STARTED && FLAG_CAVE_BRIDGE`.
- `SURVEYOR_FOUND => SURVEY_STARTED` and `SURVEY_NOTES_READ => SURVEY_STARTED`. The trail can be recorded before the search starts. Both clues are optional for rescue.
- The bridge/hatch state persists even when the quest has not started or the surveyor has not yet been found.
- Surveyor is in exactly one of the two specified policy sites (trapped if not rescued, returned if rescued), subject to actor-pool admission; never both.
- The rescue is available even if other mine enemies respawn under The World Remembers. No permanent enemy defeat is a prerequisite.
- No dependency on Lantern Keeper's start, chosen repair method, or completion.
- Test missing notes, early bridge opening, discovery before acceptance, repeat talk, save/load before and after rescue, cave-to-outdoor traversal, and forced actor admission/retirement.

### 2.3 Cross-quest dependency and effect matrix

| Fact/event | Lantern Keeper | Missing Surveyor | Other gameplay |
|---|---|---|---|
| `FLAG_LANTERN_DIAGNOSED`, `FLAG_LANTERN_RECORD_FOUND` | Both required for repair | — | NPCs respond with contextual topics |
| `FLAG_LANTERN_RELAY_FOUND` | Unlocks ancient choice | — | Can be discovered during unrelated mine exploration |
| `FLAG_LANTERN_RESTORED`, `FLAG_LANTERN_MODERN` | Completion and final method | — | Lantern visual state; village reactions |
| `FLAG_CAVE_GATE`, `FLAG_CAVE_NOOK` | The east gate controls access to the relay | Used while navigating | Reversible terrain; never quest completion by itself |
| `FLAG_CAVE_BRIDGE` | **Not required** | Needed to rescue; already permanently opens shortcut | Hatch active after bridge is opened |
| `FLAG_SURVEYOR_FOUND` | — | Contact milestone | Can be discovered without reading inn notes |
| `FLAG_SURVEYOR_RESCUED` | — | Completion | Surveyor returns to village; conversations change |
| Mine wolf victory | — | Optional hazard | Standard battle rewards; can respawn |
| `FLAG_CAVE_CHEST` | — | Optional nearby loot | One-time container reward; independent of quests |

**Intentional narrative overlap:** an NPC may mention both the mine's old mechanisms and the lantern relay. The returning surveyor can offer **optional** additional information about the relay; that information must not become a hidden prerequisite for Lantern Keeper. Both quests stand alone and admit either order of completion.

## 3. NPC conversations and interactions

Millcross has eight named NPC identities. Multi-topic conversations use `dialogue_options<...>()`. B cancels the menu before any selected topic can run.

| NPC | Relevant topics / actions | State mutations | Conditional reaction |
|---|---|---|---|
| Mara | Inn rest; lantern trouble; surveyor’s notes | Start `QUEST_LANTERN_KEEPER` on acceptance; existing rest service unchanged | Lantern modern/ancient branch; surveyor return |
| Bram | Examine lantern; old workshop; smithing; repair choices at the lantern | Set `FLAG_LANTERN_DIAGNOSED`; finalize eligible repair method with guard | Lantern completion and selected branch |
| Orrin | Workshop history; mine mechanisms; local observations | None required; **do not gate progression** | Lantern restoration; surveyor return |
| Nell | Surveyor disappearance; trail/entrance; cave observations | Start `QUEST_MISSING_SURVEYOR` on acceptance; trail topic gives directions | Surveyor rescue; bridge opening |
| Rowan | Join party; village directions; mine warning; old span | Recruits as existing Step/Shoot member | Bridge opening, lantern restoration, surveyor return |
| Tessa | Orchard; work; lantern | None required; retain as ordinary resident | Lantern status and repair choice |
| Ada | Buy supplies; village trade; mine | Existing shop service; no new gate | Lantern status and surveyor return |
| Surveyor | Rescue agreement when trapped; mine routes/historical findings after return | Set `FLAG_SURVEYOR_FOUND`; on confirmed rescue set `FLAG_SURVEYOR_RESCUED` | Different location and topic availability after rescue |

**Service locations unchanged:** the inn still offers free party rest, and the shop sells the same supplies. Quest clues must not disable either service.

The mine notice, chalk warning, and tally board are environmental hints.
They leave quest and reward state unchanged.

## 4. Items, inventory, services and rewards

### 4.1 Equipment item catalog

Ownership is one persistent bit per `ITEM_ID` (`FLAG_ITEM_FIRST + ID`). A party member's equipment slots store **item ID + 1** or zero for empty. One globally owned copy of an item may be equipped by at most one member at a time. Transferring it from another member unequips its previous wearer. Unavailable/unequipped items remain owned.

| `ITEM_ID` | Numeric ID | Slot | Implemented effect | Acquisition |
|---|---:|---|---|---|
| `ITEM_IRON_FANG` | 0 | `SLOT_WEAPON` | +1 direct normal and special attack damage; **does not** affect movement engagement damage | Owned on new game; starts unequipped |
| `ITEM_TRAVELERS_BOOTS` | 1 | `SLOT_ARMOR` | +2 maximum HP while equipped | `CaveChest` |
| `ITEM_DAWN_PENDANT` | 2 | `SLOT_RELIC` | +1 AP recovery per **victory**, while equipped (not +1 max AP) | `DungeonChest` |
| `ITEM_VITAL_PENDANT` | 3 | `SLOT_RELIC` | +3 maximum HP while equipped | `DungeonChest` |

`ITEM_DEFINED_COUNT = 4`; `ITEM_NONE = 0xff`. A member can wear one weapon, one armor item, and one relic. Because both pendants use `SLOT_RELIC`, they cannot be simultaneously equipped on the same member. Changing equipment clamps current HP/AP down to a reduced maximum; it does **not** heal or replenish those statistics. There is no implemented item selling or equipment purchasing service.

### 4.2 Consumable supplies and shop

A supply is an owned **stack quantity**, not an equipment flag. Each has a maximum stack size of **15**. Using an invalid supply, targeting an ineligible member, or attempting to purchase an uncarryable stack does not consume resources.

| `SUPPLY_ID` | Starts with | Effect when used outside combat | Eligible target / condition | Shop price |
|---|---:|---|---|---:|
| `SUPPLY_POTION` | 2 | Heal up to **8 HP** | One living, injured member | 12 G |
| `SUPPLY_ELIXIR` | 1 | Heal up to **4 HP** to each eligible member | Whole party; at least one living injured member; does not revive fallen members | 22 G |
| `SUPPLY_REVIVE` | 1 | Set a fallen member to up to **5 HP** | One member at 0 HP | 30 G |
| `SUPPLY_ETHER` | 1 | Restore up to **3 AP** | One member below max AP, including one at 0 HP | 18 G |

The shop sells these four supplies with unlimited stock at the fixed prices above. Each confirmed purchase consumes gold and adds **one** supply if enough money and capacity remain. No price changes, selling, stock depletion, merchant reputation, or inventory weight system is implemented. `party_rest` via the inn refills every member's derived max HP and max AP, including those at 0 HP, **free of charge**.

### 4.3 Chest rewards and payment semantics

| Container | Requirement | Successful payout | Persistence |
|---|---|---|---|
| `positions::CaveChest` | Access cave chest, `!FLAG_CAVE_CHEST`; enough room for **all** supplies; enough gold capacity; equipment not already owned | **25 G**, `ITEM_TRAVELERS_BOOTS`, **1 Potion**, **1 Ether** | Sets `FLAG_CAVE_CHEST` once; opened tile thereafter |
| `positions::DungeonChest` | Access vault chest, `!FLAG_DUNGEON_CHEST`; enough room for **all** supplies; enough gold capacity; equipment not already owned | **25 G**, `ITEM_DAWN_PENDANT`, `ITEM_VITAL_PENDANT`, **1 Elixir**, **1 Revive** | Sets `FLAG_DUNGEON_CHEST` once; opened tile thereafter |

`ContainerReward::claim()` is **all-or-nothing**. Failed capacity/ownership validation leaves supplies, gold, equipment and chest flag unchanged. Chests are authored at locations; the reward policy is **not** automatically a reward of any quest. The player can revisit an unclaimed chest after freeing space.

### 4.4 Rewards and replay

| Source | Effect | Status / replay |
|---|---|---|
| Any victorious battle | 2 XP to each roster member, 3 G per deployed enemy, eligible AP recovery | repeatable if enemy respawns |
| Lantern Keeper final repair | **6 XP/member + 12 G**, permanent village lantern repair and branch-specific reactions | Once, guarded by `FLAG_LANTERN_RESTORED` |
| Missing Surveyor rescue | **8 XP/member + 15 G**, surveyor moves to village | Once, guarded by `FLAG_SURVEYOR_RESCUED` |
| Mine bridge pressure plate | Permanent bridge/hatch access | Permanent mechanism used in Missing Surveyor |
| Cave and vault chests | Listed in §4.3 | independent of all quests |
| Inn rest | Free full party HP/AP | repeatable |
| Rowan's recruitment | New Step/Shoot level-1 member | once |

Both quest rewards cap gold at the wallet limit. Each XP payout belongs to the first confirmed transition to completion.

## 5. Character classes, progression and perks

### 5.1 Starting party and base progression

- New game: one traveler (`NPC_NONE`), `MOVEMENT_STEP`, `ATTACK_STRIKE`, **level 1**, **20/20 HP**, **4/4 AP**, no equipped items, **0 G**, Iron Fang owned but unequipped, and the supply counts above. Rowan is an optional recruit: `MOVEMENT_STEP` / `ATTACK_SHOOT`, **18/18 HP**, **4/4 AP**, level 1.
- Party capacity: **4**. Identity cannot be duplicated. New recruits begin at full derived HP/AP and level 1; recruiting never resets existing members' progress.
- Party member levels: **1–10**. XP to next level from current level `L`: `5 × (L + 1)` (e.g. 10 at level 1, 15 at level 2). XP is the amount **toward** the next level, not cumulative; overflow carries across level-ups, and XP at level 10 is discarded.
- Base maximum HP: **20**, except Rowan **18**. Add **+2 at level 3**, **+2 more at level 7** and equipped HP bonuses. Leveling does **not** itself restore current HP.
- Normal/special **direct attack base damage** gains **+1 at level 5** and **+1 more at level 9**. Engagement and secondary attack damage use their own base damage and perks.
- Max AP: **4** for all current members. No rank/item currently increases maximum AP.
- Victory AP recovery: **1 baseline**, plus any `ITEM_DAWN_PENDANT` bonus, capped at 4. AP is not automatically restored on turn changes or within combat.
- Perk choices: one at **levels 2, 4, 6, 8, and 10**, derived as `floor(level / 2) - movement_perk_rank - attack_perk_rank`. Each member has one movement perk track and one attack perk track. Ranks are learned in order and their effects stack. Each track has four learnable ranks. Spending the first four choices on one track unlocks its capstone at level 8. Unspent choices remain available until used.

### 5.2 Battle movement classes

| Movement class | Code | Base move geometry | Mechanical distinctions |
|---|---|---|---|
| `MOVEMENT_STEP` | 0 | Orthogonal, **1 cell** | Must pass through/land on empty nonblocking cell |
| `MOVEMENT_LEAP` | 1 | Knight leap: **2 cells on one axis + 1 perpendicular** | Can pass over units/obstacles, but landing cell must be free |
| `MOVEMENT_CHARGE` | 2 | Orthogonal, **1–2 cells** | Every traversed cell and the destination must be clear |
| `MOVEMENT_VAULT` | 3 | Orthogonal, **exactly 2 cells** | The landing must be open. The intermediate cell can hold terrain or a unit |
| `MOVEMENT_GLIDE` | 4 | Diagonal, **1–2 cells** | Both axes move the same distance. The diagonal path and landing must be open, regardless of the two orthogonal corner cells |

### 5.3 Battle attack classes (normal actions)

Each attack selects one living enemy. The table shows base attack geometry. Rank 4 targeting changes are listed under implemented perks. Orthogonal attacks follow a shared row or column, with
intervening living units and blocked terrain screening longer-range attacks. Reach's Through the
Ranks allows a range-2 hit through a living ally. Arc reaches only an adjacent diagonal. Normal attacks cost **0 AP** and take one team turn.

| Attack class | Code | Base geometry | Base primary damage | Extra effect |
|---|---:|---|---:|---|
| `ATTACK_STRIKE` | 0 | Orthogonal 1 | 3 | |
| `ATTACK_SHOOT` | 1 | Orthogonal 2–4 | 3 | |
| `ATTACK_REACH` | 2 | Orthogonal 1–2 | 3 | |
| `ATTACK_IMPACT` | 3 | Orthogonal 1 | 2 | Push a surviving target one open cell directly away |
| `ATTACK_REND` | 4 | Orthogonal 1 | 2, or 4 if target HP is below half its maximum | |
| `ATTACK_ARC` | 5 | Diagonal 1 on each axis | 3 | |
| `ATTACK_DRAIN` | 6 | Orthogonal 1 | 2 | Restore up to 1 HP to the attacker, including on a killing blow |
| `ATTACK_RECOIL` | 7 | Orthogonal 1 | 3 | Retreat one open cell directly away from the target after damage, even on a kill. If the retreat is blocked, remain in place |
| `ATTACK_FLANK` | 8 | Orthogonal 1 | 2, or 4 with allied support | Support requires another living ally of the attacker orthogonally adjacent to the target immediately before the hit |
| `ATTACK_BLAST` | 9 | Orthogonal 2–3 | 2 | Deal 1 splash damage to each other living enemy orthogonally adjacent to the primary target's original cell, even on a primary kill |

Apply level, weapon, and direct attack perk bonuses to the selected base primary damage. Enemy
presets receive none of those party bonuses. Secondary damage from Blast, Piercing Shot, and
Collision uses the primary attack's line of sight and hits each qualifying enemy once.
When the target survives, Drain healing is capped at one
less than the target's actual HP loss, with a floor of zero. A finishing blow allows full Drain
healing, even against a target at 1 HP. All Drain healing is capped at the attacker's maximum HP.

### 5.4 Special abilities

Each attack class grants a **2 AP starter at level 3** and a **3 AP signature at level 5**.
Both use one selected enemy eligible for the normal attack, including any learned targeting
perks. Hooking Thrust and Wall Slam also require the setup below. A on the target offers Attack
and any unlocked, affordable, legal specials. Below level 3 or with less than 2 AP, A attacks
normally. A special spends AP once and takes one team turn. Both unlocks use the member's saved
level. Enemy presets use normal attacks.

#### Starting specials

The starter's damage is normal direct damage including bonuses, plus the following bonus based
on whether the primary target is at full HP or below full HP.

| Special | Attack class / ID | AP | Bonus vs full HP | Bonus vs below full HP | Additional effect |
|---|---|---:|---:|---:|---|
| **Power Strike** | `ATTACK_STRIKE` | 2 | +3 | +3 | None |
| **Opening Shot** | `ATTACK_SHOOT` | 2 | +4 | +1 | None |
| **Driving Thrust** | `ATTACK_REACH` | 2 | +2 | +2 | Knockback surviving target one open cell |
| **Crushing Bash** | `ATTACK_IMPACT` | 2 | +3 | +3 | Knockback (also provided by normal Impact) |
| **Wound Hunter** | `ATTACK_REND` | 2 | +1 | +3 | None |
| **Crescent Cut** | `ATTACK_ARC` | 2 | +2 | +2 | Diagonal target |
| **Siphon** | `ATTACK_DRAIN` | 2 | +3 | +3 | Retains healing |
| **Spring Blow** | `ATTACK_RECOIL` | 2 | +3 | +3 | Retains retreat |
| **Pincer Strike** | `ATTACK_FLANK` | 2 | +2 | +2 | Retains conditional base damage |
| **Detonation** | `ATTACK_BLAST` | 2 | +2 | +2 | Retains splash at the normal amount |

#### Signature specials

Determine additional victims, damage, support, and open destinations from the board before the
hit. Signatures use their class's normal attack effects unless specified below. Full damage for
Split Shot, Wall Slam, Backsweep, and Implosion is the primary attack's normal pre-defense
damage, including its level, equipment, and perk bonuses. These extra hits are secondary damage.
Apply the whole attack before deciding victory or defeat.

| Signature | Attack class | Effect |
|---|---|---|
| **Breach Cut** | `ATTACK_STRIKE` | +2 primary damage. Occupy the target's original cell. If it survives, move it into the attacker's vacated cell. |
| **Split Shot** | `ATTACK_SHOOT` | Deal full damage to each enemy in the two lateral cells beside the selected target, perpendicular to the firing line. Piercing Shot and Throughshot still follow that line. |
| **Hooking Thrust** | `ATTACK_REACH` | Requires range 2. +2 primary damage. Pull the survivor into the middle cell. If Through the Ranks let the hit pass through a living ally, swap that ally and the target. |
| **Wall Slam** | `ATTACK_IMPACT` | Requires blocked terrain or the board edge immediately behind the target along the attack line. Deal full damage to each enemy in the two lateral cells beside the target. |
| **Feeding Frenzy** | `ATTACK_REND` | Each other enemy strictly below half maximum HP that the attacker can legally attack takes its own normal Rend damage. |
| **Whirlwind** | `ATTACK_ARC` | Every other diagonally adjacent enemy takes its own normal Arc damage. Corner Sweep applies at the selected target only. |
| **Life Channel** | `ATTACK_DRAIN` | +2 primary damage. Heal each other living party member for the actual HP the primary target lost, reduced by 1 if it survives. On a kill with Soul Dividend, each other living party member instead heals up to 5 HP. Healing caps at each recipient's maximum HP. |
| **Backsweep** | `ATTACK_RECOIL` | Each other enemy orthogonally adjacent to any cell entered during the retreat takes full damage once. Dragback still applies to the surviving primary target. |
| **Combined Arms** | `ATTACK_FLANK` | Each other living ally orthogonally adjacent to the target who can legally attack it adds their own normal primary attack damage, with level, gear, and perk bonuses, before target defenses are applied to the combined hit. |
| **Implosion** | `ATTACK_BLAST` | Enemies exactly two cells away along the target's row or column take full damage. Each survivor moves one cell toward the primary target if that cell was open before the hit. The usual local splash retains its damage and Concussion rules. |

### 5.5 Implemented perks

Learn perks through the Party menu. The implementation catalog is `src/perks.def`. Attack
perks apply to normal attacks and specials. Movement damage bonuses apply to
engagement after voluntary moves. All tracks have four learnable ranks. Rank 4 effects are
resolved from the current board and the original target or destination, using the usual turn and AP rules.

| Track | Rank | Perk ID | Effect |
|---|---:|---|---|
| `MOVEMENT_STEP` | 1 | `HOLD_TOGETHER` | Reduce incoming direct damage from a normal attack or special by 1, to a minimum of 1, while orthogonally adjacent to a living ally before the hit. |
| `MOVEMENT_STEP` | 2 | `FOOTWORK` | A Step that deals engagement damage costs 0 HP. Quiet Steps and passes cost 1 HP. |
| `MOVEMENT_STEP` | 3 | `SHIELD_WALL` | Hold Together reduces direct damage by 2 total while next to a living ally, to a minimum of 1. |
| `MOVEMENT_STEP` | 4 | `SHIELD_REBOUND` | When orthogonally adjacent to at least one living ally, reflect half the incoming direct damage rounded down to the attacker as secondary damage. Apply Shield Wall to the remainder. Reflected damage applies as part of the same hit. |
| `MOVEMENT_LEAP` | 1 | `LANDING_STRIKE` | +1 engagement damage to the enemy when the Leap lands orthogonally adjacent to exactly one living enemy. |
| `MOVEMENT_LEAP` | 2 | `WIDE_LANDING` | +1 engagement damage to each enemy when the Leap lands orthogonally adjacent to exactly two living enemies. |
| `MOVEMENT_LEAP` | 3 | `SKYFALL` | +1 engagement damage to every orthogonally adjacent enemy on landing. Total damage per enemy is 3 with one or two such enemies and 2 with three or more. |
| `MOVEMENT_LEAP` | 4 | `SKYHOOK` | If exactly one living enemy is orthogonally adjacent to the landing before engagement, pull it onto the vacated takeoff cell if it survives the hit. |
| `MOVEMENT_CHARGE` | 1 | `SPEARHEAD` | On a one- or two-cell Charge, deal +1 engagement damage to the living enemy one cell directly ahead of the landing, in the direction traveled. |
| `MOVEMENT_CHARGE` | 2 | `MOMENTUM` | +1 engagement damage to every adjacent enemy after a two-cell Charge. A forward enemy takes 3 total engagement damage from both ranks. |
| `MOVEMENT_CHARGE` | 3 | `OVERRUN` | After engagement on a one- or two-cell Charge, push the surviving enemy directly ahead one open cell farther in the direction of travel. |
| `MOVEMENT_CHARGE` | 4 | `TRAMPLE` | A two-cell Charge can cross an enemy in the middle cell if the landing is open. The crossed enemy stays in place and takes Spearhead in addition to its ordinary landing engagement. Overrun still applies to a different surviving enemy directly ahead of the landing. |
| `MOVEMENT_VAULT` | 1 | `VAULT_STRIKE` | +1 engagement damage to the living enemy in the jumped cell. |
| `MOVEMENT_VAULT` | 2 | `VAULT_FORCE` | +1 engagement damage to every enemy adjacent to the landing cell. The jumped enemy takes 3 total engagement damage when both perks apply. |
| `MOVEMENT_VAULT` | 3 | `SHOCK_LANDING` | Deal 1 engagement damage to each enemy diagonally adjacent to the landing. |
| `MOVEMENT_VAULT` | 4 | `HIGH_VAULT` | A blocked or occupied two-cell landing can be replaced by the open cell three steps away on the same line. Both crossed enemies take Vault Strike. A crossed enemy beside the landing also takes normal landing engagement. Each direction offers one landing at most. |
| `MOVEMENT_GLIDE` | 1 | `CUTBACK` | +1 engagement damage after a one-cell Glide that lands beside exactly one living enemy. |
| `MOVEMENT_GLIDE` | 2 | `SLIPSTREAM` | +1 engagement damage to every adjacent enemy after a two-cell Glide. |
| `MOVEMENT_GLIDE` | 3 | `PASSING_CUT` | Deal 1 engagement damage to enemies orthogonally beside the takeoff cell or midpoint. Each enemy takes damage once, using landing damage when eligible. Cutback counts only orthogonal landing neighbors. |
| `MOVEMENT_GLIDE` | 4 | `DIAMOND_WAKE` | For a two-cell Glide, Passing Cut also reaches enemies diagonally adjacent to the takeoff or midpoint. Each enemy eligible for Passing Cut or landing engagement takes full two-cell landing damage once. |
| `ATTACK_STRIKE` | 1 | `KEEN_EDGE` | +1 direct damage against an enemy at full HP before the hit. |
| `ATTACK_STRIKE` | 2 | `SWORD_MASTERY` | +1 direct damage on every hit. |
| `ATTACK_STRIKE` | 3 | `DUELIST` | +2 direct damage if the target has no living orthogonally adjacent ally. |
| `ATTACK_STRIKE` | 4 | `BLADE_LOCK` | An isolated enemy beside a rank 4 Strike fighter cannot move voluntarily. Isolation uses the same condition as Duelist. |
| `ATTACK_SHOOT` | 1 | `LONGSHOT` | +1 direct damage at range 4. |
| `ATTACK_SHOOT` | 2 | `SHARPSHOOTER` | +1 direct damage at every legal range. |
| `ATTACK_SHOOT` | 3 | `PIERCING_SHOT` | Deal 2 damage to an enemy in the cell immediately behind the primary target along the shot, including on a primary kill. |
| `ATTACK_SHOOT` | 4 | `THROUGHSHOT` | Shoot the first enemy in a row or column at range 2 or farther. The same shot deals full damage to every enemy behind it through any gaps until terrain, a living ally, or the board edge stops it. Evaluate range and HP bonuses separately for each target. |
| `ATTACK_REACH` | 1 | `LUNGE` | +1 direct damage at range 2. |
| `ATTACK_REACH` | 2 | `SHORT_GRIP` | +2 direct damage at range 1. |
| `ATTACK_REACH` | 3 | `THROUGH_THE_RANKS` | Range-2 hits can pass through a living ally in the middle cell. Blocking terrain and enemies still screen attacks. |
| `ATTACK_REACH` | 4 | `PIKE_FENCE` | An enemy outside this fighter's legal Reach cannot voluntarily move to a cell the fighter could attack after it lands. Evaluate the intended board with the mover's origin vacated. Any newly threatening Pike Fence bars the move. |
| `ATTACK_IMPACT` | 1 | `BODY_CHECK` | +1 direct damage if a living unit occupies the cell directly behind the target before the hit. A board edge or blocked terrain alone grants no bonus. |
| `ATTACK_IMPACT` | 2 | `HEAVY_BLOW` | +1 direct damage on every hit. |
| `ATTACK_IMPACT` | 3 | `COLLISION` | Deal 3 damage to an enemy in the push destination, including on a primary kill. The occupied cell blocks the push for that hit. |
| `ATTACK_IMPACT` | 4 | `WRECKING_LINE` | Every enemy in the contiguous line directly behind the primary target takes Collision damage. Count the consecutive open cells beyond the original line before damage, then push every survivor that many cells away together. |
| `ATTACK_REND` | 1 | `BLOODLETTING` | Raises the threshold for 4 base damage to strictly below three quarters of the target's maximum HP. |
| `ATTACK_REND` | 2 | `BLOOD_SENSE` | +1 direct damage against an enemy below full HP before the hit. |
| `ATTACK_REND` | 3 | `BLOOD_RUSH` | +2 direct damage while the attacker is at or below half their own maximum HP. |
| `ATTACK_REND` | 4 | `BLOOD_HUNT` | Rend and Wound Hunter can hit an enemy strictly below half its maximum HP from range 2 along a clear row or column. The attacker stays in place. |
| `ATTACK_ARC` | 1 | `OPEN_ANGLE` | +1 direct damage if the two orthogonal corner cells between attacker and target are both unblocked and free of living units. |
| `ATTACK_ARC` | 2 | `CROSSCUT` | +1 direct damage on every hit. |
| `ATTACK_ARC` | 3 | `INSIDE_ANGLE` | +2 direct damage if either orthogonal corner cell contains a living enemy. |
| `ATTACK_ARC` | 4 | `CORNER_SWEEP` | Any enemies in the two orthogonal corner cells take the primary hit's pre-mitigation damage as secondary damage. |
| `ATTACK_DRAIN` | 1 | `LIFE_TAP` | Raises healing from up to 1 HP to up to 2 HP. The surviving-target and maximum-HP caps still apply. Weapon and level damage bonuses can increase the surviving-target cap. |
| `ATTACK_DRAIN` | 2 | `DEEP_CUT` | +1 direct damage on every hit. |
| `ATTACK_DRAIN` | 3 | `DEEP_SIPHON` | Raises healing to up to 3 HP. The surviving-target and maximum-HP caps still apply. |
| `ATTACK_DRAIN` | 4 | `SOUL_DIVIDEND` | A finishing blow heals all living allies including the attacker for up to 5 HP each, capped at their own maximum HP. A surviving target uses the usual Drain healing. |
| `ATTACK_RECOIL` | 1 | `PARTING_BLOW` | +1 direct damage when the retreat cell is open and the attack completes the retreat, including on a killing blow. |
| `ATTACK_RECOIL` | 2 | `COUNTERFORCE` | +1 direct damage on every hit. |
| `ATTACK_RECOIL` | 3 | `SPRINGBACK` | Retreat up to two cells away from the target, stopping at the first obstruction. A retreat of either distance activates Parting Blow. |
| `ATTACK_RECOIL` | 4 | `DRAGBACK` | When the attacker retreats, the surviving target moves into the attacker's vacated cell. |
| `ATTACK_FLANK` | 1 | `ENCIRCLE` | +1 direct damage with allied support. |
| `ATTACK_FLANK` | 2 | `TACTICAL_EDGE` | +1 direct damage on every hit. |
| `ATTACK_FLANK` | 3 | `CONVERGING_BLADES` | +1 direct damage per other living ally orthogonally adjacent to the target. |
| `ATTACK_FLANK` | 4 | `BLIND_SIDE` | Flank can hit an enemy two cells away along a row or column, or one cell diagonally, if a living ally is orthogonally adjacent to both attacker and target. All living allies orthogonally adjacent to the target count for support as usual. |
| `ATTACK_BLAST` | 1 | `CLUSTER_BURST` | +1 primary damage if the attack hits at least two living splash targets. |
| `ATTACK_BLAST` | 2 | `SHRAPNEL` | +1 splash damage to each orthogonal secondary target. |
| `ATTACK_BLAST` | 3 | `SHOCKWAVE` | Deal 1 splash damage to each enemy diagonally adjacent to the primary target. Orthogonal splash still deals 2, and both types count toward Cluster Burst. |
| `ATTACK_BLAST` | 4 | `CONCUSSION` | Each surviving splash victim is pushed one cell straight away from the original primary target, including diagonal pushes, if its destination was open before the hit. |

Existing saves with Reach rank 2 receive Short Grip. The choice count is derived from level and learned ranks. Learned movement rank occupies `perk_ranks` bits 0–2; attack rank bits 3–5; bits 6–7 reserved. Learning a perk consumes one earned choice but does not directly spend gold, AP, or equipment.

## 6. Combat, encounters and The World Remembers

### 6.1 Tactical combat core

- Combat board is **8×4 cells**. Up to **four** deployed party units and **four** enemies; battlefield selection chooses among the allowed map/tileset boards. Initial positions are randomized within designated deployment rectangles, respecting blocked cells.
- One **unit action** (move, normal attack, special) consumes **the whole acting side's turn**. Sides alternate; a team may also pass. Automatic pass occurs if no legal action exists.
- **Move sequence:** Move to the destination, deal **1 base engagement damage** to every
  orthogonally adjacent living enemy, then apply movement perk effects. Overrun pushes a surviving
  forward enemy after engagement damage. The mover pays **1 HP** unless Footwork waives it.
  Actors with **1 HP may move** and may fall from the movement price. If engagement kills the
  last opponent, victory is decided **before** movement HP is paid.
- **Attack sequence:** Spend AP once for a special. Determine primary damage, positional bonuses,
  and secondary victims from the original position. Apply Hold Together and Shield Wall to the
  primary hit if the target has learned them and is supported. Apply all primary and secondary
  damage, then any Drain healing. Resolve knockback for a surviving victim or retreat for the
  attacker, using the original positions. Forced displacement requires an empty, unblocked cell inside the board and
  ignores movement class. It costs no HP and triggers no engagement or voluntary-move restriction. Check
  victory or defeat after all attack effects, then hand the turn to the other team if play continues.
  Ordinary actions either reduce total battle HP or eliminate at least one combatant. Life Channel
  can restore more total HP than it removes and spends 3 AP from the attacker's finite battle pool.
- Battle presentation follows the resolved action. Step walks, Leap arcs high, Charge accelerates,
  Vault springs over the crossed cells, and Glide leaves a diagonal trail. Attacks use class-specific
  contact effects, with arrows for Shoot and an orb for Blast. Starters and signatures cue their
  different wind-ups before contact. Damage, healing, and forced displacement animate on the units
  actually affected, including secondary hits, reflected damage, and perk effects. Damage bursts
  show actual HP lost, capped at 99 for display.
- **Pass:** every living unit on that side loses **1 HP**, then battle checks for defeat. Passing is not a free defensive action.
- Zero-HP party members do not deploy in subsequent encounters until revived/rested. Battle outcome is victory/defeat, never a draw. Defeat returns to the title screen rather than applying quest rewards.
- Victory writes surviving and fallen party HP back to saved members; grants each roster member (including those not deployed or at 0 HP) **2 XP** and victory AP recovery; credits **3 G for each deployed enemy** subject to wallet cap.

**Existing battlefield IDs:** outdoor (`BATTLEFIELD_GROVE`, `MEADOW`, `RIVERBANK`); cave (`MINE_GALLERY`, `MINE_CROSSING`, `MINE_CHAMBER`); dungeon (`VAULT_HALL`, `VAULT_CRYPT`, `VAULT_COURT`); interiors (`HOUSE_TAVERN`, `HOUSE_STORE`, `HOUSE_HALL`). The four tileset selections choose from their corresponding three boards. Geometry/obstacles reside in `src/battlefields.cpp`.

### 6.2 Enemy type presets

| `ENEMY_ID` | Name | Max HP | Movement | Attack | Sprite row |
|---|---|---:|---|---|---:|
| `ENEMY_WOLF` | Wolf | 9 | Charge | Rend | 17 |
| `ENEMY_BRUTE` | Brute | 12 | Step | Impact | 25 |
| `ENEMY_GIANT_BAT` | Giant Bat | 6 | Glide | Arc | 18 |
| `ENEMY_GIANT_SPIDER` | Giant Spider | 7 | Leap | Flank | 19 |
| `ENEMY_SKELETON` | Skeleton | 10 | Step | Reach | 23 |
| `ENEMY_FLOATING_EYE` | Floating Eye | 8 | Vault | Blast | 29 |

### 6.3 Authored map encounters (catalog order is save-sensitive)

The catalog concatenates `VillageEnemies`, `CaveEnemies`, `DungeonEnemies`, `OrchardEnemies`, `MineWestEnemies`, and `VaultEastEnemies` in that order. The list order defines persistent `enemy_defeated_flag_at(index)` ownership. New entries use the next free indices. Changing existing entries' ordering changes saved meanings and requires compatibility review.

| Spawn index / logical NPC ID | Marker | Behavior | Battle roster and arena pool | Respawn | Victory and dialogue hooks |
|---|---|---|---|---|---|
| **0 / 9** | `GroveWolf` | `ENEMY_WANDER`, normal speed | `ENEMY_WOLF` ×1; outdoor boards | `RESPAWN_REMEMBER` | Default; optional independent wolf encounter |
| **1 / 10** | `MineScout` | `ENEMY_CHASE`, fast speed | `ENEMY_WOLF` ×1; cave boards | `RESPAWN_REMEMBER` | Standard combat and respawn behavior |
| **2 / 11** | `VaultGuard` | `ENEMY_ROUTE`, normal speed, `routes::VaultPatrol` | `ENEMY_BRUTE` ×2 + `ENEMY_WOLF` ×1; dungeon boards | `RESPAWN_NEVER` | Before-battle and after-victory dialogue; no additional saved story flag beyond defeat bit |
| **3 / 12** | `OrchardSpiders` | `ENEMY_WANDER`, normal speed | `ENEMY_GIANT_SPIDER` ×2; outdoor boards | `RESPAWN_REMEMBER` | Optional, default hooks and rewards |
| **4 / 13** | `MineBatRoost` | `ENEMY_WANDER`, normal speed | `ENEMY_GIANT_BAT` ×1; cave boards | `RESPAWN_REMEMBER` | Optional, default hooks and rewards |
| **5 / 14** | `VaultEastWatch` | `ENEMY_WANDER`, normal speed | `ENEMY_SKELETON` ×1 + `ENEMY_FLOATING_EYE` ×1; dungeon boards | `RESPAWN_REMEMBER` | Optional, default hooks and rewards |

Spawns 0–5 use defeat bits **26–31** (`FLAG_ENEMY_FIRST + spawn_index`). The reserve holds 16 bits (10 unused). A map battle triggers on hostile actor contact, with area-local hooks for availability, pre-battle dialogue, battle selection, victory effects, and post-victory dialogue.

### 6.4 The World Remembers

- On victory against an enemy marked `RESPAWN_REMEMBER`, set its defeat bit and set `game.remember_timer = REMEMBER_DELAY_FRAMES` (**15,000 exploration frames**, 50 Hz nominal, about five minutes of simulation). A subsequent respawnable victory restarts the timer.
- After the countdown reaches zero, the next safe opportunity triggers the World Remembers reset/cutscene. `remember_reset()` clears defeat bits **only for respawnable spawns**, allowing later ordinary re-admission. The permanent `VaultGuard` bit is not cleared.
- Do **not** clear quest evidence or story flags during the reset. The new lantern/surveyor facts and all cave/dungeon mechanism/chest flags persist.
- Only authored, currently eligible spawns reappear through normal map activation; the reset is not a forced instant on-screen teleport of all actors.

## 7. Environmental mechanics, puzzles and world objects

### 7.1 General mechanism semantics

Mechanism objects are declared by area in `AreaMechanisms<AREA>`; their Tiled handlers apply state from flags to tiles. **Levers** toggle; **pressure plates, doors, orbs and chests** latch; gates, bridges, blocks, spikes and runes are read-only visual/terrain responses to flags. Successful chest claims are all-or-nothing. Inputs are either facing/pressing A or stepping onto a specific tile. Sound/visual changes do not store extra puzzle state.

### 7.2 Hollow Mine (`AREA_CAVE`)

| Object/region | Input | Flag affected/read | Gameplay consequence |
|---|---|---|---|
| `CaveLever` | A facing | Toggle `FLAG_CAVE_GATE` | `CaveGate` open/closed; controls access to lantern relay |
| `CaveVaultLever` | A facing | Toggle `FLAG_CAVE_NOOK` | `CaveNookGate` open/closed |
| `CavePlate` | Step | Latch `FLAG_CAVE_BRIDGE` | `CaveBridgeRegion` extended; `CaveHole` unlocked; enables surveyor rescue (not an automatic quest completion) |
| `CaveChest` | A facing | Latch `FLAG_CAVE_CHEST` on full successful claim | Existing one-time chest reward, §4.3 |
| `CaveVent` | Automatic, `game.ticks` | **No saved flag** | Solid during low byte tick 0–95 out of 256; opens for 160 ticks |
| `MineLanternRelay` | A facing | Set `FLAG_LANTERN_RELAY_FOUND` once | Unlock ancient Lantern Keeper resolution; no consumable item required |
| `SurveyorTrapped` | Talk to `NPC_SURVEYOR` | `FLAG_SURVEYOR_FOUND`, then `FLAG_SURVEYOR_RESCUED` on confirmation after bridge open | Missing Surveyor completion and offscreen relocation |
| `CaveNotice`, `MineWarning`, `MineLedger` | A facing | None | Maintenance instructions, wolf warning and historic records |
| `CaveMouth`/`CaveExit` | Step | None | Existing outdoor↔cave passage |
| `CaveHole`/`CaveLadder` | Step after bridge opens | Reads `FLAG_CAVE_BRIDGE` | Existing optional second passage / shortcut |

`WinchChalk`, `VentChalk`, `PoolEcho` and `LadderChalk` remain environmental clues where appropriate. No part of either quest should be reset by ordinary wolf respawning. Do not gate lantern relay access on the surveyor's rescue, or surveyor rescue on completing Lantern Keeper.

### 7.3 Old Vault (`AREA_DUNGEON`)

| Object/region | Input | Flag | Gameplay consequence |
|---|---|---|---|
| `DungeonLever` | A facing, reversible | `FLAG_DUNGEON_SPIKES` | Raise/lower `DungeonSpikesRegion` |
| `DungeonCellDoor` | A facing, permanent | `FLAG_DUNGEON_CELL` | Door opens permanently |
| `DungeonPlate` | Step, permanent | `FLAG_DUNGEON_GATE` | `DungeonGate` opens |
| `DungeonBlockLever` | A facing, reversible | `FLAG_DUNGEON_BLOCKS` | Raise/lower `DungeonBlocksRegion` |
| `DungeonOrb` | A facing, permanent | `FLAG_DUNGEON_ORB` | Orb and `DungeonRuneWest/East` light; `DungeonVaultDoor` opens |
| `DungeonChest` | A facing | `FLAG_DUNGEON_CHEST` | One-time chest reward, see §4.3 |
| `VaultGuard` | Contact | Defeat bit index 2 / `RESPAWN_NEVER` | 3-enemy battle and one-time enemy elimination |
| `DungeonPlaque` | A facing | None | Information only |
| `MineEntrance` / `DungeonExit` | Step | None | Two-way outdoor↔vault passage |

No `QUEST_ID` currently governs Old Vault progression; access, switches, chest and permanent guard all work as standalone content. Vault guardian win yields normal **2 XP/member and 9 G**, but there is no additional quest reward.

### 7.4 Millcross and houses

- `VillageLantern`: interactable rendered as a dark, modern or ancient lantern tile according to the restoration and branch flags.
- `WorkshopRecord`: the old workshop record on a table in the smithy, sets `FLAG_LANTERN_RECORD_FOUND`; revisit does not reset it.
- `SurveyorNotes`: inn note sets `FLAG_SURVEY_NOTES_READ` and starts Missing Surveyor if not already started. It remains inspectable after rescue without granting rewards.
- `SurveyTrailMark`: optional navigational evidence (`FLAG_SURVEY_TRAIL_FOUND`), not a mandatory barrier.
- `SurveyorReturned`: policy home for `NPC_SURVEYOR` outside the inn after rescue.
- `GroveWolf`: existing independent, respawning outdoor encounter.
- `WellSign`, `OrchardSign`, `SmithySign`, `CaveSign`, `LadderSign`, `SluiceStone`, `InnRegister`: ordinary informational messages unless a specific new marker and corresponding handler supersede them.
- `InnRest`: free full party restoration after confirmation; Mara offers the same action.
- `ShopCounter`: fixed-price supplies shop; Ada offers the same action.
- All ten house pairs remain functional.

## 8. Global invariants, ordering and design extension checklist

### 8.1 Logical invariants

1. **Lantern Keeper:** `RESTORED => STARTED && DIAGNOSED && RECORD_FOUND`; ancient branch additionally requires relay; modern branch tag implies restored. The final decision is irreversible. Any early clue discovery is retained; completion awards exactly once.
2. **Missing Surveyor:** `RESCUED => FOUND && STARTED && CAVE_BRIDGE`; `FOUND => STARTED`; note/trail clues are optional. Surveyor is in the cave before rescue and village afterward. Rescue awards exactly once.
3. **Independence:** neither quest may require completion, item, branch, or special event from the other. The cave can be explored and bridge opened before either quest is accepted.
4. **Enemy flags:** defeat flag meaning follows spawn-index order; permanent guardian never returns. World Remembers cannot erase new quest facts or permanent mechanisms.
5. **Equipment:** a globally owned item has at most one wearer; a compatible owned item or empty occupies each slot; removing max-HP equipment clamps current HP.
6. **Consumables/chests:** prevent capacity overflow and underflow; container rewards are all-or-nothing and single-claim.
7. **Party/perks:** no duplicate recruits; party size ≤ 4; ranks constrained by implementation and earned milestones; current HP/AP ≤ derived maxima.
8. **Quests:** status, journal objectives and markers are read-only state derivations; cancellation must not mutate completion flags; loads must not replay rewards.
9. **No softlocks:** mine crossing, both mine exits, house returns, cave mechanisms, NPC sites and vault all retain viable escape/recovery paths.
10. **Defeat:** player returns to the title screen; no quest-success flags or rewards are granted by a lost encounter.
11. **Quest catalog:** the Journal registry consists of `QUEST_LANTERN_KEEPER` and `QUEST_MISSING_SURVEYOR`. Quest identities and the flag layout belong to save format 22.

### 8.2 Future content entry template

Copy and fill this structure **in this document before or together with each new implementation**:

```md
### QUEST_ID — Name
- **Implementation status:** Implemented / Planned (only use Planned in an explicitly separate future section)
- **Owner:** source files / area / responsible quest header
- **NPCs:** IDs and roles (giver, witness, adversary, reacting residents)
- **Places:** symbolic `positions::` markers, areas and passage dependencies
- **Start prerequisites / trigger:** exact predicates and events
- **Saved facts:** flags and any other saved fields; name the owner of each fact
- **Progression:** numbered logical steps and optional/reorderable paths
- **Completion predicate:** precise flag expression
- **Outcomes/branches:** choice labels or symbolic branch IDs; permanent state changes
- **Rewards:** XP per member, G, item IDs/quantities, shortcut access; identify one-time behavior
- **Journal/markers:** objective stages; where markers point and when they disappear
- **NPC reactions:** post-completion/branch-dependent behavior
- **Dependencies:** other quests, objects, services; distinguish clue linkage from prerequisite
- **Save constraints / replay:** invariants; cancellation, repeat use, early completion, save/load
- **Tests:** normal, alternate, out-of-order and persistence cases
```

For **new items** record ID, ownership/stack state, slot/target, exact numerical effect, acquisition source and reusability. For **new perks/specials** record track/class, rank/unlock, cost, target legality, damage/effect ordering, interactions with level and equipment, and tests. For **new encounters** record marker, roster, battlefield pool, behavior, respawn mode, index ownership, hooks, rewards and quests affected.

### 8.3 Source index

| Concern | Executable implementation |
|---|---|
| Saved world state, classes, progression formulas | [`src/rpg.h`](../src/rpg.h), [`src/game.cpp`](../src/game.cpp), [`src/save.cpp`](../src/save.cpp) |
| Stable flag, quest, NPC and item identities | [`src/flags.h`](../src/flags.h), [`src/quest_ids.h`](../src/quest_ids.h), [`src/npc_ids.h`](../src/npc_ids.h), [`src/equipment_ids.h`](../src/equipment_ids.h) |
| Quests, journal tracking and event delivery | [`src/quests/`](../src/quests/), [`src/quest.h`](../src/quest.h), [`src/quest_runtime.cpp`](../src/quest_runtime.cpp) |
| NPC content, village interactions and locations | [`src/logic/area_village.h`](../src/logic/area_village.h), [`src/logic/area_cave.h`](../src/logic/area_cave.h), [`src/npc_policy.h`](../src/npc_policy.h), [`generated/map_positions.h`](../generated/map_positions.h) |
| Caves, vault, doors, mechanism and chest policy | [`src/logic/area_cave.h`](../src/logic/area_cave.h), [`src/logic/area_dungeon.h`](../src/logic/area_dungeon.h), [`src/logic/mechanisms.h`](../src/logic/mechanisms.h), [`src/logic/container_rewards.h`](../src/logic/container_rewards.h) |
| Combat, specials, classes, enemies, battlefields | [`src/battle.h`](../src/battle.h), [`src/battle.cpp`](../src/battle.cpp), [`src/battle_enemies.cpp`](../src/battle_enemies.cpp), [`src/battlefields.cpp`](../src/battlefields.cpp), [`src/logic/enemy_spawn.h`](../src/logic/enemy_spawn.h), [`src/logic/enemy_catalog.h`](../src/logic/enemy_catalog.h) |
| Encounter respawns and bespoke hooks | [`src/encounter.cpp`](../src/encounter.cpp), [`src/encounter.h`](../src/encounter.h), area headers |
| Perks and rewards | [`src/perks.def`](../src/perks.def), [`src/party_progress.cpp`](../src/party_progress.cpp), [`src/equipment.cpp`](../src/equipment.cpp), [`src/supplies.cpp`](../src/supplies.cpp), [`src/shop.cpp`](../src/shop.cpp) |
| Current world canon and authoring requirements | [`docs/world_bible.md`](world_bible.md), [`docs/content_authoring.md`](content_authoring.md) |

**Maintenance rule:** after a feature changes, update its ledger entry, any dependent quests, the cross-reference matrix, the flag/identity registry, and the relevant tests **in the same change**. The aim is one usable shared source of truth, not a second unofficial implementation.
