#include "rpg.h"
#include "battle.h"
#include "tile_ids.h"
#include "npc.h"
#include "runtime_workspace.h"
#include "actor_draw_order.h"
#include "ambient.h"
#include "journal.h"
#include "quest.h"
#include "title.h"
#include "world_assets.h"
#include "world_map.h"

#include <avm.h>
#include <string.h>

static constexpr uint16_t DISPLAY_WIDTH = 128;

// Breathing crosshair: opaque black halo around every white arm, open target center.
static constexpr auto MAP_CURSOR PROGMEM = [] {
    struct Sprite {
        uint8_t width, height;
        uint8_t frames[2][44];
    } sprite{11, 16, {}};
    for(uint8_t phase = 0; phase < 2; ++phase) {
        for(int8_t y = 0; y < 11; ++y) {
            for(int8_t x = 0; x < 11; ++x) {
                bool white = false, opaque = false;
                for(int8_t dy = -1; dy <= 1; ++dy) {
                    for(int8_t dx = -1; dx <= 1; ++dx) {
                        int8_t ax = x - 5 + dx, ay = y - 5 + dy;
                        int8_t distance = ax < 0 ? -ax : ax;
                        if(!ax)
                            distance = ay < 0 ? -ay : ay;
                        bool arm = (!ax || !ay) && distance >= 2 && distance <= 3 + phase;
                        opaque |= arm;
                        if(!dx && !dy)
                            white = arm;
                    }
                }
                uint8_t offset = ((y / 8) * 11 + x) * 2;
                if(white)
                    sprite.frames[phase][offset] |= (uint8_t)(1 << (y % 8));
                if(opaque)
                    sprite.frames[phase][offset + 1] |= (uint8_t)(1 << (y % 8));
            }
        }
    }
    return sprite;
}();

// A black-edged white diamond, centered on the tracked objective. Its rows are the black outline
// (radius 3) and the white fill (radius 2), so it needs no sprite asset.
static void draw_quest_marker(int16_t x, int16_t y) {
    for(int8_t dy = -3; dy <= 3; ++dy) {
        int8_t reach = (int8_t)(3 - (dy < 0 ? -dy : dy));
        avm_draw_filled_rect_black(x - reach, y + dy, 2 * reach + 1, 1);
    }
    for(int8_t dy = -2; dy <= 2; ++dy) {
        int8_t reach = (int8_t)(2 - (dy < 0 ? -dy : dy));
        avm_draw_filled_rect_white(x - reach, y + dy, 2 * reach + 1, 1);
    }
}

void render_world_map(WorldMapView const& view, uint8_t ticks, bool markers) {
    avm_draw_sprite_overwrite(-view.camera_x, -view.camera_y, WORLD_MAP_SPRITE, 0);
    if(!markers)
        return;
    int16_t player_x = (game.player.x + FOOT_CENTER_X) / TILE - view.camera_x;
    int16_t player_y = (game.player.y + FOOT_CENTER_Y) / TILE - view.camera_y;
    if(!(ticks & 16)) {
        avm_draw_filled_rect_black(player_x - 2, player_y - 2, 5, 5);
        avm_draw_filled_rect_white(player_x - 1, player_y - 1, 3, 3);
    }
    // The tracked quest alone is queried. Its marker blinks opposite the player's.
    TilePos objective;
    int16_t objective_x, objective_y;
    if((ticks & 16) && quest_map_marker(AREA_OUTDOOR, objective) &&
       map_tile_on_screen(objective, view, objective_x, objective_y))
        draw_quest_marker(objective_x, objective_y);
    uint8_t phase = (ticks / 8) % 4;
    avm_draw_sprite_plus_mask(map_pixel(view.x) - view.camera_x - 5,
                              map_pixel(view.y) - view.camera_y - 5, &MAP_CURSOR,
                              phase == 1 || phase == 2);
}

// Eight 20 ms steps per half of the transition. Seven intermediate erase masks live in flash;
// the endpoints need no sprite. A 16x64 strip repeats eight times across the framebuffer, keeping
// the asset under 1 KiB while letting the native sprite service do all the pixel work.
static constexpr auto FADE_SPRITE PROGMEM = [] {
    struct Sprite {
        uint8_t width, height;
        uint8_t pixels[FADE_STEPS - 1][16 * 8];
    } sprite{16, 64, {}};
    constexpr uint8_t BAYER[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
    for(uint8_t frame = 0; frame < FADE_STEPS - 1; ++frame) {
        for(uint8_t page = 0; page < 8; ++page) {
            for(uint8_t x = 0; x < 16; ++x) {
                uint8_t mask = 0;
                for(uint8_t bit = 0; bit < 8; ++bit) {
                    if(BAYER[bit % 4][x % 4] < (frame + 1) * 2)
                        mask |= (uint8_t)(1 << bit);
                }
                sprite.pixels[frame][page * 16 + x] = mask;
            }
        }
    }
    return sprite;
}();

// Apply black coverage without presenting, so an overlay can remain bright above the world.
static void draw_fade(uint8_t step) {
    if(step >= FADE_STEPS) {
        avm_draw_filled_rect_black(0, 0, DISPLAY_WIDTH, 64);
    } else if(step) {
        avm_set_sprite(&FADE_SPRITE);
        for(uint8_t x = 0; x < DISPLAY_WIDTH; x += 16)
            avm_draw_erase(x, 0, step - 1);
    }
}

// Entry starts at the menu's half-dimmed world (240 ms). Exit restores the fully
// visible world (320 ms). Rebuild frames without advancing gameplay or adding a second buffer.
__attribute__((noinline)) void transition_world_map(WorldMapView const& view, uint8_t ticks,
                                                    bool closing) {
    uint8_t first = closing ? 1 : FADE_STEPS / 2 + 1;
    for(uint8_t step = first; step <= FADE_STEPS; ++step) {
        input_wait_frame();
        if(closing)
            render_world_map(view, ticks);
        else
            rpg_render_world();
        draw_fade(step);
        avm_display(true);
    }

    for(int8_t step = FADE_STEPS - 1; step >= 0; --step) {
        input_wait_frame();
        if(closing)
            rpg_render_world();
        else
            render_world_map(view, ticks, step == 0);
        draw_fade((uint8_t)step);
        avm_display(true);
    }
}

// Preserve the framebuffer; fade-out accumulates masks without redrawing the outgoing scene.
void show_fade(uint8_t step) {
    draw_fade(step);
    input_wait_frame();
    avm_display(false);
}

void fade_to_black() {
    for(uint8_t step = 1; step <= FADE_STEPS; ++step)
        show_fade(step);
}

// The view's first cache entry, its world tile and its screen position.
struct View {
    uint8_t const* tiles;
    uint8_t tx, ty;
    int16_t left, top;
};

static View world_view(Camera camera) {
    uint8_t tx = (uint8_t)(camera.x >> TILE_SHIFT), ty = (uint8_t)(camera.y >> TILE_SHIFT);
    // The cache trails the view by its margin, so the view starts inside the window.
    return {cache.tiles + (uint8_t)(ty - cache.origin_ty) * CACHE_WIDTH +
                (uint8_t)(tx - cache.origin_tx),
            tx, ty, (int16_t) - (int16_t)(camera.x & (TILE - 1)),
            (int16_t) - (int16_t)(camera.y & (TILE - 1))};
}

static constexpr uint8_t GUST_VISIBLE_LINES = [] {
    uint8_t maximum = 0;
    for(uint8_t origin = 0; origin < GUST_SPACING; ++origin) {
        uint8_t count = 0;
        for(GustBand band : GUST_BANDS)
            for(uint8_t line = 0; line < band.width; ++line)
                if(((origin - band.behind - line) & (GUST_SPACING - 1)) <=
                   VIEW_SPAN_WIDTH + VIEW_SPAN_HEIGHT - 2)
                    ++count;
        if(count > maximum)
            maximum = count;
    }
    return maximum;
}();

struct GustLine {
    uint8_t position, count;
};

struct GustPlan {
    uint8_t count;
    GustLine lines[GUST_VISIBLE_LINES];
};

static_assert(VIEW_SPAN_WIDTH <= 16 && VIEW_SPAN_HEIGHT <= 16,
              "gust positions pack a row and column into one byte");

// For every front position, retain only the diagonals intersecting the view and
// their visible runs, in the original band/line/row order.
static constexpr auto GUST_PLANS PROGMEM = [] {
    struct Plans {
        GustPlan front[GUST_SPACING];
    } plans{};
    for(uint8_t origin = 0; origin < GUST_SPACING; ++origin) {
        GustPlan& plan = plans.front[origin];
        for(GustBand band : GUST_BANDS) {
            for(uint8_t line = 0; line < band.width; ++line) {
                uint8_t diagonal = (uint8_t)((origin - band.behind - line) & (GUST_SPACING - 1));
                if(diagonal > VIEW_SPAN_WIDTH + VIEW_SPAN_HEIGHT - 2)
                    continue;
                uint8_t row = diagonal < VIEW_SPAN_WIDTH ? 0 : diagonal - VIEW_SPAN_WIDTH + 1;
                uint8_t last_row = diagonal < VIEW_SPAN_HEIGHT ? diagonal : VIEW_SPAN_HEIGHT - 1;
                plan.lines[plan.count++] = {(uint8_t)((row << 4) | (diagonal - row)),
                                            (uint8_t)(last_row - row + 1)};
            }
        }
    }
    return plans;
}();

// Redraws the grass under a passing gust with its wind frame.
// How far the view's top-left tile lies behind the gust front.
static uint8_t gust_origin(Camera camera) {
    return gust_distance(gust_front(game.ticks), (uint8_t)(camera.x >> TILE_SHIFT),
                         (uint8_t)(camera.y >> TILE_SHIFT));
}

RPG_NOINLINE static void draw_gusts(Camera camera, uint8_t height) {
    uint8_t tileset = area_tileset(game.area);
    TileFrames wind = tile_wind_frames(tileset);
    if(!wind.count)
        return;
    uint8_t origin = gust_origin(camera);
    // The wind frames follow the atlas frames in the tileset's first sheet.
    avm_set_sprite(tileset_sprites(tileset));
    // The tileset's ambient classes, resolved once rather than per gust tile.
    auto const* classes = &TILE_AMBIENT[(uint16_t)tileset << 8];
    View view = world_view(camera);
    uint8_t rows = (uint8_t)((height + (camera.y & (TILE - 1)) + TILE - 1) >> TILE_SHIFT);
    GustPlan plan;
    uint16_t offset = (uint16_t)(origin * sizeof(GustPlan));
    memcpy_P(&plan, reinterpret_cast<uint8_t const PROGMEM*>(&GUST_PLANS) + offset, sizeof(plan));
    for(uint8_t index = 0; index < plan.count; ++index) {
        GustLine line = plan.lines[index];
        uint8_t row = line.position >> 4, column = line.position & 15;
        if(row >= rows)
            continue;
        if(row + line.count > rows)
            line.count = rows - row;
        uint8_t const* tiles = view.tiles + row * CACHE_WIDTH + column;
        int16_t x = view.left + column * TILE, y = view.top + row * TILE;
        for(uint8_t count = line.count; count; --count) {
            uint8_t tile = *tiles;
            uint8_t ambient = classes[tile];
            if(ambient == AMBIENT_GRASS || ambient == AMBIENT_FLOWERS)
                avm_draw_overwrite(x, y, (uint16_t)(wind.base + (uint8_t)(tile - wind.first)));
            if(count > 1)
                tiles += CACHE_WIDTH - 1;
            x -= TILE;
            y += TILE;
        }
    }
}

// One slot whose tile is in view, at a column and row of the view packed into one byte: looks up
// the tile's class and draws what the slot shows there. Out of line and with few arguments, so
// that little is on the stack unless a slot's tile is in view.
RPG_NOINLINE static void draw_ambient_effect(Camera camera, uint8_t position, uint16_t seed,
                                             uint16_t pick) {
    uint8_t column = position & 15, row = position >> 4;
    // The cache trails the view by its margin, so the view starts inside the window.
    uint8_t tile =
        cache.tiles[(uint8_t)((uint8_t)(camera.y >> TILE_SHIFT) - cache.origin_ty + row) *
                        CACHE_WIDTH +
                    (uint8_t)((uint8_t)(camera.x >> TILE_SHIFT) - cache.origin_tx + column)];
    uint8_t ambient_class = tile_ambient(tile);
    if(ambient_class == AMBIENT_NONE)
        return;
    AmbientSprite sprite =
        ambient_sprite(ambient_class, ambient_slot_of(seed, pick, game.ticks), game.ticks);
    if(sprite.visible)
        avm_draw_plus_mask(column * TILE - (int16_t)(camera.x & (TILE - 1)) + sprite.dx,
                           row * TILE - (int16_t)(camera.y & (TILE - 1)) + sprite.dy, sprite.frame);
}

// A creature or a small effect on the tile that the slot of each chunk in view picked. The loop
// only finds the picked tile; a chunk whose tile is out of view costs nothing more. Selects the
// ambient sheet.
RPG_NOINLINE static void draw_ambient_tiles(Camera camera, uint8_t height) {
    avm_set_sprite(AMBIENT_SPRITES);
    uint8_t rows = (uint8_t)((height + (camera.y & (TILE - 1)) + TILE - 1) >> TILE_SHIFT);
    uint8_t view_tx = (uint8_t)(camera.x >> TILE_SHIFT);
    uint8_t view_ty = (uint8_t)(camera.y >> TILE_SHIFT);
    uint8_t chunk_x = view_tx / CHUNK_WIDTH, chunk_y = view_ty / CHUNK_HEIGHT;
    // The first chunk's first tile, as a column and row of the view: zero or negative.
    uint8_t first_column = (uint8_t)(chunk_x * CHUNK_WIDTH - view_tx);
    uint8_t row = (uint8_t)(chunk_y * CHUNK_HEIGHT - view_ty);
    uint16_t row_seed = ambient_seed(chunk_x, chunk_y);
    // At most two chunks each way: the view is no larger than a chunk plus one tile.
    static_assert(VIEW_SPAN_WIDTH <= CHUNK_WIDTH + 1 && VIEW_SPAN_HEIGHT <= CHUNK_HEIGHT + 1);
    for(; row < rows || row >= (uint8_t)-CHUNK_HEIGHT; row += CHUNK_HEIGHT) {
        uint16_t seed = row_seed;
        for(uint8_t column = first_column;
            column < VIEW_SPAN_WIDTH || column >= (uint8_t)-CHUNK_WIDTH; column += CHUNK_WIDTH) {
            uint16_t pick = ambient_pick(seed, game.ticks);
            uint8_t tile_column = (uint8_t)(column + (pick & (CHUNK_WIDTH - 1)));
            uint8_t tile_row = (uint8_t)(row + ((pick >> 3) & (CHUNK_HEIGHT - 1)));
            if(tile_column < VIEW_SPAN_WIDTH && tile_row < rows)
                draw_ambient_effect(camera, (uint8_t)(tile_row << 4 | tile_column), seed, pick);
            seed += AMBIENT_SEED_X;
        }
        row_seed += AMBIENT_SEED_Y;
    }
}

// Birds fly over everything, actors included. Only one lane can show in the view, and its test
// is cheap, so the ambient sheet is selected only for a bird in view.
RPG_NOINLINE static void draw_birds(Camera camera) {
    AmbientBird bird = ambient_bird(bird_lane(camera.y), game.ticks, camera.x);
    if(!bird.visible)
        return;
    avm_set_sprite(AMBIENT_SPRITES);
    int16_t x = bird.x, y = (int16_t)(bird.y - camera.y);
    // The leader, then its followers behind it on either side, each a little later in its rise
    // and fall.
    for(uint8_t index = 0; index <= bird.followers; ++index) {
        BirdPose pose = ambient_bird_pose((uint8_t)(bird.clock - index * BIRD_TRAIL_CLOCK));
        int16_t side = index == 1 ? BIRD_TRAIL_Y : index == 2 ? -BIRD_TRAIL_Y : 0;
        avm_draw_plus_mask(index ? x + bird.trail : x, y + side + pose.dy, pose.frame);
    }
}

// The terrain. Each phase of the tile cycle has its own sheet, so every animated tile shows its
// current frame at the cost of selecting that sheet.
RPG_NOINLINE static void draw_tiles(Camera camera, uint8_t height) {
    View view = world_view(camera);
    uint8_t const* tiles = view.tiles;
    avm_set_sprite(tileset_phase_sprites(area_tileset(game.area), tile_phase(game.ticks)));
    for(int16_t y = view.top; y < height; y += TILE) {
        // A fixed row span preserves unrolling. Native drawing clips the
        // extra column when the camera is tile-aligned.
        uint8_t const* row_end = tiles + VIEW_SPAN_WIDTH;
        for(int16_t x = view.left; tiles != row_end; x += TILE) {
            uint8_t tile = *tiles++;
            if(tile == TILE_OUTSIDE)
                continue;
            avm_draw_overwrite(x, y, tile);
        }
        if(y + TILE < height)
            tiles += CACHE_WIDTH - VIEW_SPAN_WIDTH;
    }
}

// Actors in ascending foot order, so a lower actor covers a higher one.
RPG_NOINLINE static void draw_actors(Camera c) {
    avm_set_sprite(ACTOR_SPRITES);
    uint8_t order[NPC_CAPACITY + 1];
    uint8_t count = actor_draw_order(order);
    for(uint8_t i = 0; i < count; ++i) {
        uint8_t slot = order[i];
        Actor const& actor = actor_in_draw_slot(slot);
        uint8_t row =
            slot == PLAYER_DRAW_SLOT ? 0 : npc_definition(game.npcs.actors[slot].id)->sprite_row;
        avm_draw_plus_mask((int16_t)actor.x - (int16_t)c.x, (int16_t)actor.y - (int16_t)c.y,
                           actor_frame(actor, row, game.ticks));
    }
}

// Terrain, the effects that lie on it, the actors, and then the birds above them. Each pass is
// out of line, so only one pass's locals are on the stack at a time.
void rpg_render_world(uint8_t covered) {
    uint8_t height = 64 - covered;
    Camera shown = covered ? game_camera(height) : camera;
    draw_tiles(shown, height);
    uint8_t tileset = area_tileset(game.area);
    if(tileset_has_ambient(tileset)) {
        // Most frames have no gust in view, and that test needs only the camera's tile.
        if(gust_in_view(gust_origin(shown)))
            draw_gusts(shown, height);
        draw_ambient_tiles(shown, height);
    }
    draw_actors(shown);
    if(tileset_has_sky(tileset))
        draw_birds(shown);

    if(game.can_interact)
        avm_draw_text(110, 7, F("A"));
}

// Rebuild the outgoing image: exploration and dialogue normally clear the framebuffer after
// presenting it. This also removes any overlay before the world is faded away.
void fade_world_out() {
    dialogue_close();
    avm_draw_filled_rect_black(0, 0, DISPLAY_WIDTH, 64);
    rpg_render_world();
    fade_to_black();
}

// The caller finishes destination cache/tile/NPC updates while the OLED stays black. Re-rendering
// this frozen scene under the masks avoids a second framebuffer or any extra static RAM.
void fade_world_in() {
    for(uint8_t step = FADE_STEPS; step--;) {
        rpg_render_world();
        show_fade(step);
    }
    input_sync();
}

static void draw_title_sprite(TitleSprite sprite) {
    if(sprite.visible)
        avm_draw_plus_mask(sprite.x, sprite.y, sprite.frame);
}

// A pine goes on band by band from its top, each band shifted as far as the tree leans there.
static void draw_title_tree(TitleTree const& tree, uint8_t const PROGMEM* bands, uint16_t ticks) {
    int16_t lean = title_tree_lean(tree, ticks);

    for(uint8_t band = 0; band < tree.bands; ++band)
        avm_draw_sprite_plus_mask((int16_t)(tree.x + title_band_shift(tree, band, lean)),
                                  (int16_t)(tree.y + band * 8), bands, band);
}

// The background covers the whole screen, so the frame needs no clearing. Everything else goes
// on top from the far sky forward: the layers of the foreground hide whatever was drawn before
// them, which is how a creature comes out from behind a tree. The name goes on last.
void render_title(uint16_t ticks) {
    avm_draw_sprite_overwrite(0, 0, TITLE_SCENE, 0);
    avm_set_sprite(TITLE_SPRITES);

    // Sky and castle.
    for(uint8_t slot = 0; slot < TITLE_TWINKLES; ++slot) {
        TitleSprite star = title_twinkle(slot, ticks);
        if(star.visible) {
            // The star's own pixel is in the background; these are the arms of its cross.
            avm_draw_filled_rect_white((int16_t)(star.x - 1), star.y, 3, 1);
            avm_draw_filled_rect_white(star.x, (int16_t)(star.y - 1), 1, 3);
        }
    }
    avm_draw_plus_mask(TITLE_FLAG_AT.x, TITLE_FLAG_AT.y, title_flag_frame(ticks));
    for(uint8_t bird = 0; bird < TITLE_FLOCK; ++bird)
        draw_title_sprite(title_flock(bird, ticks));

    // Water.
    for(uint8_t slot = 0; slot < TITLE_RIPPLES; ++slot)
        draw_title_sprite(title_ripple(slot, ticks));
    TitleFish fish = title_fish(ticks);
    draw_title_sprite(fish.ring);
    draw_title_sprite(fish.fish);

    // The bank, from its far side to its near side.
    draw_title_tree(PINE_SMALL, TITLE_PINE_SMALL, ticks);
    draw_title_sprite(title_rabbit(ticks));
    draw_title_tree(PINE_LEFT, TITLE_PINE_LEFT, ticks);
    draw_title_sprite(title_mouse(ticks));
    draw_title_tree(PINE_RIGHT, TITLE_PINE_RIGHT, ticks);
    avm_draw_sprite_plus_mask(TITLE_ROCK_AT.x, TITLE_ROCK_AT.y, TITLE_ROCK, 0);
    for(TitlePerch const& perch : TITLE_PERCHES)
        draw_title_sprite(title_bird(perch, ticks));

    // Air.
    for(uint8_t mote = 0; mote < TITLE_MOTE_COUNT; ++mote) {
        TitleSprite dust = title_mote(mote, ticks);
        if(dust.visible)
            avm_draw_filled_rect_white(dust.x, dust.y, MOTE_LENGTH, 1);
    }

    avm_draw_sprite_plus_mask(TITLE_LOGO_X, TITLE_LOGO_Y, TITLE_LOGO, 0);
}

static char const PROGMEM* menu_label(PAUSE_ITEM item) {
    static char const PROGMEM* const PROGMEM LABELS[] = {F("Resume"),  F("Save"), F("Party"),
                                                         F("Journal"), F("Map"),  F("Resign"),
                                                         F("Audio"),   F("Pass"), F("Load")};
    return LABELS[item];
}

uint8_t menu_width(bool in_battle) {
    uint16_t width = avm_text_width(F("Paused")) + 8;
    for(uint8_t item = 0; item < PAUSE_ITEM_COUNT; ++item) {
        if(!menu_item_visible((PAUSE_ITEM)item, save_exists, in_battle))
            continue;
        uint16_t item_width = avm_text_width(menu_label((PAUSE_ITEM)item)) + 14;
        if(item_width > width)
            width = item_width;
    }
    return (uint8_t)width;
}

void render_menu(uint8_t revealed, uint8_t width, PAUSE_ITEM selection, bool can_load,
                 char const PROGMEM* feedback, MENU_EXIT action, BattleScreen const* battle) {
    if(battle)
        render_battle(*battle);
    else
        rpg_render_world();
    uint8_t fade = (uint8_t)((uint16_t)revealed * (FADE_STEPS / 2) / width);
    draw_fade(menu_opens_screen(action) ? FADE_STEPS / 2
              : action == MENU_LOAD     ? FADE_STEPS - fade
                                        : fade);
    if(!revealed)
        return;

    int16_t x = (int16_t)revealed - width;
    avm_draw_filled_rect_black(x, 0, width, 64);
    avm_draw_filled_rect_white(x + 1, 1, width - 2, 62);
    avm_draw_filled_rect_black(x + 2, 2, width - 4, 60);
    avm_draw_text(x + 4, 8, F("Paused"));

    uint8_t y = 17;
    for(uint8_t item = 0; item < PAUSE_ITEM_COUNT; ++item) {
        if(!menu_item_visible((PAUSE_ITEM)item, can_load, battle != nullptr))
            continue;
        avm_draw_text(x + 10, y, menu_label((PAUSE_ITEM)item));
        if(item == selection)
            avm_draw_text(x + 4, y, F(">"));
        y += 7;
    }
    if(feedback && revealed == width)
        avm_draw_text(width + 4, 58, feedback);
}

void render_menu_confirmation(PAUSE_ITEM selection) {
    char const PROGMEM* prompt = F("A Accept  B Cancel");
    char const PROGMEM* title = selection == PAUSE_PASS ? F("Pass turn?") : F("Resign?");
    uint8_t width = (uint8_t)avm_text_width(prompt) + 12;
    int16_t x = (DISPLAY_WIDTH - width) / 2;
    constexpr uint8_t Y = 18, HEIGHT = 28;
    avm_draw_filled_rect_black(x, Y, width, HEIGHT);
    avm_draw_filled_rect_white(x + 1, Y + 1, width - 2, HEIGHT - 2);
    avm_draw_filled_rect_black(x + 2, Y + 2, width - 4, HEIGHT - 4);
    avm_draw_text((DISPLAY_WIDTH - avm_text_width(title)) / 2, Y + 10, title);
    avm_draw_text(x + 6, Y + 21, prompt);
}

void render_audio(uint8_t revealed, OPTION_ITEM selection, bool closing,
                  BattleScreen const* battle) {
    if(battle)
        render_battle(*battle);
    else
        rpg_render_world();
    draw_fade(closing ? (uint8_t)((uint16_t)revealed * (FADE_STEPS / 2) / AUDIO_WIDTH)
                      : FADE_STEPS / 2);
    if(!revealed)
        return;

    int16_t x = DISPLAY_WIDTH - revealed;
    avm_draw_filled_rect_black(x, 0, AUDIO_WIDTH, 64);
    avm_draw_filled_rect_white(x + 1, 1, AUDIO_WIDTH - 2, 62);
    avm_draw_filled_rect_black(x + 2, 2, AUDIO_WIDTH - 4, 60);
    avm_draw_text(x + 4, 8, F("Audio"));
    avm_draw_text(x + 51, 8, F("B resume"));

    static char const PROGMEM* const PROGMEM LABELS[] = {F("Music"), F("SFX")};
    for(uint8_t item = 0; item < OPTION_ITEM_COUNT; ++item) {
        uint8_t y = 22 + item * 16;
        uint8_t level = option_level((OPTION_ITEM)item);
        avm_draw_text(x + 10, y, LABELS[item]);
        if(item == selection)
            avm_draw_text(x + 4, y, F(">"));
        if(!level)
            avm_draw_text(x + 63, y, F("Off"));
        else
            avm_draw_textf(x + 63, y, F("%u%%"),
                           (level * 100 + OPTION_SLIDER_MAX / 2) / OPTION_SLIDER_MAX);

        constexpr uint8_t TRACK_WIDTH = AUDIO_WIDTH - 20;
        avm_draw_filled_rect_white(x + 10, y + 5, TRACK_WIDTH, 1);
        avm_draw_filled_rect_white(x + 10, y + 3, 1, 5);
        avm_draw_filled_rect_white(x + 10 + TRACK_WIDTH - 1, y + 3, 1, 5);
        int16_t knob = x + 10 + (uint16_t)level * (TRACK_WIDTH - 3) / OPTION_VOLUME_MAX;
        avm_draw_filled_rect_black(knob - 1, y + 1, 5, 9);
        avm_draw_filled_rect_white(knob, y + 2, 3, 7);
    }
}

// Measures text[from, to) four characters at a time, as the dialogue measures words. Each chunk is
// a short NUL-terminated scratch string on the stack, and the text itself stays in program memory.
// Advances are additive, so the width of a range is the sum of its chunks.
__attribute__((noinline)) static uint16_t journal_range_width(char const PROGMEM* text,
                                                              uint8_t from, uint8_t to) {
    char chunk[5];
    uint16_t width = 0;
    while(from < to) {
        uint8_t count = (uint8_t)(to - from < 4 ? to - from : 4);
        memcpy_P(chunk, text + from, count);
        from = (uint8_t)(from + count);
        chunk[count] = 0;
        width += avm_text_width(chunk);
    }
    return width;
}

// Draws text[from, to) in chunks from the pen at (x, y), and returns the pen's x after the last one.
__attribute__((noinline)) static int16_t journal_draw_range(int16_t x, int16_t y,
                                                            char const PROGMEM* text,
                                                            uint8_t from, uint8_t to) {
    char chunk[25];
    while(from < to) {
        uint8_t count = (uint8_t)(to - from < 24 ? to - from : 24);
        memcpy_P(chunk, text + from, count);
        from = (uint8_t)(from + count);
        chunk[count] = 0;
        x = avm_draw_text(x, y, chunk).x;
    }
    return x;
}

// The wrap's width callback. It holds no state, so no program-memory pointer is captured or passed
// by value. A captured flash pointer was truncated to 16 bits when the closure was coerced.
struct JournalRangeWidth {
    uint16_t operator()(char const PROGMEM* text, uint8_t from, uint8_t to) const {
        return journal_range_width(text, from, to);
    }
};

void journal_layout(JournalView& view) {
    char const PROGMEM* track = quest_selected() == view.open ? F("A untrack") : F("A track");
    view.status_x = (uint8_t)(4 + avm_text_width(F("Status: ")));
    view.track_x = (uint8_t)(124 - avm_text_width(track));
    view.goal_x = (uint8_t)(4 + avm_text_width(F("Goal: ")));
}

void journal_open(JournalView& view, QUEST_ID id) {
    view.open = id;
    view.scroll = 0;
    view.line_count = journal_wrap(quest_summary(id), JournalRangeWidth{}, view.starts,
                                   JOURNAL_SUMMARY_LINES);
    journal_layout(view);
}

static char const PROGMEM* journal_status_label(QUEST_ID id) {
    switch(quest_status(id)) {
        case QUEST_ACTIVE:
            return F("Active");
        case QUEST_COMPLETED:
            return F("Completed");
        case QUEST_FAILED:
            return F("Failed");
        default:
            return F("");
    }
}

static void render_journal_list(int16_t x, JournalView const& view) {
    avm_draw_text(x + 4, 8, F("Journal"));
    avm_draw_text(x + 89, 8, F("B back"));
    uint8_t count = quest_journal_count();
    if(!count) {
        char const PROGMEM* empty = F("No quests yet.");
        avm_draw_text(x + (JOURNAL_WIDTH - avm_text_width(empty)) / 2, 34, empty);
        return;
    }
    // The window keeps the highlighted row on its last line once the list scrolls.
    uint8_t first = view.selection >= JOURNAL_ROWS ? view.selection - (JOURNAL_ROWS - 1) : 0;
    for(uint8_t row = 0; row < JOURNAL_ROWS && first + row < count; ++row) {
        uint8_t index = first + row;
        QUEST_ID id = quest_journal_entry(index);
        int16_t y = 20 + row * 10;
        if(index == view.selection)
            avm_draw_text(x + 4, y, F(">"));
        avm_draw_text(x + 10, y, quest_title(id));
        char const PROGMEM* state = quest_selected() == id ? F("Tracked") : journal_status_label(id);
        avm_draw_text(x + 118 - avm_text_width(state), y, state);
    }
}

static void render_journal_detail(int16_t x, JournalView const& view) {
    QUEST_ID id = view.open;
    avm_draw_text(x + 4, 8, quest_title(id));
    avm_draw_text(x + 89, 8, F("B back"));
    avm_draw_text(x + 4, 18, F("Status:"));
    avm_draw_text(x + view.status_x, 18, journal_status_label(id));
    if(quest_status(id) == QUEST_ACTIVE) {
        char const PROGMEM* track = quest_selected() == id ? F("A untrack") : F("A track");
        avm_draw_text(x + view.track_x, 18, track);
    }

    // Each line stops at its last word, so the trailing spaces that join it to the next line are
    // not drawn.
    char const PROGMEM* summary = quest_summary(id);
    for(uint8_t row = 0; row < JOURNAL_DETAIL_LINES && view.scroll + row < view.line_count; ++row) {
        uint8_t line = view.scroll + row;
        uint8_t from = view.starts[line], to = view.starts[line + 1];
        while(to > from && summary[to - 1] == ' ')
            --to;
        journal_draw_range(x + 4, 27 + 7 * row, summary, from, to);
    }
    if(view.scroll)
        avm_draw_text(x + 118, 27, F("^"));
    if(view.scroll + JOURNAL_DETAIL_LINES < view.line_count)
        avm_draw_text(x + 118, 48, F("v"));

    char const PROGMEM* objective = quest_objective(id);
    if(objective) {
        avm_draw_text(x + 4, 57, F("Goal:"));
        avm_draw_text(x + view.goal_x, 57, objective);
    }
}

// The panel's frame and contents, with its left edge at x.
static void render_journal_panel(int16_t x, JournalView const& view) {
    avm_draw_filled_rect_black(x, 0, JOURNAL_WIDTH, 64);
    avm_draw_filled_rect_white(x + 1, 1, JOURNAL_WIDTH - 2, 62);
    avm_draw_filled_rect_black(x + 2, 2, JOURNAL_WIDTH - 4, 60);
    if(view.open == QUEST_NONE)
        render_journal_list(x, view);
    else
        render_journal_detail(x, view);
}

void render_journal(uint8_t revealed, bool closing, JournalView const& view) {
    // A fully revealed panel covers every pixel, so nothing under it is drawn.
    if(revealed == JOURNAL_WIDTH && !closing) {
        render_journal_panel(0, view);
        return;
    }
    rpg_render_world();
    draw_fade(closing ? (uint8_t)((uint16_t)revealed * (FADE_STEPS / 2) / JOURNAL_WIDTH)
                      : FADE_STEPS / 2);
    if(!revealed)
        return;
    render_journal_panel(DISPLAY_WIDTH - revealed, view);
}

// Draws a label, then its value one space after it.
static void draw_label_value(int16_t x, int16_t baseline, char const PROGMEM* label,
                             char const PROGMEM* value) {
    int16_t end = avm_draw_text(x, baseline, label).x;
    avm_draw_text((int16_t)(end + avm_text_width(F(" "))), baseline, value);
}

// The page of one party member. Its rows are the font's seven-pixel lines, from baseline 9 down.
void render_party(uint8_t revealed, bool closing, uint8_t member_index) {
    rpg_render_world();
    draw_fade(closing ? (uint8_t)((uint16_t)revealed * (FADE_STEPS / 2) / PARTY_WIDTH)
                      : FADE_STEPS / 2);
    if(!revealed)
        return;

    int16_t x = DISPLAY_WIDTH - revealed;
    avm_draw_filled_rect_black(x, 0, PARTY_WIDTH, 64);
    avm_draw_filled_rect_white(x + 1, 1, PARTY_WIDTH - 2, 62);
    avm_draw_filled_rect_black(x + 2, 2, PARTY_WIDTH - 4, 60);
    int16_t left = x + 4, right = x + PARTY_WIDTH - 4;
    PartyMember const& member = game.party.members[member_index];

    avm_draw_textf(left, 9, F("Party %u/%u"), member_index + 1, game.party.count);
    avm_draw_text((int16_t)(right - avm_text_width(F("B back"))), 9, F("B back"));

    auto name = npc_name(member.id);
    auto title = name ? name : F("Traveler");
    avm_draw_text((int16_t)(x + (PARTY_WIDTH - avm_text_width(title)) / 2), 17, title);
    if(game.party.count > 1) {
        avm_draw_text(left, 17, F("<"));
        avm_draw_text((int16_t)(right - avm_text_width(F(">"))), 17, F(">"));
    }

    avm_draw_textf(left, 25, F("HP %u/%u"), member.hp, party_max_hp(member));
    avm_draw_textf(x + PARTY_WIDTH / 2, 25, F("AP %u/%u"), member.ap, party_max_ap(member));

    static char const PROGMEM* const PROGMEM MOVEMENT[] = {F("Step"), F("Leap"),
                                                           F("Charge")};
    static char const PROGMEM* const PROGMEM ATTACK[] = {F("Strike"), F("Shoot"), F("Reach"),
                                                         F("Impact"), F("Rend")};
    static_assert(sizeof(MOVEMENT) / sizeof(MOVEMENT[0]) == MOVEMENT_CLASS_COUNT);
    static_assert(sizeof(ATTACK) / sizeof(ATTACK[0]) == ATTACK_CLASS_COUNT);
    draw_label_value(left, 33, F("Move:"), MOVEMENT[member.movement_class]);
    draw_label_value(left, 40, F("Attack:"), ATTACK[member.attack_class]);

    // Each attack class's starting special, known from the first level, with its AP cost from the
    // battle rules.
    avm_draw_text(left, 48, F("Specials"));
    avm_draw_text(left, 56, BATTLE_SPECIAL_NAMES[member.attack_class]);
    avm_draw_textf(x + PARTY_WIDTH / 2, 56, F("%u AP"), SPECIAL_INFO[member.attack_class].ap);
}
