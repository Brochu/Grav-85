# Code Organization Notes for Puzzle Game Engine

## Current Structure (QG-ChainLinks)

Traditional header/implementation split:

```
project/
├── include/           # All .hpp files
│   ├── qg_bus.hpp
│   ├── qg_config.hpp
│   ├── qg_memory.hpp
│   └── ...
├── src/               # All .cpp files
│   ├── qg_bus.cpp
│   ├── qg_conf.cpp
│   ├── qg_mem.cpp
│   └── ...
├── libs/              # 3rd party
├── bin/               # Build output
├── docs/
├── tools/
└── assets/
```

---

## Proposed Structure (Logical Split + Hot-Reload)

```
project/
├── libs/              # 3rd party libs (SDL, etc.)
├── bin/               # Built exe, libs, dlls
├── docs/              # Documentation, notes, brainstorm
├── tools/             # Dev utilities, tests
├── engine/            # Engine .hpp + .cpp together → compiles to .exe
├── game/              # Game .hpp + .cpp together → compiles to .dll (hot-reload)
├── shared/            # Types/interfaces used by BOTH engine and game
└── build scripts      # At root for easy access
```

---

## Why This Structure is Better

### 1. Locality of Reference

Working on a module means having both files nearby:

```
# Old: jumping between directories
include/qg_bus.hpp
src/qg_bus.cpp

# New: everything together
engine/qg_bus.hpp
engine/qg_bus.cpp
```

Less context switching. File tree matches mental model.

### 2. Hot-Reload Workflow

Game DLL recompiles constantly during development:
- **Faster incremental builds** (only recompile game DLL)
- **Clear boundary** of what can change at runtime
- **Engine stays stable** while iterating on gameplay

```cpp
// engine/main.cpp
GameDLL game = load_dll("game.dll");
while (running) {
    if (file_modified("game.dll")) {
        game.unload();
        copy_file("game.dll", "game_hot.dll");  // Avoid file lock
        game = load_dll("game_hot.dll");
    }
    game.update(&engine_state);
    game.render(&engine_state);
}
```

### 3. Clear Dependency Direction

```
engine/  ◄── game/
   │
   ▼
 libs/
```

Game depends on engine, engine depends on libs. Never reverse. Harder to accidentally violate with separate directories.

### 4. Simpler Build Scripts

```batch
@echo off
:: build_engine.bat
cl /c engine/*.cpp /I libs /Fe:bin/engine.exe

:: build_game.bat (run constantly during dev)
cl /LD game/*.cpp /I engine /I libs /Fe:bin/game.dll
```

`build_game.bat` is fast because it only touches game code.

---

## Engine Sub-Modules (If It Grows)

```
engine/
├── core/
│   ├── memory.hpp
│   ├── memory.cpp
│   └── types.hpp
├── bus/
│   ├── bus.hpp
│   └── bus.cpp
├── render/
│   ├── render.hpp
│   └── render.cpp
└── engine.hpp          # Main include for games
```

Games just `#include "engine/engine.hpp"` for curated public API.

---

## Unity Builds for Release

With co-located files, unity builds are easy:

```cpp
// engine/unity_build.cpp
#include "memory.cpp"
#include "bus.cpp"
#include "config.cpp"
#include "render.cpp"
// ... everything in one translation unit
```

Benefits:
- Faster full rebuilds
- Better optimization (compiler sees everything)
- Single object file

---

## Hot-Reload Interface (ABI Boundary)

Stable interface between engine and game:

```cpp
// shared/game_interface.hpp
struct EngineAPI {
    void* (*alloc)(size_t size);
    void (*free)(void* ptr);
    void (*log)(const char* msg);
    float (*get_delta_time)();
    // ... engine services exposed to game
};

struct GameAPI {
    void (*init)(EngineAPI* engine);
    void (*update)(float dt);
    void (*render)();
    void (*shutdown)();
    void (*on_hot_reload)();  // Called after reload
    // ... game entry points called by engine
};

// game/game.cpp
extern "C" __declspec(dllexport) GameAPI* get_game_api() {
    static GameAPI api = {
        game_init,
        game_update,
        game_render,
        game_shutdown,
        game_on_hot_reload
    };
    return &api;
}
```

**Keep this interface minimal and stable**—changing it requires engine restart.

---

## Shared Directory Purpose

```
shared/
├── game_interface.hpp    # Engine ↔ Game contract
├── shared_types.hpp      # Common data structures
└── constants.hpp         # Shared constants
```

Makes hot-reload boundary explicit. Prevents circular dependencies.

---

## Build Considerations

### Debug (Fast Iteration)
- Compile engine once
- Compile game DLL on every change
- Hot-reload without restart

### Release (Optimized)
- Unity build for engine
- Link game statically (no DLL)
- Full optimizations

### Build Script Example

```batch
@echo off
set COMMON_FLAGS=/nologo /W4 /EHsc /I libs /I shared

if "%1"=="debug" (
    set FLAGS=%COMMON_FLAGS% /Od /Zi /DDEBUG
) else (
    set FLAGS=%COMMON_FLAGS% /O2 /DNDEBUG
)

:: Engine (rebuild only if needed)
cl %FLAGS% /Fe:bin/engine.exe engine/*.cpp

:: Game DLL (fast, frequent rebuilds)
cl %FLAGS% /LD /Fe:bin/game.dll game/*.cpp
```

---

## File Naming Convention

Suggested prefixes for clarity:

| Prefix | Meaning |
|--------|---------|
| `qg_*` | Engine/core modules |
| `gv_*` | Gravnic game-specific |
| `ui_*` | UI related |
| `net_*` | Networking |

Or drop prefixes entirely since directories provide context:
- `engine/memory.hpp` vs `include/qg_memory.hpp`

---

## Summary

| Aspect | Old (Header/Impl Split) | New (Logical Split) |
|--------|------------------------|---------------------|
| File locality | Poor | Good |
| Hot-reload | Awkward | Natural |
| Build speed | Slower | Faster (game-only builds) |
| Dependency clarity | Implicit | Explicit |
| Unity builds | Harder | Easy |

**Recommendation**: Adopt the logical split. The hot-reload workflow alone makes it worthwhile for rapid iteration.

---

*Last updated: [Current conversation date]*
