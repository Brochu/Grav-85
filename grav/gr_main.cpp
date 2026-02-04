#include <cstdio>

#include "SDL3/SDL_timer.h"
#include "qg_bus.hpp"
#include "qg_config.hpp"
#include "qg_math.hpp"
#include "qg_memory.hpp"
#include "qg_parse.hpp"
#include "qg_random.hpp"
#include "shared.hpp"

#include "SDL3/SDL.h"

engine_api g_api {};

enum class color : u8 {
    RED,
    GREEN,
    BLUE,
};
constexpr i32 colors_def[3][4] = {
    { 255, 0  , 0  , 255 },
    { 0  , 255, 0  , 255 },
    { 0  , 0  , 255, 255 },
};

#define ELEMENTS_MAX_NUM 32
#define MAP_MAX_SIZE 256
#define LEVEL_FILE_SIZE 108

struct level {
    u8 solid[MAP_MAX_SIZE / 8];       // 1 bit per cell
    ivec2 crate_starts[ELEMENTS_MAX_NUM];
    ivec2 gem_starts[ELEMENTS_MAX_NUM];
    color gem_colors[ELEMENTS_MAX_NUM];
    direction start_gravity;
    i8 width;
    i8 height;
    i8 num_crates;
    i8 num_gems;
};

inline ivec2 unpack_pos(u8 packed) {
    return { packed >> 4, packed & 0xF };
}

inline bool level_is_solid(level *lvl, ivec2 pos) {
    u32 idx = (pos.y * lvl->width) + pos.x;
    return (lvl->solid[idx / 8] >> (idx % 8)) & 1;
}

void level_file_init(level *lvl, const char *file_path) {
    FILE *lvl_file;
    i32 err = fopen_s(&lvl_file, file_path, "rb");
    assert(err == 0);

    u8 lvl_data[LEVEL_FILE_SIZE];
    u64 read_len = fread_s(lvl_data, LEVEL_FILE_SIZE, LEVEL_FILE_SIZE, 1, lvl_file);
    assert(read_len >= 1);
    fclose(lvl_file);

    u8 dims = (u8)lvl_data[0];
    lvl->width = (dims >> 4) & 0xf;
    lvl->height = dims & 0xf;

    lvl->start_gravity = (direction)lvl_data[1];
    lvl->num_crates = (i8)lvl_data[2];
    lvl->num_gems = (i8)lvl_data[3];

    u8 *crate_data = (u8*)&lvl_data[12];
    for (int i = 0; i < lvl->num_crates; i++) {
        lvl->crate_starts[i] = unpack_pos(crate_data[i]);
    }

    u64 colors_data = *(u64*)&lvl_data[4];
    u8 *gem_data = (u8*)&lvl_data[44];
    for (int i = 0; i < lvl->num_gems; i++) {
        lvl->gem_colors[i] = (color)((colors_data >> (2 * i)) & 0b11);
        lvl->gem_starts[i] = unpack_pos(gem_data[i]);
    }

    u8 *solid_data = (u8*)&lvl_data[76];
    memcpy_s(lvl->solid, MAP_MAX_SIZE / 8, solid_data, MAP_MAX_SIZE / 8);
}
void level_data_init(level *lvl, arena_ptr data) { }

#define RUN_MAX_MOVES 99

struct run {
    u64 start_timestamp;
    ivec2 crates[ELEMENTS_MAX_NUM];
    ivec2 gems[ELEMENTS_MAX_NUM];
    direction moves[RUN_MAX_MOVES];
    direction current_gravity;
    i8 num_crates;
    i8 num_gems;
    i8 num_moves;
};

void run_level_init(run *run, level *lvl) {
    run->start_timestamp = SDL_GetTicksNS();

    memcpy_s(run->crates, sizeof(run->crates), lvl->crate_starts, sizeof(lvl->crate_starts));
    memcpy_s(run->gems, sizeof(run->gems), lvl->gem_starts, sizeof(lvl->gem_starts));

    run->current_gravity = lvl->start_gravity;
    run->num_crates = lvl->num_crates;
    run->num_gems = lvl->num_gems;
    run->num_moves = 0;
}

level g_lvl;
run g_run;

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
    run_level_init(&g_run, &g_lvl);
}

void grav_tick(f32 dt) {
}

void grav_draw(f32 dt) {
    SDL_SetRenderDrawColor(g_api.context, 127, 127, 127, 255);
    f32 cell_size = 32;

    for (i32 y = 0; y < g_lvl.height; y++) {
        for (i32 x = 0; x < g_lvl.width; x++) {
            ivec2 p { x, y };
            if (level_is_solid(&g_lvl, p)) {
                SDL_FRect r { x*cell_size, y*cell_size, cell_size-5, cell_size-5 };
                SDL_RenderFillRect(g_api.context, &r);
            }
        }
    }

    SDL_SetRenderDrawColor(g_api.context, 255, 255, 255, 255);
    for (i32 i = 0; i < g_run.num_crates; i++) {
        auto [x, y] = g_run.crates[i];
        SDL_FRect r { x*cell_size, y*cell_size, cell_size-5, cell_size-5 };
        SDL_RenderFillRect(g_api.context, &r);
    }

    for (i32 i = 0; i < g_run.num_gems; i++) {
        const i32 *col = colors_def[(i32)g_lvl.gem_colors[i]];
        SDL_SetRenderDrawColor(g_api.context, col[0], col[1], col[2], col[3]);

        auto [x, y] = g_run.gems[i];
        SDL_FRect r { x*cell_size, y*cell_size, cell_size-5, cell_size-5 };
        SDL_RenderFillRect(g_api.context, &r);
    }
    //TODO: Need to rework rendering to a lower level so I could take advantage of instanced rendering here
    //TODO: Could also look into baking the background once and reusing it with one copy operation per frame
}

void grav_exit() {
}
