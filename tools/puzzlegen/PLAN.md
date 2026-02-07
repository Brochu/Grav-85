# Plan: puzzlegen CLI Tool

## Context

Grav-85 needs procedurally generated puzzle bundles (5 levels each) for competitive multiplayer. Players race through bundles, so we need verified-solvable puzzles graded by difficulty, arranged in escalating curves within each bundle, with multiple bundle difficulty tiers for different skill levels.

## Language & Approach

**C++17** — reuses engine modules directly (qg_memory, qg_random, qg_parse, qg_math). Config module forked with `fopen` replacing SDL file I/O.

**Generation strategy:** Random + verify (generate candidates, BFS-solve, score, assemble). Simpler than backwards construction and sufficient for offline batch generation.

## File Structure

```
tools/puzzlegen/
    ~puzzlegen.cpp      # Unity build (includes engine modules + pg_*.cpp)
    pg_main.cpp         # main(), CLI args, orchestration
    pg_config.cpp       # Forked config_init with fopen/fread (no SDL)
    pg_sim.cpp          # Headless gravity sim + combo detection (flood fill)
    pg_solver.cpp       # BFS solver with state hashing
    pg_gen.cpp          # Random puzzle generation
    pg_difficulty.cpp   # Difficulty scoring
    pg_bundle.cpp       # Bundle assembly (5 levels, difficulty curve)
    pg_level_io.cpp     # Read/write 108-byte level binary format
    puzzlegen.cfg       # Default config with all generation knobs
```

### Unity build (`~puzzlegen.cpp`)

```cpp
// Engine modules (SDL-free)
#include "../../engine/qg_memory.cpp"
#include "../../engine/qg_random.cpp"
#include "../../engine/qg_parse.cpp"

// Puzzlegen modules
#include "pg_config.cpp"
#include "pg_level_io.cpp"
#include "pg_sim.cpp"
#include "pg_solver.cpp"
#include "pg_gen.cpp"
#include "pg_difficulty.cpp"
#include "pg_bundle.cpp"
#include "pg_main.cpp"
```

## Key Modules

### 1. `pg_config.cpp` — Config loader (forked from `engine/qg_config.cpp`)

Replace `SDL_LoadFile`/`SDL_free` with `fopen`/`fread`/`free`. Same parsing logic, same `config`/`config_value` structs from `engine/qg_config.hpp`.

### 2. `pg_sim.cpp` — Headless simulation

Reuse `level` struct layout from `grav/gr_main.cpp`. New headless `sim_state`:

```cpp
struct sim_state {
    ivec2 crates[ELEMENTS_MAX_NUM];
    ivec2 gems[ELEMENTS_MAX_NUM];
    direction current_gravity;
    u32 gems_active;    // bitmask
    i8 num_crates, num_gems;
};
```

Key functions:
- `sim_init(sim_state*, level*)` — copy start positions
- `sim_apply_gravity(sim_state*, level*, direction)` — port `attempt_gravity_change` logic (sort by distance, slide until collision), no animation offsets
- `sim_check_combos(sim_state*, level*)` — **flood fill**: for each active gem, BFS to find connected same-color gems (4-dir adjacency). Components of size >= 2 are deactivated. Returns true if any matched.
- `sim_apply_move(sim_state*, level*, direction)` — full move with chain reactions:
  ```
  apply_gravity(dir)
  loop:
    if !check_combos() → break
    apply_gravity(current_gravity)  // re-settle under current gravity
  ```
- `sim_is_solved(sim_state*)` — `gems_active == 0`

### 3. `pg_solver.cpp` — BFS solver

```cpp
struct solve_result {
    bool solvable;
    i32 optimal_moves;
    i32 states_explored;
    direction solution[64];
};
```

Algorithm:
1. **BFS** over `sim_state`, trying all 4 directions per state
2. Skip moves where `dir == current_gravity` (no-op)
3. State hash: FNV-1a over sorted element positions + gems_active + gravity
4. Visited set: `std::unordered_set<u64>`, cap at 2M entries (abandon if exceeded)
5. Depth limit: configurable `max_solve_moves` (default 15)

### 4. `pg_gen.cpp` — Random puzzle generation

Config-driven parameters:
- Grid dimensions range, gem count range, crate count range
- Number of colors (1-3), wall density range
- Gem count per color can be even or odd (3+ adjacent matches are valid)
- Border cells always solid
- Interior walls placed randomly by density

### 5. `pg_difficulty.cpp` — Scoring

Weighted composite score (0.0–1.0):

| Metric | Weight | Normalization |
|---|---|---|
| Optimal moves | 0.45 | [1, max_solve_moves] |
| Gem count | 0.20 | [2, 16] |
| Color count | 0.15 | [1, 3] |
| Grid density | 0.20 | [0.1, 0.5] |
| Odd-color matches | +0.05 bonus | Per color requiring 3+ group match |

All weights configurable via `puzzlegen.cfg`. Puzzles where a gem color has an odd count require at least one 3+ adjacent group to clear — this inherently makes the puzzle harder and adds a difficulty bonus.

### 6. `pg_bundle.cpp` — Bundle assembly

- Sort puzzle pool by difficulty score
- For a given tier (e.g. `[0.25, 0.60]`), divide range into 5 slots
- Pick one puzzle per slot → escalating difficulty within bundle
- Bundle tiers defined in config: easy, medium, hard, expert

### 7. `pg_level_io.cpp` — Binary I/O

- `level_write_binary(level*, u8[108])` — pack to 108-byte format
- `level_read_binary(u8[108], level*)` — unpack (port from `level_file_init` in gr_main.cpp)
- `bundle_write(bundle*, path)` — write 5 × 108 = 540 bytes raw
- Metadata (difficulty scores, optimal moves) written to separate `.txt` sidecar

### 8. `pg_main.cpp` — CLI

```
Usage: puzzlegen.exe [options]
  -c <path>    Config file (default: puzzlegen.cfg)
  -n <count>   Number of puzzles to generate
  -t <tier>    Bundle tier: easy|medium|hard|expert
  -s <seed>    RNG seed (0 = random)
  -o <dir>     Output directory
  -v           Verbose output
```

Flow: parse args → load config → seed RNG → generate pool (generate + solve + score) → sort by difficulty → assemble bundles → write files → print summary.

## Config File (`puzzlegen.cfg`)

```ini
# Generation
seed = 0
num_puzzles = 100
max_attempts = 1000

# Grid
grid_width = [6, 10]
grid_height = [6, 10]

# Elements
num_gems = [4, 12]
num_crates = [0, 4]
num_colors = [2, 3]
wall_density = [15, 35]

# Solver
max_solve_moves = 15
max_visited_states = 2000000

# Difficulty weights (sum to 100)
weight_moves = 45
weight_gems = 20
weight_colors = 15
weight_density = 20

# Bundle tiers [min, max] (0-100 scale)
bundle_tier_easy = [0, 30]
bundle_tier_medium = [25, 60]
bundle_tier_hard = [50, 85]
bundle_tier_expert = [75, 100]

# Output
output_dir = "bundles"
bundle_tier = "medium"
```

## Build Integration (`config.xml`)

Add target:
```xml
<Target Name="puzzlegen">
    <Exec Command="cl $(c_flags) tools\puzzlegen\~puzzlegen.cpp" />
    <Exec Command="link -NOLOGO -DEBUG $(build_dir)\~puzzlegen.obj -out:$(bin_dir)\puzzlegen.exe" />
</Target>
```

Add to clean_files: `$(bin_dir)\puzzlegen*.*`

Invoked via: `compile puzzlegen`

## Implementation Order

1. **pg_level_io.cpp** — binary read/write. Verify by round-tripping `level-demo.bin`.
2. **pg_config.cpp** — forked config loader with fopen.
3. **pg_sim.cpp** — headless gravity + flood fill combos. Test on demo level.
4. **pg_solver.cpp** — BFS solver. Verify demo level is solvable.
5. **pg_gen.cpp** — random puzzle generation.
6. **pg_difficulty.cpp** — scoring formula.
7. **pg_bundle.cpp** — bundle assembly.
8. **pg_main.cpp** — CLI orchestration.
9. **Build target** in config.xml + puzzlegen.cfg defaults.

## Verification

1. Round-trip test: read `assets/level-demo.bin` → write back → diff (byte-identical)
2. Solver test: solve `level-demo.bin`, confirm solvable with expected move count
3. Generation test: generate 50 puzzles with `-v`, verify all reported as solvable
4. Bundle test: generate bundles, load in game (or level editor) to spot-check
5. Config test: modify weights/ranges in .cfg, verify output changes accordingly

## Key Files Referenced

- `grav/gr_main.cpp` — level struct, gravity logic to port, constants
- `engine/qg_config.cpp` — config parser to fork (lines 10-80)
- `engine/qg_config.hpp` — config types/API (reused directly)
- `engine/qg_random.cpp` — RNG (included directly, no SDL)
- `engine/qg_memory.cpp` — arena allocator (included directly, no SDL)
- `engine/qg_parse.cpp` — string views (included directly, no SDL)
- `engine/qg_math.hpp` — ivec2, direction, direction_vectors
- `shared/shared_types.hpp` — u8, i32, f32, etc.
- `config.xml` — add build target
- `docs/brainstorm.md` — level format spec
