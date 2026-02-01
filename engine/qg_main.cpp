#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <windows.h>

#include "SDL3/SDL_init.h"
#include "SDL3/SDL_render.h"

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

#define VERSION "ALPHA" //TODO: Export version from the main game DLL
#define TITLE "QG-ChainLinks"
#define WIDTH 800
#define HEIGHT 600

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
    if (!SDL_CreateWindowAndRenderer("[" VERSION "]" TITLE, 800, 600, 0, &window, &context)) {
        printf("Could not create Window or Renderer\nerror: %s\n", SDL_GetError());
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
    // -----------------------------------------

    game_init(g_eng);

    SDL_Event event;
    while (g_running) {

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                g_running = false;
            }
            if (event.type == SDL_EVENT_KEY_UP && event.key.key == SDLK_ESCAPE) {
                g_running = false;
            }
        }

        // Draw current frame
        SDL_SetRenderDrawColor(context, 0, 0, 0, 255);
        SDL_RenderFillRect(context, NULL);

        SDL_RenderPresent(context);
    }

    gamelib_free();
    SDL_DestroyWindow(window);
    SDL_Quit();
    printf("[QG] quitting SDL3!\n");
    return 0;
}
