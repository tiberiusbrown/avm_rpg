#include "rpg.h"
#include "battle.h"
#include "tile_ids.h"
#include "npc.h"
#include "runtime_workspace.h"
#include "actor_draw_order.h"
#include "ambient.h"
#include "journal.h"
#include "party_ui.h"
#include "pause_menu.h"
#include "supplies_ui.h"
#include "shop_ui.h"
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

static int16_t text_draw_clipped(int16_t x, int16_t y, char const PROGMEM* text, uint16_t width);

// Five columns per icon, with the least significant bit at the top.
enum MENU_ICON : uint8_t {
    MENU_LEFT,
    MENU_RIGHT,
    MENU_UP,
    MENU_DOWN,
    MENU_BOOK,
    MENU_BLADE,
    MENU_STAR,
    MENU_PACK,
};

static constexpr uint8_t MENU_ICONS[] PROGMEM = {
    5, 5,
    0x04, 0x0a, 0x11, 0x00, 0x00, // Left
    0x11, 0x0a, 0x04, 0x00, 0x00, // Right
    0x04, 0x02, 0x01, 0x02, 0x04, // Up
    0x01, 0x02, 0x04, 0x02, 0x01, // Down
    0x0f, 0x09, 0x1f, 0x09, 0x0f, // Open book
    0x10, 0x18, 0x0c, 0x06, 0x03, // Blade
    0x04, 0x0e, 0x1f, 0x0e, 0x04, // Star
    0x1e, 0x15, 0x17, 0x15, 0x1e, // Pack
};

enum MENU_BUTTON : uint8_t { MENU_BUTTON_A, MENU_BUTTON_B };

static constexpr uint8_t MENU_BUTTON_SPRITES[] PROGMEM = {
    7, 7,
    0x3e, 0x7f, 0x43, 0x75, 0x43, 0x7f, 0x3e, // A
    0x3e, 0x7f, 0x41, 0x55, 0x6b, 0x7f, 0x3e, // B
};

static constexpr int16_t MENU_BUTTON_LABEL_OFFSET = 10;
static constexpr int16_t MENU_LEFT_EDGE = 4;
static constexpr int16_t MENU_RIGHT_EDGE = 124;
static constexpr int16_t MENU_FOOTER = 60;

static void draw_menu_rule(int16_t x, int16_t y, int16_t width = DISPLAY_WIDTH) {
    avm_draw_filled_rect_white(x + MENU_LEFT_EDGE, y, width - 2 * MENU_LEFT_EDGE, 1);
}

static void draw_menu_frame(int16_t x, int16_t y = 0, int16_t width = DISPLAY_WIDTH,
                            int16_t height = 64) {
    avm_draw_filled_rect_black(x, y, width, height);
    avm_draw_filled_rect_white(x + 1, y + 1, width - 2, height - 2);
    avm_draw_filled_rect_black(x + 2, y + 2, width - 4, height - 4);
}

static int16_t draw_menu_title(int16_t x, char const PROGMEM* title, uint16_t width, int16_t y = 10) {
    avm_set_text_font(AVM_FONT_PIXELOIDSANSBOLD);
    int16_t end = text_draw_clipped(x, y, title, width);
    avm_set_text_font(AVM_FONT_BR5D);
    return end;
}

static void draw_menu_button(int16_t x, int16_t y, MENU_BUTTON button,
                             char const PROGMEM* action) {
    avm_draw_sprite_overwrite(x, y - 6, MENU_BUTTON_SPRITES, button);
    avm_draw_text(x + MENU_BUTTON_LABEL_OFFSET, y, action);
}

static void draw_menu_footer(int16_t x, char const PROGMEM* action,
                             char const PROGMEM* back = F("Back")) {
    draw_menu_rule(x, 52);
    if(action)
        draw_menu_button(x + MENU_LEFT_EDGE, MENU_FOOTER, MENU_BUTTON_A, action);
    draw_menu_button(x + MENU_RIGHT_EDGE - MENU_BUTTON_LABEL_OFFSET - avm_text_width(back),
                     MENU_FOOTER, MENU_BUTTON_B, back);
}

static void begin_menu_row(int16_t x, int16_t y, bool selected, int16_t width) {
    if(selected) {
        // Leave one pixel above BR5D's ascenders and include the baseline for descenders.
        avm_draw_filled_rect_white(x + MENU_LEFT_EDGE, y - 6, width, 7);
        avm_set_text_mode(AVM_TEXT_BLACK_TRANSPARENT);
    }
}

static void draw_menu_scroll(int16_t x, int16_t top, int16_t bottom, bool above, bool below,
                             int16_t width = DISPLAY_WIDTH) {
    if(above)
        avm_draw_sprite_self_masked(x + width - 10, top - 5, MENU_ICONS, MENU_UP);
    if(below)
        avm_draw_sprite_self_masked(x + width - 10, bottom - 3, MENU_ICONS, MENU_DOWN);
}

static void draw_dialogue_footer_button(int16_t x, int16_t y, MENU_BUTTON button,
                                        char const PROGMEM* label, uint16_t label_width) {
    avm_draw_filled_rect_black(x - 1, y, MENU_BUTTON_LABEL_OFFSET + label_width + 2, 1);
    draw_menu_button(x, y, button, label);
}

void render_dialogue_options(char const PROGMEM* const* rows, uint8_t count, uint8_t selected,
                             uint8_t baseline, bool above, bool below, bool allow_back) {
    constexpr uint8_t ROW_HEIGHT = 7, BAR_WIDTH = 112;
    for(uint8_t row = 0; row < count; ++row) {
        int16_t y = baseline + row * ROW_HEIGHT;
        begin_menu_row(0, y, row == selected, BAR_WIDTH);
        text_draw_clipped(7, y, rows[row], BAR_WIDTH - 6);
        avm_set_text_mode(AVM_TEXT_WHITE_TRANSPARENT);
    }
    draw_menu_scroll(0, baseline, baseline + (DIALOGUE_OPTION_ROWS - 1) * ROW_HEIGHT,
                     above, below);
    int16_t footer = baseline + DIALOGUE_OPTION_ROWS * ROW_HEIGHT + 1;
    auto pick = F("Pick");
    draw_dialogue_footer_button(MENU_LEFT_EDGE, footer, MENU_BUTTON_A, pick, avm_text_width(pick));
    if(allow_back) {
        auto back = F("Back");
        uint16_t width = avm_text_width(back);
        draw_dialogue_footer_button(MENU_RIGHT_EDGE - MENU_BUTTON_LABEL_OFFSET - width,
                                    footer, MENU_BUTTON_B, back, width);
    }
}

RPG_NOINLINE static char const PROGMEM* menu_label(PAUSE_ITEM item) {
    static char const PROGMEM* const PROGMEM LABELS[] = {
        F("Resume"), F("Save"), F("Party"), F("Supplies"), F("Journal"), F("Map"),
        F("Resign"), F("Audio"), F("Pass"), F("Load"),
    };
    if(item == PAUSE_PARTY && party_has_unspent_perks(game.party))
        return F("Party !");
    return LABELS[item];
}

uint8_t menu_width(PauseMenuView const& view) {
    avm_set_text_font(AVM_FONT_PIXELOIDSANSBOLD);
    uint16_t width = avm_text_width(F("Paused")) + 8;
    avm_set_text_font(AVM_FONT_BR5D);
    for(uint8_t row = 0; row < view.count; ++row) {
        uint16_t item_width = avm_text_width(menu_label(pause_item(view, row))) + 14;
        if(item_width > width)
            width = item_width;
    }
    return (uint8_t)(width + (view.count > PAUSE_ROWS ? 6 : 0));
}

void render_menu(uint8_t revealed, uint8_t width, PauseMenuView const& view,
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
    // Keep one black row beneath the last highlight before the bottom border.
    draw_menu_frame(x, 0, width, 65);
    draw_menu_title(x + 4, F("Paused"), width - 8);
    draw_menu_rule(x, PAUSE_DIVIDER, width);

    uint8_t y = PAUSE_FIRST_BASELINE;
    uint8_t end = view.first + PAUSE_ROWS;
    uint8_t gutter = view.count > PAUSE_ROWS ? 8 : 0;
    for(uint8_t row = view.first; row < end && row < view.count; ++row) {
        begin_menu_row(x, y, row == view.selected, width - 8 - gutter);
        avm_draw_text(x + 7, y, menu_label(pause_item(view, row)));
        avm_set_text_mode(AVM_TEXT_WHITE_TRANSPARENT);
        y += PAUSE_ROW_STEP;
    }
    constexpr int16_t LAST_BASELINE = PAUSE_FIRST_BASELINE + (PAUSE_ROWS - 1) * PAUSE_ROW_STEP;
    draw_menu_scroll(x, PAUSE_FIRST_BASELINE, LAST_BASELINE, view.first, end < view.count, width);
    if(feedback && revealed == width)
        avm_draw_text(width + 4, 58, feedback);
}

void render_menu_confirmation(PAUSE_ITEM selection) {
    char const PROGMEM* accept = F("Accept");
    char const PROGMEM* cancel = F("Cancel");
    char const PROGMEM* title = selection == PAUSE_PASS ? F("Pass turn?") : F("Resign?");
    int16_t accept_width = MENU_BUTTON_LABEL_OFFSET + avm_text_width(accept);
    int16_t cancel_width = MENU_BUTTON_LABEL_OFFSET + avm_text_width(cancel);
    int16_t width = 12 + accept_width + 8 + cancel_width;
    int16_t x = (DISPLAY_WIDTH - width) / 2;
    constexpr uint8_t Y = 18, HEIGHT = 28;
    draw_menu_frame(x, Y, width, HEIGHT);
    draw_menu_title(x + 6, title, width - 12, Y + 10);
    draw_menu_rule(x, Y + 13, width);
    draw_menu_button(x + 6, Y + 23, MENU_BUTTON_A, accept);
    draw_menu_button(x + width - 6 - cancel_width, Y + 23, MENU_BUTTON_B, cancel);
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
    draw_menu_frame(x, 0, AUDIO_WIDTH);
    draw_menu_title(x + 4, F("Audio"), AUDIO_WIDTH - 8);
    draw_menu_rule(x, 13, AUDIO_WIDTH);
    draw_menu_rule(x, 52, AUDIO_WIDTH);
    draw_menu_button(x + 53, MENU_FOOTER, MENU_BUTTON_B, F("Back"));

    static char const PROGMEM* const PROGMEM LABELS[] = {F("Music"), F("SFX")};
    for(uint8_t item = 0; item < OPTION_ITEM_COUNT; ++item) {
        uint8_t y = 22 + item * 16;
        uint8_t level = option_level((OPTION_ITEM)item);
        bool selected = item == selection;
        begin_menu_row(x, y, selected, AUDIO_WIDTH - 8);
        avm_draw_text(x + 7, y, LABELS[item]);
        if(!level)
            avm_draw_text(x + 63, y, F("Off"));
        else
            avm_draw_textf(x + 63, y, F("%u%%"),
                           (level * 100 + OPTION_SLIDER_MAX / 2) / OPTION_SLIDER_MAX);

        avm_set_text_mode(AVM_TEXT_WHITE_TRANSPARENT);
        if(selected) {
            if(level)
                avm_draw_sprite_self_masked(x + 5, y + 3, MENU_ICONS, MENU_LEFT);
            if(level < OPTION_VOLUME_MAX)
                avm_draw_sprite_self_masked(x + AUDIO_WIDTH - 9, y + 3, MENU_ICONS, MENU_RIGHT);
        }
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
__attribute__((noinline)) static uint16_t text_range_width(char const PROGMEM* text, uint8_t from,
                                                           uint8_t to) {
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

// Draws text[from, to) in chunks from the pen at (x, y), and returns the pen's x after the last
// one.
__attribute__((noinline)) static int16_t
text_draw_range(int16_t x, int16_t y, char const PROGMEM* text, uint8_t from, uint8_t to) {
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

// The wrap's width callback for any text. It holds no state, so no program-memory pointer is
// captured or passed by value. A captured flash pointer was truncated to 16 bits when the closure
// was coerced.
struct TextRangeWidth {
    uint16_t operator()(char const PROGMEM* text, uint8_t from, uint8_t to) const {
        return text_range_width(text, from, to);
    }
};

// Draws as much of text as fits in `width` pixels, and returns the pen's x after it.
__attribute__((noinline)) static int16_t
text_draw_clipped(int16_t x, int16_t y, char const PROGMEM* text, uint16_t width) {
    if(avm_text_width(text) <= width)
        return avm_draw_text(x, y, text).x;
    uint8_t fit = 0;
    while(fit < UINT8_MAX && text[fit] && text_range_width(text, 0, (uint8_t)(fit + 1)) <= width)
        ++fit;
    return text_draw_range(x, y, text, 0, fit);
}

void journal_open(JournalView& view, QUEST_ID id) {
    view.open = id;
    view.scroll = 0;
    view.line_count =
        journal_wrap(quest_summary(id), TextRangeWidth{}, view.starts, JOURNAL_SUMMARY_LINES);
}

static char const PROGMEM* journal_status_label(QUEST_ID id, bool compact = false) {
    switch(quest_status(id)) {
        case QUEST_ACTIVE:
            return F("Active");
        case QUEST_COMPLETED:
            return compact ? F("Done") : F("Completed");
        case QUEST_FAILED:
            return F("Failed");
        default:
            return F("");
    }
}

static void render_journal_list(int16_t x, JournalView const& view) {
    avm_draw_sprite_self_masked(x + 5, 4, MENU_ICONS, MENU_BOOK);
    draw_menu_title(x + 14, F("Journal"), 100);
    draw_menu_rule(x, 12);

    uint8_t count = quest_journal_count();
    draw_menu_footer(x, count ? F("Open") : nullptr);
    if(!count) {
        char const PROGMEM* empty = F("No quests yet");
        avm_draw_text(x + (JOURNAL_WIDTH - avm_text_width(empty)) / 2, 34, empty);
        return;
    }

    // Keep the selection visible once the list scrolls.
    uint8_t first = view.selection >= JOURNAL_ROWS ? view.selection - (JOURNAL_ROWS - 1) : 0;
    for(uint8_t row = 0; row < JOURNAL_ROWS && first + row < count; ++row) {
        uint8_t index = first + row;
        QUEST_ID id = quest_journal_entry(index);
        int16_t y = 20 + row * 9;
        char const PROGMEM* state =
            quest_selected() == id ? F("Tracked") : journal_status_label(id, true);
        int16_t state_x = 113 - avm_text_width(state);
        begin_menu_row(x, y, index == view.selection, 112);
        text_draw_clipped(x + 7, y, quest_title(id), state_x - 10);
        avm_draw_text(x + state_x, y, state);
        avm_set_text_mode(AVM_TEXT_WHITE_TRANSPARENT);
    }
    draw_menu_scroll(x, 20, 47, first, first + JOURNAL_ROWS < count);
}

static void render_journal_detail(int16_t x, JournalView const& view) {
    QUEST_ID id = view.open;
    avm_draw_sprite_self_masked(x + 5, 4, MENU_ICONS, MENU_BOOK);
    char const PROGMEM* title = quest_title(id);
    avm_set_text_font(AVM_FONT_PIXELOIDSANSBOLD);
    if(avm_text_width(title) > 110 && title[0] == 'T' && title[1] == 'h' &&
       title[2] == 'e' && title[3] == ' ')
        title += 4;
    draw_menu_title(x + 14, title, 110);
    draw_menu_rule(x, 21);

    bool tracked = quest_selected() == id;
    avm_draw_text(x + 4, 18, tracked ? F("Tracked") : journal_status_label(id));
    if(quest_status(id) == QUEST_ACTIVE) {
        char const PROGMEM* action = tracked ? F("Untrack") : F("Track");
        draw_menu_button(x + MENU_RIGHT_EDGE - MENU_BUTTON_LABEL_OFFSET - avm_text_width(action),
                         18, MENU_BUTTON_A, action);
    }

    char const PROGMEM* summary = quest_summary(id);
    for(uint8_t row = 0; row < JOURNAL_DETAIL_LINES && view.scroll + row < view.line_count; ++row) {
        uint8_t line = view.scroll + row;
        uint8_t from = view.starts[line], to = view.starts[line + 1];
        while(to > from && summary[to - 1] == ' ')
            --to;
        text_draw_range(x + 4, 28 + 7 * row, summary, from, to);
    }
    draw_menu_scroll(x, 28, 49, view.scroll,
                     view.scroll + JOURNAL_DETAIL_LINES < view.line_count);

    draw_menu_footer(x, nullptr);
    if(auto objective = quest_objective(id)) {
        int16_t end = avm_draw_text(x + 4, MENU_FOOTER, F("Goal: ")).x;
        text_draw_clipped(end, MENU_FOOTER, objective, x + 88 - end);
    }
}

// The panel's frame and contents, with its left edge at x.
static void render_journal_panel(int16_t x, JournalView const& view) {
    draw_menu_frame(x);
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

static char const PROGMEM* party_member_name(PartyMember const& member) {
    auto name = npc_name(member.id);
    return name ? name : F("Alden");
}

static constexpr int16_t PARTY_ROW_SUBTITLE = 18;
static constexpr int16_t PARTY_ROW_BODY = 29;
static constexpr int16_t PARTY_ROW_STEP = 7;
static constexpr int16_t PARTY_MARGIN = MENU_LEFT_EDGE;
static constexpr int16_t PARTY_EDGE = MENU_RIGHT_EDGE;

static void render_party_divider(int16_t x) {
    draw_menu_rule(x, 21);
}

void render_battle_result(bool victory, uint8_t gold_gained) {
    constexpr int16_t X = 24, WIDTH = 80;
    int16_t y = victory ? 10 : 14;
    draw_menu_frame(X, y, WIDTH, victory ? 45 : 36);
    draw_menu_title(X + 6, victory ? F("VICTORY") : F("DEFEAT"), WIDTH - 12, y + 10);
    draw_menu_rule(X, y + 13, WIDTH);
    if(victory) {
        avm_draw_textf(X + 6, y + 23, F("+%u Gold"), gold_gained);
        avm_draw_textf(X + 6, y + 31, F("+%u XP to party"), PARTY_VICTORY_EXPERIENCE);
    } else {
        avm_draw_text(X + 6, y + 23, F("Game Over"));
    }
    draw_menu_button(X + 6, y + (victory ? 40 : 31), MENU_BUTTON_A, F("Continue"));
}

void render_battle_offer(char const PROGMEM* special, uint8_t ap, uint8_t choice, bool at_bottom) {
    uint16_t widest = avm_text_width(special);
    uint16_t attack_width = avm_text_width(F("Attack"));
    if(attack_width > widest)
        widest = attack_width;
    uint16_t cost_width = avm_text_width(F("9 AP"));
    int16_t width = 18 + widest + cost_width;
    // Leave one interior pixel between each outer row and the frame.
    constexpr int16_t HEIGHT = 20;
    int16_t x = (DISPLAY_WIDTH - width) / 2;
    int16_t y = at_bottom ? 64 - HEIGHT - 2 : 2;
    draw_menu_frame(x, y, width, HEIGHT);
    for(uint8_t row = 0; row < 2; ++row) {
        int16_t baseline = y + 9 + row * 7;
        begin_menu_row(x, baseline, row == choice, width - 8);
        avm_draw_text(x + 7, baseline, row == 0 ? F("Attack") : special);
        if(row)
            avm_draw_textf(x + width - 6 - cost_width, baseline, F("%u AP"), ap);
        avm_set_text_mode(AVM_TEXT_WHITE_TRANSPARENT);
    }
}

void render_level_up_notice(char const PROGMEM* title, LevelUpNotice notice) {
    draw_menu_frame(3, 2, 122, 60);
    draw_menu_title(8, title, 112, 13);
    draw_menu_rule(3, 16, 122);
    uint8_t y = 24;
    for(uint8_t i = 0; i < game.party.count; ++i) {
        if(!(notice.members & (1 << i)))
            continue;
        PartyMember const& member = game.party.members[i];
        text_draw_clipped(8, y, party_member_name(member), 78);
        avm_draw_textf(90, y, F("Lv %u"), member.level);
        y += 7;
    }
    if(notice.perk_earned)
        avm_draw_text(8, 57, F("Perk available!"));
    draw_menu_rule(3, 49, 122);
    draw_menu_button(79, 57, MENU_BUTTON_A, F("Continue"));
}

static void render_party_header(int16_t x, PartyUi const& ui) {
    PartyMember const& member = game.party.members[ui.member];
    uint8_t count = game.party.count;
    bool pending_perk = party_unspent_perks(member);
    int16_t end = draw_menu_title(x + PARTY_MARGIN, party_member_name(member), pending_perk ? 76 : 84);
    if(pending_perk)
        avm_draw_sprite_self_masked(end + 2, 4, MENU_ICONS, MENU_STAR);

    if(count > 1) {
        avm_draw_sprite_self_masked(x + 95, 4, MENU_ICONS, MENU_LEFT);
        avm_draw_textf(x + 102, 9, F("%u/%u"), ui.member + 1, count);
        avm_draw_sprite_self_masked(x + 120, 4, MENU_ICONS, MENU_RIGHT);
    }
}

// Draws the visible lines of a detail page's description from its scroll position. A mark at the
// right shows that more text lies above or below.
static void render_party_text(int16_t x, PartyUi const& ui) {
    char const PROGMEM* text = party_ui_text(ui, game.party);
    if(!text)
        return;
    for(uint8_t row = 0; row < PARTY_TEXT_ROWS && ui.scroll + row < ui.line_count; ++row) {
        uint8_t line = ui.scroll + row;
        uint8_t from = ui.starts[line], to = ui.starts[line + 1];
        // Each line stops at its last word, so the spaces that join it to the next line are not
        // drawn.
        while(to > from && text[to - 1] == ' ')
            --to;
        text_draw_range((int16_t)(x + PARTY_MARGIN),
                        (int16_t)(PARTY_ROW_BODY + PARTY_ROW_STEP * row), text, from, to);
    }
    draw_menu_scroll(x, PARTY_ROW_BODY, PARTY_ROW_BODY + PARTY_ROW_STEP * 3, ui.scroll,
                     ui.scroll + PARTY_TEXT_ROWS < ui.line_count);
}

static void render_party_overview(int16_t x, PartyUi const& ui) {
    PartyMember const& member = game.party.members[ui.member];
    int16_t left = x + PARTY_MARGIN;
    render_party_header(x, ui);
    draw_menu_rule(x, 12);

    avm_draw_textf(left, 20, F("Lv %u"), member.level);
    avm_draw_filled_rect_white(x + 27, 14, 1, 7);
    avm_draw_textf(x + 33, 20, F("HP %u/%u"), member.hp, party_max_hp(member));
    avm_draw_filled_rect_white(x + 77, 14, 1, 7);
    avm_draw_textf(x + 83, 20, F("AP %u/%u"), member.ap, party_max_ap(member));

    draw_label_value(left, 28, F("Move:"), party_movement_name(member.movement_class));
    draw_label_value(x + 64, 28, F("Attack:"), party_attack_name(member.attack_class));
    if(member.level == PARTY_MAX_LEVEL)
        avm_draw_text(left, 36, F("Max level"));
    else
        avm_draw_textf(left, 36, F("XP %u/%u"), member.experience,
                       party_experience_needed(member.level));
    draw_menu_rule(x, 38);

    static constexpr char const PROGMEM* CATEGORIES[CATEGORY_COUNT] PROGMEM = {
        F("Special Attacks"), F("Perks"), F("Equipment"),
    };
    for(uint8_t category = 0; category < CATEGORY_COUNT; ++category) {
        int16_t y = 46 + 7 * category;
        bool selected = category == ui.selection[PARTY_PAGE_OVERVIEW];
        MENU_ICON icon = static_cast<MENU_ICON>(MENU_BLADE + category);
        begin_menu_row(x, y, selected, PARTY_EDGE - PARTY_MARGIN);
        avm_draw_text(x + 15, y, CATEGORIES[category]);
        if(category == CATEGORY_SPECIALS)
            avm_draw_textf(x + 108, y, F("%u"), party_special_count(member));
        if(category == CATEGORY_PERKS && party_unspent_perks(member))
            avm_draw_textf(x + 108, y, F("%u"), party_unspent_perks(member));
        avm_set_text_mode(AVM_TEXT_WHITE_TRANSPARENT);
        if(selected) {
            avm_draw_sprite_erase(x + 7, y - 5, MENU_ICONS, icon);
            avm_draw_sprite_erase(x + 118, y - 5, MENU_ICONS, MENU_RIGHT);
        } else {
            avm_draw_sprite_self_masked(x + 7, y - 5, MENU_ICONS, icon);
            avm_draw_sprite_self_masked(x + 118, y - 5, MENU_ICONS, MENU_RIGHT);
        }
    }
}

static void render_party_specials(int16_t x, PartyUi const& ui) {
    PartyMember const& member = game.party.members[ui.member];
    int16_t left = x + PARTY_MARGIN;
    render_party_header(x, ui);
    draw_label_value(left, PARTY_ROW_SUBTITLE, F("SPECIALS -"),
                     party_attack_name(member.attack_class));
    render_party_divider(x);

    // Each row is one special. The starting special is the only one for now, so one row names it.
    uint8_t count = party_special_count(member);
    uint8_t selected = ui.selection[PARTY_PAGE_SPECIALS];
    for(uint8_t row = 0; row < count; ++row) {
        int16_t y = (int16_t)(PARTY_ROW_BODY + PARTY_ROW_STEP * row);
        begin_menu_row(x, y, row == selected, PARTY_EDGE - PARTY_MARGIN);
        avm_draw_text((int16_t)(left + 3), y, party_special_name(member.attack_class));
        avm_draw_textf((int16_t)(x + PARTY_EDGE - 24), y, F("%u AP"),
                       SPECIAL_INFO[member.attack_class].ap);
        avm_set_text_mode(AVM_TEXT_WHITE_TRANSPARENT);
    }
    draw_menu_footer(x, F("Inspect"));
}

static void render_party_special_details(int16_t x, PartyUi const& ui) {
    PartyMember const& member = game.party.members[ui.member];
    uint8_t attack = member.attack_class;
    AttackInfo const& normal = ATTACK_INFO[attack];
    int16_t left = x + PARTY_MARGIN;
    draw_menu_title(left, party_special_name(attack), 94);
    avm_draw_textf((int16_t)(x + PARTY_EDGE - 24), 9, F("%u AP"),
                   SPECIAL_INFO[attack].ap);
    avm_draw_text(left, PARTY_ROW_SUBTITLE, party_attack_name(attack));
    // The range is the normal attack's, along a row or column.
    if(normal.attack_min == normal.attack_max)
        avm_draw_textf((int16_t)(x + 64), PARTY_ROW_SUBTITLE, F("Range %u"), normal.attack_min);
    else
        avm_draw_textf((int16_t)(x + 64), PARTY_ROW_SUBTITLE, F("Range %u-%u"), normal.attack_min,
                       normal.attack_max);
    render_party_divider(x);
    render_party_text(x, ui);
    draw_menu_footer(x, nullptr);
}

static void render_party_perk_index(int16_t x, PartyUi const& ui) {
    PartyMember const& member = game.party.members[ui.member];
    int16_t left = x + PARTY_MARGIN;
    render_party_header(x, ui);
    avm_draw_text(left, PARTY_ROW_SUBTITLE, F("PERKS"));
    render_party_divider(x);

    uint8_t selected = ui.selection[PARTY_PAGE_PERKS];
    int16_t right = x + PARTY_EDGE - 20;
    uint8_t available = party_unspent_perks(member);
    for(uint8_t row = 0; row < PERK_ROW_COUNT; ++row) {
        int16_t y = (int16_t)(PARTY_ROW_BODY + PARTY_ROW_STEP * row);
        int16_t label_x = (int16_t)(left + 3);
        begin_menu_row(x, y, row == selected, PARTY_EDGE - PARTY_MARGIN);
        if(row == PERK_ROW_MOVEMENT) {
            draw_label_value(label_x, y, F("Movement:"), party_movement_name(member.movement_class));
            avm_draw_textf(right, y, F("%u/5"), party_movement_perk_rank(member));
        } else if(row == PERK_ROW_ATTACK) {
            draw_label_value(label_x, y, F("Attack:"), party_attack_name(member.attack_class));
            avm_draw_textf(right, y, F("%u/5"), party_attack_perk_rank(member));
        } else if(row == PERK_ROW_EQUIPMENT) {
            avm_draw_text(label_x, y, F("From Equipment"));
        } else {
            avm_draw_text(label_x, y, available ? F("Learn Perk!") : F("Perk Choices"));
            avm_draw_textf(right, y, F("%u"), available);
        }
        avm_set_text_mode(AVM_TEXT_WHITE_TRANSPARENT);
    }
    draw_menu_footer(x, F("Open"));
}

static void render_party_perk_list(int16_t x, PartyUi const& ui) {
    int16_t left = x + PARTY_MARGIN;
    uint8_t kind = ui.selection[PARTY_PAGE_PERKS];
    bool equipment = kind == PERK_ROW_EQUIPMENT;
    uint8_t count = party_ui_entries(ui, game.party);
    uint8_t selected = ui.selection[PARTY_PAGE_PERK_LIST];

    // A track's entries take one line each. An equipment entry takes two lines, the effect and the
    // item that grants it, so fewer of them fit on the list.
    uint8_t lines = equipment ? 2 : 1;
    uint8_t visible = PARTY_TEXT_ROWS / lines;
    uint8_t first = selected >= visible ? (uint8_t)(selected - (visible - 1)) : 0;

    if(equipment) {
        draw_menu_title(left, F("From Equipment"), 120);
        if(!count)
            avm_draw_text(left, PARTY_ROW_BODY, F("No equipment effects"));
    } else {
        draw_menu_title(left, kind == PERK_ROW_MOVEMENT ? F("Movement Perks") : F("Attack Perks"),
                        120);
        uint8_t track = party_perk_track(party_ui_row_perk(ui, game.party, 0));
        avm_draw_text(left, PARTY_ROW_SUBTITLE, party_track_name(track));
    }
    render_party_divider(x);

    for(uint8_t k = 0; k < visible && first + k < count; ++k) {
        uint8_t index = (uint8_t)(first + k);
        uint8_t code = party_ui_row_perk(ui, game.party, index);
        int16_t y = (int16_t)(PARTY_ROW_BODY + PARTY_ROW_STEP * lines * k);
        begin_menu_row(x, y, index == selected, 112);
        if(equipment) {
            avm_draw_text(left + 3, y, party_perk_name(code));
            avm_set_text_mode(AVM_TEXT_WHITE_TRANSPARENT);
            avm_draw_text(left + 3, y + PARTY_ROW_STEP, party_perk_source(code));
        } else {
            avm_draw_textf((int16_t)(left + 3), y, F("%u."), index + 1);
            avm_draw_text((int16_t)(left + 15), y, party_perk_name(code));
            PartyMember const& member = game.party.members[ui.member];
            PERK_TRACK_KIND track = kind == PERK_ROW_MOVEMENT ? PERK_MOVEMENT : PERK_ATTACK;
            PARTY_PERK_STATUS status = party_perk_status(member, track, index + 1);
            if(status != PERK_STATUS_FUTURE) {
                char const PROGMEM* label = status == PERK_STATUS_EARNED ? F("Earned") :
                                            status == PERK_STATUS_LEARNABLE ? F("Ready") : F("Locked");
                avm_draw_text(x + PARTY_EDGE - 8 - avm_text_width(label), y, label);
            }
        }
        avm_set_text_mode(AVM_TEXT_WHITE_TRANSPARENT);
    }
    draw_menu_scroll(x, PARTY_ROW_BODY, PARTY_ROW_BODY + PARTY_ROW_STEP * 3, first,
                     first + visible < count);
    draw_menu_footer(x, count ? F("Open") : nullptr);
}

static void render_party_perk_choice(int16_t x, PartyUi const& ui) {
    int16_t left = x + PARTY_MARGIN;
    PartyMember const& member = game.party.members[ui.member];
    uint8_t available = party_unspent_perks(member);
    draw_menu_title(left, F("Choose Perk"), 120);
    avm_draw_textf(left, PARTY_ROW_SUBTITLE, F("Available: %u"), available);
    if(ui.perk_learned)
        avm_draw_text(x + PARTY_EDGE - 44, PARTY_ROW_SUBTITLE, F("Learned!"));
    render_party_divider(x);

    uint8_t selected = ui.selection[PARTY_PAGE_PERK_CHOICE];
    uint8_t count = party_ui_entries(ui, game.party);
    for(uint8_t index = 0; index < count; ++index) {
        int16_t y = (int16_t)(PARTY_ROW_BODY + PARTY_ROW_STEP * index);
        begin_menu_row(x, y, index == selected, PARTY_EDGE - PARTY_MARGIN);
        PERK_TRACK_KIND track = index == 0 ? PERK_MOVEMENT : PERK_ATTACK;
        char const PROGMEM* name = party_perk_rank(member, track) >= PERK_IMPLEMENTED_RANKS
                                        ? F("Next unavailable")
                                        : party_perk_name(party_ui_row_perk(ui, game.party, index));
        draw_label_value((int16_t)(left + 3), y, index == 0 ? F("Move:") : F("Attack:"), name);
        avm_set_text_mode(AVM_TEXT_WHITE_TRANSPARENT);
    }
    int16_t note_y = PARTY_ROW_BODY + PARTY_ROW_STEP * 3;
    if(party_movement_perk_rank(member) >= PERK_IMPLEMENTED_RANKS &&
       party_attack_perk_rank(member) >= PERK_IMPLEMENTED_RANKS)
        avm_draw_text(left, note_y, F("More ranks in future"));
    else if(!available)
        avm_draw_text(left, note_y, F("No choices available"));
    draw_menu_footer(x, F("Inspect"));
}

static void render_party_perk_details(int16_t x, PartyUi const& ui) {
    int16_t left = x + PARTY_MARGIN;
    uint8_t track = party_perk_track(ui.entry);
    draw_menu_title(left, party_perk_name(ui.entry), 120);
    if(track == PARTY_NO_TRACK)
        avm_draw_text(left, PARTY_ROW_SUBTITLE, party_perk_source(ui.entry));
    else
        draw_label_value(left, PARTY_ROW_SUBTITLE,
                         track < MOVEMENT_CLASS_COUNT ? F("Movement:") : F("Attack:"),
                         party_track_name(track));
    render_party_divider(x);
    render_party_text(x, ui);

    draw_menu_footer(x, ui.back == PARTY_PAGE_PERK_CHOICE ? F("Continue") : nullptr);
}

static void render_party_perk_confirm(int16_t x, PartyUi const& ui) {
    int16_t left = x + PARTY_MARGIN;
    draw_menu_title(left, F("Learn Perk?"), 120);
    avm_draw_text(left, PARTY_ROW_SUBTITLE, party_perk_name(ui.entry));
    render_party_divider(x);
    avm_draw_text(left, PARTY_ROW_BODY, F("Spend 1 choice."));
    avm_draw_text(left, PARTY_ROW_BODY + PARTY_ROW_STEP, F("This is permanent."));
    draw_menu_footer(x, F("Learn"), F("Cancel"));
}

static void render_party_equipment(int16_t x, PartyUi const& ui) {
    PartyMember const& member = game.party.members[ui.member];
    int16_t left = x + PARTY_MARGIN;
    render_party_header(x, ui);
    avm_draw_text(left, PARTY_ROW_SUBTITLE, F("EQUIPMENT"));
    render_party_divider(x);

    uint8_t selected = ui.selection[PARTY_PAGE_EQUIPMENT];
    for(uint8_t slot = 0; slot < SLOT_COUNT; ++slot) {
        int16_t y = (int16_t)(PARTY_ROW_BODY + PARTY_ROW_STEP * slot);
        begin_menu_row(x, y, slot == selected, PARTY_EDGE - PARTY_MARGIN);
        avm_draw_text(left + 3, y, party_slot_name(slot));
        char const PROGMEM* equipped = party_equipped(member, slot);
        avm_draw_text(x + 46, y, equipped);
        avm_set_text_mode(AVM_TEXT_WHITE_TRANSPARENT);
    }
    draw_menu_footer(x, F("Open"));
}

static void render_party_item_list(int16_t x, PartyUi const& ui) {
    int16_t left = x + PARTY_MARGIN;
    draw_menu_title(left, party_slot_name(party_ui_slot(ui)), 120);
    if(ui.equipment_feedback == PartyUi::UNEQUIPPED)
        avm_draw_text(left, PARTY_ROW_SUBTITLE, F("Unequipped"));
    render_party_divider(x);

    constexpr uint8_t VISIBLE = 2;
    uint8_t count = party_ui_entries(ui, game.party);
    uint8_t selected = ui.selection[PARTY_PAGE_ITEM_LIST];
    uint8_t first = selected >= VISIBLE ? selected - (VISIBLE - 1) : 0;
    for(uint8_t row = 0; row < VISIBLE && first + row < count; ++row) {
        uint8_t index = first + row;
        ITEM_ID item = party_ui_row_item(ui, index);
        auto definition = equipment_item(item);
        int16_t y = PARTY_ROW_BODY + 2 * PARTY_ROW_STEP * row;
        begin_menu_row(x, y, index == selected, 112);
        avm_draw_text(left + 3, y, definition ? definition->name : F("(Unequip)"));
        avm_set_text_mode(AVM_TEXT_WHITE_TRANSPARENT);
        uint8_t wearer = equipment_wearer(item);
        if(wearer != PARTY_CAPACITY)
            avm_draw_text(left + 3, y + PARTY_ROW_STEP,
                          wearer == ui.member ? F("Equipped") :
                                                party_member_name(game.party.members[wearer]));
    }
    draw_menu_scroll(x, PARTY_ROW_BODY, PARTY_ROW_BODY + PARTY_ROW_STEP * 3, first,
                     first + VISIBLE < count);
    draw_menu_footer(x, selected ? F("Inspect") : F("Unequip"));
}

static void render_party_item_details(int16_t x, PartyUi const& ui) {
    int16_t left = x + PARTY_MARGIN;
    ITEM_ID item = (ITEM_ID)ui.entry;
    auto definition = equipment_item(item);
    draw_menu_title(left, definition->name, 120);
    avm_draw_text(left, PARTY_ROW_SUBTITLE, party_slot_name(definition->slot));
    uint8_t wearer = equipment_wearer(item);
    if(wearer != PARTY_CAPACITY) {
        auto name = party_member_name(game.party.members[wearer]);
        int16_t name_x = x + PARTY_EDGE - avm_text_width(name);
        avm_draw_text(name_x - avm_text_width(F("Worn: ")), PARTY_ROW_SUBTITLE, F("Worn: "));
        avm_draw_text(name_x, PARTY_ROW_SUBTITLE, name);
    }
    render_party_divider(x);
    render_party_text(x, ui);

    draw_menu_footer(x, wearer == ui.member ? nullptr :
                        wearer == PARTY_CAPACITY ? F("Equip") : F("Transfer"));
    if(wearer == ui.member)
        avm_draw_text(left, MENU_FOOTER,
                      ui.equipment_feedback == PartyUi::TRANSFERRED ? F("Transferred") :
                                                                    F("Equipped"));
}

static void render_party_panel(int16_t x, PartyUi const& ui) {
    draw_menu_frame(x);
    switch(ui.page) {
        case PARTY_PAGE_OVERVIEW:
            render_party_overview(x, ui);
            break;
        case PARTY_PAGE_SPECIALS:
            render_party_specials(x, ui);
            break;
        case PARTY_PAGE_SPECIAL_DETAILS:
            render_party_special_details(x, ui);
            break;
        case PARTY_PAGE_PERKS:
            render_party_perk_index(x, ui);
            break;
        case PARTY_PAGE_PERK_LIST:
            render_party_perk_list(x, ui);
            break;
        case PARTY_PAGE_PERK_CHOICE:
            render_party_perk_choice(x, ui);
            break;
        case PARTY_PAGE_PERK_DETAILS:
            render_party_perk_details(x, ui);
            break;
        case PARTY_PAGE_PERK_CONFIRM:
            render_party_perk_confirm(x, ui);
            break;
        case PARTY_PAGE_EQUIPMENT:
            render_party_equipment(x, ui);
            break;
        case PARTY_PAGE_ITEM_LIST:
            render_party_item_list(x, ui);
            break;
        case PARTY_PAGE_ITEM_DETAILS:
            render_party_item_details(x, ui);
            break;
        case PARTY_PAGE_COUNT:
            break;
    }
}

// The Party panel slides in over the world. Once it covers the whole screen, the world beneath it
// is no longer drawn.
void render_party(uint8_t revealed, bool closing, PartyUi const& ui) {
    if(revealed == PARTY_WIDTH && !closing) {
        render_party_panel(0, ui);
        return;
    }
    rpg_render_world();
    draw_fade(closing ? (uint8_t)((uint16_t)revealed * (FADE_STEPS / 2) / PARTY_WIDTH)
                      : FADE_STEPS / 2);
    if(!revealed)
        return;
    render_party_panel(DISPLAY_WIDTH - revealed, ui);
}

void party_layout(PartyUi& ui) {
    party_ui_layout(ui, game.party, TextRangeWidth{});
}

void supplies_layout(SuppliesUi& ui) {
    ui.lines = text_wrap(supply_description(ui.supply), 110, TextRangeWidth{}, ui.starts,
                         SUPPLIES_WRAP_CAPACITY);
}

static void render_supplies_rows(int16_t x, SuppliesUi const& ui) {
    bool targets = ui.page == SUPPLIES_TARGET;
    uint8_t count = targets ? game.party.count : SUPPLY_COUNT;
    for(uint8_t row = 0; row < count; ++row) {
        int16_t y = 22 + row * 8;
        bool selected = row == (targets ? (uint8_t)ui.target : (uint8_t)ui.supply);
        begin_menu_row(x, y, selected, 120);
        if(targets) {
            PartyMember const& member = game.party.members[row];
            bool ap = ui.supply == SUPPLY_ETHER;
            bool valid = supply_can_use(ui.supply, game.party, row);
            if(!valid)
                avm_draw_text(x + 7, y, F("-"));
            text_draw_clipped(x + 15, y, party_member_name(member), 57);
            avm_draw_textf(x + 76, y, ap ? F("AP %u/%u") : F("HP %u/%u"),
                           ap ? member.ap : member.hp,
                           ap ? party_max_ap(member) : party_max_hp(member));
        } else {
            auto id = (SUPPLY_ID)row;
            avm_draw_text(x + 7, y, supply_name(id));
            avm_draw_textf(x + 104, y, F("x%u"), supply_count(game.supplies, id));
        }
        avm_set_text_mode(AVM_TEXT_WHITE_TRANSPARENT);
    }
}

static void render_supplies_panel(int16_t x, SuppliesUi const& ui) {
    draw_menu_frame(x);
    draw_menu_title(x + 4, ui.page == SUPPLIES_LIST ? F("Supplies") : supply_name(ui.supply), 120);
    draw_menu_rule(x, 13);
    if(ui.page == SUPPLIES_TARGET)
        avm_draw_text(x + 65, 10, F("Choose target"));

    switch(ui.page) {
        case SUPPLIES_LIST:
        case SUPPLIES_TARGET:
            render_supplies_rows(x, ui);
            draw_menu_footer(x, ui.page == SUPPLIES_LIST ? F("Inspect") : F("Use"));
            if(ui.feedback)
                avm_draw_text(x + 60, MENU_FOOTER, ui.feedback);
            break;
        case SUPPLIES_DETAILS: {
            auto description = supply_description(ui.supply);
            for(uint8_t row = 0; row < SUPPLIES_DETAIL_LINES && ui.scroll + row < ui.lines; ++row) {
                uint8_t line = ui.scroll + row;
                uint8_t from = ui.starts[line], to = ui.starts[line + 1];
                while(to > from && description[to - 1] == ' ')
                    --to;
                text_draw_range(x + 4, 22 + 7 * row, description, from, to);
            }
            draw_menu_scroll(x, 22, 36, ui.scroll, ui.scroll + SUPPLIES_DETAIL_LINES < ui.lines);
            avm_draw_textf(x + 4, 47, F("Owned: %u"), supply_count(game.supplies, ui.supply));
            if(ui.feedback)
                text_draw_clipped(x + 47, 47, ui.feedback, 77);
            draw_menu_footer(x, F("Use"));
            break;
        }
        case SUPPLIES_CONFIRM:
            avm_draw_text(x + 4, 25, F("Restore up to 4 HP to all"));
            avm_draw_text(x + 4, 33, F("conscious party members?"));
            draw_menu_footer(x, F("Confirm"), F("Cancel"));
            break;
    }
}

void render_supplies(uint8_t revealed, bool closing, SuppliesUi const& ui) {
    if(revealed < SUPPLIES_WIDTH) {
        rpg_render_world();
        draw_fade(closing ? (uint8_t)((uint16_t)revealed * (FADE_STEPS / 2) / SUPPLIES_WIDTH)
                          : FADE_STEPS / 2);
    }
    if(revealed)
        render_supplies_panel(DISPLAY_WIDTH - revealed, ui);
}


void shop_layout(ShopUi& ui) {
    ui.lines = text_wrap(supply_description(ui.item), 110, TextRangeWidth{}, ui.starts,
                         SUPPLIES_WRAP_CAPACITY);
}

static void render_shop_panel(int16_t x, ShopUi const& ui) {
    draw_menu_frame(x);
    draw_menu_title(x + 4, ui.page == SHOP_LIST ? F("General Store") : supply_name(ui.item),
                    ui.page == SHOP_LIST ? 81 : 120);
    draw_menu_rule(x, 13);

    if(ui.page == SHOP_LIST) {
        avm_draw_textf(x + 88, 10, F("%u G"), game.gold);
        for(uint8_t row = 0; row < SUPPLY_COUNT; ++row) {
            auto item = (SUPPLY_ID)row;
            int16_t y = 22 + row * 8;
            begin_menu_row(x, y, item == ui.item, 120);
            avm_draw_text(x + 7, y, supply_name(item));
            avm_draw_textf(x + 78, y, F("%u G"), shop_price(item));
            avm_draw_textf(x + 106, y, F("x%u"), supply_count(game.supplies, item));
            avm_set_text_mode(AVM_TEXT_WHITE_TRANSPARENT);
        }
        draw_menu_footer(x, F("Inspect"), F("Exit"));
        if(ui.feedback)
            avm_draw_text(x + 52, MENU_FOOTER, ui.feedback);
        return;
    }

    if(ui.page == SHOP_DETAILS) {
        auto description = supply_description(ui.item);
        for(uint8_t row = 0; row < SUPPLIES_DETAIL_LINES && ui.scroll + row < ui.lines; ++row) {
            uint8_t line = ui.scroll + row;
            uint8_t from = ui.starts[line], to = ui.starts[line + 1];
            while(to > from && description[to - 1] == ' ')
                --to;
            text_draw_range(x + 4, 22 + 7 * row, description, from, to);
        }
        draw_menu_scroll(x, 22, 36, ui.scroll, ui.scroll + SUPPLIES_DETAIL_LINES < ui.lines);
        draw_menu_footer(x, F("Buy"));
    } else {
        avm_draw_text(x + 4, 23, F("Buy one?"));
        if(ui.feedback)
            avm_draw_text(x + 4, 35, ui.feedback);
        draw_menu_footer(x, F("Confirm"), F("Cancel"));
    }
    avm_draw_textf(x + 4, 47, F("Price: %u G"), shop_price(ui.item));
    avm_draw_textf(x + 70, 47, F("Owned: %u/%u"), supply_count(game.supplies, ui.item),
                   SUPPLY_LIMIT);
}

void render_shop(uint8_t revealed, bool closing, ShopUi const& ui) {
    if(revealed < SUPPLIES_WIDTH) {
        rpg_render_world();
        draw_fade(closing ? (uint8_t)((uint16_t)revealed * (FADE_STEPS / 2) / SUPPLIES_WIDTH)
                          : FADE_STEPS / 2);
    }
    if(revealed)
        render_shop_panel(DISPLAY_WIDTH - revealed, ui);
}
