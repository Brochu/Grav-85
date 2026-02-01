#include <cstdio>

#include "qg_bus.hpp"
#include "qg_memory.hpp"
#include "qg_parse.hpp"
#include "qg_random.hpp"
#include "shared.hpp"

#include "SDL3/SDL.h"

engine_api g_api {};

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

    /*
    config cfg;
    g_api.config_init(&cfg, "../assets/city_gen.ini");
    printf("CONFIG VALUE = %d\n", cfg.value);
    config_value val;
    if (g_api.config_read(&cfg, "ComposedWordLengthMax", &val)) {
        printf(" ComposedWordLengthMax -> '%d'\n", val.single);
    }
    if (g_api.config_read(&cfg, "CrimeRandomRange", &val)) {
        printf(" CrimeRandomRange -> ['%d', '%d']\n", val.range.min, val.range.max);
    }
    if (g_api.config_read(&cfg, "AssetsHomeDir", &val)) {
        printf(" AssetsHomeDir -> '%s'\n", val.str.arr);
    }
    */

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
