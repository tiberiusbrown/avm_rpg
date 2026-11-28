# Venlor — Game Logic Canon

**Save format:** `SAVE_VERSION = 26`

**Implemented quests:** Go Home and Eliza's Giant Rat.

## 0. Purpose and authority

This document is the canonical specification for Venlor’s concrete game rules,
content, persistent state, and dependencies. It contains no dialogue prose.

- **Update this file in the same change as any modification** to a quest, NPC, item, perk, special, encounter, reward, progression formula, permanent world interaction, or persistent state contract. Add entries before coding new content where practical.
- **Do not duplicate or fork these definitions** in new design notes. Other documents may discuss worldbuilding (`docs/world_bible.md`), presentation and density (`docs/content_authoring.md`), mechanics architecture (`docs/quest_programming.md`), and map authoring (`assets/maps/AUTHORING*.md`); this file owns the logical inventories, prerequisites, effects, dependencies, and rewards.
- **Stable identities:** preserve already-shipped IDs, and audit save compatibility whenever the layout, enum values, NPC identities, enemy catalog order, or map coordinates change. Do not derive identity from source-file order except where the current enemy catalog explicitly does so.
- **Content style:** use identifiers, symbolic positions, gameplay conditions, effects and outcomes. Keep dialogue text, narrative descriptions, tile-art instructions and speculative story ideas out of this document.

### Entry conventions

- `F` / `!F` means a world flag is set / clear; `A && B` means both conditions are true.
- **Start** is the first event that makes a quest active. **Completion condition** is the actual status predicate, which may also be satisfied through free exploration before accepting a quest.
- **Reward** means an actual state change (gold, XP, items, access). A chest in the same area is **not** a quest completion reward unless quest code gates or grants it.
- **Position** means a symbolic `positions::Name` generated from Tiled's Logic layer; do not duplicate hardcoded coordinates in logic.

## 1. World, state and identity registry

### 1.1 Areas

| Area | ID | Tileset | Implemented use |
|---|---:|---|---|
| `AREA_OUTDOOR` | 1 | World | The starting clearing with the boulder gate to its east, a fenced southern clearing, and the village with its river bridge (64x32) |
| `AREA_INTERIORS` | 2 | House | Alden's home, Eliza's home, the Shop and the Inn (76x11) |

The cave and dungeon tilesets and battlefields remain for later areas. No map uses them.

House doors and exits are paired by `handler_impl::HOUSE_LINKS` in
`src/logic/area_village.h`: `Alden`, `Eliza`, `Shop`, `Inn` (`<Name>Door` ↔ `<Name>Exit`, with
`<Name>RoomsRegion` as the floor). Facing a door and pressing A enters; walking onto the exit
leaves, in front of the door. `ElizaRoomsRegion` includes the bedroom,
`ElizaBedroomRegion`, which a hallway joins to the living room.

### 1.2 NPC identities and roles

NPC identities persist independently of the eight-slot live NPC/enemy pool. `NPC_NONE = 0`
means no identity.

| `NPC_ID` | Value | Body | Position policy and role |
|---|---:|---|---|
| `NPC_ELIZA` | 1 | Character, wandering | `ElizaAtAlden` (inside Alden's home) until `FLAG_RAT_REPORTED`, then `ElizaGarden`. Gives Eliza's Giant Rat |
| `NPC_BRAM` | 2 | Fixed character | `BramPost`, the middle of the bridge. Never yields to the player, so the only way out of the village is closed. Turns to a speaker and says nothing |
| `NPC_ALDEN` | 3 | None | The player character and first party member; a named speaker with a portrait, never placed on the map |
| `NPC_BOULDER_A`–`NPC_BOULDER_D` | 4–7 | Static object | `BoulderA`–`BoulderD` in the boulder gate. Unnamed; pushed with A |

`NPC_ID_LIMIT = 8`. Enemy identities start at 8.

**Static objects** are NPCs drawn from `assets/images/npc_static.png` with the plus-mask format.
`assets/tilesets/npc_static.tsx` gives each image a 16-bit collision mask in the terrain format,
and the object blocks the player and other actors with those pixels instead of a foot box. A
static object never turns, yields or walks out of terrain, and slides without detours. It
stays in the 8x4 chunk of its site, keeps its place while the terrain cache reaches that chunk
or it is on screen, and otherwise unloads with every other object of the chunk; it is later cast
at its policy site again.

**Casting.** On scene entry every eligible actor may be cast, including on screen. After entry a
frame casts at most one actor, only off screen.

### 1.3 Quest IDs

| `QUEST_ID` | Value | Definition | Completion predicate |
|---|---:|---|---|
| `QUEST_NONE` | 0 | Sentinel | — |
| `QUEST_GO_HOME` | 1 | `src/quests/go_home.h` | `FLAG_HOME_REACHED` |
| `QUEST_GIANT_RAT` | 2 | `src/quests/giant_rat.h` | `FLAG_RAT_REPORTED` |

`QUEST_ID_LIMIT = 3`. The Journal lists started and completed quests from this registry.

### 1.4 Persistent flag ledger

All indices refer to save format 25.

| Bit | Flag | Set by | Read by / invariant |
|---:|---|---|---|
| 0 | `FLAG_RESERVED` | Never | Sentinel |
| 1 | `FLAG_INTRO_SHOWN` | The first simulated frame of a new game | The waking line is spoken once |
| 2 | `FLAG_HOME_STARTED` | Leaving `NapAreaRegion` | Go Home active until `FLAG_HOME_REACHED` |
| 3 | `FLAG_BOULDERS_NOTICED` | Entering `BoulderSightRegion` while Go Home is active and unfinished and the clearing has not been left | Go Home objective; requires `HOME_STARTED` |
| 4 | `FLAG_LEFT_CLEARING` | Entering `PathEastRegion` | Go Home objective and marker; requires `HOME_STARTED` |
| 5 | `FLAG_HOME_REACHED` | Standing inside `AldenRoomsRegion` | Completes Go Home; opens Eliza's story; requires `HOME_STARTED` |
| 6 | `FLAG_ELIZA_GREETED` | Eliza's greeting inside Alden's home, or talking to her first | Requires `HOME_REACHED` |
| 7 | `FLAG_RAT_ACCEPTED` | Answering yes to Eliza (`quest_start`) | Quest active; casts the rat; requires `ELIZA_GREETED` |
| 8 | `FLAG_RAT_SIGHTED` | Being in `ElizaBedroomRegion` within three tiles of the rat | Opens the class choice; requires `RAT_ACCEPTED` |
| 9 | `FLAG_CLASSES_CHOSEN` | Confirming the class choice | Requires `RAT_SIGHTED` |
| 10 | `FLAG_RAT_REPORTED` | Telling Eliza after the rat's defeat | Completes the quest; moves Eliza; requires `RAT_DEFEATED` |
| 11 | `FLAG_MENU_JOURNAL` | The Journal hint, one frame after Go Home starts | Unlocks Journal; requires `HOME_STARTED` |
| 12 | `FLAG_MENU_PARTY` | The Party hint after the class choice | Unlocks Party; the rat then chases; requires `CLASSES_CHOSEN` |
| 13 | `FLAG_MENU_SUPPLIES` | Never | Would unlock Supplies |
| 14–47 | `FLAG_ITEM_FIRST + ITEM_ID` | Equipment acquisition | One ownership bit for each of the 34 items |
| 48–63 | `FLAG_ENEMY_FIRST + spawn_index` | Victory against an authored map enemy | 16 defeat slots. `FLAG_RAT_DEFEATED` = 48; requires `RAT_ACCEPTED` |

`FLAG_ITEM_FIRST = 14`, `FLAG_ENEMY_FIRST = 48`, and `FLAG_COUNT = 64`. The flag set occupies
eight bytes.

### 1.5 Saved fields and transient state

- **Saved `Game`:** save version, player actor and position, RNG seed, gold (`uint16_t`, maximum
  65,535), world tick counter, world flags, World Remembers timer, current area, level field,
  party (identities, classes, current HP/AP, member level, XP, perk ranks and equipment
  references), supplies, area dimensions, previous tile, interaction readiness, NPC pool,
  audio settings and `selected_quest`.
- **Rebuilt/transient:** tile cache, camera and workspaces, the tick counter, dialogue and
  menu navigation, pending quest evaluations and notices, battle state and pending contacts.
  Loading validates IDs, party data, flags, positions, quest invariants and the NPC pool, then
  rebuilds the view.
- **NPC pool:** a load keeps every actor as saved: identity, position, behavior, movement
  progress and pursuit. Boulders therefore stay where they were pushed, a sliding one finishes
  its slide, and a chase resumes. A load casts and activates nothing by itself; ordinary
  casting and unloading continue from the saved pool. Whether an actor has a fixed site or a
  static or immovable body comes from the current content. A save is rejected when its pool
  holds an unknown, repeated or party identity, a gap between actors, pursuit without an enemy,
  or an impossible facing, behavior or position.
- **Quest tracking:** only one active quest may be selected. The Quest Start panel names the
  quest and its starting objective and offers A (Track) or B (Close). The story selects Go Home and Eliza's Giant Rat when it starts them.
  A completed quest is untracked.
- **Exploration guidance:** the tracked quest's objective, or the local exit toward it, has a
  bobbing down arrow above its tile while visible; off screen the arrow slides along the screen
  edge and points in one of eight directions.
- **Save compatibility:** version 26 resumes the saved NPC pool. Version 25 replaced the earlier
  world, NPC, quest and flag layouts. Earlier saves are rejected. Because saved actors keep their
  positions, bump the version when a map or NPC change would leave them wrong. The high bit of the saved SFX level marks audio as disabled. Saved
  Music and SFX levels above 15 are capped at 15 on startup.

### 1.6 Pause menu unlocks

| Item | Available |
|---|---|
| Resume, Audio | Always |
| Save | Outside battle |
| Map | Outdoors, outside battle |
| Load | When a valid save exists, outside battle |
| Journal | After `FLAG_MENU_JOURNAL` |
| Party | After `FLAG_MENU_PARTY` |
| Supplies | After `FLAG_MENU_SUPPLIES`, which nothing sets yet |

## 2. Implemented quests and story

The story script is `src/logic/story.cpp`. It runs after each simulated frame's NPC dialogue.
Each step derives from saved facts.

### 2.1 Opening

On the first simulated frame Alden, facing south at `PlayerStart`, speaks once
(`FLAG_INTRO_SHOWN`).

### 2.2 `QUEST_GO_HOME` — Go Home

| State or transition | Requirements | Action / result |
|---|---|---|
| Start | Outdoors and outside `NapAreaRegion` | One line, then `quest_start` and `quest_select`; the Quest Start panel |
| Journal hint | The frame after the start | Set `FLAG_MENU_JOURNAL` and explain the pause menu |
| Gate noticed | In `BoulderSightRegion` (the gate mouth, where a boulder shows beside the dialogue), gate not yet left | Set `FLAG_BOULDERS_NOTICED`; two lines, one explaining A |
| Clearing left | In `PathEastRegion` | Set `FLAG_LEFT_CLEARING` |
| Completion | In `AldenRoomsRegion` | Set `FLAG_HOME_REACHED`; reward |

- **Objectives:** "Leave the clearing", then "Push the boulders aside" once noticed, then
  "Go to your home" once left.
- **Markers:** `ClearingExit` until the clearing is left, then `AldenDoor`.
- **Reward:** 3 XP per party member, once.

### 2.3 `QUEST_GIANT_RAT` — Eliza's Giant Rat

| State or transition | Requirements | Action / result |
|---|---|---|
| Greeting | In `AldenRoomsRegion` with Eliza present, or talking to her, after `HOME_REACHED` | Set `FLAG_ELIZA_GREETED` |
| Offer | Talk to Eliza, not yet accepted | Yes: `quest_start` and `quest_select`. No: she can be asked again |
| Waiting | Talk to Eliza while accepted and the rat lives | One line |
| Sighting | In `ElizaBedroomRegion` within three tiles (48 pixels, both axes summed) of the rat | Set `FLAG_RAT_SIGHTED`; one line |
| Class choice | `RAT_SIGHTED && !CLASSES_CHOSEN && !RAT_DEFEATED` | Fade to the class panel; confirming sets Alden's movement and attack classes and `CLASSES_CHOSEN` |
| Party hint | After the choice | Set `FLAG_MENU_PARTY`; the rat starts its pursuit |
| Battle | Contact with the rat | §6.3 |
| Report | Talk to Eliza after `RAT_DEFEATED` | Two lines; set `FLAG_RAT_REPORTED`; reward; Eliza belongs at `ElizaGarden` |
| After completion | Talk to Eliza | Nothing |

- **Objectives:** "Go to Eliza's home", then "Go back to Eliza" after the rat falls.
- **Markers:** outdoors `ElizaDoor`, then `AldenDoor`; indoors the rat (from `RatNest`) and then
  Eliza (from `ElizaAtAlden`), following the actor while it is present.
- **Reward:** 5 XP per party member and 10 G, once.
- **Invariants:** each flag of the chain requires the one before it, and
  `RAT_REPORTED => RAT_DEFEATED => RAT_ACCEPTED`.

**Class choice.** The panel lists all five movement classes, then all ten attack classes, each
with a description page, then a confirmation. B steps back one page and refuses on the first.
The choice is permanent. Level, HP, AP and perks are unchanged.

## 3. NPC conversations and signs

| Target | Result |
|---|---|
| Eliza | §2.3 |
| Bram | Turns to the player; no lines |
| Boulders | §7.1 |
| `AldenSign`, `ElizaSign`, `BramSign`, `ShopSign`, `InnSign` | Name the building |
| `NorthHouseSign`, `SouthHouseSign` | A house |
| `BarnSign` | A storehouse |
| `BramDoor` | Locked |
| `NorthHouseDoor`, `SouthHouseDoor`, `BarnDoor` | No answer |

The Shop and Inn can be entered; neither offers a service yet.

## 4. Items, inventory, services and rewards

### 4.1 Equipment item catalog

Ownership is one persistent bit per `ITEM_ID` (`FLAG_ITEM_FIRST + ID`). Each member has
one weapon, armor, and relic slot. A slot stores zero for empty or the item ID plus one.
Each owned item has at most one wearer. A transfer clears the previous wearer's slot.
Both members must be conscious for a transfer. Equipping and unequipping also require a
conscious member. When an equipment change lowers maximum HP or AP, clamp the current
value to the new maximum.

A deployed party member has one transient used bit for each slot in `BattleUnit`.
These share the byte holding enemy AP. All three clear at deployment, and each battle
simulation copy has its own bits. Previews leave the bits untouched.

#### Early game

All 34 entries below have engine support. New games start with no equipment owned.
Nothing in the world grants an entry yet.

| `ITEM_ID` | ID | Slot | Implemented effect |
|---|---:|---|---|
| `ITEM_FIRSTSTRIKE_SWORD` | 0 | Weapon | The first normal attack each battle deals +2 damage. |
| `ITEM_HUNTERS_BLADE` | 1 | Weapon | The first normal hit against a previously injured target deals +3 damage. |
| `ITEM_NEEDLE_SPEAR` | 2 | Weapon | Normal hits against enemies at full HP deal +1 damage. |
| `ITEM_CRACKED_SABER` | 3 | Weapon | Normal hits deal +1 damage. The first normal hit made while the wielder is at or below half HP deals +3 damage instead. The wielder has 3 less maximum HP. |
| `ITEM_BRASS_KNUCKLE` | 4 | Weapon | Normal hits deal +1 damage while at least two enemies are adjacent to the wielder. |
| `ITEM_STONEBREAKER` | 5 | Weapon | Normal hits deal +1 damage if the target is adjacent to a solid obstacle. |
| `ITEM_WOLF_FANG` | 6 | Weapon | Normal hits deal +1 damage if another living party member is adjacent to the target. |
| `ITEM_THORN_PIKE` | 7 | Weapon | Each move deals +1 engagement damage to one adjacent enemy. |
| `ITEM_PATHFINDERS_KNIFE` | 8 | Weapon | On the first move that leaves cover and engages, deal +2 engagement damage to one adjacent enemy. Start next to a solid obstacle and land next to none. |
| `ITEM_WOODCUT_AXE` | 9 | Weapon | Normal hits deal +1 damage to a target on the battle board's outer edge. |
| `ITEM_MUD_SLING` | 10 | Weapon | A normal hit from three or more cells away deals +2 damage if the wielder stands adjacent to a solid obstacle. |
| `ITEM_BARN_FORK` | 11 | Weapon | Normal hits deal +1 damage while the wielder is at full HP. |
| `ITEM_GUARD_VEST` | 12 | Armor | Reduce the first enemy hit each battle by 2 damage. |
| `ITEM_LEATHER_JERKIN` | 13 | Armor | +3 maximum HP. |
| `ITEM_OAK_SHIELD` | 14 | Armor | Reduce enemy hits by 1 damage while the wearer is at or below half HP. |
| `ITEM_REED_ARMOR` | 15 | Armor | While the wearer has no adjacent ally, reduce the first enemy hit that would deal at least 3 damage by 2. |
| `ITEM_RIVER_CLOAK` | 16 | Armor | The first enemy hit from at least three cells away deals 3 less damage. |
| `ITEM_ASH_CLOAK` | 17 | Armor | +1 maximum HP. The first move without engagement costs 1 less HP. |
| `ITEM_SCOUT_SANDALS` | 18 | Armor | +1 maximum HP. The first move that ends with engagement costs 1 less HP. |
| `ITEM_SOFT_BANDAGES` | 19 | Armor | Potions heal the wearer for up to 2 extra HP. |
| `ITEM_PADDED_SHAWL` | 20 | Armor | The first enemy hit taken while adjacent to a living ally deals 3 less damage. |
| `ITEM_LEAD_BOOTS` | 21 | Armor | Reduce enemy hits by 1 damage and increase each move's HP cost by 1. |
| `ITEM_TURNSHOE` | 22 | Armor | Reduce the first enemy hit taken on a board edge each battle by 3 damage. |
| `ITEM_HOPPER_VEST` | 23 | Armor | +1 maximum HP. Elixirs heal the wearer for up to 1 extra HP. |
| `ITEM_MOON_SHARD` | 24 | Relic | +1 maximum AP. |
| `ITEM_SURVIVORS_RIBBON` | 25 | Relic | Restore 1 HP to the conscious wearer after each victory. |
| `ITEM_FERRY_BEAD` | 26 | Relic | At victory, heal an adjacent conscious ally for 2 HP if the wearer is still conscious. Pick the ally with the most missing HP, breaking ties by party order. |
| `ITEM_THORN_RING` | 27 | Relic | The first move that engages at least two enemies deals +1 damage to each adjacent enemy. |
| `ITEM_GRAVE_TOKEN` | 28 | Relic | At exactly 1 HP, each hit on the wearer's action deals +3 damage, including engagement and secondary hits. |
| `ITEM_SLUICE_TALLY` | 29 | Relic | Gain 1 extra G after every victory while equipped, up to wallet capacity. |
| `ITEM_SHARED_CUP` | 30 | Relic | When the party uses a Potion on another member, the conscious wearer also heals up to 1 HP. |
| `ITEM_LANTERN_WICK` | 31 | Relic | The first enemy hit against a conscious ally adjacent to the wearer deals 2 less damage, once per battle. |
| `ITEM_TESSAS_ROSETTE` | 32 | Relic | +1 maximum HP. A Revive used on the wearer returns them to up to 7 HP. |
| `ITEM_SPLIT_BUTTON` | 33 | Relic | If the wearer finishes a victory at exactly 1 HP, restore 4 HP to them. |

Normal weapon effects modify the selected normal attack target. Range for equipment checks
counts orthogonal grid steps. Cover means sharing an edge with blocked terrain. For a
single engagement target, use the lowest pre-hit HP among adjacent enemies, breaking ties by
battle-unit index. Scout Sandals and Ash Cloak spend their activation only when the move
would cost HP and battle continues after engagement. Lead Boots add 1 HP to the final
movement cost, including when Footwork waives the base cost.

Primary enemy hits apply movement-rank protection first, followed by armor and Lantern Wick.
Reed Armor tests the damage remaining after movement-rank protection. Armor and the wick can
both activate on the same hit, with a floor of 1 damage. The first-hit triggers are spent on
their first qualifying hit. Lantern Wick requires a living adjacent wearer and protects only
another party member. Victory healing uses the final board and HP snapshot before recovery and
XP awards. Sluice Tally contributes 1 G to the party's victory reward whenever equipped,
including on a fallen wearer, subject to wallet capacity.

#### Midgame

Ideas only. Iron Fang, Traveler's Boots, Dawn Pendant, and Vital Pendant are listed in the
[midgame proposals](ideas_for_items_perks_abilities.md#midgame).

#### Late game

Ideas only. See the [late-game proposals](ideas_for_items_perks_abilities.md#late-game).

#### Endgame

Ideas only. See the [endgame proposals](ideas_for_items_perks_abilities.md#endgame).

### 4.2 Consumable supplies

A supply is an owned **stack quantity**, not an equipment flag. Each has a maximum stack size of
**15**. Using an invalid supply or targeting an ineligible member consumes nothing.

| `SUPPLY_ID` | Starts with | Effect when used outside combat | Eligible target / condition |
|---|---:|---|---|
| `SUPPLY_POTION` | 2 | Heal up to **8 HP** | One living, injured member |
| `SUPPLY_ELIXIR` | 1 | Heal up to **4 HP** to each eligible member | Whole party; at least one living injured member; does not revive |
| `SUPPLY_REVIVE` | 1 | Set a fallen member to up to **5 HP** | One member at 0 HP |
| `SUPPLY_ETHER` | 1 | Restore up to **3 AP** | One member below max AP, including one at 0 HP |

The Supplies menu stays locked, so the starting supplies cannot be used yet. The shop, inn rest
and chest reward rules remain implemented (`src/shop.cpp`, `party_rest`,
`src/logic/container_rewards.h`) with no service or container in the world.

### 4.3 Rewards

| Source | Effect | Replay |
|---|---|---|
| Any victorious battle | 2 XP to each roster member, 3 G per deployed enemy, eligible equipment rewards and AP recovery | Per battle |
| Go Home | 3 XP per member | Once |
| Eliza's Giant Rat | 5 XP per member and 10 G | Once |

Gold credits up to the wallet limit. The Quest Complete panel names the quest and reports party
XP and gold after the completing dialogue, before any level-up notice. Quest Start, Quest
Complete and level-up notices each play their own sound effect; the level-up sound also plays
for a level earned in battle. At most three quest panels wait at once. Further starts and
completions are not announced, but they happen and reward as usual. A new player who finishes both quests holds
10 XP, which reaches level 2, and 13 G.

## 5. Character classes, progression and perks

### 5.1 Starting party and base progression

- New game: Alden alone, `MOVEMENT_STEP` / `ATTACK_STRIKE` until the class choice, **level 1**,
  **12/12 HP**, **4/4 AP**, no owned equipment, **0 G**, and the supply counts above.
- Party capacity: **4**. Identities are unique. A recruit begins at full derived HP/AP and level 1.
- Party member levels: **1–10**. XP to the next level from level `L`: `5 × (L + 1)` (10 at level 1,
  15 at level 2). XP counts toward the next level, carries over, and is discarded at level 10.
- Maximum HP: **12** for every member, **+2 at level 3**, **+2 more at level 7**, plus equipped HP
  bonuses. Leveling does not restore current HP.
- Normal and special direct attack base damage gains **+1 at level 5** and **+1 more at level 9**.
  Engagement and secondary damage use their own base damage and perks.
- Maximum AP: **4**, plus 1 with Moon Shard.
- Victory AP recovery: **1** per member, capped at the member's maximum.
- Perk choices at **levels 2, 4, 6, 8 and 10**, derived as
  `floor(level / 2) - movement_perk_rank - attack_perk_rank`. Each member has one movement and one
  attack track of four ranks, learned in order and cumulative. Unspent choices remain.

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

Apply direct attack perk bonuses to the selected base primary damage for both sides. Party members
also receive their level and weapon bonuses. Secondary damage from Blast, Piercing Shot, and
Collision uses the primary attack's line of sight and hits each qualifying enemy once.
When the target survives, Drain healing is capped at one
less than the target's actual HP loss, with a floor of zero. A finishing blow allows full Drain
healing, even against a target at 1 HP. All Drain healing is capped at the attacker's maximum HP.

### 5.4 Special abilities

Each attack class grants a **2 AP starter at level 3** and a **3 AP signature at level 5**.
Both use one selected enemy eligible for the normal attack, including any learned targeting
perks. Hooking Thrust and Wall Slam also require the setup below. A on the target offers Attack
and any unlocked, affordable, legal specials. Below level 3 or with less than 2 AP, A attacks
normally. A special spends AP once and takes one team turn. Party unlocks use the member's saved
level. When an enemy attacks, it uses an affordable, legal signature if available, then an
affordable, legal starter if available, then its normal attack. Its AP comes from its preset at
deployment and is spent independently for each unit. Enemy AP has no level gate or recovery
during combat. The AI predicts the same spending rule in its search.

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
| **Life Channel** | `ATTACK_DRAIN` | +2 primary damage. Heal each other living ally for the actual HP the primary target lost, reduced by 1 if it survives. On a kill with Soul Dividend, each other living ally instead heals up to 5 HP. Healing caps at each recipient's maximum HP. |
| **Backsweep** | `ATTACK_RECOIL` | Each other enemy orthogonally adjacent to any cell entered during the retreat takes full damage once. Dragback still applies to the surviving primary target. |
| **Combined Arms** | `ATTACK_FLANK` | Each other living ally orthogonally adjacent to the target who can legally attack it adds their own normal primary attack damage, with level, gear, and perk bonuses, before target defenses are applied to the combined hit. |
| **Implosion** | `ATTACK_BLAST` | Enemies exactly two cells away along the target's row or column take full damage. Each survivor moves one cell toward the primary target if that cell was open before the hit. The usual local splash retains its damage and Concussion rules. |

### 5.5 Implemented perks

Party members learn perks through the Party menu. Enemies use ranks from their presets. The
implementation catalog is `src/perks.def`. Attack perks apply to normal attacks and specials.
Movement damage bonuses apply to
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

The choice count is derived from level and learned ranks. Learned movement rank occupies `perk_ranks` bits 0–2; attack rank bits 3–5; bits 6–7 reserved. Learning a perk consumes one earned choice but does not directly spend gold, AP, or equipment.

## 6. Combat, encounters and The World Remembers

### 6.1 Tactical combat core

- Combat board is **8×4 cells**. Up to **four** deployed party units and **four** enemies; battlefield selection chooses among the allowed map/tileset boards. Initial positions are randomized within designated deployment rectangles, respecting blocked cells.
- **Formation:** Before the first turn, the player may select a deployed party member and place them on any unblocked cell in the party's deployment rectangle. Choosing another party member swaps their cells. Formation changes only the starting positions for this encounter and costs no HP, AP or turn. B clears a pending selection or opens the confirmation. Canceling the confirmation keeps the current arrangement, and accepting it starts combat.
- The Formation panel appears while the cursor is on a legal starting cell. Hovering over a deployed party member or enemy shows the same actor information as in combat. Moved and swapped party members slide to their new cells together.
- One **unit action** (move, normal attack, special) consumes **the whole acting side's turn**. Sides alternate; a team may also pass. Automatic pass occurs if no legal action exists.
- **Move sequence:** Move to the destination, deal **1 base engagement damage** to every
  orthogonally adjacent living enemy, then apply movement perk and equipment effects. Overrun pushes a surviving
  forward enemy after engagement damage. The mover pays **1 HP** before any Footwork or equipment adjustment.
  Actors with **1 HP may move** and may fall from the movement price. If engagement kills the
  last opponent, victory is decided **before** movement HP is paid.
- **Attack sequence:** Spend AP once for a special. Determine primary damage, positional bonuses,
  and secondary victims from the original position. Apply Hold Together and Shield Wall to the
  primary hit if the target has learned them and is supported, then apply equipment protection. Apply all primary and secondary
  damage, then any Drain healing. Resolve knockback for a surviving victim or retreat for the
  attacker, using the original positions. Forced displacement requires an empty, unblocked cell inside the board and
  ignores movement class. It costs no HP and triggers no engagement or voluntary-move restriction. Check
  victory or defeat after all attack effects, then hand the turn to the other team if play continues.
  Ordinary damaging actions remove at least 1 HP from an opponent. Ash Cloak can waive the
  cost of one quiet move. Life Channel can restore more total HP than it removes and spends
  3 AP from the attacker's finite battle pool.
- Battle presentation follows the resolved action. Step walks, Leap arcs high, Charge accelerates,
  Vault springs over the crossed cells, and Glide leaves a diagonal trail. Attacks use class-specific
  contact effects, with arrows for Shoot and an orb for Blast. Starters and signatures cue their
  different wind-ups before contact. Damage, healing, and forced displacement animate on the units
  actually affected, including secondary hits, reflected damage, and perk effects. Damage bursts
  show actual HP lost, capped at 99 for display.
- During an attack, a centered banner reads `<actor> attacks <victim>!` or `<actor> uses <ability>!`
  for a starter or signature. The banner fits the measured text and uses the edge with fewer units
  involved in the action, favoring the edge farther from the target when both are equally clear.
- **Pass:** every living unit on that side loses **1 HP**, then battle checks for defeat. Passing is not a free defensive action.
- Hovering over a living unit shows its movement and attack class names above its current and
  maximum HP. Ranks 1–4 follow the class name, as in `Step 2 / Strike 3`. Rank 0 uses the name
  alone. Enemy panels show AP while the unit has AP remaining. Party panels show AP from level 3,
  including 0 AP. The panel fits the wider rendered line.
- Zero-HP party members do not deploy in subsequent encounters until revived/rested. Battle outcome is victory/defeat, never a draw. Defeat returns to the title screen rather than applying quest rewards.
- Victory writes surviving and fallen party HP back to saved members; grants each roster member (including those not deployed or at 0 HP) **2 XP** and victory AP recovery; credits **3 G for each deployed enemy** plus any equipment gold bonus, subject to wallet cap.

**Battlefield IDs:** world (`BATTLEFIELD_GROVE`, `MEADOW`, `RIVERBANK`); cave (`MINE_GALLERY`, `MINE_CROSSING`, `MINE_CHAMBER`); dungeon (`VAULT_HALL`, `VAULT_CRYPT`, `VAULT_COURT`); house (`HOUSE_TAVERN`, `HOUSE_STORE`, `HOUSE_HALL`). Each tileset selects its three boards. No authored encounter uses the cave or dungeon boards. Geometry and obstacles reside in `src/battlefields.cpp`.

### 6.2 Enemy type presets

| `ENEMY_ID` | Name | Max HP | Movement | Attack | Starting AP | Sprite row |
|---|---|---:|---|---|---:|---:|
| `ENEMY_WOLF` | Wolf | 9 | Charge | Rend | 0 | 17 |
| `ENEMY_BRUTE` | Brute | 12 | Step | Impact | 0 | 25 |
| `ENEMY_GIANT_BAT` | Giant Bat | 6 | Glide | Arc | 0 | 18 |
| `ENEMY_GIANT_SPIDER` | Giant Spider | 7 | Leap | Flank | 0 | 19 |
| `ENEMY_SKELETON` | Skeleton | 10 | Step 2 | Reach 1 | 2 | 23 |
| `ENEMY_FLOATING_EYE` | Floating Eye | 8 | Vault 1 | Blast 2 | 3 | 29 |
| `ENEMY_GIANT_RAT` | Giant Rat | 5 | Step | Strike | 0 | 16 |
| `ENEMY_GIANT_BEETLE` | Giant Beetle | 10 | Charge 1 | Impact | 0 | 20 |
| `ENEMY_SNAKE` | Snake | 7 | Leap | Recoil 1 | 0 | 21 |
| `ENEMY_SLIME` | Slime | 8 | Step | Drain | 0 | 22 |
| `ENEMY_ZOMBIE` | Zombie | 12 | Step 1 | Rend 2 | 0 | 24 |
| `ENEMY_GOBLIN` | Goblin | 8 | Leap 1 | Shoot 1 | 2 | 26 |
| `ENEMY_GHOST` | Ghost | 11 | Glide 3 | Drain 2 | 2 | 27 |
| `ENEMY_MUSHROOM` | Mushroom | 10 | Step 1 | Blast 1 | 2 | 28 |
| `ENEMY_GOLEM` | Golem | 18 | Step 3 | Impact 4 | 3 | 30 |
| `ENEMY_HOODED_CULTIST` | Hooded Cultist | 14 | Vault 2 | Shoot 4 | 5 | 31 |

### 6.3 Authored map encounters (catalog order is save-sensitive)

`VillageEnemies` is the whole catalog. List order defines each spawn's defeat flag.

| Spawn index / NPC ID | Marker | Availability and behavior | Battle | Respawn |
|---|---|---|---|---|
| **0 / 8** (giant rat) | `RatNest` | `FLAG_RAT_ACCEPTED`. Stands still (`ENEMY_WANDER`, zero range) until `FLAG_MENU_PARTY`, then chases: detect 10 tiles, disengage and leash 12 tiles | `ENEMY_GIANT_RAT` ×1, house boards | `RESPAWN_NEVER` |

Spawn 0 uses defeat bit 48 (`FLAG_RAT_DEFEATED`). The reserve holds 16 bits. A battle begins on
contact with a hostile actor.

### 6.4 The World Remembers

- A victory against an enemy marked `RESPAWN_REMEMBER` sets its defeat bit and
  `game.remember_timer = REMEMBER_DELAY_FRAMES` (15,000 exploration frames, about five minutes).
- When the countdown ends, the next safe opportunity runs the reset cutscene, which clears
  defeat bits of respawnable spawns only.
- No authored enemy is respawnable yet.

## 7. Environmental mechanics, puzzles and world objects

### 7.1 Boulder gate

The gate is `BoulderGateRegion` (tiles x 24–28, y 12–15), inside one 8x4 chunk. Its terrain walls
and four boulders are the only barrier between the clearing and the village.

- A while facing a resting boulder slides it one tile away at one pixel per frame
  (`npc_push`). The destination must lie in the region and in the boulder's chunk, be free of
  terrain and of the player and other actors, and is reserved while the boulder slides. A push
  that moves plays a grinding sound that ends as the 16-frame slide does; a blocked push plays
  the error tone.
- Boulders return to their sites only when they unload (§1.2). Saving and loading keeps them
  where they are.
- The authored arrangement takes at least four pushes. No order of pushes from either side can
  trap the player: every reachable arrangement can still reach the village. The content test
  checks both properties.

### 7.2 Barriers and clearings

Cliffs, water, forest and rocks surround the clearings and the village. The only exit from the
village is the bridge south, which Bram holds.

Each clearing has its own way to the village, and neither leads to the other. The starting
clearing is reached through the boulder gate. A path along the south of the lake leads to the
southern clearing (`SouthClearingRegion`), and the fence in `SouthClearingFenceRegion` closes it.
No flag or interaction opens that fence yet.

### 7.3 Mechanism support

Levers, pressure plates, doors, orbs, chests, passages and steam vents remain implemented in
`src/logic/mechanisms.h` for later areas. No authored map uses them.

## 8. Global invariants and design extension checklist

### 8.1 Logical invariants

1. **Story order:** every story flag requires its predecessor in §1.4; a load rejects a violation.
2. **Gate:** the player can never be trapped in the clearing by pushes, unloading or loading.
3. **Enemy flags:** defeat flag meaning follows spawn-index order; the rat never returns.
4. **Equipment:** an owned item has at most one wearer; changing equipment requires conscious
   wearers and clamps current HP/AP.
5. **Party/perks:** no duplicate members; party size ≤ 4; ranks bounded by earned milestones;
   current HP/AP ≤ derived maxima.
6. **Quests:** status, objectives and markers are read-only derivations; loads never replay
   rewards.
7. **Defeat:** the player returns to the title screen; a lost battle grants nothing.

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
| Story script and class choice | [`src/logic/story.cpp`](../src/logic/story.cpp), [`src/class_choice.cpp`](../src/class_choice.cpp) |
| Quests, journal tracking and event delivery | [`src/quests/`](../src/quests/), [`src/quest.h`](../src/quest.h), [`src/quest_runtime.cpp`](../src/quest_runtime.cpp) |
| NPC content, houses, signs, enemies and locations | [`src/logic/area_village.h`](../src/logic/area_village.h), [`src/npc_policy.h`](../src/npc_policy.h), [`generated/map_positions.h`](../generated/map_positions.h) |
| NPC engine, static objects and casting | [`src/npc.cpp`](../src/npc.cpp), [`src/npc.h`](../src/npc.h) |
| Mechanisms and chest policy | [`src/logic/mechanisms.h`](../src/logic/mechanisms.h), [`src/logic/container_rewards.h`](../src/logic/container_rewards.h) |
| Combat, specials, classes, enemies, battlefields | [`src/battle.h`](../src/battle.h), [`src/battle.cpp`](../src/battle.cpp), [`src/battle_enemies.cpp`](../src/battle_enemies.cpp), [`src/battlefields.cpp`](../src/battlefields.cpp), [`src/logic/enemy_spawn.h`](../src/logic/enemy_spawn.h), [`src/logic/enemy_catalog.h`](../src/logic/enemy_catalog.h) |
| Encounter respawns and hooks | [`src/encounter.cpp`](../src/encounter.cpp), [`src/encounter.h`](../src/encounter.h) |
| Perks and rewards | [`src/perks.def`](../src/perks.def), [`src/party_progress.cpp`](../src/party_progress.cpp), [`src/equipment.cpp`](../src/equipment.cpp), [`src/supplies.cpp`](../src/supplies.cpp), [`src/shop.cpp`](../src/shop.cpp) |
| World canon and authoring requirements | [`docs/world_bible.md`](world_bible.md), [`docs/content_authoring.md`](content_authoring.md) |

**Maintenance rule:** after a feature changes, update its ledger entry, any dependent quests, the cross-reference matrix, the flag/identity registry, and the relevant tests **in the same change**. The aim is one usable shared source of truth, not a second unofficial implementation.
