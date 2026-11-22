# Venlor content authoring

Use this guide when adding maps, NPCs, quests, encounters, or environmental detail.
Read the [world bible](world_bible.md) for established setting facts and the
[game design](game_design.md) for the larger project goals.

## Author for an 8×4 viewport

The Arduboy shows about eight terrain tiles across and four down, with smooth
scrolling. Walk the common routes at game zoom. In dense places, give players
something recognizable to see or reach roughly every screenful. A gap of more
than eight horizontal or four vertical tiles deserves a reason. Buildings,
fences, signs, people, changes in terrain, paths, and small secrets can all
anchor a view. Scenery can do that job on its own.

Favor compact, connected spaces with distinct intersections and occasional
breathing room. Judge density along navigable routes, including from a doorway
and on the return trip. Avoid evenly spaced ornaments. Meadows, forests, lakes
and approaches can be spacious when they establish geography or mood. Give
them an outline, recognizable landmarks, and enough variation to orient by.
Treat all these distances as heuristics.

## Settlements

Give each settlement a reason to exist, a visual identity, and a central
landmark. Group useful services near a place people would actually gather.
Arrange houses according to terrain, trades, and streets. Connect courtyards,
work yards, gardens, and districts with more than one route where space allows.
Keep thresholds and busy routes clear even when residents are walking.

Place signs where travelers make decisions. Show work through objects and
landscape, such as tools beside a workshop or a path worn down to the water.
Give residents relationships, concerns, and local knowledge. Include something
worth finding by wandering. Let important events change what people say or
where they are, or show their effects in the terrain.

## NPCs and dialogue

Give important named residents a voice, an occupation or place in the community,
and an opinion of someone nearby. Let some residents simply talk about their
work, rumors, or local history. Give each a reliable place to look for them,
bounded movement, or an intelligible reason to move. Test approaches with
the shared eight-slot actor pool full or near capacity.

Use `dialogue_show(NPC_ID, ...)` for the speaker and portrait. Store static
text in program memory. Use `dialogue_options<...>()` when distinct topics
let the player explore at their own pace. It takes two or more short, single-line
labels, scrolls through three visible options, and returns 255 when B cancels.
Reserve `dialogue_options_noback<...>()` for choices that genuinely require an
answer. A short exchange is enough for many characters. Check the actual
screen for fit and handle closing a speech page, canceling a choice, revisiting
a topic, and returning after a relevant event. Keep menu state transient.

## Quests

Vary play across investigation, exploration, conversation, decisions, combat,
mechanisms, repairs, hidden routes, and reconciliation. Use retrieval or
delivery when the story warrants the trip. Give a quest a premise, a discovery
or complication, a resolution, and a consequence someone can observe. A
clue or physical change can start a quest without an offer. Some discoveries
need no Journal entry or marker.

Connect quests to places and people. Let an outcome change a conversation,
open a route, or explain something useful elsewhere. Keep independent quests
playable in any order. Derive status and objectives from saved facts using
the [native quest registry](quests.md). Use Journal summaries to help a player
resume after a break. Mark useful destinations and leave room for deduction.
Make rewards single-use and test repeated interactions and reloads.

## Caves and dungeons

Establish why the place exists and what sets it apart. Vary geography,
branches, hazards, mechanisms, enemies, and traces of previous occupants to
fit that premise. A quiet or short cave can be as distinctive as a dangerous
one. Put rewards where a person might have left them. Use sightlines and
nearby clues to show the relationship between a mechanism and its effect.
Players should be able to reason through it from observation. Walk every
relevant configuration, including dead ends, reverse entry, changing a
lever twice, returning through a hazard, and reloading on either side of a
barrier. Preserve a way out.

## Lore and rewards

Reveal Venlor in small pieces through work, turns of phrase, inscriptions,
ruins, and objects. Let clues gain significance when players encounter other
evidence. Defeated enemies sometimes return under **The World Remembers**.
Residents can have differing accounts of what comes back and why. Preserve
room for later stories about old magic and the world's changing behavior.

Reward curiosity with supplies, equipment, gold, experience, shortcuts,
information, relationships, or visible improvements. Prefer effects that
change a choice over another numerical increase. Scale rewards to the
challenge and explain their presence through the setting.

## Play and presentation

Teach mechanics through early choices and clear landmarks. Signal danger
before contact, keep early battles manageable, and put recovery and saving
within reach. A defeat returns the player to the title screen. Use existing
art, animation, audio, services, and mechanisms before adding new systems.
Keep return walks short and let players decide what to pursue next.

Read the [outdoor guide](../assets/maps/AUTHORING.md) and the relevant
[interior](../assets/maps/AUTHORING_INTERIOR.md),
[cave](../assets/maps/AUTHORING_CAVE.md), or
[dungeon](../assets/maps/AUTHORING_DUNGEON.md) guides. The
[Tiled pipeline](tiled_pipeline.md) explains Logic markers, generated
positions, handlers, and asset generation. Use the named positions in the
owning area's code.

## Acceptance checklist

- [ ] Review generated previews and walk a few actual 8×4 views in both directions.
- [ ] Run `python tools/review_content_density.py` and inspect sparse stretches in context.
- [ ] Check navigation, doors, portal arrivals, NPC approaches, and actor retirement.
- [ ] Try alternate orders, cancellation, repeat interactions, save/load, and post-quest reactions.
- [ ] Try puzzle branches and returns, including saving on either side of a mechanism.
- [ ] Check warnings, recovery, readable dialogue, and one-time rewards.
- [ ] Update the world bible and run asset, native, and guest validation.
