#include "encounter.h"
#include "rpg.h"
#include "equipment.h"
#include "chunk.h"
#include "world_assets.h"
#include "map_positions.h"
#include "npc.h"
#include "story_stub.h"
#include "logic/enemy_spawns.h"
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

bool dialogue_show(char const PROGMEM* text) {
    story_stub_record_dialogue(text);
    CHECK(chunk_trace_count[STAGE_TILES] > 0);
    return true;
}

bool dialogue_show(NPC_ID id, char const PROGMEM* words) {
    (void)id;
    return dialogue_show(words);
}

static bool answer = true;

bool dialogue_ask(char const PROGMEM* question) {
    dialogue_show(question);
    return answer;
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

constexpr NPC_ID WOLF = (NPC_ID)NPC_ENEMY_FIRST;
constexpr NPC_ID SCOUT = (NPC_ID)(NPC_ENEMY_FIRST + 1);
constexpr NPC_ID GUARD = (NPC_ID)(NPC_ENEMY_FIRST + 2);

using OrdinaryWolf = EnemySpawn<positions::GroveWolf, &BATTLE_GROVE_WOLVES, 16>;
using ConfiguredWolf = EnemySpawn<positions::GroveWolf, &BATTLE_GROVE_WOLVES, 16, ENEMY_WANDER,
                                 SPEED_FAST, 0, RESPAWN_NEVER, EnemyHooks,
                                 EnemySettings{.facing = EAST, .priority = 7}>;
static_assert(NPC_VILLAGER == 1 && NPC_WANDERER == 2 && NPC_ROWAN == 3);
static_assert(NPC_ENEMY_END == NPC_ENEMY_FIRST + ENEMY_SPAWN_COUNT);
static_assert(enemy_defeated_flag(WOLF) == FLAG_ENEMY_FIRST);
static_assert(!unique_enemy_positions<OrdinaryWolf, ConfiguredWolf>());
static_assert(unique_enemy_positions<OrdinaryWolf>());
static_assert(OrdinaryWolf::definition.period == 2 && !OrdinaryWolf::definition.talkable);
static_assert(OrdinaryWolf::definition.behavior == NPC_WANDER &&
              OrdinaryWolf::definition.priority == 2 && OrdinaryWolf::respawn == RESPAWN_REMEMBER);
static_assert(ConfiguredWolf::definition.period == 1 && ConfiguredWolf::definition.priority == 7 &&
              ConfiguredWolf::definition.facing == EAST && ConfiguredWolf::respawn == RESPAWN_NEVER);
static_assert(routes::area_for(routes::VaultPatrol) == positions::VaultGuard.area);

struct ConditionalWolfHooks : EnemyHooks {
    static bool available() {
        return game.get_flag(FLAG_CAVE_GATE);
    }

    static BattleEncounter const* select_battle(BattleEncounter const* original) {
        return game.get_flag(FLAG_CAVE_GATE) ? &BATTLE_MINE_SCOUTS : original;
    }
};

using ConditionalWolf = EnemySpawn<positions::GroveWolf, &BATTLE_GROVE_WOLVES, 17, ENEMY_WANDER,
                                  SPEED_NORMAL, 0, RESPAWN_REMEMBER, ConditionalWolfHooks>;
using ConditionalCatalog = EnemyCatalog<std::index_sequence<0>, TypeList<ConditionalWolf>>;

static void enter_enemy_site(TilePos site, uint8_t east) {
    world_enter(site.area, site.x * TILE + east, site.y * TILE, WEST);
}

static void enemy_encounters() {
    game_new();
    story_stub_reset(BATTLE_VICTORY);
    CHECK(ConditionalCatalog::location(WOLF) == NO_LOCATION);
    CHECK(ConditionalWolf::prepare_battle() == &BATTLE_GROVE_WOLVES);
    game.set_flag(FLAG_CAVE_GATE);
    CHECK(ConditionalCatalog::location(WOLF) == positions::GroveWolf);
    CHECK(ConditionalWolf::prepare_battle() == &BATTLE_MINE_SCOUTS);
    game.clear_flag(FLAG_CAVE_GATE);
    CHECK(!party_add(party_new_member(WOLF, MOVEMENT_STEP, ATTACK_STRIKE)));
    CHECK(!npc_definition(WOLF)->talkable && npc_definition(WOLF)->sprite_row == 17);
    CHECK(npc_definition(SCOUT)->period == 1 && npc_definition(SCOUT)->behavior == NPC_CHASE);
    CHECK(npc_definition(GUARD)->route == routes::VaultPatrol);
    CHECK(battle_encounter_valid(&BATTLE_GROVE_WOLVES));
    CHECK(battle_encounter_valid(&BATTLE_MINE_SCOUTS));
    CHECK(battle_encounter_valid(&BATTLE_VAULT_GUARDS));
    CHECK(!battle_encounter_valid(nullptr));

    enter_enemy_site(positions::GroveWolf, 0);
    CHECK(!npc_find(WOLF)); // Scene entry still excludes the player's feet.
    enter_enemy_site(positions::GroveWolf, FOOT_WIDTH);
    auto* wolf = npc_find(WOLF);
    CHECK(wolf && npc_location(WOLF) == positions::GroveWolf);
    wolf->timer = 10;
    CHECK(!npc_player_clear(game.player.x - 1, game.player.y, WEST, false));
    CHECK(encounter_run() && story_stub_battles() == 0);
    CHECK(!actor_move(&game.player, WEST, 1));
    CHECK(!wolf->push && !(wolf->state & NPC_YIELDING));
    CHECK(story_stub_battles() == 0);
    CHECK(encounter_run());
    CHECK(story_stub_battles() == 1 && story_stub_event(0).encounter == &BATTLE_GROVE_WOLVES);
    CHECK(game.get_flag(enemy_defeated_flag(WOLF)) && !npc_find(WOLF));
    CHECK(game.remember_timer == REMEMBER_DELAY_FRAMES && save_valid());
    for(uint8_t i = 0; i < 20; ++i)
        CHECK(!remember_tick());
    CHECK(game.remember_timer == REMEMBER_DELAY_FRAMES - 20);

    // Even ticks move a normal wanderer, odd ticks keep its position.
    game.clear_flag(enemy_defeated_flag(WOLF));
    enter_enemy_site(positions::GroveWolf, TILE);
    wolf = npc_find(WOLF);
    CHECK(wolf);
    wolf->state = NPC_WALK_PHASE;
    wolf->actor.facing = WEST;
    wolf->timer = 20;
    uint16_t wolf_x = wolf->actor.x;
    game.ticks = 1;
    npc_update();
    CHECK(wolf->actor.x == wolf_x);
    game.ticks = 2;
    npc_update();
    CHECK(wolf->actor.x == wolf_x - 1);
    for(unsigned i = 0; i < 360; ++i) {
        ++game.ticks;
        npc_update();
        CHECK(wolf->actor.x >= wolf_x - TILE && wolf->actor.x <= wolf_x + TILE);
        CHECK(wolf->actor.y >= positions::GroveWolf.y * TILE - TILE &&
              wolf->actor.y <= positions::GroveWolf.y * TILE + TILE);
    }
    game.set_flag(enemy_defeated_flag(WOLF));

    // A fast scout moves every frame and changes direction when the player moves.
    enter_enemy_site(positions::MineScout, 20);
    auto* scout = npc_find(SCOUT);
    CHECK(scout);
    uint16_t x = scout->actor.x, y = scout->actor.y;
    game.ticks = 1;
    npc_update();
    CHECK(scout->behavior == NPC_CHASE && scout->actor.x == x + 1);
    game.player = {scout->actor.x, (uint16_t)(y + 16), NORTH, 0};
    npc_update();
    CHECK(scout->actor.y == y + 1);
    game.player.x = x + 81;
    npc_update();
    CHECK(scout->behavior == NPC_RETURN);

    // Enemy-initiated contact cannot overlap the player or enter battle inside npc_update.
    enter_enemy_site(positions::MineScout, FOOT_WIDTH);
    scout = npc_find(SCOUT);
    CHECK(scout);
    npc_update();
    CHECK(!npc_overlap(game.player.x, game.player.y, scout->actor.x, scout->actor.y));
    CHECK(story_stub_battles() == 1);
    CHECK(encounter_run());
    CHECK(story_stub_battles() == 2 && story_stub_event(1).encounter == &BATTLE_MINE_SCOUTS);
    CHECK(game.get_flag(enemy_defeated_flag(SCOUT)) && !npc_find(SCOUT));
    CHECK(game.remember_timer == REMEMBER_DELAY_FRAMES);

    enter_enemy_site(positions::VaultGuard, FOOT_WIDTH);
    auto* guard = npc_find(GUARD);
    CHECK(guard);
    game.player.y += TILE;
    uint16_t guard_x = guard->actor.x;
    for(unsigned i = 0; i < 160 && !(guard->state & NPC_BACKWARD); ++i) {
        ++game.ticks;
        npc_update();
    }
    CHECK(guard->actor.x > guard_x && (guard->state & NPC_BACKWARD));
    enter_enemy_site(positions::VaultGuard, FOOT_WIDTH);
    guard = npc_find(GUARD);
    CHECK(guard && guard->behavior == NPC_ROUTE);
    uint16_t timer = game.remember_timer;
    uint16_t gold = game.gold;
    answer = false;
    CHECK(!actor_move(&game.player, WEST, 1));
    CHECK(encounter_run());
    CHECK(story_stub_battles() == 2 && !npc_find(GUARD));
    CHECK(!game.get_flag(enemy_defeated_flag(GUARD)) && game.gold == gold &&
          game.remember_timer == timer);
    game_update(0);
    CHECK(!npc_find(GUARD) && encounter_run());

    answer = true;
    enter_enemy_site(positions::VaultGuard, FOOT_WIDTH);
    CHECK(npc_find(GUARD) && !actor_move(&game.player, WEST, 1));
    story_stub_set_result(BATTLE_DEFEAT);
    CHECK(!encounter_run());
    CHECK(!game.get_flag(enemy_defeated_flag(GUARD)) && game.gold == gold &&
          game.remember_timer == timer);
    CHECK(story_stub_count() == 5 && story_stub_battles() == 3);
    CHECK(encounter_run() && story_stub_battles() == 3);

    story_stub_set_result(BATTLE_VICTORY);
    enter_enemy_site(positions::VaultGuard, FOOT_WIDTH);
    CHECK(npc_find(GUARD) && !actor_move(&game.player, WEST, 1));
    CHECK(encounter_run());
    CHECK(story_stub_battles() == 4 && story_stub_count() == 8);
    CHECK(strstr(story_stub_event(7).text, "falls still"));
    CHECK(game.get_flag(enemy_defeated_flag(GUARD)) && !npc_find(GUARD));
    CHECK(game.remember_timer == timer);

    enter_enemy_site(positions::GroveWolf, FOOT_WIDTH);
    CHECK(!npc_find(WOLF));
    game.set_flag(FLAG_CAVE_GATE);
    CHECK(save_restore() && game.remember_timer == timer);
    update_visible_world(0, false);
    CHECK(!npc_find(WOLF));
    for(uint16_t i = 1; i < timer; ++i)
        CHECK(!remember_tick());
    CHECK(remember_tick() && game.remember_timer == 0);
    CHECK(save_restore() && remember_tick()); // An event due at save remains due after load.
    update_visible_world(0, false);
    remember_reset();
    CHECK(!remember_tick() && game.get_flag(FLAG_CAVE_GATE));
    CHECK(!game.get_flag(enemy_defeated_flag(WOLF)) && !game.get_flag(enemy_defeated_flag(SCOUT)));
    CHECK(game.get_flag(enemy_defeated_flag(GUARD)));
    game_update(0);
    CHECK(!npc_find(WOLF)); // A cleared flag cannot cast into the protected camera.
    enter_enemy_site(positions::GroveWolf, FOOT_WIDTH);
    CHECK(npc_find(WOLF));
    game.remember_timer = REMEMBER_DELAY_FRAMES + 1;
    CHECK(!save_valid());
    game.remember_timer = 1;
    CHECK(!save_valid()); // A positive countdown requires a defeated respawnable enemy.
}

static void saves() {
    game_new();
    CHECK(!memcmp(&game.supplies, &STARTING_SUPPLIES, sizeof(Supplies)));
    game.supplies = {{0, 9, 6, 15}};
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
    for(uint8_t i = 0; i < SUPPLY_COUNT; ++i) {
        game = saved;
        game.supplies.quantity[i] = SUPPLY_LIMIT + 1;
        reject();
    }
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
    CHECK(!memcmp(&game.supplies, &saved.supplies, sizeof(Supplies)));
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

static void permanent_perks() {
    game_new();
    PartyMember& member = game.party.members[0];
    CHECK(member.perk_ranks == 0 && party_unspent_perks(member) == 0);
    CHECK(!party_learn_perk(member, PERK_MOVEMENT));
    CHECK(member.perk_ranks == 0 && save_valid());
    member.hp = 16;
    CHECK(party_gain_experience(member, 10) == 1 && party_unspent_perks(member) == 1);
    CHECK(party_learn_perk(member, PERK_MOVEMENT));
    CHECK(party_movement_perk_rank(member) == 1 && party_attack_perk_rank(member) == 0);
    CHECK(member.hp == 16 && member.ap == 4 && party_max_hp(member) == 23);
    CHECK(!party_learn_perk(member, PERK_ATTACK));
    CHECK(party_gain_experience(member, 35) == 2 && party_unspent_perks(member) == 1);
    CHECK(!party_learn_perk(member, PERK_MOVEMENT));
    CHECK(party_learn_perk(member, PERK_ATTACK) && party_unspent_perks(member) == 0);
    CHECK(party_max_hp(member) == 25);
    party_gain_experience(member, 255);
    CHECK(member.level == 10 && party_unspent_perks(member) == 3 && party_max_hp(member) == 27);
    CHECK(!party_learn_perk(member, PERK_MOVEMENT) && !party_learn_perk(member, PERK_ATTACK));
    CHECK(!party_learn_perk(member, (PERK_TRACK_KIND)255));
    Game saved = game;
    CHECK(save_valid());
    game_new();
    CHECK(game.party.members[0].perk_ranks == 0);
    game = saved;
    CHECK(save_restore());
    CHECK(party_movement_perk_rank(member) == 1 && party_attack_perk_rank(member) == 1);
    CHECK(party_unspent_perks(member) == 3 && member.hp == 16);

    static constexpr uint8_t INVALID_RANKS[] = {0x40, 0x80, 2, 16, 17, 47, 63};
    for(uint8_t bad : INVALID_RANKS) {
        member.perk_ranks = bad;
        PartyMember before = member;
        CHECK(!save_valid() && !save_restore());
        CHECK(!party_learn_perk(member, PERK_MOVEMENT) && !party_learn_perk(member, PERK_ATTACK));
        CHECK(!memcmp(&before, &member, sizeof(member)));
    }
    member = saved.party.members[0];
    member.level = 2;
    CHECK(!save_valid() && !party_learn_perk(member, PERK_ATTACK));

    PartyMember delayed = party_new_member(NPC_ROWAN, MOVEMENT_LEAP, ATTACK_SHOOT);
    CHECK(party_gain_experience(delayed, 100) == 5);
    CHECK(delayed.level == 6 && party_unspent_perks(delayed) == 3);
    CHECK(party_learn_perk(delayed, PERK_ATTACK));
    CHECK(party_attack_perk_rank(delayed) == 1 && party_movement_perk_rank(delayed) == 0);
    CHECK(party_learn_perk(delayed, PERK_MOVEMENT) && party_unspent_perks(delayed) == 1);
}

static void equipment_operations() {
    static_assert(equipment_valid_item(ITEM_IRON_FANG) && FLAG_ITEM_FIRST > FLAG_RESERVED);
    static_assert(equipment_flag(ITEM_IRON_FANG) == FLAG_ITEM_FIRST);
    static_assert(equipment_flag(ITEM_DEFINED_COUNT - 1) == FLAG_ITEM_END - 1);
    static_assert(equipment_flag(ITEM_NONE) == FLAG_COUNT);
    static_assert(equipment_flag(ITEM_DEFINED_COUNT) == FLAG_COUNT);
    for(uint8_t id = 1; id < ITEM_DEFINED_COUNT; ++id)
        CHECK(equipment_flag(id) == equipment_flag(id - 1) + 1);

    for(uint8_t count = 1; count <= PARTY_CAPACITY; ++count) {
        game_start();
        CHECK(equipment_owned(ITEM_IRON_FANG));
        for(uint8_t i = 1; i < count; ++i)
            CHECK(party_add(party_new_member((NPC_ID)i, MOVEMENT_STEP, ATTACK_STRIKE)));
        for(auto const& member : game.party.members)
            for(uint8_t stored : member.equipment)
                CHECK(stored == 0);

        Game before = game;
        CHECK(!equipment_equip(count, ITEM_IRON_FANG));
        CHECK(!equipment_equip(0, ITEM_TRAVELERS_BOOTS));
        CHECK(!equipment_equip(0, ITEM_NONE) && !equipment_owned(ITEM_NONE));
        CHECK(!equipment_unequip(0, SLOT_COUNT));
        CHECK(!equipment_acquire(ITEM_IRON_FANG));
        CHECK(!equipment_acquire(ITEM_NONE));
        CHECK(!equipment_acquire(ITEM_DEFINED_COUNT));
        CHECK(!equipment_equip(0, ITEM_DEFINED_COUNT));
        CHECK(!memcmp(&before, &game, sizeof(Game)));

        CHECK(equipment_acquire(ITEM_TRAVELERS_BOOTS));
        CHECK(equipment_acquire(ITEM_DAWN_PENDANT));
        CHECK(equipment_acquire(ITEM_VITAL_PENDANT));
        for(uint16_t flag = 0; flag < FLAG_COUNT; ++flag) {
            bool acquired = flag >= equipment_flag(ITEM_TRAVELERS_BOOTS) &&
                            flag <= equipment_flag(ITEM_VITAL_PENDANT);
            CHECK(game.flags.get_flag(flag) == (acquired || before.flags.get_flag(flag)));
        }

        PartyMember& leader = game.party.members[0];
        leader.level = 7;
        CHECK(party_learn_perk(leader, PERK_MOVEMENT));
        uint8_t hp = leader.hp;
        CHECK(party_max_hp(leader) == 27);
        CHECK(equipment_equip(0, ITEM_IRON_FANG));
        CHECK(equipment_get(leader, SLOT_ARMOR) == ITEM_NONE);
        CHECK(equipment_equip(0, ITEM_TRAVELERS_BOOTS));
        CHECK(party_max_hp(leader) == 29);
        CHECK(equipment_equip(0, ITEM_VITAL_PENDANT));
        CHECK(party_max_hp(leader) == 32 && leader.hp == hp);
        leader.hp = 32;

        if(count > 1) {
            CHECK(equipment_equip(count - 1, ITEM_VITAL_PENDANT));
            CHECK(equipment_wearer(ITEM_VITAL_PENDANT) == count - 1);
            CHECK(equipment_get(leader, SLOT_RELIC) == ITEM_NONE && leader.hp == 29);
        } else {
            CHECK(equipment_equip(0, ITEM_DAWN_PENDANT));
            CHECK(equipment_owned(ITEM_VITAL_PENDANT));
            CHECK(leader.hp == 29 && party_victory_ap(leader) == 2);
        }
        CHECK(equipment_unequip(0, SLOT_ARMOR));
        CHECK(equipment_owned(ITEM_TRAVELERS_BOOTS) && leader.hp == 27);
        before = game;
        CHECK(!equipment_equip(0, ITEM_IRON_FANG) && !equipment_unequip(0, SLOT_ARMOR));
        CHECK(!memcmp(&before, &game, sizeof(Game)));
        CHECK(save_valid());
    }
}

static void equipment_saves() {
    game_start();
    CHECK(party_add(party_new_member(NPC_ROWAN, MOVEMENT_LEAP, ATTACK_SHOOT)));
    CHECK(equipment_acquire(ITEM_VITAL_PENDANT));
    CHECK(equipment_equip(0, ITEM_VITAL_PENDANT) && equipment_equip(0, ITEM_IRON_FANG));
    game.party.members[0].hp = 22;
    game.party.members[0].ap = 2;
    Game saved = game;

    auto reject = [&] {
        Game corrupt = game;
        CHECK(!save_valid() && !save_restore());
        CHECK(!memcmp(&corrupt, &game, sizeof(Game)));
        game = saved;
    };
    game.party.members[0].equipment[SLOT_WEAPON] = 255;
    reject();
    game.clear_flag(equipment_flag(ITEM_IRON_FANG));
    reject();
    game.party.members[1].equipment[SLOT_WEAPON] = equipment_encode(ITEM_IRON_FANG);
    reject();
    game.party.members[0].equipment[SLOT_ARMOR] = equipment_encode(ITEM_VITAL_PENDANT);
    reject();
    game.party.members[2].equipment[SLOT_WEAPON] = equipment_encode(ITEM_IRON_FANG);
    reject();
    game.party.members[0].hp = 24;
    reject();
    game.flags.bytes[WorldFlags::BYTE_COUNT - 1] |= 0x80;
    reject();

    game_start();
    unsigned char image[sizeof(Game)];
    memcpy(image, &saved, sizeof(image));
    memcpy(&game, image, sizeof(image));
    CHECK(save_restore());
    PartyMember const& leader = game.party.members[0];
    CHECK(equipment_owned(ITEM_IRON_FANG) && equipment_owned(ITEM_VITAL_PENDANT));
    CHECK(equipment_get(leader, SLOT_WEAPON) == ITEM_IRON_FANG);
    CHECK(equipment_get(leader, SLOT_RELIC) == ITEM_VITAL_PENDANT);
    CHECK(leader.hp == 22 && leader.ap == 2 && party_max_hp(leader) == 23);
}

int main() {
    dialogue_camera_bounds();
    animate();
    movement();
    enemy_encounters();
    saves();
    party_statistics();
    party_progression();
    permanent_perks();
    equipment_operations();
    equipment_saves();
    printf("PASS 16x16 core: %u checks\n", checks);
}
