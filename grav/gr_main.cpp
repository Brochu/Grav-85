#include <cstdio>

#include "qg_bus.hpp"
#include "qg_config.hpp"
#include "qg_memory.hpp"
#include "qg_parse.hpp"
#include "qg_random.hpp"
#include "shared.hpp"

#include "SDL3/SDL.h"

engine_api g_api {};

enum class space : i8 {
    EMPTY,
    SOLID,
    BLOCK,
    STONE,
};

enum class color : i8 {
    RED,
    GREEN,
    BLUE,
};

enum class direction : i8 {
    UP,
    RIGHT,
    DOWN,
    LEFT,
};

#define STONES_MAX_NUM 32
#define MAP_MAX_SIZE 32*32

struct level {
    i8 width;
    i8 height;
    direction start_gravity;
    color stone_colors[STONES_MAX_NUM];
    space map[MAP_MAX_SIZE];
};

void level_file_init(level *lvl, const char *file_path) { }
void level_data_init(level *lvl, arena_ptr data) { }

struct run {
    direction current_gravity;
    //TODO: How do we represent the current map status - based off of map from level
};

void run_level_init(run *run, level *lvl) { }

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
}

void grav_tick(f32 dt) {
}

void grav_draw(f32 dt) {
    SDL_SetRenderDrawColor(g_api.context, 0, 127, 0, 255);

    SDL_FRect r { 0, 0, 100, 100 };
    SDL_RenderFillRect(g_api.context, &r);
}

void grav_exit() {
}
