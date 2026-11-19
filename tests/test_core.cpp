#include "rpg.h"
#include "chunk.h"
#include "world_assets.h"
#include "map_positions.h"
#include "npc.h"
#include "story_stub.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <initializer_list>

static unsigned checks;
#define CHECK(value)                                                                               \
    do {                                                                                           \
        ++checks;                                                                                  \
        if(!(value)) {                                                                             \
            fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #value);                        \
            exit(1);                                                                               \
        }                                                                                          \
    } while(0)

static unsigned dialogues;
static char const* last_dialogue;

bool dialogue_show(char const PROGMEM* text) {
    ++dialogues;
    last_dialogue = text;
    story_stub_record_dialogue(text);
    CHECK(chunk_trace_count[STAGE_TILES] > 0);
    CHECK(cached_tile(positions::GroveSign) == TILE_SIGN);
    return true;
}

bool dialogue_show(NPC_ID id, char const PROGMEM* words) {
    (void)id;
    return dialogue_show(words);
}

bool dialogue_ask(char const PROGMEM*) {
    return true;
}

bool dialogue_ask(NPC_ID id, char const PROGMEM* question) {
    (void)id;
    return dialogue_ask(question);
}

static void animate() {
    for(uint8_t row = 0; row < ACTOR_ROW_COUNT; ++row) {
        for(DIRECTION dir = NORTH; dir <= WEST; dir = (DIRECTION)(dir + 1)) {
            Actor actor{0, 0, dir, 0};
            for(uint16_t ticks = 0; ticks < 64; ++ticks) {
                CHECK(actor_frame(actor, row, ticks) == row * 12 + dir * 3);
                actor.walking = 1;
                uint8_t expected[] = {0, 1, 0, 2};
                CHECK(actor_frame(actor, row, ticks) ==
                      row * 12 + dir * 3 + expected[(ticks / WALK_POSE_TICKS) & 3]);
                actor.walking = 0;
            }
        }
    }
}

static void movement() {
    game_new();
    uint16_t const start_x = positions::PlayerStart.x * TILE;
    uint16_t const start_y = positions::PlayerStart.y * TILE;
    CHECK(game.player.x == start_x && game.player.y == start_y && game.player.facing == NORTH);
    game_update(BTN_RIGHT);
    CHECK(game.player.x == start_x + PLAYER_PIXELS_PER_FRAME && game.player.walking);
    game_update(BTN_LEFT);
    CHECK(game.player.x == start_x && game.player.facing == WEST);
    game_update(BTN_UP | BTN_DOWN | BTN_RIGHT);
    CHECK(game.player.x == start_x + PLAYER_PIXELS_PER_FRAME);
    game_update(BTN_UP | BTN_DOWN | BTN_LEFT | BTN_RIGHT);
    CHECK(!game.player.walking);
    game_new();
    CHECK(actor_move(&game.player, NORTH, 8)); // Feet can approach the solid sign.
    CHECK(game.player.y == start_y - 8);
    CHECK(!actor_move(&game.player, NORTH, 1) && !game.player.walking);
    world_enter(AREA_OUTDOOR, start_x, start_y, WEST);
    npc_reset();
    // Walk west until the tree line stops the player, whatever the walking speed.
    for(unsigned i = 0; i < 1000; ++i)
        game_update(BTN_LEFT);
    CHECK(game.player.x < start_x && player_terrain_clear(game.player.x, game.player.y));
    CHECK(!game.dimensions.pixel_bounds(65535, 0));
    CHECK(!terrain_clear(0, 0) && !terrain_clear(65535, 65535));

    game_new();
    Game first = game;
    for(unsigned i = 0; i < 500; ++i)
        game_update(i % 40 < 20 ? BTN_RIGHT : BTN_LEFT);
    Game expected = game;
    game = first;
    for(unsigned i = 0; i < 500; ++i)
        game_update(i % 40 < 20 ? BTN_RIGHT : BTN_LEFT);
    CHECK(!memcmp(&game, &expected, sizeof(Game)));
}

// Each valid interaction shows the welcome, runs one battle, and then shows an outcome line.
static void sign() {
    game_new();
    CHECK(game.can_interact);
    story_stub_reset(BATTLE_VICTORY);
    unsigned before = dialogues;
    game_update(BTN_A); // Held A is not a fresh interaction.
    CHECK(dialogues == before && story_stub_count() == 0);
    uint16_t x = game.player.x, y = game.player.y, ticks = game.ticks, rng = game.rng;
    game_update(BTN_RIGHT, BTN_A);
    CHECK(dialogues == before + 2 && story_stub_battles() == 1);
    CHECK(strstr(story_stub_event(0).text, "Nocturne Grove"));
    CHECK(game.player.x == x && game.player.y == y && game.ticks == ticks && game.rng == rng);
    CHECK(!frame_pressed);
    game_update(BTN_A);
    CHECK(dialogues == before + 2 && story_stub_battles() == 1);
    game.player.facing = EAST;
    game_update(0, BTN_A);
    CHECK(dialogues == before + 2 && story_stub_battles() == 1);
    world_enter(AREA_OUTDOOR, (positions::GroveSign.x - 1) * TILE,
                (positions::GroveSign.y + 1) * TILE, NORTH);
    game_update(0, BTN_A);
    CHECK(dialogues == before + 2 && story_stub_battles() == 1); // Diagonals do not read the sign.
    world_enter(AREA_OUTDOOR, (positions::GroveSign.x + 1) * TILE, positions::GroveSign.y * TILE,
                WEST);
    game_update(0, BTN_A);
    CHECK(dialogues == before + 4 && story_stub_battles() == 2);
}

// The welcome, the battle, and the outcome line run in that order. The outcome picks the line,
// and every valid interaction starts a fresh battle without changing overworld state.
static void grove_battle() {
    game_new();
    story_stub_reset(BATTLE_VICTORY);
    uint8_t hp = game.party.members[0].hp;
    uint16_t gold = game.gold, x = game.player.x, y = game.player.y;

    // Neither a held A nor a sign that is not faced starts a battle.
    game_update(BTN_A);
    game.player.facing = EAST;
    game_update(0, BTN_A);
    CHECK(story_stub_count() == 0 && story_stub_battles() == 0);

    game.player.facing = NORTH;
    game_update(0, BTN_A);
    CHECK(story_stub_count() == 3 && story_stub_battles() == 1);
    StoryEvent const& welcome = story_stub_event(0);
    StoryEvent const& battle = story_stub_event(1);
    StoryEvent const& outcome = story_stub_event(2);
    CHECK(welcome.kind == STORY_EVENT_DIALOGUE && strstr(welcome.text, "Nocturne Grove"));
    CHECK(battle.kind == STORY_EVENT_BATTLE && battle.encounter == &BATTLE_GROVE);
    CHECK(outcome.kind == STORY_EVENT_DIALOGUE && strstr(outcome.text, "withdraw"));
    CHECK(game.player.x == x && game.player.y == y && game.area == AREA_OUTDOOR);
    CHECK(game.party.members[0].hp == hp && game.gold == gold);

    story_stub_set_result(BATTLE_DEFEAT);
    game_update(0, BTN_A);
    CHECK(story_stub_count() == 6 && story_stub_battles() == 2);
    CHECK(story_stub_event(3).kind == STORY_EVENT_DIALOGUE &&
          strstr(story_stub_event(3).text, "Nocturne Grove"));
    CHECK(story_stub_event(4).kind == STORY_EVENT_BATTLE &&
          story_stub_event(4).encounter == &BATTLE_GROVE);
    CHECK(story_stub_event(5).kind == STORY_EVENT_DIALOGUE &&
          strstr(story_stub_event(5).text, "drive you back"));
    CHECK(game.player.x == x && game.player.y == y && game.party.members[0].hp == hp);
}

static void saves() {
    game_new();
    game.gold = 37;
    game.set_flag(FLAG_RESERVED);
    Game saved = game;
    CHECK(save_valid());
    CHECK(!memcmp(&game, &saved, sizeof(Game)));

    auto reject = [] {
        Game before = game;
        CHECK(!save_valid());
        CHECK(!save_restore());
        CHECK(!memcmp(&game, &before, sizeof(Game)));
    };
    for(uint8_t version = 0; version < SAVE_VERSION; ++version) {
        game.save_version = version;
        reject();
    }
    game = saved;
    game.area = 255;
    reject();
    game = saved;
    game.player.x = 65535;
    reject();
    game = saved;
    game.player.y = 65535;
    reject();
    game = saved;
    game.player.facing = STILL;
    reject();
    game = saved;
    game.party.members[0].hp = party_max_hp(game.party.members[0]) + 1;
    reject();
    game = saved;
    game.party.members[0].ap = party_max_ap(game.party.members[0]) + 1;
    reject();
    game = saved;
    game.party.count = 0;
    reject();
    game = saved;
    game.party.count = PARTY_CAPACITY + 1;
    reject();
    game = saved;
    game.party.members[0].movement_class = MOVEMENT_CLASS_COUNT;
    reject();
    game = saved;
    game.party.members[0].attack_class = ATTACK_CLASS_COUNT;
    reject();
    game = saved;
    game.party.members[0].id = NPC_VILLAGER;
    reject();
    game = saved;
    game.party.count = 2;
    game.party.members[1] = game.party.members[0];
    reject();
    game = saved;
    game.level = 0;
    reject();
    game = saved;
    game.rng = 0;
    reject();
    game = saved;
    game.flags.bytes[WorldFlags::BYTE_COUNT - 1] |= 0x80;
    reject();
    game = saved;
    game.options.music = OPTION_VOLUME_MAX + 1;
    reject();
    game = saved;
    game.options.sfx = OPTION_VOLUME_MAX + 1;
    reject();

    game.options = {4, 8};
    game_new();
    CHECK(game.options.music == 4 && game.options.sfx == 8);
    game = saved; // AVM load replaces this live object directly, without a staging buffer.
    game.ticks = 100;
    game.player.walking = 1;
    CHECK(save_restore());
    CHECK(game.gold == 37 && game.get_flag(FLAG_RESERVED));
    CHECK(!game.ticks && !game.player.walking);
    CHECK(game.flags.get_flag(FLAG_VILLAGER_EAST) == saved.flags.get_flag(FLAG_VILLAGER_EAST));
    update_visible_world(0, false);
    CHECK(game.dimensions.width_tiles == OUTDOOR_WIDTH);
    CHECK(cached_tile(positions::GroveSign) == TILE_SIGN);

    game.player.x = positions::GroveSign.x * TILE;
    game.player.y = positions::GroveSign.y * TILE;
    CHECK(save_restore()); // Structurally valid, but inside the sign.
    update_visible_world(0, false);
    CHECK(player_terrain_clear(game.player.x, game.player.y));
    CHECK(game.player.x != positions::GroveSign.x * TILE ||
          game.player.y != positions::GroveSign.y * TILE);
}

// Every uncovered view must fit the original cache, including pixel phases and map edges.
static void dialogue_camera_bounds() {
    game_new();
    for(uint8_t height_tiles : {uint8_t(1), uint8_t(3), uint8_t(4), uint8_t(7), uint8_t(248)}) {
        game.dimensions.height_tiles = height_tiles;
        for(uint16_t y = 0; y <= (height_tiles - 1) * TILE; ++y) {
            game.player.y = y;
            camera = game_camera();
            uint8_t origin = (uint8_t)(camera.y >> TILE_SHIFT);
            if(origin >= CACHE_MARGIN)
                origin -= CACHE_MARGIN;
            Game before = game;
            uint16_t previous_y = camera.y;
            for(uint8_t covered = 0; covered <= 32; ++covered) {
                uint8_t height = 64 - covered;
                Camera view = game_camera(height);
                CHECK(view.x == camera.x);
                CHECK(view.y >= previous_y);
                CHECK(view.y >= camera.y);
                CHECK(view.y + height <= camera.y + 64);
                CHECK((view.y >> TILE_SHIFT) >= origin);
                CHECK(((view.y + height - 1) >> TILE_SHIFT) < origin + CACHE_HEIGHT);
                int16_t centered = (int16_t)y - ((int16_t)height - TILE) / 2;
                int16_t maximum = (int16_t)height_tiles * TILE - height;
                if(centered >= 0 && centered <= maximum)
                    CHECK(view.y == centered);
                previous_y = view.y;
            }
            CHECK(!memcmp(&before, &game, sizeof(Game)));
        }
    }
    game_new();
}

// Maximum HP and AP come from each member's identity and classes, so no member stores them.
static void party_statistics() {
    game_new();
    PartyMember traveler = game.party.members[0];
    CHECK(traveler.hp == 20 && party_max_hp(traveler) == 20);
    CHECK(traveler.ap == 4 && party_max_ap(traveler) == 4);

    PartyMember rowan = party_new_member(NPC_ROWAN, MOVEMENT_STEP, ATTACK_SHOOT);
    CHECK(rowan.hp == 18 && party_max_hp(rowan) == 18);
    CHECK(rowan.ap == 4 && party_max_ap(rowan) == 4);
    CHECK(rowan.movement_class == MOVEMENT_STEP && rowan.attack_class == ATTACK_SHOOT);
    CHECK(party_max_hp({0, 0, MOVEMENT_LEAP, ATTACK_REND, NPC_VILLAGER, 1, 0}) == 20);

    // HP or AP above the member's maximum is rejected, and the party does not change.
    Party before = game.party;
    CHECK(!party_add({21, 4, MOVEMENT_STEP, ATTACK_STRIKE, NPC_VILLAGER, 1, 0}));
    CHECK(!party_add({20, 5, MOVEMENT_STEP, ATTACK_STRIKE, NPC_VILLAGER, 1, 0}));
    CHECK(!memcmp(&before, &game.party, sizeof(Party)));
    CHECK(save_valid());
}

// Experience is per level and resets at each level-up. Completed levels are processed at once.
static void party_progression() {
    game_new();
    PartyMember member = party_new_member(NPC_ROWAN, MOVEMENT_STEP, ATTACK_SHOOT);
    CHECK(member.level == 1 && member.experience == 0);
    CHECK(party_experience_needed(1) == 10 && party_experience_needed(9) == 50);
    CHECK(party_victory_experience(member) == 2);

    // Odd-level rewards: maximum HP rises at levels 3 and 7, from the traveler's 20 or Rowan's 18.
    PartyMember traveler_at = {20, 4, MOVEMENT_STEP, ATTACK_STRIKE, NPC_NONE, 1, 0};
    uint8_t traveler_hp[PARTY_MAX_LEVEL] = {20, 20, 22, 22, 22, 22, 24, 24, 24, 24};
    uint8_t rowan_hp[PARTY_MAX_LEVEL] = {18, 18, 20, 20, 20, 20, 22, 22, 22, 22};
    // Normal-attack bonuses arrive at levels 5 and 9.
    uint8_t attack[PARTY_MAX_LEVEL] = {0, 0, 0, 0, 1, 1, 1, 1, 2, 2};
    for(uint8_t level = 1; level <= PARTY_MAX_LEVEL; ++level) {
        traveler_at.level = level;
        member.level = level;
        CHECK(party_max_hp(traveler_at) == traveler_hp[level - 1]);
        CHECK(party_max_hp(member) == rowan_hp[level - 1]);
        CHECK(party_attack_bonus(member) == attack[level - 1]);
    }
    member.level = 1;

    CHECK(party_gain_experience(member, 8) == 0 && member.level == 1 && member.experience == 8);
    CHECK(party_gain_experience(member, 2) == 1 && member.level == 2 && member.experience == 0);
    CHECK(party_gain_experience(member, 30) == 1 && member.level == 3 && member.experience == 15);
    // The 115 total covers levels 3 through 6 (20 + 25 + 30 + 35); the remaining 5 is short of 40.
    CHECK(party_gain_experience(member, 100) == 4 && member.level == 7 && member.experience == 5);

    // Level 10 is the cap: the level-9 surplus is kept only until the cap, then discarded.
    member.level = 9;
    member.experience = 0;
    CHECK(party_gain_experience(member, 60) == 1 && member.level == PARTY_MAX_LEVEL);
    CHECK(member.experience == 0 && party_progress_valid(member));
    CHECK(party_gain_experience(member, 5) == 0 && member.level == PARTY_MAX_LEVEL &&
          member.experience == 0);

    CHECK(party_progress_valid(party_new_member(NPC_ROWAN, MOVEMENT_STEP, ATTACK_SHOOT)));
    CHECK(!party_progress_valid({18, 4, MOVEMENT_STEP, ATTACK_SHOOT, NPC_ROWAN, 0, 0}));
    CHECK(!party_progress_valid({18, 4, MOVEMENT_STEP, ATTACK_SHOOT, NPC_ROWAN, 11, 0}));
    CHECK(!party_progress_valid({18, 4, MOVEMENT_STEP, ATTACK_SHOOT, NPC_ROWAN, 1, 10}));
    CHECK(!party_progress_valid({18, 4, MOVEMENT_STEP, ATTACK_SHOOT, NPC_ROWAN, 10, 1}));

    // Recruits join at level 1, and a malformed level leaves the party unchanged.
    Party before = game.party;
    CHECK(!party_add({18, 4, MOVEMENT_STEP, ATTACK_SHOOT, NPC_VILLAGER, 0, 0}));
    CHECK(!memcmp(&before, &game.party, sizeof(Party)));
    CHECK(save_valid());
}

int main() {
    dialogue_camera_bounds();
    animate();
    movement();
    sign();
    grove_battle();
    saves();
    party_statistics();
    party_progression();
    printf("PASS 16x16 core: %u checks\n", checks);
}
