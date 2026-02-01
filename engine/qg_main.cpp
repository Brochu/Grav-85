#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <windows.h>

#include "SDL3/SDL.h"

#include "qg_bus.hpp"
#include "qg_config.hpp"
#include "qg_memory.hpp"
#include "qg_parse.hpp"
#include "qg_random.hpp"
#include "shared.hpp"

// ------------- GAMELIB LOADING

#define GAMELIB_BASE_PATH "./gr.dll"
#define GAMELIB_LOAD_PATH "./gr_loaded.dll"

#define X(ret, name, sym, args) typedef ret (*name##_t) args;
GAME_MODULE_DEF
#undef X

#define X(ret, name, sym, args) name##_t name = NULL;
GAME_MODULE_DEF
#undef X

HMODULE game_module = NULL;
engine_api g_eng {};
event_bus g_bus {};

void gamelib_load() {
    assert(game_module == NULL && "Did not properly free the gamelib module");

    bool should_copy = false;

    if (GetFileAttributes(GAMELIB_LOAD_PATH) == INVALID_FILE_ATTRIBUTES) {
        should_copy = true;
    } else {
        // Check if base file is more recent than load file
        WIN32_FILE_ATTRIBUTE_DATA base_attrib, load_attrib;
        if (GetFileAttributesEx(GAMELIB_BASE_PATH, GetFileExInfoStandard, &base_attrib) &&
            GetFileAttributesEx(GAMELIB_LOAD_PATH, GetFileExInfoStandard, &load_attrib)) {

            if (CompareFileTime(&base_attrib.ftLastWriteTime, &load_attrib.ftLastWriteTime) > 0) {
                should_copy = true;
            }
        }
    }

    if (should_copy) {
        CopyFile(GAMELIB_BASE_PATH, GAMELIB_LOAD_PATH, false);
    }

    game_module = LoadLibrary(GAMELIB_LOAD_PATH);
    assert(game_module != NULL && "Failed to load game library");

    #define X(ret, name, sym, args) \
        name = (name##_t)GetProcAddress(game_module, sym); \
        assert(name != NULL && "Failed to load function " sym);
    GAME_MODULE_DEF
    #undef X
}

void gamelib_free() {
    if (game_module != NULL) {
        FreeLibrary(game_module);
        game_module = NULL;
    }
}

void gamelib_refresh() {
    gamelib_free();
    CopyFile(GAMELIB_BASE_PATH, GAMELIB_LOAD_PATH, false);
    gamelib_load();
}

// ------------- CONFIG HANDLING

config *root_cfg;

// ------------- ENGINE - MAIN

#define WINDOW_VERSION "ALPHA" //TODO: Export version from the main game DLL
#define WINDOW_TITLE "Grav - 85"
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define WINDOW_FPS 60

static const u64 NS_PER_FRAME = (1000 * 1000 * 1000 / WINDOW_FPS);
static const u64 MAX_LAG_TIME = NS_PER_FRAME * 5; // Cap catchup to 5 ticks
static const f32 FRAME_TIME = SDL_NS_TO_SECONDS((f32)NS_PER_FRAME);
bool g_running = true;

int main(int argc, char **argv) {
    rand_seed(time(NULL));

    if (!SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO)) {
        printf("Could not init SDL3\nerror: %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }
    printf("[QG] SDL3 Correctly init'ed!\n");

    SDL_Window *window;
    SDL_Renderer *context;
    if (!SDL_CreateWindowAndRenderer("[" WINDOW_VERSION "] " WINDOW_TITLE, WINDOW_WIDTH, WINDOW_HEIGHT, 0, &window, &context)) {
        printf("Could not create Window or Renderer\nerror: %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }
    if (!SDL_SetRenderVSync(context, 1)) {
        printf("Could not set vsync on Renderer\nerror: %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }
    gamelib_load();

    // ENGINE INIT SEQUENCE
    #define X(ret, name, params) g_eng.name = &name;
    BUS_MODULE_DEF
    CONFIG_MODULE_DEF
    MEMORY_MODULE_DEF
    PARSE_MODULE_DEF
    RANDOM_MODULE_DEF
    #undef X

    bus_init(&g_bus, 2 * 1024 * 1024);
    g_eng.bus = &g_bus;

    g_eng.context = context;
    // -----------------------------------------

    game_init(g_eng);

    u64 last_time = SDL_GetTicksNS();
    u64 lag_time = 0;
    SDL_Event event;
    while (g_running) {

        u64 this_time = SDL_GetTicksNS();
        u64 elapsed = this_time - last_time;
        last_time = this_time;
        lag_time += elapsed;

        // Prevent spiral of death: if we fall too far behind (debugger pause,
        // window drag, etc.), cap the lag so we don't run hundreds of ticks
        if (lag_time > MAX_LAG_TIME) lag_time = MAX_LAG_TIME;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                g_running = false;
            }
            if (event.type == SDL_EVENT_KEY_UP && event.key.key == SDLK_ESCAPE) {
                g_running = false;
            }
        }

        while (lag_time >= NS_PER_FRAME) {
            game_tick(FRAME_TIME);
            bus_process(g_eng.bus);

            lag_time -= NS_PER_FRAME;
        }

        // Draw current frame
        SDL_SetRenderDrawColor(context, 0, 0, 0, 255);
        SDL_RenderFillRect(context, NULL);

        game_draw(SDL_NS_TO_SECONDS((f32)elapsed));

        SDL_RenderPresent(context);

    }
    game_exit();
    gamelib_free();

    SDL_DestroyWindow(window);
    SDL_Quit();
    printf("[QG] quitting SDL3!\n");
    return 0;
}
