#include "world_map.h"

#include <stdio.h>
#include <stdlib.h>

#define CHECK(value)                                                                               \
    do {                                                                                           \
        if(!(value)) {                                                                             \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #value);                               \
            exit(1);                                                                               \
        }                                                                                          \
    } while(0)

int main() {
    WorldMapView view{{100 * MAP_SUBPIXELS}, {100 * MAP_SUBPIXELS}, 36, 68};
    update_world_map(view, BTN_RIGHT, 240, 248);
    CHECK(map_pixel(view.x) == 101);
    int16_t previous = view.x.position;
    update_world_map(view, BTN_RIGHT, 240, 248);
    CHECK(view.x.position - previous > 0 && view.x.position - previous < MAP_SUBPIXELS);
    for(int i = 0; i < 6; ++i)
        update_world_map(view, BTN_RIGHT, 240, 248);
    previous = view.x.position;
    update_world_map(view, BTN_RIGHT, 240, 248);
    CHECK(view.x.position - previous < MAP_SUBPIXELS);
    for(int i = 0; i < 20; ++i)
        update_world_map(view, BTN_RIGHT, 240, 248);
    previous = view.x.position;
    update_world_map(view, BTN_RIGHT, 240, 248);
    CHECK(view.x.position - previous == MAP_SUBPIXELS);
    int16_t stopped = map_pixel(view.x);
    update_world_map(view, 0, 240, 248);
    for(int i = 0; i < 10; ++i)
        update_world_map(view, 0, 240, 248);
    CHECK(map_pixel(view.x) == stopped);
    update_world_map(view, BTN_LEFT, 240, 248);
    CHECK(map_pixel(view.x) == stopped - 1);
    previous = view.x.position;
    update_world_map(view, BTN_LEFT | BTN_RIGHT, 240, 248);
    CHECK(map_pixel(view.x) == (previous + 8) / 16);

    WorldMapView diagonal{{100 * MAP_SUBPIXELS}, {100 * MAP_SUBPIXELS}, 36, 68};
    for(int i = 0; i < 20; ++i)
        update_world_map(diagonal, BTN_RIGHT | BTN_DOWN, 240, 248);
    int16_t diagonal_x = diagonal.x.position, diagonal_y = diagonal.y.position;
    update_world_map(diagonal, BTN_RIGHT | BTN_DOWN, 240, 248);
    CHECK(diagonal.x.position - diagonal_x == MAP_SUBPIXELS);
    CHECK(diagonal.y.position - diagonal_y == MAP_SUBPIXELS);
    // Releasing one axis must not change the other axis's speed.
    diagonal_x = diagonal.x.position;
    update_world_map(diagonal, BTN_RIGHT, 240, 248);
    CHECK(diagonal.x.position - diagonal_x == MAP_SUBPIXELS);

    for(int i = 0; i < 1000; ++i) {
        int16_t x = view.x.position, y = view.y.position;
        update_world_map(view, BTN_RIGHT | BTN_DOWN, 240, 248);
        int16_t dx = view.x.position - x, dy = view.y.position - y;
        CHECK(dx >= 0 && dx <= MAP_SUBPIXELS);
        CHECK(dy >= 0 && dy <= MAP_SUBPIXELS);
    }
    CHECK(map_pixel(view.x) == 239 && map_pixel(view.y) == 247);
    CHECK(view.camera_x == 112 && view.camera_y == 184);
    for(int i = 0; i < 1000; ++i)
        update_world_map(view, BTN_LEFT | BTN_UP, 240, 248);
    CHECK(map_pixel(view.x) == 0 && map_pixel(view.y) == 0);
    CHECK(view.camera_x == 0 && view.camera_y == 0);
    CHECK(map_follow(0, 115, 240, 128) == 0);
    CHECK(map_follow(0, 116, 240, 128) == 1);
    CHECK(map_follow(10, 21, 240, 128) == 9);
    for(int i = 0; i < 200; ++i)
        update_world_map(view, BTN_RIGHT | BTN_DOWN, 16, 8);
    CHECK(map_pixel(view.x) == 15 && map_pixel(view.y) == 7);
    CHECK(view.camera_x == 0 && view.camera_y == 0);
    puts("World map movement, acceleration, diagonal speed and bounds passed");
}
