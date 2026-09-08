#pragma once
#include "qg_shared_types.hpp"

#include <cstring>

struct engine_api; // This will exist

// EVENTS ======================================
// Define your event types here
enum class event_type : u8 {
    NONE = 0,

    // ENGINE EVENTS (0-127)
    // These are reserved for engine-level events
    PERF_FRAME_STAT,
    PERF_MEMORY_STAT,
    PERF_BUDGET_EXCEEDED,

    RENDER_RESOLUTION_CHANGED,
    RENDER_BACKEND_LOST,
    RENDER_BACKEND_RESTORED,

    ASSET_LOADED,
    ASSET_UNLOADED,

    AUDIO_REQUEST_PLAY,

    // Add more engine event types as needed (up to 127)
    GAME_EVENTS_START = 128,

    // GAME EVENTS (128-255)
    // Games can define their own event types starting from here
    // Example in game code:
    //   enum class game_event : u16 {
    //       QUEST_COMPLETED = (u16)event_type::GAME_EVENTS_START,
    //       DIALOGUE_STARTED,
    //       MERCHANT_OPENED,
    //       // ... more game events
    //   };

    COUNT = 255  // Total capacity for all event types
};

// Handler ID that encodes generation, slot index, and event type
struct handler_id {
    union {
        u64 packed;
        struct {
            u32 generation;  // Must match slot's generation to be valid
            u16 slot_idx;    // Which slot in the handlers array
            u16 type_idx;    // Event type index
        };
    };
};

typedef void (*event_handler_fn)(event_type type, void* data, void* user_data);
struct event_bus;
#define BUS_MODULE_DEF \
    X(void, bus_init, (event_bus*, u64)) \
    X(void, bus_free, (event_bus*)) \
    X(handler_id, bus_subscribe, (event_bus*, event_type, event_handler_fn, void*)) \
    X(bool, bus_unsubscribe, (event_bus*, handler_id)) \
    X(bool, bus_fire, (event_bus*, event_type, const void*, u32)) \
    X(void, bus_process, (event_bus*)) \
    X(void, bus_reset, (event_bus*))

// CONFIG ======================================
enum class value_type : u8 { INTEGER, FLOAT, RANGE, ARRAY, STRING };

struct config_value {
    value_type type;

    union {
        i32 integer;
        f32 flt;

        struct { i32 min; i32 max; } range;
        struct { i32 *arr; u64 len; } array;
        struct { const char *arr; u64 len; } str;
    };
};
struct config;
#define CONFIG_MODULE_DEF \
    X(void, config_init, (config*, const char*)) \
    X(void, config_free, (config*)) \
    X(bool, config_read, (config*, const char*, config_value*))

// INPUT  ======================================
enum class key_code : u16 {
    W, A, S, D, R,
    UP, DOWN, LEFT, RIGHT,
    RETURN, SPACE, ESCAPE,
    PAGE_UP, PAGE_DOWN,
    COUNT
};
struct input_state;
#define INPUT_MODULE_DEF \
    X(void, input_bind_key, (input_state*, key_code, u8)) \
    X(bool, input_down, (input_state*, u8)) \
    X(bool, input_pressed, (input_state*, u8)) \
    X(bool, input_released, (input_state*, u8))

// MEM    ======================================
struct arena_ptr {
    u8 *p;
    u64 gen;
};
struct arena_off {
    u64 off;
    u64 gen;
};
struct mem_arena;

template<class T>
static inline T *mem_arena_at(engine_api *api, mem_arena *arena, arena_off offset) {
    return reinterpret_cast<T *>(api->mem_arena_at(arena, offset));
}
#define MEMORY_MODULE_DEF \
    X(void*, qg_malloc, (u64)) \
    X(void*, qg_calloc, (u64, u64)) \
    X(void*, qg_realloc, (void*, u64)) \
    X(void, qg_free, (void*)) \
    X(void, mem_arena_init, (mem_arena*, u64)) \
    X(void, mem_arena_reset, (mem_arena*)) \
    X(void, mem_arena_clear, (mem_arena*)) \
    X(arena_ptr, mem_arena_alloc, (mem_arena*, u64, u64)) \
    X(arena_off, mem_arena_offloc, (mem_arena*, u64, u64)) \
    X(void *, mem_arena_at, (mem_arena*, arena_off))

// PARSE  ======================================
struct strview {
    const char* ptr;
    size_t len;
};

#define SV_FMT "%.*s"
#define SV_ARG(sv) (int)sv.len, sv.ptr

static inline strview sv(const char *s) {
    return { s, strlen(s) };
}
#define PARSE_MODULE_DEF \
    X(strview, sv_find, (strview, const char*)) \
    X(u64, sv_split, (strview, const char*, strview*, u64)) \
    X(bool, sv_split_once, (strview, const char*, strview*, strview*)) \

// RANDOM ======================================
#define RANDOM_MODULE_DEF \
    X(void, rand_seed, (i64)) \
    X(f32, rand_float01, (void)) \
    X(i32, rand_int, (i32)) \
    X(i32, rand_int_min, (i32, i32))

//TODO: Look into having a separate renderer based off of SDL3, could also make it hot-reloadable?
struct SDL_Renderer;

// ENGINE ======================================
#define QG_ENGINE_VERSION 1

struct engine_api {
    u32 version = QG_ENGINE_VERSION;
    #define X(ret, name, params) ret (*name) params;

    struct { BUS_MODULE_DEF };
    struct { CONFIG_MODULE_DEF };
    struct { INPUT_MODULE_DEF };
    struct { MEMORY_MODULE_DEF };
    struct { PARSE_MODULE_DEF };
    struct { RANDOM_MODULE_DEF };

    #undef X
};

struct engine_state {
    event_bus *bus;
    input_state *input;
    mem_arena *core_mem;

    // RENDERER --------------
    SDL_Renderer *context;
};

// GAME   ======================================
struct game_api {
    u64 (*game_state_size) (void);
    void (*game_init) (engine_state *);
    void (*game_tick) (float);
    void (*game_draw) (float);
    void (*game_exit) (void);
};

#define GRAV_API __declspec(dllexport)
extern "C" game_api GRAV_API grav_get_api(engine_api *eng);

#define GAME_MODULE_DEF \
    X(game_api, game_get_api, "grav_get_api", (engine_api *))
