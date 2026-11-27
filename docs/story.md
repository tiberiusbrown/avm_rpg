## Start of story

At the beginning, only these menu items are unlocked:
- Resume
- Save
- Map
- Audio
- Load

Party/Supplies/Journal will be unlocked later. They have associated flags for their unlocking.

Alden is in a forest clearing, having just woken from a nap. The forst clearing is near the edge of the map, a short distance west from the village. It is surrounded by impassable terrain (mostly trees but can be rocks, cliffs, river, etc) except for one opening on the east side, which is blocked by some movable boulders (static NPCs). Alden is at the west side of the clearing from the boulder. The boulders can be pushed around (sokoban-style) to allow the player to leave the clearing. The boulders are arranged in a simple sokoban-puzzle arrangement (not too difficult) so the player has a simple puzzle to solve before leaving the clearing.

Note: we need static NPCs. Static NPCs are drawn from a separate sprite sheet of static images. Static NPCs have 16-bit collision bitmasks like tiles rather than the normal NPC collision. Static NPCs can hold position, follow routes, path to a destination, retire, etc. like normal NPCs. Their purpose is to allow for more interactive static objects in the game (e.g., objects that can move/slide).

> Alden: [dialogue about waking up, and better get back home soon.]
> Alden: Now which way was home again?

As Alden approaches the east side of the clearing (flag to make this only show up once):

> Alden: Hey! Those boulders must have rolled while I slept. The way out is blocked! Maybe I can push them back?
> Press A while facing something to interact with it.

