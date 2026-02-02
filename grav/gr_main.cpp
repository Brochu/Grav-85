#include <cstdio>

#include "qg_bus.hpp"
#include "qg_config.hpp"
#include "qg_math.hpp"
#include "qg_memory.hpp"
#include "qg_parse.hpp"
#include "qg_random.hpp"
#include "shared.hpp"

#include "SDL3/SDL.h"

engine_api g_api {};

enum class space : u8 {
    SOLID,
    EMPTY,
    CRATE,
    GEM,
};

enum class color : u8 {
    RED,
    GREEN,
    BLUE,
};

#define GEMS_MAX_NUM 32
#define MAP_MAX_SIZE 16*16
#define LEVEL_FILE_SIZE 76

struct level {
    i8 width;
    i8 height;
    direction start_gravity;

    i8 num_colors;
    color gem_colors[GEMS_MAX_NUM];

    space map[MAP_MAX_SIZE];
};

void level_file_init(level *lvl, const char *file_path) {
    FILE *lvl_file;
    i32 err = fopen_s(&lvl_file, file_path, "rb");
    assert(err == 0);

    u8 lvl_data[LEVEL_FILE_SIZE];
    u64 read_len = fread_s(lvl_data, LEVEL_FILE_SIZE, LEVEL_FILE_SIZE, 1, lvl_file);
    assert(read_len >= 1);

    lvl->width = (i8)lvl_data[0];
    lvl->height = (i8)lvl_data[1];
    lvl->start_gravity = (direction)lvl_data[2];

    lvl->num_colors = (i8)lvl_data[3];
    u64 colors_data = *(u64*)&lvl_data[4];
    for (int i = 0; i < lvl->num_colors; i++) {
        lvl->gem_colors[i] = (color)((colors_data >> (2 * i)) & 0b11);
    }

    u64 spaces_data[8];
    for (int i = 0; i < 8; i++) {
        spaces_data[i] = *(u64*)&lvl_data[12 + i * 8];
    }

    int total_spaces = lvl->width * lvl->height;
    for (int i = 0; i < total_spaces; i++) {
        int chunk = i / 32;
        int bit_pos = (i % 32) * 2;
        lvl->map[i] = (space)((spaces_data[chunk] >> bit_pos) & 0b11);
    }
}
void level_data_init(level *lvl, arena_ptr data) { }

space level_space_at(level *lvl, ivec2 pos) {
    u32 idx = (pos.y * lvl->width) + pos.x;
    return lvl->map[idx];
}

struct run {
    direction current_gravity;

    ivec2 crates[GEMS_MAX_NUM];
    ivec2 gems[GEMS_MAX_NUM];
};

void run_level_init(run *run, level *lvl) {
    run->current_gravity = lvl->start_gravity;
    //TODO: create positions based off of the map definition
}

level g_lvl;

void grav_init(engine_api api) {
    g_api = api;

    mem_arena a;
    g_api.mem_arena_init(&a, 1024);

    arena_off offset = g_api.mem_arena_offloc(&a, 8, 8);
    i64 *value = mem_arena_at<i64>(&a, offset);
    *value = ~0;

    g_api.mem_arena_clear(&a);

    for (int i = 0; i < 10; i++) {
        f32 val = g_api.rand_float01();
        printf("%f\n", val);
    }

    config cfg;
    g_api.config_init(&cfg, "assets/game.cfg");
    printf("CONFIG VALUE = %d\n", cfg.value);
    config_value val;
    if (g_api.config_read(&cfg, "gravity_speed", &val)) {
        printf(" Gravity Speed -> '%d'\n", val.single);
    }
    if (g_api.config_read(&cfg, "levels_per_round", &val)) {
        printf(" Levels Per Round -> '%d'\n", val.single);
    }
    g_api.config_free(&cfg);

    u8 ws[2] = { 25, 12 };
    for (int i = 0; i < 10; i++) {
        i32 idx = rand_weighted_index(g_api.rand_float01(), ws, 2);
        printf("RANDOM INDEX = %d\n", idx);
    }

    strview text = sv("0,1,2,3,4,5,6,7,8,9");
    strview out[16];
    u64 n = g_api.sv_split(text, ",", out, 16);
    for (int i = 0; i < n; i++) {
        printf("ELEM -> '" SV_FMT "'\n", SV_ARG(out[i]));
    }

    level_loaded_event evt {};
    evt.level_id = 0;
    evt.level_name = "LEVEL0";

    g_api.bus_fire(g_api.bus, event_type::LEVEL_LOADED, &evt, sizeof(evt));

    level_file_init(&g_lvl, "assets/demo_level.bin");
}

void grav_tick(f32 dt) {
}

void grav_draw(f32 dt) {
    SDL_SetRenderDrawColor(g_api.context, 127, 127, 127, 255);
    f32 cell_size = 32;

    for (i32 y = 0; y < g_lvl.height; y++) {
        for (i32 x = 0; x < g_lvl.width; x++) {
            ivec2 p { x, y };
            if (level_space_at(&g_lvl, p) == space::SOLID) {
                SDL_FRect r { x*cell_size, y*cell_size, cell_size-5, cell_size-5 };
                SDL_RenderFillRect(g_api.context, &r);
            }
        }
    }
}

void grav_exit() {
}
