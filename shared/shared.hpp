#pragma once
#include "shared_types.hpp"

// ENGINE ======================================

enum class event_type : u16;
struct event_bus;
struct handler_id;
typedef void (*event_handler_fn)(event_type, void*, void*);
#define BUS_MODULE_DEF \
    X(void, bus_init, (event_bus*, u64)) \
    X(void, bus_free, (event_bus*)) \
    X(handler_id, bus_subscribe, (event_bus*, event_type, event_handler_fn, void*)) \
    X(bool, bus_unsubscribe, (event_bus*, handler_id)) \
    X(bool, bus_fire, (event_bus*, event_type, const void*, u32)) \
    X(void, bus_process, (event_bus*)) \
    X(void, bus_reset, (event_bus*))

enum class value_type : u8;
struct config_value;
struct config;
#define CONFIG_MODULE_DEF \
    X(void, config_init, (config*, const char*)) \
    X(void, config_free, (config*)) \
    X(bool, config_read, (config*, const char*, config_value*))

enum class input_action : u8;
struct input_state;
#define INPUT_MODULE_DEF \
    X(bool, input_down, (input_state*, input_action)) \
    X(bool, input_pressed, (input_state*, input_action)) \
    X(bool, input_released, (input_state*, input_action))

struct mem_arena;
struct arena_ptr;
struct arena_off;
#define MEMORY_MODULE_DEF \
    X(void*, qg_malloc, (u64)) \
    X(void*, qg_calloc, (u64, u64)) \
    X(void*, qg_realloc, (void*, u64)) \
    X(void, qg_free, (void*)) \
    X(void, mem_arena_init, (mem_arena*, u64)) \
    X(void, mem_arena_reset, (mem_arena*)) \
    X(void, mem_arena_clear, (mem_arena*)) \
    X(arena_ptr, mem_arena_alloc, (mem_arena*, u64, u64)) \
    X(arena_off, mem_arena_offloc, (mem_arena*, u64, u64))

struct strview;
#define PARSE_MODULE_DEF \
    X(strview, sv_find, (strview, const char*)) \
    X(u64, sv_split, (strview, const char*, strview*, u64)) \
    X(bool, sv_split_once, (strview, const char*, strview*, strview*)) \

#define RANDOM_MODULE_DEF \
    X(void, rand_seed, (i64)) \
    X(f32, rand_float01, (void)) \
    X(i32, rand_int, (i32)) \
    X(i32, rand_int_min, (i32, i32))  \
    X(i8, rand_actor_age, (void))

//TODO: Look into having a separate renderer based off of SDL3, could also make it hot-reloadable?
struct SDL_Renderer;

struct engine_api {
    #define X(ret, name, params) ret (*name) params;

    struct { BUS_MODULE_DEF };
    struct { CONFIG_MODULE_DEF };
    struct { INPUT_MODULE_DEF };
    struct { MEMORY_MODULE_DEF };
    struct { PARSE_MODULE_DEF };
    struct { RANDOM_MODULE_DEF };

    #undef X

    event_bus *bus;
    input_state *input;

    // RENDERER --------------
    SDL_Renderer *context;
};

extern engine_api g_eng;

// GAME   ======================================

#define GRAV_API __declspec(dllexport)

extern "C" void GRAV_API grav_init(engine_api engine);
extern "C" void GRAV_API grav_tick(f32 dt);
extern "C" void GRAV_API grav_draw(f32 dt);
extern "C" void GRAV_API grav_exit();

#define GAME_MODULE_DEF \
    X(void, game_init, "grav_init", (engine_api)) \
    X(void, game_tick, "grav_tick", (float)) \
    X(void, game_draw, "grav_draw", (float)) \
    X(void, game_exit, "grav_exit", (void))
