#ifndef WORLD_MAP_H
#define WORLD_MAP_H

#include "rpg.h"

inline constexpr int16_t MAP_SUBPIXELS = 16;

struct MapAxis {
    int16_t position;
    int8_t direction = 0;
    uint8_t velocity = 0;
};

struct WorldMapView {
    MapAxis x, y;
    int16_t camera_x, camera_y;
};

constexpr int16_t map_pixel(MapAxis const& axis) {
    return (axis.position + MAP_SUBPIXELS / 2) / MAP_SUBPIXELS;
}

constexpr int16_t map_clamp(int16_t value, int16_t maximum) {
    return value < 0 ? 0 : value > maximum ? maximum : value;
}

inline void map_move_axis(MapAxis& axis, int8_t direction, uint8_t maximum, uint8_t tiles) {
    if(!direction) {
        axis.position = map_pixel(axis) * MAP_SUBPIXELS;
        axis.velocity = 0;
    } else {
        // Immediate precise tap; a held direction then accelerates over sixteen frames.
        bool fresh = direction != axis.direction;
        axis.velocity = fresh ? 0 : axis.velocity + 1 > maximum ? maximum : axis.velocity + 1;
        axis.position = map_clamp(axis.position + direction * (fresh ? maximum : axis.velocity),
                                  (tiles - 1) * MAP_SUBPIXELS);
    }
    axis.direction = direction;
}

constexpr int16_t map_follow(int16_t camera, int16_t cursor, uint8_t tiles, uint8_t screen) {
    constexpr int16_t MARGIN = 12;
    if(cursor < camera + MARGIN)
        camera = cursor - MARGIN;
    else if(cursor > camera + screen - 1 - MARGIN)
        camera = cursor - screen + 1 + MARGIN;
    return map_clamp(camera, tiles > screen ? tiles - screen : 0);
}

inline void update_world_map(WorldMapView& view, uint8_t held, uint8_t width, uint8_t height) {
    int8_t dx = !!(held & BTN_RIGHT) - !!(held & BTN_LEFT);
    int8_t dy = !!(held & BTN_DOWN) - !!(held & BTN_UP);
    // Each axis accelerates independently to one pixel/frame.
    map_move_axis(view.x, dx, MAP_SUBPIXELS, width);
    map_move_axis(view.y, dy, MAP_SUBPIXELS, height);
    view.camera_x = map_follow(view.camera_x, map_pixel(view.x), width, 128);
    view.camera_y = map_follow(view.camera_y, map_pixel(view.y), height, 64);
}

#endif
