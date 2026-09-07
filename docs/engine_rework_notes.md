# Engine rework notes

Derived from a read of the DOOM 3 BFG source (`neo/framework`, `neo/d3xp`), comparing
how id separates engine from game logic against how Grav-85 currently does it.

Reference points in that codebase, if needed again:
- `d3xp/Game.h` — the entire engine/game contract, 343 lines
- `d3xp/Game_local.cpp:118` — `GetGameAPI`, the handshake
- `framework/Common.cpp:817` — `LoadGameDLL`, the engine side of it
- `framework/common_frame.cpp:327` — `ProcessGameReturn`, control flow coming back
- `d3xp/gamesys/SaveGame.cpp` — state serialization owned by the game
- `renderer/RenderSystem.h` vs `renderer/tr_local.h` — public/private header split

## The five principles worth stealing

1. **Interfaces publish no layout.** `idRenderSystem` is pure virtual with zero data
   members, so game code bakes in no field offsets. Our `engine_api` is already a
   hand-rolled vtable and gets this for free — except we also hand out `event_bus*`,
   `input_state*`, `mem_arena*` with their definitions visible, which throws it away.
2. **Version the contract and fail loud.** `GAME_API_VERSION` is checked on both
   sides; mismatch is `FatalError`. We check nothing.
3. **One-way authority.** The game cannot change maps or quit. It fills
   `gameReturn_t` and the engine interprets it. That is what lets `RunFrame` run on
   a worker thread at all.
4. **Never persist a pointer across a boundary.** `idSaveGame::WriteObject` writes
   an index into an object list, never an address. Restore is two-phase: create all
   objects, then fill them in.
5. **The owner of the data owns its serialization.** The engine passes a byte sink
   and a version integer and knows nothing about layout.

DOOM does not hot-reload. But its save/restore cycle is the same problem — state
crossing a boundary to code that may differ — and the shape of its answer (byte
stream + version, not shared allocation) is what we should copy.

## Decisions already made — do not relitigate

- **Keep free functions taking the system pointer first.** Pointer-to-member is not
  a function pointer, so anything dispatched through a table must be a free function.
  Methods also need the layout at the call site, which conflicts with opaque types.
  Non-virtual methods compile identically anyway.
- **Do not port to abstract classes.** We would take on vtable slot ordering and
  vptr-in-instance problems to gain a property `engine_api` already has. A vptr baked
  into a surviving object points into unloaded code after a reload; a function-pointer
  table can just be re-pointed.
- **C at the boundary, C++ inside a module.** Vtable layout, name mangling, template
  instantiation, `inline` linkage and exceptions are all compiler-defined and not
  stable across separately-compiled modules. That is why COM, Vulkan's loader and
  DOOM's own handshake all converge on `extern "C"` + function pointers + handles.
- **Three zones, only two are constrained:**
  - *State* (`game_state`, `level`, `attempt`, `match`) — POD required, because hot
    reload and saves both need a relocatable, trivially-copyable, code-pointer-free
    block. This is a consequence of wanting hot reload, not a style preference.
  - *Boundary* (`engine_api`, `game_api`) — C ABI required.
  - *Everything else* (level gen, parsing, engine internals, `tools/`) — unconstrained.
    Use whatever reads best. Applying the near-C style uniformly costs productivity
    here for no architectural benefit.

## Phase 0 — standalone bugs

Independent of any restructuring; fix first so later work isn't debugging these.

1. `mem_arena_init` uses `qg_malloc`, so arena memory is uninitialized.
   `g_s->phase` at `grav/gr_main.cpp:437` reads garbage and the `INIT` branch fires
   at random. Zero the block, or zero `game_state` explicitly after allocating.
2. `mem_arena_init` never sets `arena->gen` (`engine/qg_memory.cpp`). Every
   `arena_ptr` it returns carries an indeterminate generation. Self-consistent by
   luck today.
3. `match_read_level` sets `lvl->start_gravity = direction::COUNT`
   (`grav/gr_main.cpp:253`), and `grav_draw` indexes the 4-element
   `direction_vectors[]` with it (`grav/gr_main.cpp:521`) before any input corrects
   it. Out-of-bounds read on frame 1.
4. `extern engine_api g_eng;` (`shared/qg_shared.hpp:84`) is declared and never defined.
   The only `g_eng` is a local in `main()` (`engine/qg_main.cpp:113`), so
   `bus_fire_event` works at `engine/qg_main.cpp:176` only by scope accident and
   cannot link from the game DLL at all.
5. `event_type` comments say ranges 0-499 / 500-2047; the values are 512 / 1024.
6. `event_bus` is ~410 KB as a `main()` local (`handlers[1024][16]`). Move it off
   the stack or shrink the type count.
7. `enum element_type : u8 { CRATE, GEM, COUNT };` is unscoped, sitting next to
   `enum class color`. Pick one.

## Phase 1 — header split (do this before anything else)

It reveals exactly what the boundary is instead of us guessing, and turns the
boundary from discipline into a compile error.

`config.xml` currently uses one `<includes>` for both targets, so `grav/~grav.cpp`
compiles with `-Iengine\` and can reach every layout we own. Split it:

    <game_includes>-Igrav\ -Ishared\ -Ilibs\SDL3-3.4.0\</game_includes>
    <engine_includes>-Iengine\ -Ishared\ -Ilibs\SDL3-3.4.0\</engine_includes>

The driver is **types, not MODULE_DEFs**. The `*_MODULE_DEF` lists are just name
lists and are already fine where they are. What has to move is the public types the
game touches, because those currently sit in `engine/qg_*.hpp` next to the layouts we
want hidden. Per module:

| module | public types | to hide |
|---|---|---|
| bus | `event_type`, `handler_id`, `event_handler_fn` | `event_bus` |
| config | `value_type`, `config_value` | `config` |
| input | `key_code` | `input_state` |
| memory | `arena_ptr`, `arena_off` | `mem_arena` |
| parse | `strview`, `SV_FMT`/`SV_ARG`, `sv()` | nothing - no private state |
| random | none | nothing |

Target shape: **no new files, no renames.** Move the public types up into
`shared/qg_shared.hpp`. Delete them from the engine headers, which keep everything else
and include `qg_shared.hpp` for the types they still reference.

    shared/
      qg_shared_types.hpp  unchanged - u8/i32/f32
      qg_math.hpp          moved from engine/ as-is - pure value types, no engine state
      qg_shared.hpp        the public types from the table above, added alongside the
                           MODULE_DEFs and engine_api it already holds (~101 -> ~180 lines)

    engine/
      qg_bus.hpp         struct event_bus + prototypes for bus_init/fire/process/...
      qg_config.hpp      struct config + its prototypes
      qg_input.hpp       struct input_state + prototypes, incl. input_init/update/handle_key
      qg_memory.hpp      struct mem_arena + align_fwd + its prototypes
      qg_parse.hpp       prototypes only - no private state to hide
      qg_random.hpp      prototypes + rand_weighted_index(T*, i32) - see below

Engine headers keep the real function prototypes, not just the layouts: the MODULE_DEF
X-macro generates struct *fields*, so the engine still needs actual declarations to
take `&bus_fire` when filling the table.

The enforcement is entirely the include path, not the file count. One public header
per module would work identically, and splitting later is mechanical - it moves
declarations between files and touches no call sites. Not worth it now: the usual
argument for splitting headers is incremental compile time, and `~grav.cpp` is a
unity build, so header granularity buys nothing.

Found while checking this: `rand_weighted_index<T>(T*, i32)` calls `rand_float01()`
directly - an ambient name resolved at link time, and `qg_random.cpp` is engine-only.
The game gets an unresolved external if it includes that header. Same latent break as
`bus_fire_event`. The other overload, `rand_weighted_index(f32 roll, T*, i32)`,
touches only its parameters and is safe to publish - exactly the `arena_at<T>` vs
`mem_arena_at<T>` distinction. Move the roll-taking one to `qg_shared.hpp`, leave the
other engine-side.

The game does not need engine headers for *functions* - it already calls everything
through `g_api.*`. It includes them today only for *types*, and those are what move.

Result: `event_bus` and `input_state` become opaque to the game immediately.
`mem_arena` follows once `mem_arena_at` moves behind the table (Phase 2, item 11);
`config` follows once it stops being embedded by value in `game_state` (Phase 3,
item 18).

Frozen public list - these cross by value or are read directly, so their layouts are
ABI from here on: `event_type`, `handler_id`, `event_handler_fn`, `key_code`,
`config_value`, `strview`, `arena_ptr`, `arena_off`. Small and deliberate.

## Phase 2 — boundary

8. **Version handshake.** `u32 version` as the first field of `engine_api`, checked
   in `grav_init`, which returns `bool` so the engine can fail loud. Since the
   `*_MODULE_DEF` lists are literal name lists, hash them at compile time and check
   the hash too — that catches reordering, which a hand-bumped integer will not.
9. **Symmetric handshake.** Replace the five exported symbols with one:
   `extern "C" GRAV_API game_api *grav_get_api(engine_api *engine);`
   Reload becomes: unload, load, call it again, replace one pointer. Today the engine
   caches five `GetProcAddress` results that would all need re-fetching.
10. **Single `on_event` in `game_api`; drop game-side `bus_subscribe`.** Every
    handler the game registers is an untracked pointer into DLL code sitting in
    engine-owned memory — dangling on the first `bus_process` after a reload. One
    entry point, refreshed atomically with the table, and the game dispatches
    internally. Losing per-type filtering at the bus is not a real cost here.
11. **Move `mem_arena_at<T>` behind the table.** Add
    `X(void*, mem_arena_at, (mem_arena*, arena_off))` to `MEMORY_MODULE_DEF`; the
    generation assert moves into the engine, where the field lives. Keep the
    ergonomics with a wrapper that touches only its parameters and the table:

        template<class T>
        inline T *arena_at(engine_api *api, mem_arena *a, arena_off off) {
            return (T *)api->mem_arena_at(a, off);
        }

12. **Fix `bus_fire_event` to take the api explicitly** rather than the phantom
    `g_eng`. Keep the `sizeof(data)` at the call site — payload crossing as opaque
    bytes plus a length is the correct shape.
13. **Name the sub-structs** (`api.bus.fire(...)` instead of the anonymous struct)
    so a single system can be passed to a subsystem instead of the whole engine.

The rule behind 11 and 12: an inline helper in a shared header compiles into *both*
binaries, so everything it touches becomes ABI. It may touch only its own parameters
and the table — no ambient globals, no struct fields. DOOM violates this knowingly
(`idCVar::Init` is `ID_INLINE` and touches every field) and pays for it with a
documented per-module rule, a handshake step, and a version check. We are small
enough to just not violate it.

Note: default arguments do not survive the table, so `mem_arena_alloc` needs an
explicit `align` at every call site. We already pass `alignof(...)` everywhere.

## Phase 3 — hot reload and state

The problem is not lifetime, it is layout. We reload because code changed, and code
changes drag struct changes — so the reload where the surviving block matters most is
exactly the reload where its bytes no longer describe the struct we cast to.

14. **Move the four DLL globals into `game_state`.** `g_cfg`, `g_gravity_speed`,
    `g_match`, `player_index` (`grav/gr_main.cpp:373-377`) die on unload today, so
    nothing currently survives and `game_state` holds only `phase`. Without this,
    none of the rest matters.
15. **`layout_id` in the state descriptor.** Replace `grav_state_size()` with
    something returning `{ size, layout_id }`. On reload the engine compares; on
    mismatch it drops the block and cold-inits. Loses the session, never reads
    garbage. Highest value-per-line change available — it converts silent corruption
    into defined behaviour.
16. **Size `g_core` with headroom.** `engine/qg_main.cpp:132` sizes it to exactly
    `game_state_size()`, so a second `grav_init` after reload overflows on its first
    allocation.
17. **Intra-arena pointers become `arena_off`.** `match.levels` and `match.attempts`
    (`grav/gr_main.cpp:237-238`) are raw pointers into `_scratch`. `arena_off` +
    generation guard is the "index not address" pattern we already built and are not
    using. Makes the block relocatable.
18. **`config *` in `game_state` instead of `config` by value**
    (`grav/gr_main.cpp:364`). Otherwise `CONFIG_NUM_KEYS` and ~2 KB of
    `keys[]`/`values[]` stay ABI *and* land inside the surviving block. Let the
    engine own the storage.

Deferred, worth building when we actually want save games: X-macro field lists per
state struct, generating writer, reader and layout hash from one declaration —
DOOM's tier, and it gives reload and saves from the same machinery. Not before.

## Phase 4 — close the boundary leaks

`grav/gr_main.cpp` calls `SDL_SetRenderDrawColor`, `SDL_RenderFillRect`,
`SDL_GetTicksNS`, `fopen_s`/`fread_s`, `printf` directly. `engine_api` carries a raw
`SDL_Renderer *context` as an admitted escape hatch. These are the three things
DOOM's game literally cannot do. The renderer and the filesystem are the two modules
we never wrote, and the two the game uses most.

19. **`render_module`.** Even five entries (`set_color`, `clear`, `fill_rect`,
    `line`, `present`) gets SDL out of the game — and gives us the instanced-render
    rework already TODO'd in `grav_draw` for free.
20. **`file_module`.** `fopen_s` in game code blocks any future asset packing.
21. **A logging entry** so the game stops calling `printf`.

## Phase 5 — build and hygiene

22. **No release target.** `config.xml` has no `-O2`, no `NDEBUG`. One build config
    and it is the debug one. Add it — but after 23, because it is what exposes it.
23. **`assert` is the entire error-handling story.** `mem_arena_alloc`'s
    out-of-memory handling *is* the assert (`engine/qg_memory.cpp:60`); the
    `return {nullptr, gen}` after it is never checked by any caller. The day
    `NDEBUG` exists, every one of those becomes a null pointer flowing into game code.
24. **Fixed capacities need a decision each.** `ELEMENTS_MAX_NUM 32`,
    `CONFIG_NUM_KEYS 128`, `ATTEMPT_MAX_MOVES 99`, `handlers[1024][16]`. Per array:
    impossible-by-construction (document why) or possible (needs a real error path).
25. **All-or-nothing struct initialization.** `mem_arena` has `u8 *base = nullptr;`
    but no initializer on `next`/`cap`/`gen`; `config` has `num_entries = 0` and
    nothing else. Partial NSDMIs are the worst case — they look self-initializing, so
    call sites stop writing `{}`. Given POD-at-the-boundary, drop the NSDMIs and make
    zero-init the caller's job.
26. **No header dependency tracking.** `compile.bat gr` rebuilds only the DLL. Edit a
    shared header, rebuild one target, and the two binaries silently disagree about
    layout. Phase 1 shrinks the exposure to the frozen public type list; Phase 2's
    version hash catches the function-list half. Real dependency tracking
    (`/showIncludes`) would close it properly.
27. **Comment the `-MD` invariant in `config.xml`.** Cross-module `malloc`/`free`
    works only because both targets share one CRT heap. That is a build-flag
    invariant, not a language guarantee.

## Suggested order

Phase 0 → Phase 1 → Phase 2 → Phase 3 → Phase 4 → Phase 5, with 22 after 23.

Phase 1 first is the important one: it makes the compiler tell us where the boundary
actually is, so Phases 2-4 are following errors rather than auditing by hand.
