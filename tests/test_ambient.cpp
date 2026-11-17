#include "ambient.h"
#include "title.h"
#include "world_assets.h"

#include <initializer_list>
#include <stdio.h>
#include <stdlib.h>

static unsigned checks;
#define CHECK(value)                                                                               \
    do {                                                                                           \
        ++checks;                                                                                  \
        if(!(value)) {                                                                             \
            fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #value);                        \
            exit(1);                                                                               \
        }                                                                                          \
    } while(0)

bool dialogue_show(char const PROGMEM*) {
    return true;
}

bool dialogue_show(NPC_ID id, char const PROGMEM* words) {
    (void)id;
    return dialogue_show(words);
}

bool dialogue_ask(char const PROGMEM*) {
    return true;
}

// The effects are constant expressions, so the guest can fold them and they keep no state.
static_assert(tile_phase(0) == 0 && tile_phase(TILE_PHASE_TICKS) == 1 &&
              tile_phase((uint16_t)(TILE_PHASES * TILE_PHASE_TICKS)) == 0);
static_assert(ambient_sine(0) == 0 && ambient_sine(64) == 64 && ambient_sine(128) == 0 &&
              ambient_sine(192) == -64 && ambient_sine(32) == 48 && ambient_sine(224) == -48);
static_assert(ambient_flap(0) == 0 && ambient_flap(1) == 1 && ambient_flap(2) == 2 &&
              ambient_flap(3) == 1);
static_assert(65536 % BIRD_SPACING == 0, "the bird trains survive the tick wrap");
static_assert((uint32_t)65536 % ((uint32_t)GUST_STEP_TICKS * GUST_SPACING) == 0 &&
                  65536 % AMBIENT_EPOCH_TICKS == 0 && 65536 % (TILE_PHASES * TILE_PHASE_TICKS) == 0,
              "gusts, epochs and tile phases survive the tick wrap");

static void tile_sheets() {
    for(uint8_t tileset = 0; tileset < TILESET_COUNT; ++tileset) {
        TileFrames wind = tile_wind_frames(tileset);
        // The wind block follows the exported atlas frames of the tileset's first sheet, which
        // stop after the highest tile that the tileset draws.
        CHECK(!wind.count || wind.base > 0);
        CHECK((uint16_t)wind.first + wind.count <= wind.base);
    }
    CHECK(tile_wind_frames(TILESET_WORLD).base == TILE_SPRITE_COUNT);
    CHECK(tile_wind_frames(TILESET_WORLD).count && !tile_wind_frames(TILESET_HOUSE).count);
    CHECK(!tile_wind_frames(TILESET_CAVE).count && !tile_wind_frames(TILESET_DUNGEON).count);
    CHECK(tileset_has_sky(TILESET_WORLD) && !tileset_has_sky(TILESET_HOUSE));
    // No birds fly underground.
    CHECK(!tileset_has_sky(TILESET_CAVE) && !tileset_has_sky(TILESET_DUNGEON));
    CHECK(tileset_has_ambient(TILESET_WORLD) && tileset_has_ambient(TILESET_HOUSE));
    CHECK(tileset_has_ambient(TILESET_CAVE) && tileset_has_ambient(TILESET_DUNGEON));
}

static void ambient_classes() {
    game.area = AREA_OUTDOOR;
    CHECK(tile_ambient(TILE_WATER) == AMBIENT_WATER &&
          tile_ambient(TILE_WATER_TOP) == AMBIENT_WATER);
    CHECK(tile_ambient(TILE_GRASS_06) == AMBIENT_GRASS &&
          tile_ambient(TILE_GRASS_59) == AMBIENT_GRASS);
    CHECK(tile_ambient(TILE_FLOWERS_26) == AMBIENT_FLOWERS);
    CHECK(tile_ambient(TILE_TREE_ROUND) == AMBIENT_TREE && tile_ambient(TILE_TREE) == AMBIENT_TREE);
    CHECK(tile_ambient(TILE_SIGN) == AMBIENT_PERCH && tile_ambient(TILE_BOULDER) == AMBIENT_PERCH);
    CHECK(tile_ambient(TILE_ROAD_EW) == AMBIENT_ROAD &&
          tile_ambient(TILE_ROAD_NESW_ALT) == AMBIENT_ROAD);
    CHECK(tile_ambient(TILE_OUTSIDE) == AMBIENT_NONE && tile_ambient(TILE_REEDS) == AMBIENT_NONE);
    CHECK(tile_ambient(TILE_BRIDGE_HORIZONTAL) == AMBIENT_NONE);
    // Exactly the grass and flowers tiles have a wind frame, so the wind block covers them.
    TileFrames wind = tile_wind_frames(TILESET_WORLD);
    for(uint16_t tile = 0; tile < 256; ++tile) {
        uint8_t ambient = tile_ambient((uint8_t)tile);
        if(ambient == AMBIENT_GRASS || ambient == AMBIENT_FLOWERS)
            CHECK(tile >= wind.first && tile < wind.first + wind.count);
    }
    game.area = AREA_INTERIORS;
    CHECK(tile_ambient(HOUSE_TILE_FLOOR_WOOD_A) == AMBIENT_FLOOR);
    CHECK(tile_ambient(HOUSE_TILE_STOVE) == AMBIENT_STEAM);
    CHECK(tile_ambient(HOUSE_TILE_EMPTY) == AMBIENT_NONE &&
          tile_ambient(HOUSE_TILE_RUG_CENTER) == AMBIENT_NONE);
    game.area = AREA_CAVE;
    CHECK(tile_ambient(CAVE_TILE_FLOOR_A) == AMBIENT_CAVE &&
          tile_ambient(CAVE_TILE_FLOOR_MOSS) == AMBIENT_CAVE);
    CHECK(tile_ambient(CAVE_TILE_PUDDLE) == AMBIENT_DRIP &&
          tile_ambient(CAVE_TILE_POOL_CENTER) == AMBIENT_POOL);
    CHECK(tile_ambient(CAVE_TILE_CRYSTALS) == AMBIENT_CRYSTAL &&
          tile_ambient(CAVE_TILE_WALL_ORE) == AMBIENT_CRYSTAL);
    CHECK(tile_ambient(CAVE_TILE_WALL_TORCH) == AMBIENT_EMBER &&
          tile_ambient(CAVE_TILE_CAMPFIRE) == AMBIENT_EMBER);
    CHECK(tile_ambient(CAVE_TILE_VENT_ACTIVE) == AMBIENT_STEAM &&
          tile_ambient(CAVE_TILE_VENT) == AMBIENT_NONE);
    CHECK(tile_ambient(CAVE_TILE_EMPTY) == AMBIENT_NONE &&
          tile_ambient(CAVE_TILE_CHASM) == AMBIENT_NONE &&
          tile_ambient(CAVE_TILE_RAIL_HORIZONTAL) == AMBIENT_NONE);
    game.area = AREA_DUNGEON;
    CHECK(tile_ambient(DUNGEON_TILE_FLOOR_A) == AMBIENT_CRYPT &&
          tile_ambient(DUNGEON_TILE_FLOOR_CRACKED) == AMBIENT_CRYPT);
    CHECK(tile_ambient(DUNGEON_TILE_PUDDLE) == AMBIENT_DRIP &&
          tile_ambient(DUNGEON_TILE_FLOOR_GRATE) == AMBIENT_DRIP);
    CHECK(tile_ambient(DUNGEON_TILE_TORCH) == AMBIENT_EMBER &&
          tile_ambient(DUNGEON_TILE_BRAZIER) == AMBIENT_EMBER);
    CHECK(tile_ambient(DUNGEON_TILE_CHANNEL_VERTICAL) == AMBIENT_POOL &&
          tile_ambient(DUNGEON_TILE_ORB_LIT) == AMBIENT_CRYSTAL);
    CHECK(tile_ambient(DUNGEON_TILE_ORB_DARK) == AMBIENT_NONE &&
          tile_ambient(DUNGEON_TILE_CARPET_CENTER) == AMBIENT_NONE &&
          tile_ambient(DUNGEON_TILE_SPIKES) == AMBIENT_NONE);
    game.area = AREA_OUTDOOR;
}

static void gusts() {
    uint16_t period = (uint16_t)GUST_STEP_TICKS * GUST_SPACING;
    uint8_t bent_width = 0;
    for(GustBand band : GUST_BANDS)
        bent_width = (uint8_t)(bent_width + band.width);
    for(uint8_t ty = 0; ty < 5; ++ty) {
        for(uint8_t tx = 0; tx < 250; tx += 7) {
            // Each tile bends for the same share of every period.
            unsigned bent = 0;
            for(uint16_t ticks = 0; ticks < period; ++ticks)
                bent += gust_bends(gust_front(ticks), tx, ty);
            CHECK(bent == (unsigned)bent_width * GUST_STEP_TICKS);
        }
    }
    // The front reaches a tile, then its east and south neighbors one step later.
    uint8_t front = gust_front(40);
    CHECK(gust_distance(front, front, 0) == 0 && gust_bends(front, front, 0));
    CHECK(!gust_bends(front, (uint8_t)(front + 1), 0) && !gust_bends(front, front, 1));
    uint8_t next = gust_front(40 + GUST_STEP_TICKS);
    CHECK(gust_distance(next, (uint8_t)(front + 1), 0) == 0 && gust_distance(next, front, 1) == 0);
    // The pattern is the same in every period.
    CHECK(gust_front(123) == gust_front((uint16_t)(123 + period)));
    // The early rejection never hides a bent tile of the view, and it passes most frames by.
    unsigned in_view = 0;
    for(uint16_t origin = 0; origin < GUST_SPACING; ++origin) {
        bool bent = false;
        for(uint8_t row = 0; row < VIEW_SPAN_HEIGHT; ++row)
            for(uint8_t column = 0; column < VIEW_SPAN_WIDTH; ++column)
                bent |= gust_bends((uint8_t)origin, column, row);
        CHECK(gust_in_view((uint8_t)origin) == bent);
        in_view += bent;
    }
    CHECK(in_view * 3 < GUST_SPACING);
    // At most one line of tiles is redrawn in a frame.
    for(uint16_t origin = 0; origin < GUST_SPACING; ++origin) {
        unsigned tiles = 0;
        for(uint8_t row = 0; row < VIEW_SPAN_HEIGHT; ++row)
            for(uint8_t column = 0; column < VIEW_SPAN_WIDTH; ++column)
                tiles += gust_bends((uint8_t)origin, column, row);
        CHECK(tiles <= VIEW_SPAN_HEIGHT);
    }
}

static bool flier(uint8_t frame) {
    return frame < FRAME_LEAF || (frame >= FRAME_MOTH && frame < FRAME_GLINT) ||
           (frame >= FRAME_BEE && frame < FRAME_STEAM) ||
           (frame >= FRAME_FAR_BIRD && frame < FRAME_DROP) || frame >= FRAME_WISP;
}

static void slots() {
    constexpr uint8_t CLASSES = AMBIENT_EMBER + 1;
    unsigned shown[CLASSES] = {}, frames[AMBIENT_FRAME_COUNT] = {},
             tiles[CHUNK_WIDTH * CHUNK_HEIGHT] = {};
    unsigned epochs = 0;
    for(uint8_t chunk_y = 0; chunk_y < 62; chunk_y += 3) {
        for(uint8_t chunk_x = 0; chunk_x < 30; chunk_x += 2) {
            AmbientSlot first = ambient_slot(chunk_x, chunk_y, 0);
            for(uint16_t start = (uint16_t)(AMBIENT_EPOCH_TICKS - first.age); start < 2000;
                start = (uint16_t)(start + AMBIENT_EPOCH_TICKS)) {
                // A slot keeps its tile and its roll for a whole epoch while its age runs up.
                AmbientSlot epoch = ambient_slot(chunk_x, chunk_y, start);
                CHECK(epoch.age == 0);
                ++epochs;
                ++tiles[epoch.y * CHUNK_WIDTH + epoch.x];
                AmbientSprite previous[CLASSES] = {};
                for(uint8_t age = 0; age < AMBIENT_EPOCH_TICKS; ++age) {
                    uint16_t ticks = (uint16_t)(start + age);
                    AmbientSlot slot = ambient_slot(chunk_x, chunk_y, ticks);
                    CHECK(slot.x < CHUNK_WIDTH && slot.y < CHUNK_HEIGHT && slot.age == age);
                    CHECK(slot.x == epoch.x && slot.y == epoch.y && slot.roll == epoch.roll &&
                          slot.vary == epoch.vary);
                    CHECK(!ambient_sprite(AMBIENT_NONE, slot, ticks).visible);
                    for(uint8_t ambient = AMBIENT_GRASS; ambient < CLASSES; ++ambient) {
                        AmbientSprite sprite = ambient_sprite(ambient, slot, ticks);
                        AmbientSprite before = previous[ambient];
                        previous[ambient] = sprite;
                        if(!sprite.visible)
                            continue;
                        ++shown[ambient];
                        CHECK(sprite.frame < AMBIENT_FRAME_COUNT);
                        ++frames[sprite.frame];
                        // Nothing strays more than three tiles from the tile that it belongs to.
                        CHECK(sprite.dx >= -3 * TILE && sprite.dx <= 3 * TILE &&
                              sprite.dy >= -3 * TILE && sprite.dy <= TILE);
                        // Motion is smooth: no effect jumps more than four pixels in a tick.
                        if(before.visible)
                            CHECK(sprite.dx - before.dx <= 4 && before.dx - sprite.dx <= 4 &&
                                  sprite.dy - before.dy <= 4 && before.dy - sprite.dy <= 4);
                        // A wandering flight starts and ends on its tile, so it does not pop in.
                        bool wanders = ambient == AMBIENT_FLOWERS ||
                                       (ambient == AMBIENT_GRASS && slot.roll < GRASS_BUTTERFLY) ||
                                       (ambient == AMBIENT_FLOOR && slot.roll < FLOOR_MOTH) ||
                                       (ambient == AMBIENT_CAVE && slot.roll < CAVE_BAT) ||
                                       (ambient == AMBIENT_CRYPT && slot.roll < CRYPT_WISP);
                        if(wanders && (age == 0 || age == AMBIENT_EPOCH_TICKS - 1))
                            CHECK(sprite.dx == 0 && sprite.dy == 0 && flier(sprite.frame));
                        // A visiting bird is a far speck when it appears and when it goes.
                        if(ambient == AMBIENT_PERCH && (age == 0 || age == AMBIENT_EPOCH_TICKS - 1))
                            CHECK(sprite.frame == FRAME_FAR_BIRD);
                        if(ambient == AMBIENT_PERCH && age == AMBIENT_EPOCH_TICKS / 2)
                            CHECK(sprite.dx == 0 && sprite.dy == PERCH_DY &&
                                  sprite.frame >= FRAME_PERCHED && sprite.frame < FRAME_STEAM);
                        // Still water underground rings and glints, and holds no fish.
                        if(ambient == AMBIENT_POOL)
                            CHECK(sprite.dx >= -4 && sprite.dx <= 2 && sprite.dy >= -4 &&
                                  sprite.dy <= 2 &&
                                  ((sprite.frame >= FRAME_RIPPLE && sprite.frame < FRAME_MOTH) ||
                                   (sprite.frame >= FRAME_GLINT && sprite.frame < FRAME_RABBIT)));
                        // A drop falls straight down onto its tile, then rings where it landed.
                        bool drips = ambient == AMBIENT_DRIP ||
                                     (ambient == AMBIENT_CAVE && slot.roll >= CAVE_BAT);
                        if(drips) {
                            CHECK(sprite.frame == FRAME_DROP || (sprite.frame >= FRAME_RIPPLE &&
                                                                 sprite.frame < FRAME_RIPPLE + 2));
                            CHECK((sprite.frame == FRAME_DROP) == (age < DRIP_FALL_TICKS));
                            CHECK(sprite.dx >= -4 && sprite.dx <= 3 && sprite.dy <= 4);
                            if(before.visible)
                                CHECK(sprite.dx == before.dx && sprite.dy >= before.dy);
                        }
                        // A glint stays on its crystal, and a spark only rises from its flame.
                        if(ambient == AMBIENT_CRYSTAL)
                            CHECK(sprite.dx >= -4 && sprite.dx <= 3 && sprite.dy >= -6 &&
                                  sprite.dy <= 1 && sprite.frame >= FRAME_GLINT &&
                                  sprite.frame < FRAME_RABBIT);
                        if(ambient == AMBIENT_EMBER) {
                            CHECK(sprite.frame >= FRAME_EMBER && sprite.frame < FRAME_WISP);
                            CHECK(sprite.dx >= -4 && sprite.dx <= 3 && sprite.dy <= -4 &&
                                  sprite.dy >= -TILE);
                            if(before.visible)
                                CHECK(sprite.dy <= before.dy);
                        }
                        // Grit falls in the dungeon, where dust rises in a house.
                        if(ambient == AMBIENT_CRYPT && slot.roll >= CRYPT_WISP && before.visible)
                            CHECK(sprite.dy >= before.dy && sprite.frame >= FRAME_DUST &&
                                  sprite.frame < FRAME_FAR_BIRD);
                        // Water effects stay on their tile, clear of the bank.
                        if(ambient == AMBIENT_WATER)
                            CHECK(sprite.dx >= -4 && sprite.dx <= 2 && sprite.dy >= -4 &&
                                  sprite.dy <= 2 && sprite.frame >= FRAME_FISH &&
                                  sprite.frame < FRAME_RABBIT && sprite.frame != FRAME_MOTH &&
                                  sprite.frame != FRAME_MOTH + 1);
                    }
                }
            }
        }
    }
    // Every class shows something, and every frame of the sheet is used.
    for(uint8_t ambient = AMBIENT_GRASS; ambient < CLASSES; ++ambient)
        CHECK(shown[ambient] > 0);
    for(uint8_t frame = 0; frame < AMBIENT_FRAME_COUNT; ++frame)
        CHECK(frames[frame] > 0);
    // The picks spread over the whole chunk: no tile gets far more or far less than its share.
    for(unsigned count : tiles)
        CHECK(count * 32 > epochs / 3 && count * 32 < epochs * 3);
    // Neighboring chunks do not move together.
    AmbientSlot a = ambient_slot(3, 4, 1000), b = ambient_slot(4, 4, 1000),
                c = ambient_slot(3, 5, 1000);
    CHECK(a.age != b.age && a.age != c.age);
}

static void flights() {
    // Two butterflies do not share a path: the slot's bits set the speed and start of each wave.
    AmbientSlot one{0, 0, 10, 0x13, 64}, two{0, 0, 10, 0xC6, 64};
    AmbientSprite first = ambient_sprite(AMBIENT_GRASS, one, 0),
                  second = ambient_sprite(AMBIENT_GRASS, two, 0);
    CHECK(first.visible && second.visible && (first.dx != second.dx || first.dy != second.dy));
    // The envelope bounds every flight by its reach across and half its reach up and down.
    for(uint16_t vary = 0; vary < 256; vary += 5) {
        for(uint16_t roll = 0; roll < 256; roll += 7) {
            for(uint8_t age = 0; age < AMBIENT_EPOCH_TICKS; ++age) {
                AmbientSprite sprite =
                    ambient_flight({0, 0, (uint8_t)roll, (uint8_t)vary, age}, {0, 20, 4, 9});
                uint8_t edge = age < 64 ? age : (uint8_t)(127 - age);
                int limit = edge < 20 ? edge : 20;
                CHECK(sprite.dx >= -limit && sprite.dx <= limit && sprite.dy * 2 >= -limit - 1 &&
                      sprite.dy * 2 <= limit + 1);
            }
        }
    }
}

static void birds() {
    unsigned seen = 0, flocks = 0, east = 0, west = 0, fast = 0;
    for(uint8_t lane = 0; lane < 31; ++lane) {
        uint16_t lane_top = (uint16_t)lane << BIRD_LANE_SHIFT;
        // Every view that can show any part of this lane's birds asks for this lane.
        for(int32_t camera_y = (int32_t)lane_top - 63; camera_y < lane_top + 64; ++camera_y)
            if(camera_y >= 0)
                CHECK(bird_lane((uint16_t)camera_y) == lane);
        for(uint32_t tick = 65000; tick < 65000 + 3000; ++tick) {
            uint16_t ticks = (uint16_t)tick, camera = 1000;
            AmbientBird bird = ambient_bird(lane, ticks, camera);
            if(!bird.visible)
                continue;
            ++seen;
            flocks += bird.followers != 0;
            CHECK(bird.x >= -TILE && bird.x < 128 + TILE && bird.followers <= 2);
            CHECK(bird.trail == BIRD_TRAIL_X || bird.trail == -BIRD_TRAIL_X);
            // The birds keep to their lane's band.
            CHECK(bird.y >= lane_top + BIRD_BAND_TOP &&
                  bird.y < lane_top + BIRD_BAND_TOP + BIRD_BAND);
            // They are anchored in the world: scrolling the camera moves them the other way.
            AmbientBird scrolled = ambient_bird(lane, ticks, (uint16_t)(camera + 1));
            if(bird.x > -TILE)
                CHECK(scrolled.visible && scrolled.x == bird.x - 1 && scrolled.y == bird.y &&
                      scrolled.clock == bird.clock);
            // They move one or two pixels per tick, across the tick wrap too, at one height.
            AmbientBird later = ambient_bird(lane, (uint16_t)(ticks + 1), camera);
            if(later.visible && bird.x > -TILE + 2 && bird.x < 128 + TILE - 2) {
                int step = later.x - bird.x;
                CHECK(step == 1 || step == -1 || step == 2 || step == -2);
                CHECK(later.y == bird.y && later.followers == bird.followers);
                CHECK((uint8_t)(later.clock - bird.clock) == 2);
                // The followers are behind the leader.
                CHECK((step > 0) == (bird.trail < 0));
                east += step > 0;
                west += step < 0;
                fast += step == 2 || step == -2;
            }
        }
    }
    CHECK(seen > 0 && flocks > 0 && flocks < seen && east > 0 && west > 0 && fast > 0);
    // A bird rises and falls smoothly, beats its wings on the way up and glides on the way down.
    unsigned flapping = 0;
    for(uint16_t clock = 0; clock < 256; ++clock) {
        BirdPose pose = ambient_bird_pose((uint8_t)clock),
                 next = ambient_bird_pose((uint8_t)(clock + 1));
        CHECK(pose.dy == ambient_sine((uint8_t)(clock + 64)) / (64 / BIRD_SWING));
        CHECK(pose.frame == FRAME_BIRD +
                  (clock < BIRD_FLAP_CLOCK ? ambient_flap((uint8_t)(clock >> 3)) : 1));
        CHECK(pose.dy >= -BIRD_SWING && pose.dy <= BIRD_SWING && next.dy - pose.dy <= 1 &&
              pose.dy - next.dy <= 1);
        CHECK(pose.frame >= FRAME_BIRD && pose.frame < FRAME_LEAF);
        flapping += pose.frame != FRAME_BIRD + 1;
        if(clock >= BIRD_FLAP_CLOCK)
            CHECK(pose.frame == FRAME_BIRD + 1 && next.dy >= pose.dy);
    }
    CHECK(flapping > 64);
}

// The renderer steps the seed from chunk to chunk instead of multiplying.
static void seeds() {
    for(uint8_t chunk_y = 0; chunk_y < 61; ++chunk_y) {
        for(uint8_t chunk_x = 0; chunk_x < 29; ++chunk_x) {
            uint16_t seed = ambient_seed(chunk_x, chunk_y);
            CHECK(ambient_seed((uint8_t)(chunk_x + 1), chunk_y) ==
                  (uint16_t)(seed + AMBIENT_SEED_X));
            CHECK(ambient_seed(chunk_x, (uint8_t)(chunk_y + 1)) ==
                  (uint16_t)(seed + AMBIENT_SEED_Y));
            AmbientSlot slot = ambient_slot(chunk_x, chunk_y, 777);
            uint16_t pick = ambient_pick(seed, 777);
            CHECK(slot.x == (pick & 7) && slot.y == ((pick >> 3) & 3));
        }
    }
}

// The title's pines sway slowly and smoothly. A band never jumps more than a pixel, rests at
// least half a second between moves, and rests more than a second before it goes back the way
// it came, so no section of a tree flicks to and fro. Gusts included, over the whole cycle.
static void title_trees() {
    constexpr int REST_TICKS = 30, TURN_BACK_TICKS = 60;
    for(TitleTree const* tree : {&PINE_SMALL, &PINE_LEFT, &PINE_RIGHT}) {
        bool moved = false;
        for(uint8_t band = 0; band < tree->bands; ++band) {
            int now = title_band_shift(*tree, band, title_tree_lean(*tree, 0));
            int before = now;
            long since = -1;
            // Two turns of the cycle, so a rest that spans the wrap counts too.
            for(long tick = 1; tick < 2L * TITLE_CYCLE; ++tick) {
                int next = title_band_shift(*tree, band,
                                            title_tree_lean(*tree, (uint16_t)(tick % TITLE_CYCLE)));
                if(next == now)
                    continue;
                moved = true;
                CHECK(next - now == 1 || now - next == 1);
                if(since >= 0) {
                    CHECK(tick - since >= REST_TICKS);
                    CHECK(next != before || tick - since >= TURN_BACK_TICKS);
                }
                before = now;
                now = next;
                since = tick;
            }
            // The bands at the foot of a tree stand still.
            CHECK(band < tree->top_weight || since < 0);
        }
        CHECK(moved);
    }
    // A gust leans a tall pine over by two pixels at its top.
    int16_t most = 0;
    for(uint16_t tick = 0; tick < GUST_PERIOD; ++tick) {
        int16_t lean = title_tree_lean(PINE_LEFT, tick);
        if(lean > most)
            most = lean;
    }
    CHECK(title_band_shift(PINE_LEFT, 0, most) == 2);
}

int main() {
    tile_sheets();
    ambient_classes();
    gusts();
    slots();
    flights();
    birds();
    seeds();
    title_trees();
    printf("PASS ambient animation: %u checks\n", checks);
}
