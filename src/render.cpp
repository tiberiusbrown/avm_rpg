#include "rpg.h"
#include "tile_ids.h"
#include "npc.h"
#include "actor_draw_order.h"
#include "ambient.h"
#include "title.h"
#include "world_assets.h"

#include <avm.h>
#include <string.h>

static constexpr uint16_t DISPLAY_WIDTH = 128;

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

static View world_view() {
    uint8_t tx = (uint8_t)(game.camera.x >> TILE_SHIFT),
            ty = (uint8_t)(game.camera.y >> TILE_SHIFT);
    // The cache trails the view by its margin, so the view starts inside the window.
    return {game.cache.tiles + (uint8_t)(ty - game.cache.origin_ty) * CACHE_WIDTH +
                (uint8_t)(tx - game.cache.origin_tx),
            tx, ty, (int16_t) - (int16_t)(game.camera.x & (TILE - 1)),
            (int16_t) - (int16_t)(game.camera.y & (TILE - 1))};
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
static uint8_t gust_origin() {
    return gust_distance(gust_front(game.ticks), (uint8_t)(game.camera.x >> TILE_SHIFT),
                         (uint8_t)(game.camera.y >> TILE_SHIFT));
}

RPG_NOINLINE static void draw_gusts() {
    TileFrames wind = tile_wind_frames(area_tileset(game.area));
    if(!wind.count)
        return;
    uint8_t origin = gust_origin();
    // The wind frames follow the atlas frames in the tileset's first sheet.
    avm_set_sprite(tileset_sprites(area_tileset(game.area)));
    View view = world_view();
    GustPlan plan;
    uint16_t offset = (uint16_t)(origin * sizeof(GustPlan));
    memcpy_P(&plan, reinterpret_cast<uint8_t const PROGMEM*>(&GUST_PLANS) + offset, sizeof(plan));
    for(uint8_t index = 0; index < plan.count; ++index) {
        GustLine line = plan.lines[index];
        uint8_t row = line.position >> 4, column = line.position & 15;
        uint8_t const* tiles = view.tiles + row * CACHE_WIDTH + column;
        int16_t x = view.left + column * TILE, y = view.top + row * TILE;
        for(uint8_t count = line.count; count; --count) {
            uint8_t tile = *tiles;
            uint8_t ambient = tile_ambient(tile);
            if(ambient == AMBIENT_GRASS || ambient == AMBIENT_FLOWERS)
                avm_draw_overwrite(x, y, (uint16_t)(wind.base + (uint8_t)(tile - wind.first)));
            tiles += CACHE_WIDTH - 1;
            x -= TILE;
            y += TILE;
        }
    }
}

// One slot whose tile is in view, at a column and row of the view packed into one byte: looks up
// the tile's class and draws what the slot shows there. Out of line and with few arguments, so
// that little is on the stack unless a slot's tile is in view.
RPG_NOINLINE static void draw_ambient_effect(uint8_t position, uint16_t seed, uint16_t pick) {
    uint8_t column = position & 15, row = position >> 4;
    // The cache trails the view by its margin, so the view starts inside the window.
    uint8_t tile =
        game.cache
            .tiles[(uint8_t)((uint8_t)(game.camera.y >> TILE_SHIFT) - game.cache.origin_ty + row) *
                       CACHE_WIDTH +
                   (uint8_t)((uint8_t)(game.camera.x >> TILE_SHIFT) - game.cache.origin_tx +
                             column)];
    uint8_t ambient_class = tile_ambient(tile);
    if(ambient_class == AMBIENT_NONE)
        return;
    AmbientSprite sprite =
        ambient_sprite(ambient_class, ambient_slot_of(seed, pick, game.ticks), game.ticks);
    if(sprite.visible)
        avm_draw_plus_mask(column * TILE - (int16_t)(game.camera.x & (TILE - 1)) + sprite.dx,
                           row * TILE - (int16_t)(game.camera.y & (TILE - 1)) + sprite.dy,
                           sprite.frame);
}

// A creature or a small effect on the tile that the slot of each chunk in view picked. The loop
// only finds the picked tile; a chunk whose tile is out of view costs nothing more. Selects the
// ambient sheet.
RPG_NOINLINE static void draw_ambient_tiles() {
    avm_set_sprite(AMBIENT_SPRITES);
    uint8_t view_tx = (uint8_t)(game.camera.x >> TILE_SHIFT);
    uint8_t view_ty = (uint8_t)(game.camera.y >> TILE_SHIFT);
    uint8_t chunk_x = view_tx / CHUNK_WIDTH, chunk_y = view_ty / CHUNK_HEIGHT;
    // The first chunk's first tile, as a column and row of the view: zero or negative.
    uint8_t first_column = (uint8_t)(chunk_x * CHUNK_WIDTH - view_tx);
    uint8_t row = (uint8_t)(chunk_y * CHUNK_HEIGHT - view_ty);
    uint16_t row_seed = ambient_seed(chunk_x, chunk_y);
    // At most two chunks each way: the view is no larger than a chunk plus one tile.
    static_assert(VIEW_SPAN_WIDTH <= CHUNK_WIDTH + 1 && VIEW_SPAN_HEIGHT <= CHUNK_HEIGHT + 1);
    for(; row < VIEW_SPAN_HEIGHT || row >= (uint8_t)-CHUNK_HEIGHT; row += CHUNK_HEIGHT) {
        uint16_t seed = row_seed;
        for(uint8_t column = first_column;
            column < VIEW_SPAN_WIDTH || column >= (uint8_t)-CHUNK_WIDTH; column += CHUNK_WIDTH) {
            uint16_t pick = ambient_pick(seed, game.ticks);
            uint8_t tile_column = (uint8_t)(column + (pick & (CHUNK_WIDTH - 1)));
            uint8_t tile_row = (uint8_t)(row + ((pick >> 3) & (CHUNK_HEIGHT - 1)));
            if(tile_column < VIEW_SPAN_WIDTH && tile_row < VIEW_SPAN_HEIGHT)
                draw_ambient_effect((uint8_t)(tile_row << 4 | tile_column), seed, pick);
            seed += AMBIENT_SEED_X;
        }
        row_seed += AMBIENT_SEED_Y;
    }
}

// Birds fly over everything, actors included. Only one lane can show in the view, and its test
// is cheap, so the ambient sheet is selected only for a bird in view.
RPG_NOINLINE static void draw_birds() {
    AmbientBird bird = ambient_bird(bird_lane(game.camera.y), game.ticks, game.camera.x);
    if(!bird.visible)
        return;
    avm_set_sprite(AMBIENT_SPRITES);
    int16_t x = bird.x, y = (int16_t)(bird.y - game.camera.y);
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
RPG_NOINLINE static void draw_tiles() {
    View view = world_view();
    uint8_t const* tiles = view.tiles;
    avm_set_sprite(tileset_phase_sprites(area_tileset(game.area), tile_phase(game.ticks)));
    for(int16_t y = view.top; y < DISPLAY_TILE_HEIGHT * TILE; y += TILE) {
        // A fixed row span preserves unrolling. Native drawing clips the
        // extra column when the camera is tile-aligned.
        uint8_t const* row_end = tiles + VIEW_SPAN_WIDTH;
        for(int16_t x = view.left; tiles != row_end; x += TILE) {
            uint8_t tile = *tiles++;
            if(tile == TILE_OUTSIDE)
                continue;
            avm_draw_overwrite(x, y, tile);
        }
        tiles += CACHE_WIDTH - VIEW_SPAN_WIDTH;
    }
}

// Actors in ascending foot order, so a lower actor covers a higher one.
RPG_NOINLINE static void draw_actors() {
    Camera c = game.camera;
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
void rpg_render_world() {
    draw_tiles();
    uint8_t tileset = area_tileset(game.area);
    if(tileset_has_ambient(tileset)) {
        // Most frames have no gust in view, and that test needs only the camera's tile.
        if(gust_in_view(gust_origin()))
            draw_gusts();
        draw_ambient_tiles();
    }
    draw_actors();
    if(tileset_has_sky(tileset))
        draw_birds();

    if(game.can_interact)
        avm_draw_text(110, 7, F("A"));
}

// Rebuild the outgoing image: exploration and dialogue normally clear the framebuffer after
// presenting it. This also removes any overlay before the world is faded away.
void fade_world_out() {
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
    draw_fade((action == MENU_AUDIO || action == MENU_PARTY) ? FADE_STEPS / 2
              : action == MENU_LOAD                          ? FADE_STEPS - fade
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

void render_party(uint8_t revealed, bool closing) {
    rpg_render_world();
    draw_fade(closing ? (uint8_t)((uint16_t)revealed * (FADE_STEPS / 2) / PARTY_WIDTH)
                      : FADE_STEPS / 2);
    if(!revealed)
        return;

    int16_t x = DISPLAY_WIDTH - revealed;
    avm_draw_filled_rect_black(x, 0, PARTY_WIDTH, 64);
    avm_draw_filled_rect_white(x + 1, 1, PARTY_WIDTH - 2, 62);
    avm_draw_filled_rect_black(x + 2, 2, PARTY_WIDTH - 4, 60);
    avm_draw_textf(x + 4, 8, F("Party %u/%u"), game.party.count, PARTY_CAPACITY);
    avm_draw_text(x + 89, 8, F("B back"));

    static char const PROGMEM* const PROGMEM NAMES[] = {F("Traveler"), F("Villager"), F("Wanderer"),
                                                        F("Rowan")};
    static char const PROGMEM* const PROGMEM MOVEMENT[] = {F("Fighter"), F("Knight"), F("Wolf")};
    static char const PROGMEM* const PROGMEM ATTACK[] = {F("Fighter"), F("Archer"), F("Spearman"),
                                                         F("Brute"), F("Bite")};
    static_assert(sizeof(NAMES) / sizeof(NAMES[0]) == NPC_ID_LIMIT);
    static_assert(sizeof(MOVEMENT) / sizeof(MOVEMENT[0]) == MOVEMENT_CLASS_COUNT);
    static_assert(sizeof(ATTACK) / sizeof(ATTACK[0]) == ATTACK_CLASS_COUNT);
    for(uint8_t i = 0; i < PARTY_CAPACITY; ++i) {
        uint8_t y = (uint8_t)(18 + i * 12);
        if(i >= game.party.count) {
            avm_draw_text(x + 4, y, F("- Empty -"));
            continue;
        }
        PartyMember const& member = game.party.members[i];
        avm_draw_text(x + 4, y, NAMES[member.id]);
        avm_draw_textf(x + 60, y, F("HP %u/%u"), member.hp, member.max_hp);
        avm_draw_text(x + 4, y + 6, F("M:"));
        avm_draw_text(x + 16, y + 6, MOVEMENT[member.movement_class]);
        avm_draw_text(x + 64, y + 6, F("A:"));
        avm_draw_text(x + 76, y + 6, ATTACK[member.attack_class]);
    }
}
