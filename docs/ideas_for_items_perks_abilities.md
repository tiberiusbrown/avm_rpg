# AVM RPG — Rewards, Inventory, and Party Progression

**Status:** Design reference and idea bank, not an implementation specification  
**Design target:** A compact, exploration-driven fantasy RPG with tactical battles and a tight Arduboy memory/UI budget.

## 1. Design goals and existing combat assumptions

The reward system should make exploring a side passage, clearing an optional dungeon, and accepting a quest feel valuable for *different* reasons. Long-term character growth should open tactical possibilities without making the existing movement/positioning system irrelevant.

This document assumes the following existing or planned rules:

- A player action advances the battle turn for the party. A perk should therefore be useful immediately, passively, or over several turns; spending an entire turn merely to guard against the next hit is not an attractive reward.
- Moving an actor automatically attacks adjacent enemies, then deducts movement HP **last**. An actor at 1 HP can deliberately move, strike, and fall to 0 HP. Nothing here should accidentally remove that tactical sacrifice.
- A character has independent movement and attack classes. The attack class determines normal-attack geometry and starting special(s); movement and attack classes each supply an ordered perk track. Normal attacks and special abilities are selected through the attack-choice UI. Specials consume ability points (AP). AP **does not** regenerate during battle, but each active party member recovers **1 AP after each victorious battle** by default (up to their maximum AP). This between-battle recovery can increase through perks and equipment.
- Battles can have different obstacle and enemy placements. Rewards should enrich positioning decisions, not bypass them wholesale.
- The game has very limited persistent RAM and a 128×64 screen. Prefer fixed tables, bitfields, compact menus, and content descriptions held in flash.

**Terminology:** Values such as `+1 damage`, `8 HP`, and gold prices are starting balance targets, **not final tuning**. An item or perk marked **[extension]** needs a mechanic beyond the baseline (e.g., burning, armor, pushing, destructible obstacles).

### Five design principles

1. **Make exploration valuable without mandatory grinding.** Hidden treasures and quests should supply some of the best permanent rewards.
2. **Make battle victory imperfect.** HP and AP persist between encounters; victory restores only a little AP initially, so an expedition still demands resource management.
3. **Reward distinctive builds, not busywork.** Each character combines independent movement and attack classes, advances one of two ordered perk tracks at each milestone, and equips one weapon, armor, and relic.
4. **Keep the player-facing rules legible.** Small numeric bonuses, recognizable triggers, and explicit caps are more valuable than elaborate chains of conditions.
5. **Separate a fixed baseline from idea banks.** The lists below are candidates; implementing all of them would be unnecessarily complex.

### Two independent class axes (recommended generic names)

Keep the existing **movement class × attack class** architecture. Do **not** add a unified character class such as Fighter, Guardian, Scout, or Mage. The same attack style can pair with any movement style; equipment and perks distinguish characters further.

The code uses these identifiers. Each name describes the rule rather than a species, named character, or story profession. No movement-class name duplicates an attack-class name.

| Axis | Identifier | Display name | Geometry / rule |
|---|---|---|---|
| Movement | `MOVEMENT_STANDARD` | **Standard** | Orthogonal move, up to 1 tile |
| Movement | `MOVEMENT_LEAPER` | **Leaper** | Chess-knight leap to an empty tile |
| Movement | `MOVEMENT_CHARGER` | **Charger** | Orthogonal move, up to 2 tiles |
| Attack | `ATTACK_STRIKE` | **Strike** | Range 1, damage 3 |
| Attack | `ATTACK_SHOT` | **Shot** | Straight-line range 2–4, damage 3 |
| Attack | `ATTACK_REACH` | **Reach** | Straight-line range 1–2, damage 3 |
| Attack | `ATTACK_IMPACT` | **Impact** | Range 1, damage 2, knockback |
| Attack | `ATTACK_REND` | **Rend** | Range 1; **2 damage** against a target at half its maximum HP or more, **4 damage** against a target strictly below half |

**Strike/Rend distinction:** Strike always deals **3 normal-attack damage**. Rend deals **2 normal-attack damage**, increasing to **4 only when the target has strictly less than half its maximum HP before the hit**. For example, a 10-HP enemy qualifies at 4 HP, but not at 5 HP. The rule is `2 * target.hp < target.max_hp`, using sufficiently wide arithmetic. The bonus applies to the attack, **not** to automatic movement engagement damage. This gives Strike reliable damage and Rend a finisher role; Rend’s special abilities and perk track deepen that identity. Likewise, **Charger** is a long-range *orthogonal movement pattern*, not an automatic attack or a move through occupied tiles. **Leaper** is a fixed knight-shaped jump, not arbitrary flying.

For readable UI, label them explicitly as `Move: Standard` and `Attack: Shot`, rather than referring ambiguously to a character's single "class". Keep these axes **fixed for a character after recruitment** in the initial version; class-changing and perk respec can be considered later. Base classes are not automatically restricted to particular weapons or relics.

---

## 2. Four consumables

**Recommended baseline:** Four fixed consumable counters, each stackable up to 15. All are used from an out-of-battle **Supplies** menu. No grid inventory, item sorting, weight, or arbitrary item slots are required.

| Consumable | Effect | Intended role | Illustrative shop price |
|---|---|---|---:|
| **Potion** | Restore **8 HP** to one conscious member, capped at maximum HP | Cheap, targeted recovery | 12 gold |
| **Elixir** | Restore **4 HP** to every party member who is conscious | Efficient recovery when several members are injured | 22 gold |
| **Revive** | Return one incapacitated member (0 HP) to **5 HP** | Prevent a casualty from ending an expedition | 30 gold |
| **Ether** | Restore **3 AP** to one member, capped at maximum AP | Replenish scarce special-ability uses | 18 gold |

### Rules worth fixing up front

- **No item use during combat** in the first implementation. This preserves the existing one-action-per-turn tactical economy and makes pre-encounter planning meaningful. In-battle consumables can be considered later as a major feature.
- A Potion or Elixir cannot revive a member at 0 HP. Revive is deliberately distinct.
- An Elixir benefits **only conscious members**; it should not accidentally provide four free revivals.
- Supplies cannot be wasted when they have no effect (e.g., Ether at full AP).
- AP **persists between battles**. After each victory, every active party member recovers a base **1 AP**, capped at maximum AP; this applies even if the member was incapacitated when the battle ended, so a defeat does not also cause an AP-progression penalty. Perks and equipped items can raise the amount recovered. Ethers, town recovery, and explicit safe-area effects provide additional recovery. **AP never regenerates during combat.**
- An inn, home, or other reliable recovery location should fully restore HP and AP and revive incapacitated members, at a low cost or free if necessary to avoid a softlock.
- The party can carry all four supplies without a global carrying-capacity system. A per-type cap of 15 controls hoarding and saves UI/RAM.
- If the entire party is incapacitated, use the game's existing defeat/retry policy; the supply system should not be the sole way to escape total defeat.

**Potential late-game upgrades, not additional consumable types:** A stronger Potion can be an unlock that improves every Potion's effect; a healing-focused perk can increase Elixir healing. This avoids inflating the inventory with Potion I / II / III.

### AP recovery: a progression mechanic

**Chosen rule:** AP is an expedition resource that gradually becomes a per-battle resource as the party acquires the right build.

- At the start of the game, AP carries over from one encounter to the next. **Win a battle → recover 1 AP per active member**, never exceeding that member's maximum AP. There is **no AP recovery during battle**.
- Recovery bonuses from a character's perks and equipment (usually relics) **add to the base 1 AP**. A character with an **Afterglow** perk (+1) and a **Wellspring Pearl** relic (+2) would recover **4 AP after each victory**. A late-game build with **Deep Wellspring** (+2) and **Wellspring Pearl** (+2) would recover **5 AP**, enough to refill a five-AP pool after almost any fight.
- Recovery applies **after the battle ends**, not just before the next battle; it cannot power another action in the battle just won. Resolve victory, award AP recovery, then present the usual post-battle results or return to exploration.
- **Cap restored AP at maximum AP** and discard overflow. Maximum AP bonuses increase how much a character can hold, but do not automatically increase recovery speed.
- **Only victory triggers this recovery** by default. Fleeing or losing should follow the game's escape/defeat rules, not grant a free AP refill.
- An Ether still restores **3 AP** outside battle. It is especially useful early in the game or before a demanding encounter, but naturally becomes less important for characters whose recovery approaches a full reset.

This produces a desirable progression: **early game: conserve specials → midgame: use them selectively but regularly → late game: specialized characters can spend most or all of their AP in each encounter**. This should be earned through perks and relics, not granted automatically to every character.

**Balance guardrails:** Giving +2 post-battle AP recovery is substantially stronger than +1 maximum AP in a long dungeon. Place strong recovery options later and make them compete with unusual, powerful relics; weapon and armor slots should not routinely supply AP recovery as well. Repeated weak encounters could be used to refill AP; prefer finite encounters and meaningful encounter pacing, and test whether repeatable trivial enemies create a tedious 'recharge by fighting' strategy. Do not add a complicated per-enemy AP-recovery rule unless testing demonstrates a real problem.

---

## 3. Inventory and equipment model

### Inventory structure

- **Supplies:** Four fixed counters and an out-of-battle use/target interface.
- **Equipment:** Each party member has **three separate slots: one weapon, one armor, one relic**. A slot may be empty, or occupied by exactly one compatible acquired item. A unique item cannot be equipped by two characters at once.
- **Key Items:** Flag- or quest-derived entries without generic object storage. The quest system determines ownership and delivery.

Gold remains a wallet counter, not an inventory object. Equipment can complement a character's **movement track, attack track, or both**; it does not change their two base class identities. There is no need for grid slots, loot weight, durability, randomized affixes, stackable equipment, selling unique finds, or item-instance data.

### What belongs in each slot?

| Slot | Design identity | Effects to favor | Examples |
|---|---|---|---|
| **Weapon** | *How I deal damage* | Normal-attack strength, target conditions, attack reach, special-attack damage, automatic attacks after movement | Iron Fang, Sunder Mace, Storm Staff |
| **Armor** | *How I endure and move* | Incoming enemy-damage mitigation, HP capacity, first-hit defense, movement range, movement HP cost, terrain movement | Guardian Crest, Traveler's Boots, Stone Plate |
| **Relic** | *What unusual rule I bend* | AP capacity/recovery, special efficiency, party synergy, post-battle healing, sacrifice bonuses, treasure sensing, odd conditional effects | Wellspring Pearl, Thorn Ring, Sun Medallion |

**Armor deliberately includes boots, cloaks, shields, plate, robes, and other movement-focused gear**; it is a gameplay category, not merely a torso armor slot. Lightweight boots may help a character reposition or lose less HP while moving, while heavy plate can reduce enemy damage but restrict movement. This tradeoff matters because movement already damages the mover **after** it attacks adjacent enemies. Armor should never reverse that order.

Categorization guideline: a weapon's primary benefit should change offensive output, armor's primary benefit should change incoming damage or movement, and a relic's primary benefit should be a distinctive passive or resource interaction. Crossovers are welcome when a clear primary identity remains. A sword can trade HP for damage, boots can enhance the automatic attack after movement, and a relic can increase damage under a peculiar condition. The three slots should not merely triple generic +1 bonuses.

### Suggested equipment UI on a 128×64 display

1. **Equipment → character** (or **Party → character → Equipment**).
2. Show three short rows: `Weapon: Iron Fang`, `Armor: Road Boots`, `Relic: Moon Shard`.
3. Select a row to view just that slot's owned compatible items; show the selected item name and 1–2 short, paged lines describing its effect. Include an **Unequip** entry.
4. Confirm a choice. If another character wears that unique item, display the current wearer and **move it** to the newly selected character, clearing the old slot.
5. When switching items changes maximum HP/AP, **clamp current HP/AP to the new maxima**; never heal by unequipping/re-equipping. Do not restore AP just because an item increases maximum AP.

Use short display names and compact abbreviations (`ATK+1`, `First hit −2`, `Move HP −1 once`, `Win AP +1`) while keeping full descriptions in flash. Allow class restrictions if desired, but do not require four separate weapon inventories.

### Stacking and tradeoffs

- Every character can benefit from **movement-track perks + attack-track perks + weapon + armor + relic** at once, so tune combinations, not isolated entries. Normal + special + automatic movement damage may need distinct modifiers to avoid accidental triple-dipping.
- **Incoming enemy damage reduction does not reduce the HP a character voluntarily loses for movement** unless an item explicitly says `movement HP cost`. Treat these as different damage categories.
- Armor movement-HP reductions apply **after** movement attacks, when the self-cost is paid. Avoid universally free movement. An explicit first-move freebie can be interesting, but stackable generic reductions should normally have a floor or a once-per-battle trigger.
- Keep bonuses to after-battle AP recovery mostly on **relics and perks**. The baseline remains **+1 AP per active member after victory**, no regeneration during battle; a recovery-focused build can eventually approach a full refill.
- Do not duplicate effects by assigning a weapon, armor, and relic each with identical +1 attack. Make at least one of the three slots produce a situational or build-defining decision.

### Save-state direction

Store an **acquired-equipment bitset** plus **twelve equipped IDs** (4 characters × 3 slots). A single byte per equipped ID can index up to 255 unique definitions, with `0 = empty`. Item category, compatibility, name, description, and effects can be derived from the ID in flash. A locked item list is a fixed table, not individually allocated objects. The much larger idea bank below is **not** a commitment to ship every item.

---

## 4. Character levels and two-track perk progression

**Chosen direction:** Every party member has independent **movement** and **attack** classes, with **one ordered perk track attached to each**. At each perk level, choose one: advance the **next movement perk** for that member's movement class **or** the **next attack perk** for that member's attack class. There is no third, unified Fighter/Guardian/Scout/Mage class and no generic pick-any-perk menu.

### Level schedule

**Recommended first campaign cap:** Level 10, with modest automatic stat growth and one perk-track advancement at every even-numbered level. The numbers below are balance placeholders.

| Level | Cumulative XP (example) | Reward |
|---:|---:|---|
| 1 | 0 | Movement + attack class rules; first attack-class special already available |
| 2 | 10 | **Advance movement track OR attack track** (rank 1) |
| 3 | 25 | +2 maximum HP |
| 4 | 45 | **Advance movement OR attack track** (next rank) |
| 5 | 70 | +1 normal-attack damage |
| 6 | 100 | **Advance movement OR attack track** (next rank) |
| 7 | 135 | +2 maximum HP |
| 8 | 175 | **Advance movement OR attack track** (next rank) |
| 9 | 220 | +1 normal-attack damage |
| 10 | 270 | **Advance movement OR attack track** (next rank); cap reached |

At each choice the player sees only two options: **the next unowned perk in the movement track** and **the next unowned perk in the attack track**. Selection permanently advances that track by one rank; previously skipped ranks remain obtainable later. Five selections can produce a 5/0 specialist, a 3/2 hybrid, and everything between. A higher-tier perk never requires choosing an arbitrary earlier perk from a branching tree: it merely requires sufficient investment in that axis.

**Example:** A `Leaper + Shot` character could choose Leaper at levels 2 and 4, Shot at level 6, then Shot again at level 8. They would have Leaper ranks 1–2 and Shot ranks 1–2. At level 10 the options would be Leaper rank 3 or Shot rank 3. A different `Leaper + Shot` character could invest only in Shot to reach its capstone early.

### Curated starter tracks

The following eight five-rank tracks are a **recommended starting layout**, not a promise to implement forty separate effect handlers. Reuse the common modifier system for ordinary bonuses; reserve extra per-battle flags for truly conditional effects. Names and values are tunable. Ranks are awarded **in sequence within each track**.

#### Movement tracks

| Rank | **Standard** (1-tile orthogonal) | **Leaper** (knight-shaped leap) | **Charger** (up to 2 orthogonal tiles) |
|---:|---|---|---|
| 1 | **Steady Step:** first move each battle costs 0 HP | **Soft Landing:** first leap each battle costs 0 HP | **Easy Stride:** first move each battle costs 0 HP |
| 2 | **Battle Stance:** first incoming enemy hit each battle deals −1 damage | **Landing Strike:** automatic movement damage +1 to one adjacent target | **Momentum:** a move of 2 tiles adds +1 automatic damage to one adjacent target |
| 3 | **Engagement:** automatic movement damage +1 to one adjacent target | **Evasion:** first incoming enemy hit each battle deals −2 damage | **Pursuit:** automatic movement damage +1 against one already-injured adjacent enemy |
| 4 | **Endurance:** +3 maximum HP | **Second Wind:** restore 2 HP after victory | **Afterglow:** recover +1 additional AP after victory |
| 5 | **Veteran March:** restore 2 HP after victory | **Skyfall:** add +2 automatic damage to one target after a leap | **Overrun:** when moving 2 tiles, +1 automatic damage to *each* adjacent enemy |

**Movement-track design:** standard movement emphasizes resilience, leaping emphasizes reliable engagements and defense, and charging emphasizes making long moves worth their HP. Movement perks may improve survival or post-battle recovery; they do not need to change move geometry. A universal +1 range modifier would violate the current compact maximum-action assumptions for some classes, so leave range-changing perks in the experimental bank until the move generator and AI bounds are explicitly updated.

#### Attack tracks

| Rank | **Strike** | **Shot** | **Reach** | **Impact** | **Rend** |
|---:|---|---|---|---|---|
| 1 | **Heavy Hand:** +1 normal damage | **First Mark:** +1 normal damage vs full-HP targets | **Extended Pressure:** +1 normal damage at range 2 | **Solid Hit:** +1 normal damage | **Wound Sense:** +1 normal damage vs injured targets |
| 2 | **Executioner:** +2 normal damage vs targets at half HP or less | **Reservoir:** +1 maximum AP | **Supporting Thrust:** +1 normal damage vs targets next to an ally | **Wall Breaker:** +2 normal damage if knockback is obstructed | **Fierce Technique:** first special each battle costs −1 AP (minimum 1) |
| 3 | **Unlock Execution** as a second special | **Unlock Deadeye** as a second special | **Unlock Impale** as a second special | **Unlock Shatter Blow** as a second special | **Unlock Blood Rush** as a second special |
| 4 | **Afterglow:** +1 AP recovered after victory | **Efficient Aim:** first special each battle costs −1 AP (minimum 1) | **Reservoir:** +1 maximum AP | **Iron Will:** first incoming enemy hit each battle deals −1 damage | **Afterglow:** +1 AP recovered after victory |
| 5 | **Mastery:** +1 to special damage | **Deadeye:** +2 special damage vs isolated targets | **Perfect Reach:** +2 normal damage at range 2 | **Siegebreaker:** +2 special damage when a push succeeds | **Relentless:** +2 special damage while below half HP |

**Attack-track design:** each class has a meaningful starter special at level 1. Attack rank **3 unlocks a second special**; a movement-focused character retains the starter and simply delays that second option. The named second specials are specified in section 7, and all use a primary enemy target. Only one attack-class special may be used per action, and AP is spent once.

**Balance caution:** A rank-5 specialization is reached only if the player spends all five perk choices on that axis. It is therefore okay for rank 5 to be relatively exciting, but a 3/2 hybrid should remain viable. Some suggested late perks need transient first-use flags or conditional hit checks; initially implement one or two ranks per class, and add remaining ranks as progression content supports them.

### Experience and interface

**Experience:** Prefer cumulative XP **per active member**, with the full XP award given to every active party member including incapacitated ones. Do not split awards four ways. New recruits can join near the current party level. A single shared party XP total remains an acceptable RAM-saving alternative if individual levels prove too costly.

**Perk screen:** At any earned choice, show `Move: <class>` with the next movement perk name/effect versus `Attack: <class>` with the next attack perk name/effect. If the player defers, keep the choice pending in the Party screen. No large perk-tree viewer is needed; optionally show `Move 2 / Attack 1` in character details. Fixed rank definitions and descriptions belong in flash.

**Persist only progress, not an entire history:** Track `movement_rank` and `attack_rank` per member, each 0–5. Together they take **six bits per character**, or **three packed bytes for a four-member party**. The number of unspent choices can be derived from earned even-level milestones minus `(movement_rank + attack_rank)`, so no per-level choice mask is required. If choices must always be made immediately, only one rank per character is needed because the sum is then known from level (about two packed bytes total). The safer deferred-choice model uses both ranks and validates that their sum never exceeds milestones earned.

**Derived values:** Compute maximum HP/AP, attack bonuses, post-battle AP recovery, and other modifiers from base class, level, chosen ranks, and equipped gear instead of storing duplicate effective stats. Keep actual current HP and AP saved; clamp them when maximum values fall. Do not grant free HP/AP merely by toggling equipment.

**Respec/class changes:** Leave both out of the first implementation. A later trainer may reset *the two track ranks* to refund earned choices, but changing a character's actual movement or attack class would require explicit handling of class-specific perks and unlocked specials.

### Growth balance

The level-10 party should be noticeably stronger without making positioning, movement sacrifice, or target choice irrelevant. XP, automatic stats, perk-track investment, and equipment each have a distinct role. A specialized late-game build can combine AP recovery perks with relics and approach a fresh AP pool every battle; the baseline remains **+1 AP after victory**, not a full reset.

## 5. Perk idea bank

These are **candidate effects for the fixed movement and attack tracks in section 4**, not a free-form perk pool and not a commitment to implement everything. Choose or replace a named track-rank effect deliberately. Movement-track candidates usually affect movement, defense, positioning, HP, and expedition recovery; attack-track candidates usually affect normal attacks, special abilities, target conditions, AP efficiency, and damage. Most are passive. Numeric triggers should be visible or easy to understand. **[extension]** denotes effects that depend on mechanics not yet guaranteed to exist. **[watch]** denotes effects that may need especially careful balance or tracking.

### A. Normal attacks and finishing (10)

| Perk | Effect |
|---|---|
| **Sharp Edge** | Normal attacks deal +1 damage. |
| **First Blood** | The character's first normal attack each battle deals +2 damage. |
| **Executioner** | Normal attacks deal +2 damage to targets at half HP or less. |
| **Desperation** | Normal attacks deal +2 damage while the character is at half HP or less. |
| **Duelist** | Deal +2 normal-attack damage when no other enemy is adjacent to the target. |
| **Opportunist** | Normal attacks deal +1 damage to already-injured enemies. |
| **Clean Hit** | Normal attacks deal +1 damage when the character is at full HP. |
| **Heavy Hand** | Normal attacks deal +2 damage, but the character's maximum HP is reduced by 2. |
| **Cleave** **[extension]** | A normal attack also deals 1 damage to one enemy adjacent to its target. |
| **Armor Breaker** **[extension]** | Normal attacks ignore 1 point of enemy armor. |

### B. Movement and the automatic movement attack (12)

These perks must preserve the existing order: **move → automatic attack(s) → movement HP cost**.

| Perk | Effect |
|---|---|
| **Light Step** | The first movement action in each battle costs 1 less HP (minimum 0). |
| **Thornstep** | The automatic attack after moving deals +1 damage to one adjacent enemy, chosen by a fixed rule. |
| **Whirlwind Step** **[watch]** | The automatic attack after moving deals +1 damage to **every** adjacent enemy. |
| **Crowd Fighter** | Gain +1 movement-attack damage against each target if at least two enemies are adjacent after the move. |
| **Last Rush** | At exactly 1 HP, moving deals +2 automatic-attack damage before the actor falls. |
| **Fleetfoot** | Gain +1 movement range while above half HP. |
| **Pain Tolerance** | Movement costs 1 less HP while below half HP (minimum 0). |
| **Hunter's Step** | After moving, the adjacent enemy with the lowest HP takes 2 additional damage. |
| **Vanguard** | The first movement attack of a battle deals +2 damage to one adjacent enemy. |
| **Iron March** | +3 maximum HP; explicitly intended to finance a more aggressive movement style. |
| **Corner Cutter** **[extension]** | Once per battle, move through one occupied tile without stopping or attacking until arrival. |
| **Shockwave Step** **[extension]** | If the movement attack kills an enemy, push another adjacent enemy one tile when legal. |

**Implementation note:** For any perk that adds damage to *one* adjacent target without a new targeting prompt, choose deterministically (e.g., lowest remaining HP, stable tie-break by actor index). Never silently introduce a targeting modal for every movement action.

### C. Durability and post-battle recovery (10)

| Perk | Effect |
|---|---|
| **Vitality** | +3 maximum HP. |
| **Thick Skin** | Reduce the first instance of incoming enemy damage each battle by 2 (minimum 0). |
| **Steady Nerves** | Reduce the first two instances of enemy damage per battle by 1 each. |
| **Survivor** | If conscious when the battle ends, recover 2 HP. |
| **Field Medic** | Potions restore +2 HP when used on this character. |
| **Restful** | Town/inn recovery is cheaper for the party if this member is present. |
| **Bloodied Resolve** | Take 1 less damage from enemy attacks while at half HP or below. |
| **Last Stand** **[watch]** | Once per battle, a lethal enemy attack leaves the character at 1 HP instead; movement self-damage is **not** prevented. |
| **Toughened** | +2 maximum HP and +1 maximum AP. |
| **Battle Mender** **[watch]** | Once per battle, a normal-attack kill restores 2 HP to the attacker. |

### D. Specials and AP economy (15)

Nothing in this category restores AP **during** combat. This category includes AP capacity, ability costs/damage, and **extra AP recovery after victory**. Post-battle recovery explicitly builds on the base +1 AP rule.

| Perk | Effect |
|---|---|
| **Reservoir** | +2 maximum AP. |
| **Efficient Casting** **[watch]** | The character's first special each battle costs 1 less AP, minimum 1. |
| **Arcane Force** | Special abilities deal +1 damage to each affected enemy. |
| **Focused Power** | Single-target specials deal +2 damage. |
| **Widened Effect** **[extension]** | A designated special can affect one additional eligible adjacent target. |
| **Deep Focus** | A special deals +2 damage if the character began the action with full AP. |
| **Last Spark** | Specials deal +2 damage when their cost would leave the character at 0 AP. |
| **Channeling** | +1 maximum AP and +1 special-attack damage. |
| **Arcane Precision** | Specials deal +2 damage to enemies already below half HP. |
| **Conservative Technique** | One named special costs 1 less AP, minimum 1; affects no other special. |
| **Afterglow** | Recover **+1 additional AP** after each victory (base 1 → 2). |
| **Deep Wellspring** **[watch: late-game]** | Recover **+2 additional AP** after each victory (base 1 → 3). |
| **Arcane Heir** **[watch: late-game]** | +1 maximum AP and +1 additional AP recovered after each victory. |
| **Battle Hymn** **[watch]** | All active party members recover +1 additional AP after victory while this character has the perk; do not stack multiple copies of this party-wide bonus. |
| **Ascetic Focus** **[watch]** | Recover +1 additional AP after victory, but −1 maximum AP (minimum 1): endurance versus burst capacity. |

### E. Positioning and battlefield tactics (9)

| Perk | Effect |
|---|---|
| **Isolationist** | Deal +1 damage to a target with no allied enemies adjacent to it. |
| **Pack Hunter** | Deal +1 normal-attack damage to targets adjacent to another party member. |
| **Pressure** | Deal +1 damage to enemies adjacent to a battlefield obstacle. |
| **Hold the Line** | Reduce incoming enemy damage by 1 while next to a solid obstacle. |
| **Close Quarters** | Gain +1 normal-attack damage when surrounded by at least two enemies. |
| **Pathfinder** | The character may take one extra movement tile on their first move of each battle. |
| **Forced March** | +1 movement range, but each move costs 1 additional HP. |
| **Displacer** **[extension]** | A special may push its target one tile when the destination is open. |
| **Trapper** **[extension]** | A special may slow the target's next movement by one tile. |

### F. Party synergy (8)

| Perk | Effect |
|---|---|
| **Banner Bearer** | Other party members deal +1 damage to enemies adjacent to the bearer. |
| **Bodyguard** | The bearer takes 1 damage that would hit an adjacent ally instead **[extension: redirection]**. |
| **Battle Leader** **[watch]** | Once per battle, the first normal attack made by another ally gains +2 damage. |
| **Linked Assault** | Deal +1 normal-attack damage to an enemy next to another party member. |
| **Steady Presence** | Adjacent allies take 1 less damage from enemy attacks. |
| **Rallying Victory** | After a win, conscious party members each restore 1 HP. |
| **Supply Master** | Elixirs heal the entire party for +1 HP when this member is present. |
| **Lone Wolf** | The character deals +2 normal-attack damage while no ally is adjacent to them. |

**Stacking caution:** Party-wide auras should either be unique, nonstacking, or capped. With only one player action per turn, unlimited passive group bonuses can dominate the combat math.

### G. High-risk / sacrifice builds (9)

| Perk | Effect |
|---|---|
| **Martyr's Blow** | Movement attacks deal +2 damage if the movement HP cost will incapacitate the actor afterward. |
| **Glass Blade** | +2 normal-attack damage, −3 maximum HP. |
| **Blood Magic** **[extension]** | A special can consume 2 HP instead of 1 AP; HP is paid **after** the attack resolves. |
| **Frenzy** | At half HP or below, deal +1 with all direct attacks; take +1 from enemy attacks. |
| **Overreach** | +1 movement range and +1 movement HP cost. |
| **Doomed Strength** | At 1 HP, normal attacks deal +3 damage. |
| **Last Ember** | If the character becomes incapacitated due to movement, the automatic movement attack deals +1 to every struck enemy. |
| **Brittle Power** | Specials deal +2 damage, but maximum AP is reduced by 1 (minimum 1). |
| **Reckless Charge** | First movement attack each battle deals +3 damage, but that move costs +2 HP. |

### H. Exploration, treasure, and supplies (8)

| Perk | Effect |
|---|---|
| **Forager** | Gain one Potion when first entering a new dungeon area, subject to a reasonable per-area cap. |
| **Treasure Sense** | Show a subtle indicator when an unopened treasure cache is nearby **[extension: local chest query]**. |
| **Frugal Traveler** | Inn recovery costs 20% less, rounded down. |
| **Prospector** | Gain +10% gold from treasure chests, rounded up. |
| **Healer's Touch** | Potions used by the party heal +1 HP. |
| **Careful Packing** | Consumable stacks have a maximum of 20 instead of 15 **[watch: needs larger quantity representation]**. |
| **Sage** | Gain +1 XP for the party from significant quest objectives; not repeatable encounters. |
| **Tomb Reader** | Reveal optional hints to nearby secrets or magic-related lore **[extension: map/content support]**. |

### How to shortlist and assign perks to class tracks

The five-rank tables in section 4 are the current recommended picks. Treat every perk in this idea bank as a **possible replacement for a particular rank in one named class track**, not an extra selectable branch. For example, `Light Step` belongs naturally to Standard or Leaper movement, `Thornstep` to Charger or Leaper movement, `Executioner` to Strike attacks, and `Afterglow` can be a rank-4 reward for an attack or movement track where its AP economy makes sense.

Avoid making an entire movement track weaker than an attack track: selecting movement is a meaningful commitment, not a penalty for skipping raw damage. Movement's durability, engagement damage, and recovery can compete with attack's direct damage, extra specials, and AP cost reduction. If a new perk replaces a previously shipped rank, plan save migration or keep perk IDs/definitions stable.

**Stacking:** a movement-damage perk, movement-attack weapon, and relic may turn moving into the default best attack. Test combinations. Damage reductions for enemy attacks **do not** automatically reduce voluntary movement HP cost. The move → engagement damage → self-HP-cost order remains unchanged.

---

## 6. Equipment and unique-item idea bank

**Chosen structure: every member may equip exactly one weapon, one armor, and one relic.** These are ideas to curate, not a request to implement everything. Favor hand-authored treasures, shop basics, and visible behavior changes over random loot. **[extension]** requires additional mechanics such as directional attacks, burning, pushing, terrain tags, enemy armor, or status effects. **[watch]** is straightforward to describe but can create strong balance or tracking demands. All damage/healing/AP values below are tuning examples.

A few former "offensive relics" have become named weapons; boots, plates, mantles, and protective shields are now armor. Relics retain more unusual effects. **A weapon primarily changes attacks; armor primarily changes defense or movement; relics primarily change special rules.**

### A. Weapons — normal attacks and attack conditions (16)

| Weapon | Effect | Suggested discovery |
|---|---|---|
| **Iron Fang** | Normal attacks deal +1 damage. | First side dungeon |
| **Hunter's Blade** | Normal attacks deal +2 against an enemy already injured. | Hunter quest |
| **Duelist's Rapier** | Normal attacks deal +2 if the target has no adjacent enemy allies. | Arena |
| **Firststrike Sword** | First normal attack each battle deals +2. | Watchtower |
| **Cracked Saber** | Normal attacks deal +2; wearer loses 2 maximum HP. | Bandit hoard |
| **Executioner's Axe** | Normal attacks deal +2 against targets at half HP or less. | Crypt |
| **Rage Axe** | Normal attacks deal +2 while the wielder is at half HP or less. | Berserker challenge |
| **Veteran's Sword** | +1 normal-attack damage and +1 maximum HP. | Soldier quest |
| **Brass Knuckle** | Normal attacks deal +1 if at least two enemies surround the wielder. | Barracks |
| **Sunder Mace** **[extension]** | Normal attacks ignore 1 point of enemy armor. | Fortress |
| **Cleaver Axe** **[extension]** | Normal attack also hits the adjacent enemy with the lowest HP for 1. | Orc den |
| **Needle Spear** | Normal attacks deal +2 to enemies at full HP. | Spear master's trial |
| **Mercy Blade** | +1 normal-attack damage; if the attack defeats an enemy, restore 1 HP **after** damage resolves, once per battle. **[watch]** | Abandoned chapel |
| **Glass Dagger** | +3 normal-attack damage; −3 maximum HP. | Smuggler cache |
| **Stonebreaker** | Normal attacks deal +1 against enemies adjacent to an obstacle. | Quarry |
| **Wolf Fang** | Normal attacks deal +1 against enemies adjacent to another party member. | Wolf den |

### B. Weapons — movement attacks and positional offense (14)

A weapon can enhance the **automatic attack after movement**, but it does **not** change movement HP cost; that belongs mainly to armor. All enhancements resolve **before** the normal movement self-damage.

| Weapon | Effect | Suggested discovery |
|---|---|---|
| **Thorn Pike** | +1 automatic movement-attack damage to the lowest-HP adjacent enemy. | Overgrown ruins |
| **Wolf Claws** | +2 automatic movement-attack damage to one adjacent target. | Hunter's cache |
| **Whirlwind Glaive** **[watch]** | Automatic movement attacks deal +1 to **every** adjacent enemy. | Wind shrine |
| **Vanguard Lance** | First movement attack in battle deals +2 to one target. | Guard captain |
| **Crowd Maul** | Movement attacks deal +1 to each target if two or more enemies are adjacent. | Arena |
| **Last-Rush Blade** | When movement HP cost will incapacitate the wielder, automatic attack gains +2 to one enemy. | Cursed tomb |
| **Jagged Halberd** | Automatic movement attacks deal +1 against injured enemies. | Ravine keep |
| **Hook Sword** **[extension]** | When a normal attack hits, pull the target one square toward the wielder if legal. | Dockside ruins |
| **Ram Spear** **[extension]** | A movement attack pushes one adjacent enemy one tile if an empty destination exists. | Stone trial |
| **Reaper Scythe** | If a movement attack defeats an enemy, +1 damage to one *other* adjacent enemy, if any. **[watch]** | Haunted fields |
| **Siege Hammer** | +2 normal-attack damage against enemies adjacent to solid obstacles, but −1 movement range. | Siege armory |
| **Pathfinder's Knife** | After movement, automatic attack deals +1 to an adjacent enemy if the character moved at least two tiles. | Scout expedition |
| **Bloodied Trident** | Movement attacks deal +2 to one adjacent enemy when wearer is at 1 HP before paying the movement cost. | Drowned temple |
| **Arcblade** | +1 automatic movement-attack damage to one target **and** +1 damage to a designated special. | Arcane forge |

### C. Weapons — special-attack specialists (14)

| Weapon | Effect | Suggested discovery |
|---|---|---|
| **Apprentice Staff** | Damaging specials deal +1 damage to their selected target. | Mage's study |
| **Focused Wand** | Single-target specials deal +2 damage. | Crystal grotto |
| **Ash Rod** | Specials deal +2 if their AP cost empties the current AP pool. | Ash shrine |
| **Prismatic Staff** | Specials deal +1 to every enemy already affected by that special. **[watch: AoE scaling]** | Observatory |
| **Moon Scepter** | +1 maximum AP; damaging specials deal +1 to the selected target. | Moonwell |
| **Sacrifice Knife** | Damaging specials deal +2; each use costs the wielder 1 HP **after** damage resolves. **[watch]** | Blood altar |
| **Spellblade** | +1 normal-attack damage and +1 special damage to the selected target. | Knight-mage quest |
| **Ember Staff** **[extension]** | Damaging specials inflict a short burn on the selected target. | Sealed crypt |
| **Frost Rod** **[extension]** | Damaging specials reduce the selected target's next movement range. | Ice cave |
| **Thunder Pike** **[extension]** | One named special jumps to the closest additional enemy for 1 damage. | Storm tower |
| **Silencer's Wand** **[extension]** | Damaging specials interrupt the target's next special action. | Mage-hunter vault |
| **Longbow** **[extension: attack-range rules]** | +1 normal-attack range; no other normal-attack bonus. | Ranger's lodge |
| **Piercing Bow** **[extension: line targeting]** | Normal attack also strikes the next enemy immediately behind the selected target for 1. | Mountain outpost |
| **Ancestral Blade** | First special used each battle gets +2 damage to its selected target; later specials are unchanged. | Family tomb |

**Weapon balance note:** Avoid stacking unconditional bonuses to *all* attacks. A weapon that improves one mode—normal, movement, or special—is usually a more interesting choice than a universally best damage item. All attack modifiers need a defined target scope (selected target, one auto-selected adjacent enemy, or every enemy hit).

### D. Armor — direct defense, heavy gear, and defensive tradeoffs (16)

Armor mostly mitigates **enemy-origin damage**, not movement HP cost. Defense can be implemented without a complicated armor statistic: e.g., `first enemy hit −2` or `enemy attacks −1`, clamped to zero. An item's movement penalty modifies **movement range**, not the order in which movement damage happens.

| Armor | Effect | Suggested discovery |
|---|---|---|
| **Leather Jerkin** | +2 maximum HP. | Starter shop |
| **Guard Vest** | First incoming enemy hit each battle deals 1 less damage. | Road patrol |
| **Guardian Crest** | First incoming enemy hit each battle deals 2 less damage. | Shield shrine |
| **Iron Mail** | Enemy attacks deal 1 less damage; −1 movement range (minimum 1). | Fortress |
| **Stone Plate** | Enemy attacks deal 2 less damage; −1 movement range and +1 movement HP cost. **[watch]** | Deep mine |
| **Knight's Shield** | Enemy attacks deal 1 less damage while wearer is adjacent to an ally. | Knight's tomb |
| **Oak Shield** | Enemy attacks deal 1 less damage at half HP or below. | Sacred grove |
| **Wallguard Armor** | Enemy attacks deal 1 less damage while adjacent to a solid obstacle. | Siege ruins |
| **Traveler's Cloak** | +2 maximum HP; first enemy hit deals 1 less damage. | Caravan quest |
| **Bastion Plate** | +5 maximum HP; −1 movement range. | Castle vault |
| **Warden's Mail** | First two enemy hits each battle deal 1 less damage each. **[watch: hit counter]** | Prison fortress |
| **Thornmail** | Once per battle, when struck by an adjacent enemy, deal 1 damage back. **[extension: response hit]** | Briar shrine |
| **Paladin's Plate** | Enemy attacks deal 1 less damage while wearer has full HP. | Cathedral |
| **Ruin Cuirass** | +6 maximum HP, but Ether restores 1 less AP to the wearer (minimum 1). | Ancient vault |
| **Cracked Armor** | Enemy hits deal 2 less damage on the first hit, then 1 **more** on subsequent hits that battle. **[watch]** | Wrecked caravan |
| **Shared Shield** **[extension]** | Adjacent allies take 1 less damage from enemy attacks; nonstacking aura. | Order of the shield |

### E. Armor — boots, cloaks, mobility, and movement HP tradeoffs (16)

This is where the requested movement-focused armor belongs. Movement-attack damage bonuses can appear on boots, but should be occasional; the armor's central identity is *how it lets you move or survive movement*.

| Armor | Effect | Suggested discovery |
|---|---|---|
| **Traveler's Boots** | First move each battle costs 1 less HP (minimum 0). | Roadside cache |
| **Swift Feather Boots** | First movement action each battle has +1 movement range. | Bird shrine |
| **Iron Soles** | +3 maximum HP; −1 movement range. | Mountain pass |
| **Marcher's Mantle** | Movement costs 1 less HP at half HP or below, **minimum 1**; no effect if base cost is already 1. | Veteran's camp |
| **Hare Boots** | +1 movement range; −2 maximum HP. | Hidden grotto |
| **Jagged Spurs** | First automatic movement attack each battle deals +2 to one adjacent target. | Road ambush |
| **Whirlwind Anklet** **[watch]** | Automatic movement attack deals +1 to each adjacent enemy on the first move per battle. | Wind temple |
| **River Boots** **[extension: terrain]** | Water terrain does not reduce movement range. | Marsh village |
| **Cliffwalk Boots** **[extension: terrain]** | Difficult rocky terrain costs no extra movement distance. | Mountain shrine |
| **Dancer's Shoes** | First movement after taking an enemy hit gets +1 range. **[watch: hit flag]** | Ballroom ruins |
| **Trailrunner Cloak** | +1 movement range while above half HP. | Ranger station |
| **Lead Boots** | Enemy damage −1, but each move costs 1 extra HP. | Iron quarry |
| **Mercury Sandals** | First movement costs **0 HP**, but enemy attacks deal +1 damage to wearer until battle ends. **[watch]** | Alchemist's cache |
| **Cinder Boots** | When wearer moves at 1 HP, automatic movement damage gains +1 to one adjacent target. | Burned village |
| **Rootweave Greaves** | Moving next to an obstacle costs 1 less HP once per battle (minimum 0). | Forest shrine |
| **Wanderer's Wraps** | +1 movement range; first incoming enemy hit each battle deals +1 damage. | Nomad caravan |

### F. Armor — robes and recovery-oriented defense (12)

| Armor | Effect | Suggested discovery |
|---|---|---|
| **Soft Bandages** | Potions restore +2 HP when used on wearer. | Healer's cellar |
| **Healer's Robe** | Elixir restores +1 extra HP to wearer. | Village shrine |
| **Moonweave Robe** | +1 maximum AP and +2 maximum HP. | Astral ruins |
| **Sage's Mantle** | First hit each battle deals 2 less enemy damage; −2 maximum HP. | Wizard's tower |
| **Pilgrim's Robe** | After victory, wearer recovers 1 HP if conscious. | Pilgrim shelter |
| **Briar Vest** | Enemy attacks deal 1 less damage if two or more enemies are adjacent to wearer. | Vine maze |
| **Cloud Cloak** | +1 movement range, but enemy damage +1 while at full HP. | Sky shrine |
| **Guardian Wraps** | First incoming enemy hit each battle deals 1 less damage; +1 maximum AP. | Moonlit crypt |
| **Ash Cloak** | −1 maximum HP; normal movement HP cost reduced by 1 on the **first move only**. | Burned tower |
| **Last Stand Mail** **[watch]** | Once per battle, a lethal enemy attack leaves wearer at 1 HP; does **not** prevent movement self-damage. | Burial ground |
| **Bone Armor** | Take 1 less incoming enemy damage while at exactly 1 HP. | Catacombs |
| **Reed Armor** | Enemy damage −1 while above half HP, but −1 movement range while below half HP. | River shrine |

### G. Relics — AP capacity, specials, and post-battle replenishment (16)

AP **always persists between battles**; every active member recovers **1 AP after victory by default**. These items add to that post-battle amount, increase AP capacity, or make specials more effective. **None restores AP during combat.**

| Relic | Effect | Suggested discovery |
|---|---|---|
| **Dawn Pendant** | Recover **+1 extra AP after victory** (base 1 → 2). | Early magic shrine |
| **Wellspring Pearl** **[watch: late]** | Recover **+2 extra AP after victory** (base 1 → 3). | Deep spring |
| **Sage's Seal** | +1 maximum AP; recover +1 extra AP after victory. | Wizard's trial |
| **Harmonic Bell** **[watch]** | All active party members get +1 extra AP recovery after victory; one party-wide copy counts. | Bell tower |
| **Moon Shard** | +2 maximum AP. | Moonwell |
| **Echo Stone** | One named special costs 1 less AP (minimum 1). | Ruined library |
| **Focused Prism** | Single-target specials deal +2 damage to selected target. | Crystal cavern |
| **High Tide Shell** | First special each battle costs 1 less AP (minimum 1). **[watch]** | Coastal cave |
| **Star Vessel** | +1 maximum AP and +2 maximum HP. | Observatory |
| **Final Spark** | Selected target takes +2 special damage if the special expends remaining AP. | Ash temple |
| **Glass Focus** | Specials deal +2 to selected target; −2 maximum HP. | Sunken laboratory |
| **Sage's Thread** | Specials deal +1 damage to selected target. | Ancient study |
| **Deep Reservoir** | +3 maximum AP, but after-victory recovery is reduced by 1 (minimum 1). | Forbidden archive |
| **Balanced Hourglass** | +1 maximum AP and first normal attack each battle deals +1. | Clock tower |
| **Lunar Dial** | If ending battle at 0 AP, post-victory AP recovery gains +1. | Night shrine |
| **Shattered Prism** **[watch]** | First special of battle deals +3 to selected target, but costs +1 AP. | Collapsed temple |

### H. Relics — HP recovery, protection, and sacrifices (15)

| Relic | Effect | Suggested discovery |
|---|---|---|
| **Vital Pendant** | +3 maximum HP. | Village errand |
| **Stone Heart** | +5 maximum HP; −1 movement range. | Ancient mine |
| **Sun Medallion** | After victory, conscious wearer heals 2 HP. | Temple quest |
| **Oak Charm** | Take 1 less incoming enemy damage while below half HP. | Sacred grove |
| **Survivor's Ribbon** | After victory, conscious wearer heals 1 HP. | Refugee rescue |
| **Pilgrim's Brooch** | Inn recovery costs 25% less while equipped. | Pilgrimage |
| **Last Breath Stone** **[watch]** | Once per battle, lethal *enemy* damage leaves wearer at 1 HP; movement self-damage still applies. | Deep crypt |
| **Siphon Tooth** **[watch]** | Once per battle, normal-attack kill heals the wearer 2 HP. | Vampire crypt |
| **Bloodstep Charm** | Automatic move attack gets +2 to one adjacent target if wearer began move at 1 HP. | Cursed tomb |
| **Soft Bandage Charm** | Potions restore +2 HP to wearer. | Healer's alcove |
| **Phoenix Ash** | After winning while at 0 HP, wearer automatically revives to 1 HP **once**, then relic remains inert until town rest. **[watch: per-rest flag]** | Fire sanctum |
| **Heart of Thorns** | +2 normal-attack damage while below half HP; enemy hits deal +1 to wearer. | Thorn temple |
| **Mourner's Tear** | If a party member is incapacitated after victory, the wearer heals 3 HP if conscious. | War memorial |
| **Black Candle** | +1 damage to selected special target; post-battle HP recovery from all sources is reduced by 1 for wearer. **[watch]** | Witch's cache |
| **Grave Token** | +1 normal damage at exactly 1 HP; movement still deals its HP cost last. | Haunted cemetery |

### I. Relics — tactical positioning and party synergy (16)

| Relic | Effect | Suggested discovery |
|---|---|---|
| **Thorn Ring** | After moving, automatic attack deals +1 to the adjacent enemy with lowest HP. | Overgrown ruins |
| **Crowd Bell** | +1 automatic movement damage to every target when at least two enemies are adjacent. **[watch]** | Amphitheater |
| **Banner Fragment** | Other members deal +1 normal-attack damage to targets adjacent to wearer. **[watch: aura]** | Fallen standard |
| **Pack Totem** | Wearer deals +1 to enemies adjacent to another party member. | Beast clan |
| **Hermit's Coin** | +2 normal damage when no ally is adjacent to wearer. | Hermit's cave |
| **Wallstone** | Incoming enemy damage −1 while wearer is adjacent to solid obstacle. | Fort ruins |
| **Medic's Crest** | Elixirs heal all conscious party members +1; one party-wide bonus counts. | Field hospital |
| **Broken Crown** | +1 normal damage and +1 special damage to selected target; −3 maximum HP. | Royal tomb |
| **Mirror Bead** **[extension]** | Ignore first enemy-applied status each battle. | Mirror maze |
| **Ram Horn** **[extension]** | After moving, push the lowest-HP adjacent enemy one legal square away. | Stone trial |
| **Frost Lens** **[extension]** | Selected special target loses 1 movement range for its next move. | Ice shrine |
| **Chain Orb** **[extension]** | A named special also hits closest other enemy for 1 damage. | Storm tower |
| **Silence Sigil** **[extension]** | Selected damaging special blocks one enemy special action. | Mage-hunter vault |
| **Ember Charm** **[extension]** | Special attacks apply 1 turn of burning to their selected target. | Sealed crypt |
| **Guardian Knot** | When adjacent to wearer, another member gets +1 normal-attack damage if it is at half HP or less. | Ranger quest |
| **Hare's Tail** | +1 movement range, but −2 maximum HP. | Woodland grotto |

### J. Relics — exploration, economy, and unusual utility (13)

| Relic | Effect | Suggested discovery |
|---|---|---|
| **Trapfinder's Eye** **[extension]** | Indicate nearby hidden chests or traps in exploration. | Thieves' hideout |
| **Merchant's Scale** | Shop purchases cost 10% less while equipped. | Merchant quest |
| **Golden Beetle** | Treasure chests grant +10% gold, rounded up. | Desert chamber |
| **Lucky Pin** | +1 XP from first-time quest discoveries while equipped. | Scholar's errand |
| **Gravestone Key** **[extension]** | Opens a special crypt route; also grants +1 maximum AP. | Crypt mystery |
| **Scholar's Lens** **[extension]** | Highlights a clue or readable inscription in the current area. | Old library |
| **Wanderer's Map** **[extension]** | Marks one discovered but unopened chest in current area. | Cartographer's hut |
| **Frugal Seal** | Inn recovery costs 20% less while equipped. | Traveler's guild |
| **Pack Rat Idol** | Increase all four consumable caps by 1 each. **[watch: save packing]** | Goblin vault |
| **Mason's Token** | Grants +1 XP for the first activation of a dungeon shortcut. **[watch: flags]** | Quarry quest |
| **Silver Compass** **[extension]** | Highlights the direction to a selected quest objective if one exists in this area. | Navigator's quest |
| **Hearth Ember** | First Potion used after town recovery heals +2 extra HP. **[watch: rest flag]** | Firekeeper's house |
| **Collector's Star** | +1 maximum AP for each **two** unique relics discovered, capped at +2 AP. **[watch]** | Curator's secret |

### How to choose a shipping set

The expanded catalog is an **idea bank, not the initial content count**. It is deliberately much larger than the eventual first release. Build a roster with distinct *play patterns* instead of trying to ship every minor variation:

| Phase | Weapons | Armor | Relics | Design objective |
|---|---:|---:|---:|---|
| **First playable pass** | 4–5 | 4–5 | 4–5 | Demonstrate the three slots and different tradeoffs |
| **First substantial region** | ~8 | ~8 | ~8 | Make equipment discoveries common enough to compare builds |
| **Full campaign target, provisional** | 12–20 | 12–20 | 12–20 | Include several distinctive mid/late-game build combinations |

**Suggested first 12:** Weapons: Iron Fang, Hunter's Blade, Thorn Pike, Apprentice Staff. Armor: Leather Jerkin, Guardian Crest, Traveler's Boots, Iron Mail. Relics: Vital Pendant, Moon Shard, Dawn Pendant, Thorn Ring. Reserve **Wellspring Pearl**, powerful all-target movement boosts, and consistent damage reduction for later stages.

**Sample builds:**

- **Aggressive mover:** Thorn Pike + Traveler's Boots + Thorn Ring → more useful movement attacks, with limited movement-HP relief.
- **Defensive anchor:** Veteran's Sword + Iron Mail + Sun Medallion → predictable attacks, safer holds, and expedition healing.
- **Ability specialist:** Focused Wand + Moonweave Robe + Dawn Pendant → stronger specials, a larger AP pool, faster post-battle recovery.
- **Last-hit gambler:** Cracked Saber + Cinder Boots + Bloodstep Charm → a dangerous 1-HP movement sacrifice that can hit hard before incapacitation.

A newly discovered item should make players **reconsider at least one existing character's loadout**, even when its two classes and earned perk ranks cannot change. If every new sword is simply `+1` larger than the previous sword, the three-slot system adds menu labor without enough strategy.

---

## 7. Special abilities by **attack class**, with a first-stage set

**Ownership rule:** The **attack class** owns a character's starting special and ordered attack-track unlocks. The **movement class does not change which enemy-targeted specials are available** in the first implementation, though its perks can enhance movement attacks, defense, or AP recovery. No unified Fighter/Guardian/Scout/Mage classes are assumed. Later, a special granted by equipment or a story reward may be an explicit exception.

**Action economy:** One action uses the entire party turn. A special must provide worthwhile immediate damage or a concrete attack-plus-effect, not a weak guard that does nothing before enemies respond. Specials spend AP once; AP does not regenerate midbattle, and baseline after-victory recovery is +1 per active member. Normal attacks remain free.

### A. Initial special abilities — implement these first

These five specials are recommended for **the existing five attack classes** and use exactly the same **primary enemy target and attack range/line-of-sight legality as the class's normal attack**. They add no area-target UI, status counters, alternate movement targeting, or special range previews. Damage values are **illustrative**, using the current base attack values shown in section 1.

| Attack class | Initial special | AP | First-stage effect | Why it fits |
|---|---|---:|---|---|
| **Strike** | **Power Strike** | 2 | Normal damage **+3** (base 3 → 6) | Simple, reliable burst for close attacks |
| **Shot** | **Opening Shot** | 2 | Normal **+4** versus a full-HP target, otherwise **+1** | Makes the opening target matter at range 2–4 |
| **Reach** | **Driving Thrust** | 2 | Normal **+2**, then attempt a one-tile **knockback** | Adds an immediate positional effect using an existing battle rule |
| **Impact** | **Crushing Bash** | 2 | Normal **+3** and retain its existing **knockback** | Converts lower-damage pushing attacks into a significant burst |
| **Rend** | **Wound Hunter** | 2 | Normal **+3** versus an already-injured target, otherwise **+1**; uses Rend’s conditional **2/4** normal damage | Rewards target selection and finishing attacks |

**Initial AP suggestion:** Start party members at **4 maximum AP** for testing, enough to use two 2-AP specials from full before needing post-battle recovery or Ether. Adjust by playtesting and later attack-track/relic bonuses.

All five have a useful outcome even if their condition is not met. **Driving Thrust** uses the existing knockback semantics: if the push destination is occupied, blocked, or off-board, still deal the special's damage, with no push. **Crushing Bash** keeps the normal Impact knockback; it does not push twice. **Rend** differs from Strike even on normal attacks (2 damage, or 4 against targets strictly below half HP). **Wound Hunter** retains its separate *injured at all* condition, giving 3 damage against a full-HP enemy, 5 against an injured enemy at or above half HP, and 7 below half HP before perks/equipment. Playtest its high-end burst rather than silently changing the approved special at this stage.

**Smallest implementation slice:** Implement the shared AP expenditure, attack-variant legality, damage/effect resolution, and popup with **Power Strike** first; then populate the remaining four definitions and host tests. You can expose Strike and Shot in the starting playable party while validating Reach, Impact, and Rend through test encounters. These are **five shared class definitions**, not twenty character-specific spells.

### B. Target-first UI and rule compatibility

Enemy-targeted flow: **select acting unit → select an enemy legal for a normal attack → choose `Attack` or the available special(s) → resolve once**. In the first stage, all five starting specials inherit normal targeting, so every enemy shown for a normal attack is also valid for that unit's starting special.

```text
 TARGET: BRUTE
 > Attack
   Power Strike  2AP
 AP: 3/5
```

When AP is insufficient, retain the normal attack choice and mark the special unavailable. Show a short effect description on selection. Do not prompt for a second target. Because the current compact battle-action/search representation packs the acting unit and destination, adding an attack variant requires deliberate integration with action resolution and AI; avoid bloating every search node just to store a character's equipment or perk effects.

**Ability compatibility tags for the idea bank:**

- **[core]** Uses the selected normal-attack enemy; modifiers, conditions, self-healing, and existing knockback can resolve immediately.
- **[extension: area/line]** More than one target or extra line-cell effects; requires clear deterministic secondary targeting and AI evaluation.
- **[extension: status]** Burns, marks, silences, slows, or defense debuffs; requires transient status representation and expiration rules.
- **[extension: move]** Moves an actor or target under new rules; requires explicit board legality and movement-cost semantics.
- **[extension: ally]** A new ally target; avoid a second targeting UI until needed. An automatic ally recipient combined with damage is easier.
- **[watch]** Easy to describe but adds difficult balancing, history flags, or special-case rules.

### C. Ordered rank-3 attack special unlocks

Advancing an **attack track to rank 3** unlocks exactly one additional class-defined special, regardless of the character's level when they reach that rank. A movement-focused build can remain on its starting special indefinitely. Each character sees **at most two specials** in the initial campaign.

| Attack class | Second special (rank 3) | AP | Effect | Fit |
|---|---|---:|---|---|
| **Strike** | **Execution** | 2 | Normal +4 vs target at/below half HP; otherwise Normal +1 | [core] |
| **Shot** | **Deadeye** | 3 | Normal +5 if the target has no adjacent enemy allies; otherwise Normal +2 | [core] |
| **Reach** | **Impale** | 3 | Normal +3 at range 2; otherwise Normal +1 | [core] |
| **Impact** | **Shatter Blow** | 3 | Normal +4 and retain normal knockback | [core] |
| **Rend** | **Blood Rush** | 3 | Normal +4 when the attacker is at/below half HP; otherwise Normal +1 | [core] |

These deliberately require no new target category. The player should understand when the conditional bonus is active from the current board/HP; consider a short highlighted description before confirmation.

### D. Attack-class idea bank: direct hits and risk (12)

**Best first fit:** Strike; some may be assigned to Reach or Rend. The names are move names, not separate character classes.

| Ability | AP | Effect | Fit |
|---|---:|---|---|
| **Power Strike** | 2 | Normal +3 damage to selected enemy. | [core; first-stage Strike] |
| **Execution** | 2 | Normal +4 if target is at half HP or below; otherwise Normal +1. | [core; Strike rank 3] |
| **Heavy Opening** | 2 | Normal +4 if target is still at full HP; otherwise Normal +1. | [core] |
| **Rising Fury** | 3 | Normal +4 while attacker is at half HP or below; otherwise Normal +2. | [core] |
| **Stonebreaker** | 2 | Normal +3 against enemy adjacent to an obstacle; otherwise Normal +1. | [core] |
| **Blood Price** | 2 | Normal +4; attacker pays 2 HP **after** damage (can self-incapacitate). | [core, watch] |
| **Warrior's Feast** | 3 | Normal +2 and restore 3 HP to attacker if target takes damage. | [core] |
| **Vengeful Cut** | 2 | Normal +3 if attacker was damaged by an enemy on the previous enemy turn; else Normal +1. | [watch: history] |
| **Wide Cleave** | 3 | Normal +1 to selected target; 2 damage to lowest-HP enemy adjacent to target. | [extension: area] |
| **Sunder Blow** | 3 | Normal +2 and ignore 2 enemy armor on this hit. | [extension: armor] |
| **Staggering Smash** | 3 | Normal +2; target's next attack deals −2 damage. | [extension: status] |
| **Lunging Thrust** | 3 | Move one legal tile toward target and strike for Normal +2; define auto-hit and movement HP payment. | [extension: move] |

### E. Attack-class idea bank: force, protection, knockback (12)

**Best first fit:** Impact; some can become equipment-granted offensive techniques. No pure guarding moves: they damage an enemy on the same action.

| Ability | AP | Effect | Fit |
|---|---:|---|---|
| **Shield Bash** | 2 | Normal +2, +1 more if adjacent to an ally. | [core] |
| **Fortress Blow** | 2 | Normal +3 while adjacent to an obstacle; otherwise Normal +1. | [core] |
| **Steadfast Strike** | 2 | Normal +3 at half HP or below; otherwise Normal +1. | [core] |
| **Healing Bash** | 3 | Normal +1, restore 3 HP to attacker after hit. | [core] |
| **Defiant Slam** | 3 | Normal +3 if attacker has lowest HP in party; otherwise Normal +1. | [core] |
| **Vanguard Smash** | 2 | Normal +2; +1 more if target is adjacent to another enemy. | [core] |
| **Guardian's Mercy** | 4 | Normal +2; restore 3 HP to most injured conscious adjacent ally, if any. | [core: deterministic ally] |
| **Oathbreaker** | 3 | Normal +5 if an ally is incapacitated; otherwise Normal +2. | [core] |
| **Bulwark Slam** | 3 | Normal +2, next enemy hit on attacker −2. | [extension: status] |
| **Bastion Sweep** | 4 | Normal +1 to selected enemy; 2 to other enemies adjacent to attacker. | [extension: area] |
| **Ramming Strike** | 3 | Normal +2, push target one open tile away if legal. | [core: existing knockback] |
| **Punishing Mark** | 3 | Normal +2; target deals −2 to allies other than attacker next turn. | [extension: status] |

### F. Attack-class idea bank: precision and opportunity (12)

**Best first fit:** Shot, with some conditional attacks suitable for Rend. The class's own range and blocking rules still apply.

| Ability | AP | Effect | Fit |
|---|---:|---|---|
| **Quickshot** | 1 | Normal +1. Low-cost efficiency. | [core] |
| **Pinpoint** | 2 | Normal +4 if selected target is at full HP; otherwise Normal +1. | [core; alternative first-stage Shot option] |
| **Pack Strike** | 2 | Normal +3 if selected enemy is adjacent to another ally; otherwise Normal +1. | [core] |
| **Deadeye** | 3 | Normal +5 if target has no adjacent enemy allies; otherwise Normal +2. | [core] |
| **Wound Hunter** | 2 | Normal +3 to injured target; otherwise Normal +1. | [core; first-stage Rend] |
| **Last Arrow** | 2 | Normal +4 if action spends attacker's last AP; otherwise Normal +2. | [core] |
| **Knife in the Gap** | 2 | Normal +3 if target is adjacent to solid obstacle; otherwise Normal +1. | [core] |
| **Close Pursuit** | 2 | Normal +2 if attacker is adjacent to 2+ enemies; otherwise Normal +1. | [core] |
| **Split Shot** | 3 | Normal +1 to selected target; 2 to closest second enemy. | [extension: area] |
| **Hamstring** | 2 | Normal +1; target's next move range −1. | [extension: status] |
| **Disabling Shot** | 3 | Normal +2; target's next attack −2. | [extension: status] |
| **Hook-and-Strike** | 3 | Pull target one legal tile toward attacker, then deal Normal +2. | [extension: move] |

### G. Attack-class idea bank: reach, rend, and spacing (10)

These emphasize the two classes not represented by a traditional profession archetype. Most remain compatible with enemy-first targeting.

| Suggested class | Ability | AP | Effect | Fit |
|---|---|---:|---|---|
| Reach | **Driving Thrust** | 2 | Normal +2, then apply existing knockback if legal. | [core; first-stage Reach] |
| Reach | **Impale** | 3 | Normal +3 at range 2, otherwise Normal +1. | [core; Reach rank 3] |
| Reach | **Linebreaker** | 2 | Normal +3 if target has an empty tile directly behind it. | [core] |
| Reach | **Close Quarters** | 2 | Normal +3 at range 1, otherwise Normal +1. | [core] |
| Reach | **Skewer** | 3 | Normal +1 to selected enemy, and +2 to next enemy on same unobstructed line. | [extension: line] |
| Rend | **Blood Rush** | 3 | Normal +4 while attacker is at/below half HP, otherwise Normal +1. | [core; Rend rank 3] |
| Rend | **Relentless Cut** | 2 | Normal +3 if attacker and target are both injured; otherwise Normal +1. | [core] |
| Rend | **Finishing Rend** | 2 | Normal +4 against enemies at/below half HP; otherwise Normal +1. | [core] |
| Rend | **Risky Assault** | 2 | Normal +4; suffer 1 HP self-cost after strike. | [core, watch] |
| Rend | **Savage Sweep** | 3 | Normal +1 to selected target and +2 to one other adjacent enemy. | [extension: area] |

### H. Future magic-themed specials (14; **not** a current class)

The repository currently has **no separate magic attack class**. Keep this as an optional *future* `Arcane` attack-style catalog, or award selected spells from items/quests. Do not pretend a unified Mage class exists. Some of these could later be adapted as a second attack-class special; fixed-damage magic may require a separate damage type.

| Ability | AP | Effect | Fit |
|---|---:|---|---|
| **Flame Bolt** | 2 | Deal 4 damage to selected enemy. | [core if fixed spell damage exists] |
| **Arcane Lance** | 3 | Deal 6 damage to selected enemy. | [core if fixed spell damage exists] |
| **Stone Shard** | 2 | Deal 3 damage, or 5 if target is adjacent to obstacle. | [core if spell damage exists] |
| **Shatter** | 2 | Deal 5 to already-injured target; otherwise 3. | [core if spell damage exists] |
| **Focused Beam** | 3 | Deal 7 if target has no adjacent enemy allies; otherwise 4. | [core if spell damage exists] |
| **Overload** | 4 | Deal 9; caster pays 2 HP after damage. | [core/watch, new damage type] |
| **Drain Spark** | 3 | Deal 4 and heal caster 3 HP if target takes damage. | [core if spell damage exists] |
| **Final Ember** | 2 | Deal 6 if special expends remaining AP; otherwise 3. | [core if spell damage exists] |
| **Frost Spike** | 2 | Deal 3 and reduce target's next movement range by 1. | [extension: status] |
| **Chain Lightning** | 3 | Deal 4 to selected target, then 2 to nearest other enemy. | [extension: area] |
| **Fireburst** | 4 | Deal 4 to selected target and 3 to adjacent enemies. | [extension: area] |
| **Silencing Ray** | 3 | Deal 3; target cannot use its next special. | [extension: status] |
| **Storm Line** | 4 | Deal 5 to selected target and 2 to enemies directly between caster and target. | [extension: line] |
| **Cinder Curse** | 3 | Deal 3 and apply short burn. | [extension: status] |

### I. Cross-class and unusual specials (10)

These are candidates for future attack-class ranks, equipment-granted techniques, or quest unlocks. Never restore AP **during** battle, even on a kill.

| Ability | AP | Effect | Fit |
|---|---:|---|---|
| **Desperate Strike** | 2 | Normal +4 at 1 HP, otherwise Normal +1. | [core] |
| **Relentless Blow** | 3 | Normal +2, plus 2 if hit would otherwise leave target at 1 HP. | [core] |
| **Life Tap** | 3 | Normal +1, heal attacker 2 HP if attack deals damage. | [core] |
| **Ally's Vengeance** | 3 | Normal +4 if an ally is incapacitated; otherwise Normal +1. | [core] |
| **Crowdbreaker** | 3 | Normal +2 if target has 2+ adjacent enemies; otherwise Normal +1. | [core] |
| **Scholar's Bolt** | 2 | Fixed 4 damage at full AP; otherwise 2. | [core if spell damage exists] |
| **Merciful Flame** | 4 | Deal 4; heal lowest-HP conscious party member for 2 automatically. | [core: auto ally] |
| **Twinstrike** | 3 | Normal +1 to primary target and 2 to closest other enemy adjacent to it. | [extension: area] |
| **Dispel Strike** | 3 | Normal +1 and remove one enemy positive status. | [extension: status] |
| **Ricochet** | 4 | Deal 4 to selected target and 2 to another unobstructed enemy in the same line. | [extension: line] |

### J. Ability implementation and balance guidelines

- **Attack class determines specials**; two characters with `Shot` attacks share the Shot starting ability and unlock, regardless of whether they are Standard, Leaper, or Charger movers. Their movement perks and gear may substantially change how well they exploit that attack style.
- **Starting ability at level 1; second at attack rank 3.** No attack rank investment is required to enjoy the AP mechanic initially. Perks mostly modify passive performance; rank 3 is an intentional visible unlock.
- **One party turn per action.** Damage and immediate position changes matter more than weak buffs, guards, or purely defensive actions.
- **AP stays persistent.** Specials cost AP in battle; each active member restores base +1 after victory, plus perk/item bonuses. Ether and safe-area recovery are out-of-battle.
- **Future movement specials:** If a class later gains a *movement-targeted* special, activate its menu only after selecting an **empty tile** and state clearly whether engagement attacks and movement HP cost still occur. This is a distinct later UI and rules feature, not part of the first stage.
- **AI/search:** The current compact action representation assumes unit + destination. Test damage, knockback, AP legality, and chosen special variant in host tests and real-device performance before widening it. Do not store redundant immutable character class/equipment data in every search node.
- **Test combinations:** For each special, compare full AP vs near-empty AP, injured vs healthy targets, different movement class pairings, and multiple relic/perk loadouts. Validate that a two-special character's popup remains readable on the 128×64 display.

## 8. Rewards and the overall economy

### The intended progression loop

1. **Hear about a place or problem** through NPC conversation, a quest, a rumor, or an unusual landmark.
2. **Set out from safety** with finite HP, AP, and consumables.
3. **Explore and take risks:** fights drain HP, and specials consume AP; each victory gives back a little AP at first. Wrong turns may still expose a secret, lore, or treasure.
4. **Earn several kinds of rewards:** modest gold and XP from regular battles; larger XP, unique weapons/armor/relics, and quest progress from discoveries and significant accomplishments.
5. **Decide whether to push on:** explore one more chamber, use supplies, retreat to town, or accept greater risk.
6. **Return and strengthen the party:** recover, purchase supplies, compare weapons, armor, and relic loadouts, and choose newly earned perks. Some upgrades increase **post-battle AP recovery**, allowing more aggressive special-ability use on future expeditions.
7. **Revisit optional challenges** with a more distinctive party, not merely inflated stats.

### Recommended XP sources

| Activity | Illustrative award | Repeats? | Design reason |
|---|---:|---|---|
| Ordinary appropriate-level encounter | 1–3 XP to each active member | Yes, but low or diminishing | Makes fighting worthwhile without dominating progression |
| Elite or dangerous encounter | 4–7 XP | Depends on encounter | Recognizes combat challenge |
| Discover a significant hidden room | 4–10 XP | **Once** | Pays players for searching |
| Complete a meaningful quest step | 3–8 XP | **Once** | Supports noncombat advancement |
| Complete a side quest | 8–18 XP | **Once** | Makes side quests attractive |
| Defeat a boss / solve a dungeon finale | 12–25 XP | **Once** | Creates a memorable growth milestone |

These are **illustrative** and should be calibrated against the level table. Assign XP to an **accomplishment**, not necessarily every interaction: finding a dungeon's hidden altar may grant XP; repeatedly walking over it should not. Quest/discovery rewards can use existing permanent flags rather than an additional generic XP-awarded log.

**Anti-grind recommendation:** Repeatable random encounters grant sharply reduced XP when party level substantially exceeds the area's intended level, with a visible message such as "No new experience." Finite quests and first-time discoveries remain the primary route to high levels. Avoid requiring kill-count grinding to beat the main story.

### Recommended gold sources and sinks

| Gold income | Example | Gold expenses | Example |
|---|---:|---|---:|
| Ordinary humanoid enemy loot | 1–4 | Potion | 12 |
| Elite enemy / bandit cache | 6–12 | Ether | 18 |
| Small chest | 10–25 | Elixir | 22 |
| Significant treasure cache | 25–50 | Revive | 30 |
| Side quest | 25–60 | Inn / safe recovery | 8–20 |
| Major quest | 50–100 | Basic equipment from shops | 35–100 |

Gold should finance **expedition readiness**, not replace the most exciting dungeon rewards. Don't make every creature drop coins: a skeleton may guard treasure but should not necessarily carry a wallet. Give the player reliable ways to recover in towns and avoid charging enough for healing that an unlucky run creates a poverty spiral.

**Gold sinks to consider later:** Limited permanent training, repairs to a specific bridge or settlement service, purchasing a map hint, or paying a guide for a shortcut. Training should be capped or finite; unlimited purchasable stats would make repeatable gold farming the dominant progression strategy.

### Loot distribution and pacing

- Put the **first consumable chest** on the early main path so the player learns what supplies do.
- Offer the **first optional unique relic** before the first major dungeon, ideally behind an obvious but optional detour.
- Give optional dungeons a clearly signaled *headline reward*: a named weapon, armor, relic, special-ability upgrade or equipment-granted technique, or unusual gameplay-changing item, rather than only more gold.
- Use modest immediate rewards in the middle of a dungeon and a distinctive reward at its end.
- Ensure secret rooms have at least one meaningful payoff: permanent treasure, first-time XP, quest clues, or shortcut access.
- Allow some noncombat resolutions of quests to grant comparable XP, preventing battle from being the only route to progression.
- Keep early treasure frequency high enough to teach the system, then space distinctive equipment finds so each discovery has time to affect play.
- Treat hidden rooms, encounter variants, and puzzle routes as content features; reward tables should not require procedural item drops.

### Sample optional dungeon: The Abandoned Crypt

1. **Entrance:** An optional side hall contains 16 gold and a Potion, paying off exploration immediately.
2. **Guarded corridor:** A difficult encounter awards ordinary combat XP and costs HP and AP. Victory restores **1 AP per member** by default, but heavy special-ability use still leaves a net AP deficit.
3. **Quiet chamber:** The player finds a faded inscription that hints at the game's weakening magic; reading it once grants discovery XP and may advance a quest flag.
4. **Optional side puzzle:** Opening a concealed alcove gives an Elixir or Ether, letting the party press further.
5. **Guardian:** A one-time elite battle grants a substantial XP award. One member may reach a perk level after the battle.
6. **Final treasure:** The unique **Ember Charm** relic (or a baseline-compatible **Focused Prism** relic until burning exists). This alters future build decisions rather than being immediately sold.

This dungeon gives the player **money, recovery, experience, lore, and unique equipment**. Another dungeon might instead award a mobility armor piece or a new attack-focused weapon; major rewards should rotate across all three slots. Each reward matters for a different reason.

---

## 9. Constraints, balance hazards, and save-data shape

### Core balance checks

- **Movement must remain costly.** Reducing movement cost is meaningful; stacking reductions until movement is free all the time can collapse the intended tradeoff.
- **Sacrifice must resolve correctly.** At 1 HP, the move and automatic damage happen before movement HP loss. A perk that enhances suicidal movement should trigger before the loss, not after.
- **No AP regeneration during a battle.** Base recovery is **1 AP per active member after victory**; perks and equipment (primarily relics) may increase that post-battle amount. Increasing maximum AP and reducing special costs are separate upgrade paths. AP-on-kill or turn-by-turn AP restoration is not part of this design.
- **The action economy is precious.** Do not add perks or special abilities consisting solely of a weak guard, a tiny buff, or another action that sacrifices a full party turn for no immediate advantage.
- **Beware of multiplicative bonuses.** A +1 perk, +1 relic, and +1 party aura can substantially alter a low-HP tactical system. Cap identical effects or avoid stacking the same type of bonus.
- **Avoid denial of recovery.** There should always be some nonconsumable way to bring an incapacitated party back to service after retreating to town. AP recovery bonuses must never exceed maximum AP, and Ether cannot be consumed at full AP.
- **Avoid essential-item hoarding traps.** If a quest requires a specific item, represent it as a quest flag rather than requiring an inventory slot that can be filled, dropped, or sold.
- **Favor deterministic effects.** A straightforward +1 or first-hit reduction is easier to display, test, and tune than hidden critical chances or dozens of status timers.

### Illustrative persistent-storage budget

For **four party members, 24 distinct unlocked unique items split across weapons/armor/relics, five ordered movement-or-attack track selections per member, and four 0–15 consumable quantities**:

| New state | Example representation | Bytes |
|---|---|---:|
| Consumables | Four 4-bit quantities | 2 |
| Unlocked equipment | 24-bit bitset | 3 |
| Equipped weapons | Four 8-bit item IDs (`0` = none) | 4 |
| Equipped armor | Four 8-bit item IDs | 4 |
| Equipped relics | Four 8-bit item IDs | 4 |
| Character XP | Four 16-bit cumulative XP totals | 8 |
| Perk track ranks | Two 3-bit ranks per character; deferred choices derived | 3 |
| **Subtotal** | **Excludes existing HP/AP, party fields, flags, and transient UI state** | **28** |

Compare with the previous single-slot model: **twelve equipped item IDs rather than four** is an **8-byte** increase, before accounting for a larger unlocked-item bitset. At 12 acquired-item definitions, the bitset uses 2 bytes; at 48 it uses 6. A useful general formula for equipment state is **`12 + ceil(unique_item_count / 8)` bytes**, with one global unique ID per item and each equipped slot storing that ID. The item category and allowed slot can be inferred from the immutable definition in flash; item definitions need no persistent instance objects.

A 24-item initial collection (8 weapons, 8 armor, 8 relics) fits the 28-byte illustrated subtotal. The **much larger idea bank is not a shipping commitment**. If, hypothetically, all 148 equipment candidates were shipped, the equipment bitset would need 19 bytes but the equipped-slot total would still be 12 bytes while IDs remain under 256. The current data-layout and actual persistent RAM budget should be checked in the repository before committing to these formats.

The packed budget is an **estimate, not a claim about existing structs**. Byte-aligned consumables, perk state, or extra "choice made" flags may cost more and may be worth the simplicity. If HP, attack, defense, movement range, and AP maxima are computed from **both independent classes + level + track ranks + equipped item IDs**, **do not persist all of those derived stats separately**. Each member's current HP and AP are stored, and neither maximum is saved. The **base +1 after-victory AP recovery needs no extra saved counter**; compute the sum of base + perk + item bonuses when victory resolves, and clamp to maximum AP.

Per-battle "first hit" or "first special" equipment effects need transient per-battle flags, not persistent save bytes. An unmade perk choice requires **no separate choice-made mask** when both movement/attack ranks are saved: compare `(movement_rank + attack_rank)` against the number of even-level perk milestones already earned. Validate both ranks individually as 0–5 and reject a sum exceeding the earned milestones.

### Implementation tiers

**Tier 1 — Minimal three-slot proof of concept:** Persist current AP and restore **1 AP per active member after victory**; four supplies; out-of-battle use; **three equipped slots per character** with about **4 weapons, 4 armor, 4 relics**. Implement a simple normal-attack weapon, a first-hit defensive armor, boots that reduce movement HP once per battle, and an AP-recovery relic. Provide one **enemy-targeted special per attack class** (the five first-stage recommendations from section 7) with an AP cost, using the target-first attack-choice menu. Test save/load, clamping when equipment changes, unique-item transfers between characters, AP caps, incapacitation, and movement-damage resolution order.

**Tier 2 — Full baseline content:** Level cap 10, five **movement-or-attack track choices** per character, **~8 equipment items in each category**, quest/discovery XP, shops, optional dungeons, and a **second special unlocked by attack-track rank 3**. Add healing/defense weapons and armor tradeoffs, stronger AP-recovery relics, and clear loadout UI. Ensure viable movement-focused, attack-focused, and mixed-rank builds for the same class combination.

**Tier 3 — New mechanical vocabulary:** Area/line attacks, push/pull variants beyond existing knockback, temporary statuses, armor-penetration logic, alternate terrain traversal, richer auras, and exploration sensing. Implement only after baseline effects, AP attrition, and party-turn economics are fun.

---

## 10. Decisions to settle before implementation

1. **AP recovery is decided:** AP persists; each active member recovers **1 AP after victory**, with additive upgrades that may eventually approach a full reset. Decide when +1 and +2 equipment/perk upgrades appear, not whether the baseline resets.
2. **Equipment slots are decided:** One **weapon, armor, and relic** for each member. The open question is how many distinct items ship and whether some have class restrictions; all equipped unique items have singular ownership.
3. **Movement-focused armor is intentional:** Boots, cloaks, and heavy plate can modify movement distance, HP cost, or the automatic movement attack. Decide a consistent floor/cap so stacked reductions do not erase movement's strategic HP cost.
4. **Independent classes and two ordered perk tracks are decided:** Standard/Leaper/Charger movements and Strike/Shot/Reach/Impact/Rend attacks. Each perk milestone advances one track by one rank; rank 3 of an attack track unlocks its second special. **XP accounting open question:** Individual or shared XP? Individual XP supports member-level identities; shared XP saves space and keeps levels aligned.
5. **Special-ability UI:** Prefer target-first selection, then `Normal` versus up to two specials. Non-target specials and secondary target picking require separate UI justification.
6. **What happens to a defeated party?** Choose save-point retry, last-town return, or another policy before adding persistent HP loss.
7. **Can track investments be changed?** A modest-cost town respec could refund movement/attack ranks later, but adds UI and state rules; no class swaps initially.
8. **What happens to newly recruited members?** Let them join near current party level to avoid permanently weak recruits.
9. **Should consumables remain out-of-battle only?** Prefer yes initially; in-battle use competes with a full party turn and needs careful design.
10. **Strike versus Rend is decided for initial testing:** Both attack at range 1, but Strike deals a flat **3** and Rend deals **2**, rising to **4 only when target HP is strictly below half maximum**. The live combat code and the AI evaluation implement this; revisit balance after playtesting.

---

## 11. Recommended first implementation subset

- **Supplies:** Potion (+8 HP), Elixir (+4 HP to conscious party), Revive (to 5 HP), Ether (+3 AP). Out-of-battle only, quantities capped at 15.
- **Equipment:** **Three slots per party member — weapon, armor, relic.** Start with four of each: simple attack-altering weapons, one damage-reducing armor, one movement-focused boots or cloak, and a mix of HP/AP/unique-effect relics. Expand to ~8 per slot when content justifies it.
- **Movement armor:** Heavy gear can reduce enemy damage but lower movement range or raise movement HP cost; light gear can grant occasional range or one-time HP-cost relief. Movement attacks always happen **before** the movement HP payment.
- **Leveling:** Ten levels, small automatic stat growth, and one decision at every even level: advance the **next movement-class perk or next attack-class perk**. Five ranks per track, five choices total; earned ranks persist. Attack rank 3 unlocks the second class special.
- **AP economy:** Persist AP between fights; restore **1 after each victory** per active member by default. Reserve +1/+2 extra AP recovery for meaningful perks and (mostly) relics. No AP recovery during combat.
- **Special abilities:** One starting special per **attack class**: Strike/Power Strike, Shot/Opening Shot, Reach/Driving Thrust, Impact/Crushing Bash, Rend/Wound Hunter. Rend’s normal attack uses **2 damage, or 4 when the enemy is strictly below half HP** (Strike stays at 3), and its special modifies that conditional normal damage. All specials use an already-legal normal-attack enemy target; second special arrives at **attack rank 3**. No special-only targeting initially. Avoid pure guarding and non-immediate effects that consume the party turn.
- **XP:** Full award to active members even if incapacitated; significant quests, discovery, bosses, and one-time encounters drive progression more than repeatable grinding.
- **Gold:** Funds supplies, recovery, and straightforward shop equipment, but unique dungeon treasures should remain special.
- **Exploration:** Hand-place weapons, armor, and relics as different dungeon headlines, occasionally reward a special upgrade or extra technique (without replacing the normal attack-class progression), and tie one-off discovery XP to flags.

### Implementation status (2026-11-19)

Built:

- The movement and attack classes use the names in section 1. Rend deals 2 damage, or 4 against a target strictly below half HP, in the battle rules and AI. The Wolf enemy also uses Rend, so the shipping Grove battle has changed.
- Party members store current HP and AP. Maximum HP and AP are derived for each member: 20 HP for the traveler and 18 for Rowan for now, and 4 AP for everyone. Items and perks will change them.
- A victory restores 1 AP to every saved member, including one that fell, up to its maximum. A defeat restores none.
- The Party panel shows one page per member, turned with left and right. It shows current/maximum HP and AP, the movement and attack types, and the attack class's starting special with its AP cost.

Not built: the attack-choice menu, so specials are not battle actions and the panel lists them only; the rank-3 second special; perk tracks, levels, and XP; equipment; consumables and the Supplies menu; and maximum HP based on class instead of identity.

**North star:** After a successful expedition, players should have new tactical choices: *“Should I advance my movement style or my attack style? Which weapon improves my offense, which armor changes my defense or movement, and which relic bends the rules enough to make a new build?”*
