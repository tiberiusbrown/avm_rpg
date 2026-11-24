# Venlor item catalog

Early-game equipment has engine support. All of its discovery locations are proposals.
Midgame through endgame equipment and the supply and key item candidates are ideas.
The [game logic canon](game_logic.md#41-equipment-item-catalog) records the implemented
effects and item IDs.

## Item rules and balance

- Each member has a weapon, armor, and relic slot. A weapon changes how the wearer deals damage.
  Armor covers shields, clothes, plate, and boots and changes how the wearer survives or moves.
  A relic can trade between resources, benefit the party, or help with exploration.
- Equipment is unique and transferable. A member who loses a maximum HP or AP bonus has their
  current value clamped to the new maximum. Both members must be conscious to transfer gear. Equipping a capacity bonus leaves the current value
  as it was. Store ownership by item ID and keep effect text short enough for the 128×64
  display.
- An action uses the side's turn. A voluntary move resolves engagement against adjacent enemies,
  checks for victory if it defeats the last enemy, then normally costs the mover 1 HP. Gear can
  change that price explicitly. Enemy-hit protection applies to primary hits unless the item
  names another source of damage.
- In the tables, **adjacent** means sharing an edge. Check attack conditions immediately before
  damage and landing conditions from the destination. If gear applies to just one engagement
  target, pick the eligible target with the lowest HP before damage, breaking ties by
  battle-unit index. In battle, a wearer must be conscious to grant an effect unless the item
  says otherwise.
- Give each item at most one single-use effect. It resets each battle unless the item specifies
  a different reset point. A single activation may affect several targets or have an immediate
  cost. An item can also have ongoing modifiers.
- Keep repeatable early-game weapon bonuses at +1 damage. Reserve larger bonuses for
  once-per-battle effects or conditions that are hard to set up, such as Mud Sling's need for
  both range and cover.
- Put repeatable damage reduction behind a positional condition or a movement tradeoff.
  Equipment can reduce a damaging hit to a minimum of 1 damage, including first-hit protection
  and damage redirection.
- A free move is worth both an HP and a chance to deal engagement damage. Keep cost waivers
  narrow, apply only one waiver to a move, and spend the item's use only when it actually
  reduces the cost. Range changes should name the legal geometry they affect.
- After victory, each member recovers 1 AP by default, up to their maximum. Check item
  conditions against the board and resources at victory before applying recovery, unless the
  item says otherwise. AP carries between fights. Ether restores up to 3 AP outside battle.
  Compare capacity against recovery over a chain of fights. The inn remains a free full reset.
- **Extension** marks ideas that need new targeting, world interactions, supply storage, or
  persistent state. Show a useful hint before the player commits to an altered move or attack.
  Track battle-only triggers in transient state, and budget any saved state before adding an
  item that needs it.

## Early game

**Implemented equipment.**

Millcross, the nearby fields and river, and the first trips into Hollow Mine. A roadside cache,
a modest shop purchase, or a short errand can teach a single rule. Give players a choice of
offensive gear and a reason to switch between HP capacity and direct protection.

### Weapons

| Item | Implemented effect | Proposed source |
|---|---|---|
| Firststrike Sword | The first normal attack each battle deals +2 damage. | Millcross smithy commission |
| Hunter's Blade | The first normal hit against a previously injured target deals +3 damage. | Wolf trail cache |
| Needle Spear | Normal hits against enemies at full HP deal +1 damage. | Farm tool shed |
| Cracked Saber | Normal hits deal +1 damage. The first normal hit made while the wielder is at or below half HP deals +3 damage instead. The wielder has 3 less maximum HP. | Abandoned roadside pack |
| Brass Knuckle | Normal hits deal +1 damage while at least two enemies are adjacent to the wielder. | Old patrol locker |
| Stonebreaker | Normal hits deal +1 damage if the target is adjacent to a solid obstacle. | Rubble at the mine mouth |
| Wolf Fang | Normal hits deal +1 damage if another living party member is adjacent to the target. | Reward for clearing the grove |
| Thorn Pike | Each move deals +1 engagement damage to one adjacent enemy. | Briars at the orchard edge |
| Pathfinder's Knife | On the first move that leaves cover and engages, deal +2 engagement damage to one adjacent enemy. Start next to a solid obstacle and land next to none. | Trail marker cache |
| Woodcut Axe | Normal hits deal +1 damage to a target on the battle board's outer edge. | Orchard tool rack |
| Mud Sling | A normal hit from three or more cells away deals +2 damage if the wielder stands adjacent to a solid obstacle. | Fisher's trade |
| Barn Fork | Normal hits deal +1 damage while the wielder is at full HP. | Reward for clearing the farm rats |

### Armor

| Item | Implemented effect | Proposed source |
|---|---|---|
| Guard Vest | Reduce the first enemy hit each battle by 2 damage. | Old patrol locker |
| Leather Jerkin | +3 maximum HP. | Weaver's shop |
| Oak Shield | Reduce enemy hits by 1 damage while the wearer is at or below half HP. | Grove cache |
| Reed Armor | While the wearer has no adjacent ally, reduce the first enemy hit that would deal at least 3 damage by 2. | Fisher's trade |
| River Cloak | The first enemy hit from at least three cells away deals 3 less damage. | Riverside cache |
| Ash Cloak | +1 maximum HP. The first move without engagement costs 1 less HP. | Hearthside chest |
| Scout Sandals | +1 maximum HP. The first move that ends with engagement costs 1 less HP. | Gift from a road scout |
| Soft Bandages | Potions heal the wearer for up to 2 extra HP. | Healer's shelf |
| Padded Shawl | The first enemy hit taken while adjacent to a living ally deals 3 less damage. | Weaver's errand reward |
| Lead Boots | Reduce enemy hits by 1 damage and increase each move's HP cost by 1. | Mine weigh station |
| Turnshoe | Reduce the first enemy hit taken on a board edge each battle by 3 damage. | Bridge keeper's kit |
| Hopper Vest | +1 maximum HP. Elixirs heal the wearer for up to 1 extra HP. | Farm supply chest |

### Relics

| Item | Implemented effect | Proposed source |
|---|---|---|
| Moon Shard | +1 maximum AP. | Cache off the north lane |
| Survivor's Ribbon | Restore 1 HP to the conscious wearer after each victory. | Traveler's thanks |
| Ferry Bead | At victory, heal an adjacent conscious ally for 2 HP if the wearer is still conscious. Pick the ally with the most missing HP, breaking ties by party order. | Fisher's bench |
| Thorn Ring | The first move that engages at least two enemies deals +1 damage to each adjacent enemy. | Overgrown orchard wall |
| Grave Token | Every hit the wearer lands while at exactly 1 HP deals +3 damage. | Old roadside grave |
| Sluice Tally | Gain 1 extra G after every victory while equipped, up to wallet capacity. | Orrin's keepsakes |
| Shared Cup | When the party uses a Potion on another member, the conscious wearer also heals up to 1 HP. | Bakery errand |
| Lantern Wick | The first enemy hit against a conscious ally adjacent to the wearer deals 2 less damage, once per battle. | Lantern Inn loft |
| Tessa's Rosette | +1 maximum HP. A Revive used on the wearer returns them to up to 7 HP. | Orchard errand |
| Split Button | If the wearer finishes a victory at exactly 1 HP, restore 4 HP to them. | Patch kit under the inn stairs |

## Midgame

**Equipment ideas.**

Hollow Mine's winch and maintenance routes, plus the Old Vault approach. Let weapons reward
deliberate positioning, armor offer clear tradeoffs, and relics change the cost of a longer
trip. Scatter finds across puzzles, local trades, and encounters.

### Weapons

| Item | Proposed effect | Possible source |
|---|---|---|
| Iron Fang | +1 direct attack damage. | Midgame smithy commission |
| Duelist's Rapier | Normal hits deal +2 damage if the target has no adjacent enemy allies. | Vault approach side cache |
| Jagged Halberd | Each move deals +1 engagement damage to every adjacent enemy that was already injured. | Winch room rack |
| Vanguard Lance | The first move with engagement deals +3 damage to one adjacent enemy. | Old guard post |
| Mercy Blade | Normal hits deal +1 damage. The first normal-hit kill each battle heals the wielder for 2 HP. | Side chapel |
| Cleaver Axe | A normal hit also deals 1 damage to one other enemy adjacent to the target. The secondary hit uses the target's original position. | Vault kitchen |
| Siege Hammer | Normal hits deal +3 damage to targets adjacent to a solid obstacle. Each voluntary move costs the wielder 1 extra HP. | Mine breaker room |
| Hook Sword | The first normal hit from exactly two cells away pulls the surviving target one cell toward the wielder if that cell is open. | Maintenance ladder niche |
| Crag Bow | The first normal hit from at least three cells away deals +3 damage. Later normal hits deal 1 less damage, to a minimum of 1. | Upper mine overlook |
| Sparring Mace | Normal hits deal +2 damage if the wielder is adjacent to a living ally and the target has no adjacent enemy allies. | Guardroom weapons chest |
| Signal Baton | The wielder's first normal hit against a target adjacent to another living party member gives that ally's next normal hit +2 damage. If several allies qualify, use party order. | Surveyor trade |
| Winch Hook | After a move that covers at least two cells along either axis, push one surviving adjacent enemy one cell directly away if that cell is open. No extra hit on the pushed unit. | Winch tools |
| Rage Axe | Normal hits deal +2 damage at or below half HP. While there, enemy hits against the wielder deal +1 damage. | Old Vault west corridor |

### Armor

| Item | Proposed effect | Possible source |
|---|---|---|
| Traveler's Boots | +2 maximum HP. | Supply cache near the Old Vault approach |
| Guardian Crest | Reduce the first enemy hit each battle by 3 damage. | Old guard post |
| Iron Mail | Reduce enemy hits by 1 damage. Longer moves that span at least two cells along either axis cost 1 extra HP. | Vault smithing alcove |
| Hare Boots | The first straight, orthogonal walking move each battle can go one extra cell if the whole path is clear. −2 maximum HP. **Extension** | Grotto ledge |
| Rootweave Greaves | The first move ending next to a solid obstacle costs 1 less HP. If that landing is on the board edge, the move uses its normal cost. | Roots above the mine gallery |
| Dancer's Shoes | Once per battle after an enemy hit, the wearer may take a one-cell diagonal move to an open cell at the usual HP cost. **Extension** | Weaver's challenge |
| Thornmail | After the first hit from an adjacent enemy, deal 2 damage back to the attacker if the wearer survives. | Briar-choked shaft |
| Moonweave Robe | +2 maximum HP and +1 maximum AP. | Vault study |
| Wallguard Armor | Reduce enemy hits by 1 damage while next to a solid obstacle. The first hit taken away from cover deals +1 damage. | Mine barricade |
| Briar Vest | Reduce enemy hits by 1 damage while at least two enemies are adjacent. | Overgrown sluice |
| Tether Belt | Cancel the first forced displacement from an enemy hit. Damage still resolves. | Maintenance ladder kit |
| Field Robe | After winning at or below half HP, the conscious wearer recovers 2 HP. | Abandoned mine infirmary |
| Riveted Apron | +3 maximum HP. Reduce the first hit from an adjacent enemy by 2 damage. Hits from enemies two or more cells away deal +1 damage. | Winch workshop |

### Relics

| Item | Proposed effect | Possible source |
|---|---|---|
| Dawn Pendant | Recover 1 additional AP after each victory, up to maximum AP. | Old Vault cache |
| Vital Pendant | +3 maximum HP. | Vault side room |
| Balanced Hourglass | +1 maximum AP. The first normal hit each battle deals +1 damage. | Old Vault timing mechanism |
| Lunar Dial | If the wearer ends a victory at 0 AP, recover 1 additional AP. | Vault east wing |
| Siphon Tooth | The first normal-hit kill each battle heals the wearer for 3 HP if the enemy had at least 3 HP before the attack. | Grotto pool cache |
| Bloodstep Charm | At exactly 1 HP, deal +2 engagement damage to one adjacent enemy before paying movement HP. | West cell-block cache |
| Ram Horn | On the first move that engages, push one surviving adjacent enemy one open cell directly away. Choose from enemies with a legal push, using their HP before damage. | Mine pressure-plate alcove |
| Pack Totem | If two other living party members are adjacent to the target, the wearer's normal hit deals +3 damage. | Survey campsite |
| Medic's Crest | A party Elixir heals each conscious member for up to 1 extra HP. | Old clinic drawer |
| Hermit's Coin | While the wearer starts a normal attack with no adjacent ally, deal +2 damage. The wearer takes +1 from the next enemy hit before their next action. | Hut by the crossing road |
| Merchant's Scale | Make a qualifying first-time discovery while wearing the scale to unlock one Elixir for 17 G from Ada. Hold up to one unclaimed discount. **Extension** | Provisions shop commission |
| Scholar's Lens | Examining an authored mine or vault inscription reveals an additional clue while equipped. **Extension** | Surveyor's notes reward |
| Silver Compass | Shows the direction to the current area's discovered exit nearest by distance along revealed walkable routes. **Extension** | Explorer's pack |
| Hearth Ember | If the whole deployed party survives, the conscious wearer heals 2 HP and the conscious other member with the most missing HP heals 1 HP. Break ties by party order. | Lantern Inn keepsake |

## Late game

**Equipment ideas.**

The Old Vault's deeper wings and routes that open after restoring the mine bridge or the well
lantern. Let gear reward a playstyle across several battles. A strong reward can demand more
exact positioning, take up the same slot as a favorite item, or make a different part of the
expedition harder.

### Weapons

| Item | Proposed effect | Possible source |
|---|---|---|
| Executioner's Axe | Normal hits deal +3 damage against enemies below half HP and 1 less damage otherwise, to a minimum of 1. | Vault crypt cache |
| Whirlwind Glaive | If a move engages at least two adjacent enemies, deal +1 engagement damage to each of them and pay 1 extra HP for the move. | Shrine court rack |
| Glass Dagger | Normal hits deal +3 damage. The wielder has 5 less maximum HP. | Hidden glassworker's chest |
| Piercing Bow | The first straight normal hit from at least two cells away also deals 2 damage to an enemy exactly one cell beyond the selected target on the same line. | Vault east wing overlook |
| Reaper Scythe | On the first move that defeats an enemy by engagement, one other adjacent enemy that survived its initial engagement hit takes 2 more damage. | Tended grave at the shrine |
| Last-Rush Blade | A move whose actual HP cost would incapacitate the wielder deals +3 engagement damage to one adjacent enemy. | Mine shaft memorial |
| Gate Maul | A normal hit with knockback deals +3 damage when a solid obstacle occupies the cell the target would be pushed into. | Vault gatehouse |
| Counterweight | A normal hit that moves the wielder into a legal retreat cell deals +2 damage if a conscious ally is adjacent to that cell. | Salvaged winch counterweight |
| Lantern Spear | A legal range-two normal hit that passes through a living ally deals +3 damage. | Reward for following the old lantern line |
| Toll Knife | The first normal hit after the wielder takes enemy damage deals +3 damage, once per battle. | Cell-block toll box |
| Crosscut Sword | The next normal hit after a move that deals engagement damage gains +2 damage. Moving again clears the bonus unless that move also deals engagement damage. | Vault armorer's rack |
| Longbow | The first Shoot action of the battle can make a normal attack at distance 5 along an unobstructed row or column. **Extension** | Road lookout above the vault entrance |

### Armor

| Item | Proposed effect | Possible source |
|---|---|---|
| Bastion Plate | +5 maximum HP. Moves ending next to any enemy cost 1 extra HP. | Vault armory |
| Stone Plate | Reduce enemy hits by 2 damage. Each voluntary move costs 2 extra HP. | Heavy locker beyond the mine bridge |
| Knight's Shield | Reduce enemy hits by 1 damage while adjacent to a living ally. Reduce the first such hit by 3 damage instead. | Vault knight's alcove |
| Mercury Sandals | −2 maximum HP. The first move each battle has no HP cost. | Sealed footlocker |
| Stormweave Cloak | Once per battle after a hit from at least three cells away, the wearer may extend a straight, orthogonal walking move by one cell along a clear path at the usual HP cost. **Extension** | High gallery cache |
| Reed Mantle | The first move that starts adjacent to an enemy and ends without engagement costs 1 less HP. Adjacent enemy attackers deal +1 damage to the wearer. | Reed-lined chamber below the sluice |
| Last Stand Mail | While the wearer has at least 2 HP, the first primary enemy hit each battle that would incapacitate them leaves them at 1 HP. | Deep burial chamber |
| Ruin Cuirass | +6 maximum HP and −1 maximum AP, to a minimum of 1 AP. | Vault armor stand |
| Vent Mask | Shows a countdown to the next closed interval while the party is near Hollow Mine's vent. **Extension** | Spare mine safety kit |
| Anchor Plate | Prevent forced displacement from enemy hits. Voluntary moves starting adjacent to solid obstacles cost 1 extra HP. | Old Vault blocking lever room |
| Shield Mantle | Once per battle, redirect up to 2 damage from a primary hit on an adjacent ally. Leave at least 1 damage on the ally and at least 1 HP on the wearer after the transfer. **Extension** | Shrine guard's effects |
| Vault Lining | +2 maximum HP. Reduce the first enemy splash hit that catches the wearer as a secondary target by up to 2 damage. | Vault east wing chest |

### Relics

| Item | Proposed effect | Possible source |
|---|---|---|
| Wellspring Pearl | Recover 2 extra AP after victory and reduce maximum AP by 1, to a minimum of 1 AP. | Spring below the maintenance route |
| Deep Reservoir | +3 maximum AP. The wearer's total recovery from a victory is capped at 1 AP. | Cavity in the vault archive |
| Sun Medallion | The conscious wearer recovers 2 HP after victory. | Shrine puzzle reward |
| Banner Fragment | The first normal hit made by another party member against an enemy adjacent to the wearer gains +3 damage, once per battle. | Fallen vault standard |
| Mirror Bead | The first normal hit after an enemy forcibly displaces the wearer deals +3 damage, once per battle. | Reflection pool |
| Collector's Star | +1 maximum HP for each three other relics owned, capped at +4 HP. | Survey collection reward |
| Mourner's Tear | If another deployed member falls during the battle, the conscious wearer recovers 4 HP after victory. | Vault memorial |
| Stone Heart | At full HP, the first primary hit that would deal more than 3 damage is reduced to 3. The wearer has 2 less maximum HP. | Mine ballast cache |
| Tideglass | If the wearer is conscious and at full AP before victory recovery, restore 1 extra AP to the other roster member with the largest deficit after ordinary recovery. Break ties by party order. | Old sluice chamber |
| Trapfinder's Eye | Shows the footprints of still-active, authored floor hazards on the current area map. **Extension** | Vault survey box |
| Wanderer's Map | Shows previously sighted, unopened containers on the current area map. **Extension** | Returned surveyor's reward |
| Lamp Seal | Examining either restored form of the well lantern while wearing the seal reveals a different clue to the same optional cache. **Extension** | Mine relay alcove |

## Endgame

**Equipment ideas.**

Reserve these for optional shrine challenges, the deepest Old Vault rooms, and late return
visits. Give each headline reward at least one meaningful encounter or expedition where it can
be used after acquisition.

### Weapons

| Item | Proposed effect | Possible source |
|---|---|---|
| Oathbreaker | Normal hits deal +1 damage. If any deployed ally has fallen, the first normal attack after that happens deals +4 damage instead, once per battle. | Shrine guardian's chest |
| Hollow Cleaver | After the first normal hit that defeats its target, deal 2 damage to each other enemy adjacent to the target's original cell. | Old Vault finale cache |
| Tollgate Hammer | If a normal hit pushes a target into an open cell where it becomes adjacent to another enemy, deal 2 damage to one such neighbor. Pick the one with the lowest HP, breaking ties by battle-unit index. | Gatekeeper's armory |
| Silver Fork | Normal hits deal +3 damage if both the wielder and another living party member are adjacent to the target on opposite sides. | Late smithy commission |
| King's Foil | Normal hits deal +3 damage if neither combatant has an adjacent teammate and +1 damage otherwise. The wielder has 3 less maximum HP. | Vault dueling court |
| Cinderbrand | The first normal hit each battle deals +4 damage. The wielder pays 2 HP after resolving the entire hit, which can incapacitate them. | Furnace-side weapon chest |
| Bloodied Trident | A move starting at 1 HP deals +2 engagement damage to every adjacent enemy if it engages at least two. The move uses its usual HP cost. | Flooded vault side chamber |
| Breakwater Pike | The first move that crosses at least two cells along either axis and engages at least two enemies deals +2 engagement damage to each. That move costs 1 extra HP. | Floodgate gantry |
| Bell Mace | Once per battle, after a normal hit against a surviving enemy, its next successful voluntary move costs 2 extra HP. **Extension** | Bell chamber |
| Tally Blade | Record the enemies the wearer damages with normal hits. The first normal hit against a third distinct enemy deals +4 damage. | Duplicate tally cache |
| Switchback Bow | On the first normal hit along a straight line from at least three cells away, pull the surviving target up to two cells toward the wielder through open cells. | Vault upper walkway |
| Sluice Glaive | Once per battle, when a move leaves at least two surviving adjacent enemies after engagement, push each of them one cell directly away if its landing is open. | Old waterworks cache |

### Armor

| Item | Proposed effect | Possible source |
|---|---|---|
| King's Bulwark | Reduce the first enemy hit against either the wearer or an adjacent ally by 4 damage. Voluntary moves cost 1 extra HP. | Vault guardian's chest |
| Cinder Boots | The first move at 1 HP that engages exactly one enemy costs 0 HP if that enemy survives all engagement damage. The wearer takes +1 damage from enemy hits for the rest of the battle after using the waiver. | Sealed furnace chamber |
| Windward Cloak | Once per battle, a straight, orthogonal walking move can go up to two cells farther along a clear path at a cost of 1 extra HP. **Extension** | High route beyond the shrine |
| Stoneweave Coat | Reduce enemy hits by 2 damage while adjacent to a solid obstacle. Enemy hits away from obstacles deal +1 damage. | Mason's hidden locker |
| Counterguard | Reduce the first primary enemy hit that would deal at least 5 damage by 4. | Challenge court |
| Pilgrim's Robe | After victory, restore 3 HP to the conscious wearer if they took at least two voluntary moves during the battle. | Road shrine beyond the vault |
| Hollow Plate | +4 maximum HP. The first normal hit from an adjacent enemy deals 3 less damage. The wearer receives up to 3 fewer HP from Potions. | Vault burial niche |
| Ferryman's Coat | Enemy hits from at least three cells away deal 1 less damage. The first knockback from an enemy hit moves the surviving wearer an extra cell in the same direction if both cells are open. | Old sluice keeper's chest |
| Switchyard Soles | The first move that would cost HP and ends in the wearer's own deployment cell costs 0 HP. The wearer also has +2 maximum HP. | Mine junction lockbox |
| Glass Mail | Reduce the first enemy hit each battle by up to 5 damage. Enemy hits after the first deal +1 damage. | Glass-lined vault room |
| Wake Waders | Once per battle, the wearer may step to an empty diagonal cell adjacent to a solid obstacle. Pay the usual move cost plus 1 HP. **Extension** | Waterworks ledge |
| Lantern Mantle | When the wearer's HP is strictly below half, reduce enemy hits by 1 damage. Heal the wearer for 2 HP after victory if they finish conscious and adjacent to another party member. | Late return to the lantern relay |

### Relics

| Item | Proposed effect | Possible source |
|---|---|---|
| Phoenix Ash | If the wearer falls in a victorious battle, return them to 1 HP after victory. One use per inn rest. **Extension** | Ash bed in the shrine annex |
| Harmonic Bell | If all deployed members finish a victory conscious, give up to two conscious allies 1 extra AP each after ordinary recovery. Pick the largest remaining deficits, breaking ties by party order. | Shrine bell chamber |
| Last Breath Stone | Once per battle, a move that would incapacitate the wearer leaves them at 1 HP if it defeated at least one enemy. The effect only triggers if battle continues after the move. | Memorial at the bottom of the shaft |
| Heart of Thorns | At or below half HP, normal hits deal +3 damage and enemy hits deal +2 damage to the wearer. | Briar-covered shrine |
| Broken Crown | Once per battle, if the wearer's normal hit defeats an enemy adjacent to two or more conscious allies, restore 2 HP to each of those allies. | Vault throne alcove |
| Ember Charm | The first normal hit against a surviving enemy each battle leaves an ember. If that enemy ends its next turn still occupying the marked cell, it takes 2 damage. **Extension** | Furnace ash box |
| Debt Seal | +2 maximum AP. On victory, the conscious wearer may recover 2 extra AP by paying 3 G through the results panel. **Extension** | Old Vault treasury |
| Last Light | If the wearer finishes a victory with the highest current HP, heal the other conscious party member with the lowest current HP for 3 HP. Use party order to break ties for both. | Restored lantern cache |
| Hearthstone | The first Elixir after an inn rest heals each conscious member for 2 extra HP. **Extension** | Inn loft on a late return |
| Cartographer's Seal | Shows both ends of unlocked shortcuts on discovered area maps and labels their destinations. **Extension** | Completed survey folio |
| Sluice Lens | While equipped, examining an authored mine or vault mechanism shows which barriers it controls and their current positions. **Extension** | Maintenance records vault |
| Waykeeper's Seal | At marked return posts, trade one Ether for a return trip to Lantern Inn's entrance. The trip leaves the party's resources as they were after paying the Ether. **Extension** | Old crossing-road marker |

## Supply and key item candidates

Four supplies already cover targeted healing, party healing, revival, and AP. Stock those along
the main routes. The proposed consumables below could be rare finds or exchanges. Each is an
**extension** if stocked as a separately selectable item. Keep key items tied to the relevant
room or quest state.

| Stage | Item | Proposed effect or use | Possible source |
|---|---|---|---|
| Early | Orchard Syrup | Restore up to 5 HP to one conscious member outside battle. A cheap alternative for smaller injuries. | Orchard errand or 6 G trade |
| Early | Trail Lunch | Outside battle, restore up to 3 HP to the two conscious members with the most missing HP. Ties use party order. | Bakery reward |
| Early | Tally Rubbing | Show Orrin the duplicated tally to learn the approach to one optional wall cache. | Hollow Mine ledger |
| Midgame | Peppermint Cordial | Give one conscious member up to 4 HP and 1 AP outside battle. Require missing HP or AP before use. | Provisions exchange |
| Midgame | Splint Roll | Outside battle, bring one fallen member to up to 3 HP and give them up to 1 AP. | Winch room first-aid kit |
| Midgame | Winch Handle | Restore a marked service lift that creates an optional route back to the mine entrance. | Maintenance locker |
| Late | Bitter Tonic | A conscious member spends 2 current HP to regain up to 4 AP outside battle. Require at least 3 HP and missing AP before use. | Surveyor's trade |
| Late | Lantern Oil Tin | Use beside a dark lamp in an optional passage to light it permanently and reveal chalk route markings. | Relay maintenance shelf |
| Late | Gatehouse Wedge | Fit it to a marked side door so the door stays open after the room's gate closes. | Old Vault tool box |
| Endgame | Field Kit | Outside battle, heal up to 6 HP each for the two conscious members with the most missing HP. Break ties by party order. | Shrine challenge cache |
| Endgame | Double Tally | Match it to the repeated tally mark on a vault panel to open a short optional route behind the archive. | Vault treasury records |
| Endgame | Keeper's Clapper | Restore a bell at an authored return post. Ringing it plays the local clue for the remaining shrine cache. | Shrine caretaker's chest |

## Reward pacing

- Place a common supply cache on the first main route and an optional equipment reward nearby.
  Use the first few permanent finds to show different slots and competing approaches.
- Put recovery supplies before dangerous branches, a small reward in a side room, and a
  distinctive piece of gear at the end of a meaningful detour. Rotate the headline slot across
  dungeons. Use commissions or one-time exchanges for town gear and hand-place the more unusual
  effects.
- Give early gear a reason to stay in rotation. Mud Sling can still pay off on a late battlefield
  with long sightlines and cover, and Guard Vest helps a character survive a dangerous opening.
- Keep single-target healing, party healing, and revival valuable as HP totals grow. A rare
  supply may solve a different problem or offer a tradeoff. Give each added supply enough reason
  to occupy a row on the Supplies screen.
- Compare combinations on short and long runs. Check defensive items against low-damage enemies,
  engagement bonuses against groups, and recovery equipment across consecutive fights. A 1-HP
  move, a first-hit shield, and a lethal hit followed by displacement need explicit outcomes.
- Keep unique rewards tied to their one-time chest, quest, or encounter state across The World
  Remembers. Equipment ownership shares a packed flag layout with other saved facts. Check flag
  allocation and save compatibility when selecting items to implement.
