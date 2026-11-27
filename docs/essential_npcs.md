# Essential NPC mechanics

One eight-slot pool and the 11x7 effective terrain cache bound NPC RAM/work.
Stable NPC identities are independent of the eight reusable actor slots. Occupied slots
form a compact prefix; retirement moves full records without resetting other actors.
`Npc` is 22 bytes and `NpcPool` is 180 bytes on AVM and the host. Definitions, routes,
character sprites, resolver functions and chunk tables remain in PROGMEM. Stage scratch
holds at most eight candidates, regardless of the identity count.

The village residents are listed in the [world bible](world_bible.md).
`npc_name(id)` returns a PROGMEM name, or null for an unknown ID.
`dialogue_show(id, F("..."))` shows that name in a fitted panel above the main
dialogue panel, aligned with its left edge. The name remains visible across pages.
An ID with a portrait in `faces32.png` (`FACE_PRESENT`) also shows it in a 32x32 tab above
the panel's left end, and the name panel moves beside it, one pixel clear of the portrait
inside a black outline. The panel, its text width and its paging do not change. The mouth
opens and closes every five letters while text types, and is closed while the panel waits
for input and while it eases in or out. The portrait slides up with the panel.
The panel is 31 pixels tall and shows four lines of text. Its last line carries the prompt at
its right end, so text on that line stops short of it. When A starts to work, half a second
after the page finishes, a bobbing arrow shows that more text follows and a blinking A button
shows the last page. A question shows the legend `A yes  B no` right-aligned instead. B still
leaves at a page of text and closes at the end.
Dialogue panels slide up from the bottom with the name panel attached, and the render
camera centers the player in the remaining view. The gameplay camera, tile cache and
simulation remain unchanged throughout the animation. Successive dialogue, questions
and recruitment announcements reuse the open panel; exploration closes it with the
same easing curve and frame count in reverse before accepting gameplay input. The shop
closes the dialogue before opening. Battle and area transitions close it before fading out.
Only the open flag and flash pointers for the visible page persist outside `Game`; no
framebuffer copy or tile reload is needed for the motion.
The text-only overloads use the unnamed panel. `dialogue_show(id, ...)` and `dialogue_ask(id, ...)` name the speaker, and NPC_NONE uses the unnamed panel.
Successful `party_add` calls for NPCs then show "<name> has joined the party!"
and play the `party_join` jingle. Failed additions and the initial traveler setup
remain silent; the announcement uses the ordinary close/dismiss prompt.

Eliza wanders where her policy places her, and Bram holds the bridge. Every resident’s dialogue
is delivered through `npc_dialogue` after `npc_talk`.

A recruit joins the four-member party through `party_add` at full HP and AP
(`party_new_member`); recruitment retires its map actor immediately, and party membership
prevents it from being cast again, including after an area change or save restoration. No
authored character is a recruit yet. Maximum HP is derived by `party_max_hp` from the base of
12, level bonuses and equipped bonuses. Maximum AP is 4. Saves validate the party count, unique
NPC identities, statistics and both perk ranks. The pause menu's Party panel shows one page per
member, turned with left and right, and freezes world simulation while it eases in and out.

`NpcDefinition` is a 12-byte ROM record: behavior/facing, period, pause/walk, leash,
priority, route, sprite_row, talkable and body. `period` is the number of frames per movement
step; Eliza uses 2, so she walks at half the player's speed. Pause and walk are frame counts.
Character rows index the actor sheet; `talkable=false` excludes a character from A targeting,
leaving nearby tile interactions available. Both NPCs and player select directional frames
through `actor_frame`: north/east/south/west groups of three, idle 0, gait 0,1,0,2.

## Bodies

`body` selects how an NPC occupies the world.

- `NPC_BODY_CHARACTER` (default): the 8x8 foot box. It yields to a pushing player as described
  under [Movement and collision](#movement-and-collision).
- `NPC_BODY_FIXED`: a character that never yields. The player cannot pass it, except to walk out
  of an existing overlap. Bram holds the only exit of the village this way.
- `NPC_BODY_STATIC`: an object drawn from `assets/images/npc_static.png`. `sprite_row` names an
  image (`NPC_STATIC_*`), and the image's 16-bit mask in `assets/tilesets/npc_static.tsx`
  (the terrain collision format) is its collision shape.

### Static objects

A static object blocks the player and other actors with its mask's solid pixels. The player's
corner assist slides past partial shapes as it does past terrain. A answers from the tile that
the player faces, and the object never turns. It moves only as scripted: `npc_push(id, dir,
bounds)` slides it one tile if the destination lies in `bounds` and in the chunk of its site and
is free of terrain, the player and other actors. `npc_move_to` and routes work as for
characters, but an object slides straight and never detours, escapes or yields. While sliding,
it reserves its destination.

Objects of a chunk live together. An object stays in the 8x4 chunk of its policy site, keeps
its slot while the terrain cache reaches that chunk or it is on screen, and then unloads with
the rest of the chunk's objects. A later cast returns it to its site, so a pushed arrangement
resets as a whole, never in part. Saving and loading keeps the arrangement. An object whose policy site changes unloads once off screen
and is cast afresh at the new site. A puzzle of objects should therefore lie inside one chunk.

## Declarative locations and presence

Each owning area header specializes `NpcPolicy<ID>` and `NpcContent<ID>` for its essential
NPCs. The stable identities in `npc_ids.h` run from 1 through `NPC_ID_LIMIT - 1`.
`NPC_ALDEN` is the player: it has a name and portrait, and its policy places it nowhere.
`NpcContent` supplies the name, definition and dialogue. Name positions in Tiled; do not duplicate coordinates in C++.
For example:

```cpp
inline bool eliza_in_village() {
    return game.get_flag(FLAG_RAT_REPORTED);
}

template <>
struct NpcPolicy<NPC_ELIZA>
    : AtOneOf<positions::ElizaAtAlden, When<positions::ElizaGarden, eliza_in_village>> {};
```

The first matching `When` wins; otherwise `current()` returns the default. Predicates
must read saved state without side effects. `AtOneOf<POS>` needs no conditional cases.
The same declaration supplies `locations` for compile-time spatial indexing. Duplicate
or out-of-bounds sites fail compilation. `NO_LOCATION` represents absence and is excluded
from the index. Missing policies and invalid template identities fail compilation.
`npc_location<ID>()` inlines the policy; `npc_location(id)` uses a dense ROM resolver table.

Every frame, after tile updates and overlap repair, `STAGE_NPCS` dispatches only cached
chunks. A site must be active under its policy and lie inside the tile cache. Inactive
alternate sites are skipped before checking their coordinates. Begin examines only active
slots, refreshes their derived `assigned_site`, and retires safely unloadable actors.
Chunk specializations check only policy sites in their
chunk and evaluate only their compile-time location policy. `npc_offer_candidate` owns
area, party, active identity, definition, cache, visibility, portal and collision checks.
The inline admission front door checks the area and active identity before party membership.
Its outlined `npc_detail::offer_candidate` slow path handles the remaining validation.
This preserves constant site IDs without putting eligibility back into `NpcSite::run`.
Candidates store their definition
priority, avoiding repeated ROM definition lookups during sorting. End admits candidates
in descending definition priority, then ascending stable ID. The buffer holds at most
eight candidates. Saturation waits without evicting visible or pinned actors;
conflicting candidates retry next frame.

Scene entry permits activation throughout the whole stage, including after a teleport.
Later activation tests the rendered 16x16 sprite at the actor origin, without the
(4,8) collision-foot offset. A shared signed rectangle test protects the current
`game_camera()` view plus `PLAYER_PIXELS_PER_FRAME` on each edge (currently one
pixel). Player corner assistance moves at most one pixel per axis per movement
iteration, so this also covers camera movement after the NPC stage and before drawing.
Exact edge contact is offscreen; intersecting even one protected pixel defers admission.
The 11x7 cache has enough margin to cache the full 8x8 feet before this clearance
is lost, including fractional camera phases. No extra tile of clearance is needed.
Ordinary retirement uses the same sprite geometry but additionally requires an uncached
footprint, and never unloads unfinished targets/routes. This deliberately stricter
lifecycle condition retains partially clipped sprites even when their feet leave the
cache. Ordinary `NPC_STAND`/`NPC_WANDER` actors also retain their slots while their
same-area logical home's full footprint remains cached. Their uncached feet freeze
simulation, preserving movement state until the cache covers them again. They unload
once both actor and home leave the cache and the sprite is safely offscreen. This avoids
retiring a local wanderer only to reconstruct it immediately at its home. A scripted departure can retire an actor when its policy has already
assigned it outside the current area. Policy changes are applied first; `NPC_TRAVEL` still completes off cache and emits its target event,
regardless of destination caching. A full pool waits without evicting retained residents,
including for a higher-priority candidate; no pressure-based eviction is added.
The retirement hook receives
`NPC_RETIRED_UNLOADED` exactly once before prefix compaction. Recruitment uses
`NPC_RETIRED_PARTY`; reset uses `NPC_RETIRED_RESET`. Reacquire `Npc*` after stage processing
because compaction can change slot addresses.

After scene entry, a frame casts only the best candidate, since several casts can overrun the
frame; the others are offered again while their sites are still out of view, which a newly
cached site stays for several frames. Scene entry casts every candidate.

Admission validates the initial world state once. The first cast checks capacity and
initializes its slot without repeating collision tests. After any successful cast,
its activation hook may have changed policy, party, camera, player, terrain or actors;
later casts revalidate these conditions, including all actor occupancy. Thus overlapping
candidates cannot both cast. Failed casts do not invalidate the initial proof. Definition
priority is sampled during admission for deterministic bounded selection; eight candidate
records now include eight priority bytes, with no identity-sized runtime storage.

Quest changes retarget active ordinary actors toward the latest assigned site, independently
of their actual coordinates, so wandering never causes spurious retargeting. Same-area
travel remains pixel-by-pixel while cached. Once safely outside the cache and view, it
completes logically and releases its slot; casting at the destination still obeys terrain,
portal, actor, party and visibility checks. Other-area and absent destinations retain the
visible actor until safe retirement. No uncached terrain is read.

Essential NPCs with unfinished `NPC_TARGET` or `NPC_ROUTE` movement keep their slots off cache. Their targets,
waypoints, detours and pauses survive until completion or scene reset. Policy changes are
applied after scripted motion completes. This can temporarily saturate the pool.

The portrait sheet groups faces by actor sprite row. `faces32.json` selects a portrait
pair for each NPC ID, and `FACE_ROW_COUNT` bounds the exported `FACE_PRESENT` table
and frame selection. NPCs without a portrait use text dialogue.
Party names come from `npc_name`.

## Motion

Every simulated position change is one pixel.
Walking, targets, routes, yields and terrain escapes all go through `step()` or the escape
step, and a frame moves an NPC at most once.

- **Cadence.** An NPC steps once every `period` frames. Timers, pauses and stuck limits count
  frames: a blocked attempt adds the NPC's period to its stuck count.
- **Escape.** A tile change can leave an NPC overlapping solid terrain. It then walks out one
  pixel at a time toward the nearest clear spot along a cardinal ray, up to a tile away. The
  distance to that spot falls by one per step, so the walk never jumps. If it cannot move,
  its ordinary logic still runs, so a blocked target can still fail.
- **Walk pose.** The walk pose holds between steps, so a walking NPC does not flicker.
- **Detours.** A steering NPC walks straight at its target. Blocked for `NPC_DETOUR_FRAMES`
  (12 frames), it sidesteps around the obstacle: toward the target across its way ahead, or
  away from it when that side is blocked. Each step it tries to rejoin the way ahead, and it
  sidesteps again while the detour's budget of `NPC_DETOUR_MAX` (15) pixels lasts. A leg
  (a route segment or a target) allows `NPC_DETOURS_PER_LEG` (3) detours. Arrival, a new
  target and a long stall start a fresh budget. A wanderer does the same on its heading,
  sidestepping toward its home, and it stops walking only when no detour is possible. The
  leash and the terrain cache are limits, not obstacles: at either one an NPC waits, or a
  wanderer stops, rather than detouring.

Wandering obeys a pixel leash around its home. Steering tries the axis toward the target
first, then the other, and a blocked NPC detours as described above. A frame, menu or
blocking dialogue freezes simulation.

## Cache margin and freezing

An NPC simulates only while its feet are inside the effective 11x7 terrain cache.
The margin lets a sprite leave view before reaching uncached terrain. A partly visible
uncached actor freezes; once safely offscreen, ordinary actors retire and travel completes
logically through slot release. Essential NPCs with unfinished targets or routes retain their movement state
and resume when cached again. No offscreen terrain lookup is introduced.

A successful foot-clear check is derived state within the active actor. Movement validates
the new foot position; escape steps discard the old answer. Tile edits and reloads that
discard edits invalidate actor answers, including edits made by synchronous hooks. Test
fixtures that write cache cells directly must call `note_tile_edit()`, just as runtime setters
do. This memoization does not skip the NPC stage or use dirty-chunk tracking.
Single-site policies also need no repeated resolver call: their fixed property is generated
from the policy itself and stored only as a derived active-actor flag.

## Movement and collision

Route waypoints remain two-byte tile coordinates expanded by TILE=16. Stand, wander, target
and once/loop/reverse routes remain supported. Each live actor updates once after the full
terrain cache, regardless of how many chunks are visible.

Player and NPC collision use an 8x8 foot box at (4,8), rather than the whole 16x16 artwork.
A partly clipped actor freezes when its feet leave cached terrain; it is never simulated
against arbitrary offscreen tiles.

After sixteen sustained push frames, two short walkaround lanes are checked. Where the player
can detour, the NPC holds position. Otherwise a safe short side step is animated; if none fits
or a yield becomes blocked, only that player/NPC pair may overlap until separation. The 8x8
footprints can fit side by side in a 16-pixel lane when aligned appropriately, so the NPC holds
still and permits an actual player detour there. Centered tight lanes still yield or allow
temporary passage. Yield search checks up to 15 pixels to cover contacts offset by as much as
seven pixels.

Rendering sorts the visible NPC slots together with the player by ascending Y. A stable
insertion sort stores at most nine byte indices on the stack, leaving the simulation pool
untouched. Lower actors draw last; equal Y preserves slot order and places the player last.

## Guarantees and how they are checked

The rules above make these properties hold for every NPC, not just the sample ones:

1. No NPC is removed while its scene lasts.
2. No NPC moves more than one pixel in a frame, except at a scene entry.
3. No NPC is cast where the player can see it, except at a scene entry.
4. An NPC comes into view only by walking there or by the camera moving.

`tests/test_npc_continuity.cpp` checks each property on every frame of random play, with
terrain edits under the NPCs. A planted one-pixel-jump fault in the escape step is caught by
it. The AVM stack budget (256 bytes) constrains the code: the lifecycle helpers and the
detour and yield helpers are kept out of line, so their frames do not grow the frames on the
deepest paths.

## Limits

- NPCs detour around an obstacle but do not plan a route. A detour sidesteps at most 15
  pixels, three times per leg. A target that lies inside an obstacle, or beyond a longer
  one, fails.
- There is no API for removing an NPC mid-scene, so a quest that needs one must end the scene
  first.
- A scene cut is not a pop. Entering an area, or teleporting the player across the map, changes
  the whole screen at once, and any NPC on the new screen appears with it.

## Hooks

The generic engine calls activation, movement-event, retirement and restore hooks declared in
`logic/npc_logic.h`. The game's activation hook resumes the giant rat's chase when it is cast
again after the story started it; the other essential hooks are empty. Events cover route start/completion/invalidity
and target reached/failed. Retirement reports reset, recruitment or safe unloading. Independent native policy
fixtures exercise route replacement, pauses, endpoints and targets without quest code inside
`src/npc.cpp`.

Saving writes the complete pointer-free live `Game` directly from `.saved`,
without a separate buffer or checksum, so the NPC pool is part of every save. Quest flags are
the authoritative location state. Restoration validates the version, player, party, flags,
area bounds and pool, then keeps each saved actor with its position, behavior and movement
state. Only the fixed-site, static and immovable bits are taken from the content again, and
terrain under the actors is checked against the rebuilt cache. No activation hook runs. The
next NPC stage retires and casts as on any other frame. Older save versions are rejected
without migration.

A hook that keeps state outside `Game` must be able to rebuild it in `npc_logic_restore`,
which runs after a load as well as after a reset.

## Enemy encounters

In Tiled, add a named point at the center of a walkable tile in the Logic layer. Rebuild the
generated assets. Define the program-memory `BattleEncounter` in the area header and register
it with the generated position and actor sprite row in the area's `TypeList`.

```cpp
using VillageEnemies = TypeList<
    EnemySpawn<positions::RatNest, &BATTLE_GIANT_RAT, 16, ENEMY_WANDER, SPEED_NORMAL, 0,
               RESPAWN_NEVER, RatHooks, RAT_SETTINGS>
>;
```

With default settings (`EnemySpawn<positions::Nest, &BATTLE, 16>`), an entry wanders up to one
tile on each axis, faces south initially, pauses and walks for 48 frames plus jitter, moves
every second frame, uses priority 2 and respawns at The World Remembers. Enemies are non-talkable and use the same eight slots, terrain checks and admission
priority as friendly actors.

`src/logic/enemy_catalog.h` joins the area lists and derives `ENEMY_SPAWN_COUNT` at compile
time. A spawn's NPC ID and defeat flag are `NPC_ENEMY_FIRST + index` and
`FLAG_ENEMY_FIRST + index`, where the index is its position in the joined list.
The save format reserves 16 defeat bits, independent of how many spawns are authored.
Unused defeat bits must be clear. Increasing that capacity or changing the saved layout
requires a save-version bump.

Edit or reorder area lists as needed during development. Insertions, removals, or
reordering can assign an existing defeat bit to a different enemy. Bump `SAVE_VERSION`
when a catalog edit removes a spawn or changes which enemy owns an existing index, so the
game rejects older saves. Bump it too when a map or NPC change would leave saved actors in
the wrong places. Appending at the end of the joined list within capacity preserves
existing indices and requires no version bump.

Choose `ENEMY_CHASE` to wander until the player comes within three tiles, then pursue the
player's current location. A five-tile separation or a six-tile home leash sends the enemy
back toward its spawn. Chase distances use Manhattan distance in pixels. `SPEED_FAST` moves
one pixel per frame, which matches the player. `SPEED_NORMAL` moves every second frame.
Pass an `EnemySettings` value as the final template argument to configure facing, pause,
walk, wander extents, detection, disengagement, leash or admission priority.
`npc_start_pursuit(id)` makes an active enemy of another behavior chase with those rules; the
rat holds still until the story starts its pursuit.

A patrol uses `ENEMY_ROUTE` and a named polyline in the same map's Routes layer. Put its points
inside walkable tiles. The first point can coincide with the spawn. The route supports
`once`, `loop` or `reverse` and a pause in frames at each waypoint. A hostile patrol releases its slot
when its sprite is safely offscreen and both its feet and spawn are outside the cache.
When normal admission casts it again, it starts its route from the spawn. A permanent guard with
dialogue before battle and a message after victory can be authored as follows.

```cpp
struct GuardHooks : EnemyHooks {
    static bool before_battle() {
        dialogue_show(F("The guardian turns toward you."));
        return true;
    }

    static void after_victory() {
        dialogue_show(F("The guardian falls still."));
    }
};

using GuardedEnemies = TypeList<
    EnemySpawn<positions::GuardPost, &BATTLE_GUARDS, 25,
               ENEMY_ROUTE, SPEED_NORMAL, routes::GuardPatrol,
               RESPAWN_NEVER, GuardHooks>
>;
```

`available()` is a cheap, nonblocking predicate for quest or world conditions. Returning
false prevents admission and lets an existing actor unload once offscreen.
`before_battle()` may show dialogue or ask a question after the simulation has unwound.
Returning false cancels battle, leaves rewards and flags alone, and releases the actor to
normal offscreen admission. Override `select_battle(default_encounter)` to choose a different
valid encounter once per contact. `victory()` can write flags while the display is black.
It cannot open UI. `after_victory()` may show dialogue after exploration has reclaimed the
workspace. Neither victory hook runs on defeat.

The shared countdown starts at 15,000 exploration frames after each respawnable victory.
At zero, the next safe exploration frame plays The World Remembers. The reset clears only
defeats marked `RESPAWN_REMEMBER` and leaves re-admission to the normal chunk stage.
`RESPAWN_NEVER` flags survive. The countdown adds two saved bytes, with defeated flags
using one bit per authored spawn within the reserved range. The live actor remains 22 bytes and the pool remains 180 bytes.

### Battlefield selection

Define an encounter with the enemy roster and a battlefield selection. The short form uses the
world tileset's defaults.

```cpp
battle_encounter(ENEMY_WOLF)
battle_encounter(BATTLEFIELDS_CAVE, ENEMY_WOLF, ENEMY_BRUTE)
battle_encounter(BATTLEFIELDS_DUNGEON, ENEMY_BRUTE)
battle_encounter(battlefields(BATTLEFIELD_GROVE, BATTLEFIELD_MINE_CROSSING), ENEMY_WOLF)
```

`battlefields_for_tileset(tileset)` selects a tileset's defaults directly. The four named
sets can be combined with `|`, and `BATTLEFIELDS_ALL` selects all boards. A one-element
selection pins the encounter to one board.

Author new boards in `src/battlefields.cpp` and add their IDs and default groups in
`src/battle.h`. Pass the tileset, board ID and four eight-character rows to `terrain()`.
Use `.` for open ground and `#` for obstacles. The board ID seeds compile-time palette
selection, giving outdoor boards patches of sparse and dense grass with roughly one
flower tile per eight open cells. Each tileset supplies its own floor, accent and obstacle
palette. The tile choices are stored in flash.

Each board has separate party and enemy rectangles in zero-based `{x, y, width, height}`
cell coordinates. They must fit the board, remain disjoint, and each contain at least four
unobstructed cells. Compile-time checks use
tileset collision metadata to validate both rectangles and generate the blocking and
screening masks used by combat.

On entry, the game selects one board uniformly and draws an open cell for each living
unit from its side's rectangle, without replacement. Battlefield and deployment rolls use the
game's shared RNG.
The rectangle metadata also identifies the legal starting area for a future party placement
screen.
