## NPCs

- Eliza: woman, village resident, Alden's friend
- Bram: village watchman, blocks the only exit from the village to the rest of the world

## Map

- compact village with few buildings:
  - Alden's home
  - Eliza's home
  - Bram's home (locked)
  - Shop
  - Inn
  - small assortment of other homes and buildings (to be assigned purposes later)
- all village homes connected by roads
- signs outside buildings displaying what the building is
- the village is surrounded by fence/river/trees/cliff/etc.
  - The only way out of the village is blocked by Bram at the start of the game.
- forest clearing west of the village

## Start of story

At the beginning, only these menu items are unlocked:
- Resume
- Save
- Map
- Audio
- Load

Party/Supplies/Journal will be unlocked later. They have associated flags for their unlocking.

Alden is in a large forest clearing, having just woken from a nap. The forst clearing is near the edge of the map, a short distance west from the village. It is surrounded by impassable terrain (mostly trees but can be rocks, cliffs, river, etc) except for one opening on the east side, which is blocked by some movable boulders (static NPCs). Alden is at the west side of the clearing from the boulder. The boulders can be pushed around (sokoban-style) to allow the player to leave the clearing. The boulders are arranged in a simple sokoban-puzzle arrangement (not too difficult) so the player has a simple puzzle to solve before leaving the clearing. The puzzle should be arranged such that it's impossible for the player to become trapped inside the clearing. The clearing is large enough that the player can't see all of it in the Arduboy display and can wander around for a bit. There should be some terrain variety so the clearing isn't boring to look at.

Note: we need static NPCs. Static NPCs are drawn from a separate sprite sheet of static images. Static NPCs have 16-bit collision bitmasks like tiles rather than the normal NPC collision. Static NPCs can hold position, follow routes, path to a destination, retire, etc. like normal NPCs. Their purpose is to allow for more interactive static objects in the game (e.g., objects that can move/slide).

> Alden: Wow, I sure slept a while....

After Alden wanders around a bit...

> Alden: Now which way was home again?

Start quest: Go home. Set objective to clearing exit.

> Press B to pause the game and access your journal.

Unlock Journal menu item.

As Alden approaches the east side of the clearing (flag to make this only show up once):

> Alden: Hey! Those boulders must have rolled while I slept. The way out is blocked! Maybe I can push them back?

> Press A while facing something to interact with it.

Update quest description to indicate Alden needs to push boulders to make an exit.

Once Alden leaves the clearing, change the quest objective to Alden's home (choose an appropriate village building).

Quest completes once Alden steps inside his home for a small XP reward.

## Eliza asks for help with giant rat inside her home.

Once the player enters inside his home and the quest completes: Eliza is wandering around inside his home.

> Eliza: Alden! I've been looking for you everywhere! Please, I really need your help!

Once Alden interacts with Eliza:

> Eliza: Alden... you have to help me! There's a giant rat inside my home... bigger than anything I've ever seen! Will you please get rid of it? (yes/no)

If yes:

> Eliza: Thank you so much! I'll just wait here.... You're a much better fighter than I am.
Quest start: Eliza's Giant Rat, Objective: Go to Eliza's home

If no:

> Eliza: You won't help me? Oh dear.... What am I going to do?

Eliza's home is two rooms, a kitchen/living area and a bedroom. The rat is inside the bedroom. As Alden draws close:

> Alden: There's the giant rat Eliza mentioned. It sure is big. I wonder how effective my fighting style would be against a giant rat....

Fade out to black and fade in to a panel (usual UI style) for the player to select movement and attack classes. Each movement and attack class offers a brief description of its mechanics and eventual style (i.e., the overall direction that the class's perks focus on). When the player is finished:

> Press B to access the Party menu, which gives information on each party member's classes, perks, equipment, and special abilities.

Unlock Party menu option.

The rat NPC now changes to chase mode so the player engages with it soon afterwards: combat with single giant rat in indoor environment.

When finished: update quest objective to go back to Eliza.

When Alden returns to Eliza:

> Alden: The rat is gone, Eliza.

> Eliza: Oh, thank goodness! I knew you could do it.

Quest complete: small XP and gold reward. Eliza NPC goes back to wandering around someplace in the village.
